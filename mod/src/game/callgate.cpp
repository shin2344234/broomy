#include "game/callgate.h"

#include <Windows.h>
#include <cstdio>
#include <cstring>

#include "core/log.h"
#include "game/farhook.h"
#include "game/mem.h"
#include "game/signatures.h"

using namespace bm::sig;

namespace
{
    typedef uint64_t (*FnLeaf)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t);
    FnLeaf        g_original = nullptr;
    volatile LONG g_broomyChosen = 0;   // the last wedge chosen was Broomy's
    volatile LONG g_chosenAt = 0;       // when, for the chart's window

    typedef uint64_t (*FnLookup)(uintptr_t, uintptr_t, uintptr_t, uintptr_t);
    FnLookup g_lookupOriginal = nullptr;
    // The call event's data with Broomy's slot key, one copy per thread; the
    // caller reads it only until it returns.
    constexpr size_t     kDataCopy = 0x100;
    thread_local uint8_t t_data[kDataCopy];
    volatile LONG        g_swaps = 0;

    uint64_t LookupDetour(uintptr_t request, uintptr_t out, uintptr_t actor, uintptr_t flag)
    {
        const uint64_t r = g_lookupOriginal(request, out, actor, flag);
        if (!g_broomyChosen || !out) return r;
        uintptr_t data = 0;
        uint32_t slot = 0;
        if (!bm::mem::ReadPtr(out + kOff_FrameEventOut_Data, &data) || !data ||
            !bm::mem::Read32(data + kOff_CallEvent_SlotKey, &slot) || slot != kSlot_Vehicle)
            return r;
        if (!bm::mem::ReadBytes(data, t_data, kDataCopy)) return r;
        const uint32_t broomy = kSlot_Broomy;
        memcpy(t_data + kOff_CallEvent_SlotKey, &broomy, sizeof broomy);
        *reinterpret_cast<uintptr_t*>(out + kOff_FrameEventOut_Data) = reinterpret_cast<uintptr_t>(t_data);
        if (InterlockedIncrement(&g_swaps) <= 4)
        {
            char hex[0x20 * 3 + 1] = {};
            for (int i = 0; i < 0x20; ++i) snprintf(hex + i * 3, 4, "%02X ", t_data[i]);
            LOG("[call] the call names VehicleSlot; Broomy's slot is called instead (event data %p: %s).",
                reinterpret_cast<void*>(data), hex);
        }
        return r;
    }

    // Slot 78 checks a chart leaf that carries data: (component, pointer to
    // the token pointer, ...). A token is a u32 leaf id and its arguments.
    uint64_t LeafDetour(uintptr_t comp, uintptr_t tokenRef, uintptr_t table, uintptr_t flag, uintptr_t a5,
                        uintptr_t a6, uintptr_t a7, uintptr_t a8)
    {
        const uint64_t r = g_original(comp, tokenRef, table, flag, a5, a6, a7, a8);
        if (!g_broomyChosen || (r & 0xFF) != 1 || !tokenRef ||
            GetTickCount() - static_cast<DWORD>(g_chosenAt) > kCallWindowMs)
            return r;
        uintptr_t token = 0;
        uint32_t leaf[2] = {};
        if (!bm::mem::ReadPtr(tokenRef, &token) || !bm::mem::ReadBytes(token, leaf, sizeof leaf)) return r;
        if (leaf[0] != kLeaf_SlotChosen || leaf[1] != kSlot_Vehicle) return r;
        return r & ~0xFFull;   // "VehicleSlot is the chosen slot"
    }
}

namespace bm::callgate
{
    void Chosen(uint32_t slotKey)
    {
        InterlockedExchange(&g_chosenAt, static_cast<LONG>(GetTickCount()));
        InterlockedExchange(&g_broomyChosen, slotKey == kSlot_Broomy ? 1 : 0);
    }

    bool Install()
    {
        uintptr_t vt[2] = {};
        if (mem::FindVtablesByName(kRtti_ClientChartComponent, vt, 2) != 1)
        {
            LOG_ERR("[call] the client's character control component was not found, so Broomy's wedge cannot call.");
            return false;
        }
        const uintptr_t at = vt[0] + kSlot_ChartDataLeaf * 8;
        uintptr_t fn = 0;
        if (!mem::ReadPtr(at, &fn) || !mem::InImage(fn))
        {
            LOG_ERR("[call] the client's chart leaf check is not where it was, so Broomy's wedge cannot call.");
            return false;
        }
        g_original = reinterpret_cast<FnLeaf>(fn);
        DWORD old = 0;
        if (!VirtualProtect(reinterpret_cast<void*>(at), 8, PAGE_READWRITE, &old))
        {
            LOG_ERR("[call] could not patch the client's chart leaf check.");
            return false;
        }
        InterlockedExchangePointer(reinterpret_cast<void* volatile*>(at), reinterpret_cast<void*>(&LeafDetour));
        VirtualProtect(reinterpret_cast<void*>(at), 8, old, &old);
        const uintptr_t lookup = mem::Find(kSig_FrameEventLookup);
        char why[128] = {};
        if (!lookup || !farhook::Install("frame event lookup", lookup, reinterpret_cast<void*>(&LookupDetour),
                                         reinterpret_cast<void**>(&g_lookupOriginal), why, sizeof why))
        {
            LOG_ERR("[call] the frame event lookup %s, so a Broomy call would call VehicleSlot instead.",
                    lookup ? why : "was not found");
            return false;
        }
        LOG("[call] ready: Broomy's wedge takes the chart's VehicleSlot call branch (leaf check +0x%llX) and calls "
            "Broomy's slot (frame event lookup +0x%llX).", static_cast<unsigned long long>(mem::Rva(fn)),
            static_cast<unsigned long long>(mem::Rva(lookup)));
        return true;
    }
}
