#include "game/callgate.h"

#include <Windows.h>

#include "core/log.h"
#include "game/broomy.h"
#include "game/farhook.h"
#include "game/mem.h"
#include "game/signatures.h"

using namespace bm::sig;

namespace
{
    volatile LONG g_broomyChosen = 0;   // the chosen wedge is the saddle wedge, and it holds Broomy

    typedef uint64_t (*FnSlotChange)(uintptr_t, uint32_t*, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
                                     uintptr_t);
    FnSlotChange  g_slotOriginal = nullptr;
    volatile LONG g_slotLogged = 0;

    // Every press of H sends the chosen wedge twice before the call branch is
    // checked, so this is current by then, the first press after a load too.
    uint64_t SlotChangeDetour(uintptr_t component, uint32_t* error, uintptr_t slotKey, uintptr_t data, uintptr_t a5,
                              uintptr_t a6, uintptr_t a7, uintptr_t a8)
    {
        const uint64_t r = g_slotOriginal(component, error, slotKey, data, a5, a6, a7, a8);
        uint32_t err = 0;
        if (error) bm::mem::Read32(reinterpret_cast<uintptr_t>(error), &err);
        const uint32_t slot = static_cast<uint32_t>(slotKey);
        if (err || (slot != kSlot_Vehicle && slot != kSlot_Mechanic && slot != kSlot_Dragon)) return r;
        uint16_t list = 0xFFFF;
        if (data) bm::mem::Read16(data + kOff_SlotData_ListIndex, &list);
        const bool broomy = slot == kSlot_Vehicle && list == bm::broomy::ListIndex();
        const LONG was = InterlockedExchange(&g_broomyChosen, broomy ? 1 : 0);
        if (broomy != (was != 0) && InterlockedIncrement(&g_slotLogged) <= 20)
            LOG("[call] the chosen wedge is now %s (reserve slot %u, mercenary list %u).",
                broomy ? "the saddle wedge with Broomy" : "another wedge or mount", slot, list);
        return r;
    }

    typedef uint64_t (*FnLeaf)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t);
    FnLeaf        g_leafOriginal = nullptr;
    volatile LONG g_closed = 0;
    volatile LONG g_closedAt = 0;

    // While Broomy is chosen, Kliff has not learned Skill_CallDragon as far as
    // his charts can tell, so the falling call's branches stay shut. A ground
    // press asks it too, in the on-foot dragon branch, which fails anyway on
    // the chosen wedge; a held press asks it every frame.
    uint64_t LeafDetour(uintptr_t comp, uintptr_t tokenRef, uintptr_t a3, uintptr_t a4, uintptr_t a5, uintptr_t a6,
                        uintptr_t a7, uintptr_t a8)
    {
        const uint64_t r = g_leafOriginal(comp, tokenRef, a3, a4, a5, a6, a7, a8);
        if (!g_broomyChosen || (r & 0xFF) != 0 || !tokenRef) return r;
        uintptr_t token = 0;
        uint32_t leaf[2] = {};
        if (!bm::mem::ReadPtr(tokenRef, &token) || !bm::mem::ReadBytes(token, leaf, sizeof leaf)) return r;
        if (leaf[0] != kLeaf_Skill || leaf[1] != kSkill_CallDragon) return r;
        const DWORD now = GetTickCount();
        if (now - static_cast<DWORD>(g_closedAt) > 1000 && InterlockedIncrement(&g_closed) <= 10)
        {
            InterlockedExchange(&g_closedAt, static_cast<LONG>(now));
            LOG("[call] a chart asked whether Kliff can call Blackstar and was told no, since the saddle wedge holds "
                "Broomy.");
        }
        return (r & ~0xFFull) | 1;
    }

    bool HookLeaf()
    {
        uintptr_t vt[2] = {};
        if (bm::mem::FindVtablesByName(kRtti_ClientChartComponent, vt, 2) != 1)
        {
            LOG_ERR("[call] the client's character control component was not found, so a call while falling with "
                    "Broomy chosen brings Blackstar.");
            return false;
        }
        const uintptr_t at = vt[0] + kSlot_ChartDataLeaf * 8;
        uintptr_t fn = 0;
        DWORD old = 0;
        if (!bm::mem::ReadPtr(at, &fn) || !bm::mem::InImage(fn) ||
            !VirtualProtect(reinterpret_cast<void*>(at), 8, PAGE_READWRITE, &old))
        {
            LOG_ERR("[call] the client's chart leaf check could not be hooked, so a call while falling with Broomy "
                    "chosen brings Blackstar.");
            return false;
        }
        g_leafOriginal = reinterpret_cast<FnLeaf>(fn);
        InterlockedExchangePointer(reinterpret_cast<void* volatile*>(at), reinterpret_cast<void*>(&LeafDetour));
        VirtualProtect(reinterpret_cast<void*>(at), 8, old, &old);
        LOG("[call] a call while falling is shut while the saddle wedge holds Broomy (leaf check +0x%llX).",
            static_cast<unsigned long long>(bm::mem::Rva(fn)));
        return true;
    }
}

namespace bm::callgate
{
    bool BroomyChosen() { return g_broomyChosen != 0; }

    bool Install()
    {
        const uintptr_t at = bm::mem::Find(kSig_ReserveSlotChange);
        char why[128] = {};
        if (!at || !bm::farhook::Install("reserve slot change", at, reinterpret_cast<void*>(&SlotChangeDetour),
                                         reinterpret_cast<void**>(&g_slotOriginal), why, sizeof why))
        {
            LOG_ERR("[call] the reserve slot change %s, so the plugin cannot tell when Broomy is chosen.",
                    at ? why : "was not found");
            return false;
        }
        LOG("[call] ready: the saddle wedge calls Broomy itself; the plugin notes when it holds Broomy (+0x%llX).",
            static_cast<unsigned long long>(bm::mem::Rva(at)));
        return HookLeaf();
    }
}
