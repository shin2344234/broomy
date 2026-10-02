#include "game/riderfix.h"

#include <Windows.h>
#include <cstring>

#include "core/log.h"
#include "game/farhook.h"
#include "game/gamefile.h"
#include "game/mem.h"

namespace
{
    // The branch check: (chart component, u32* error, u8 layer, ..., 8th
    // argument a pointer whose first field is the branch record). The error
    // is 0 when the branch may be taken. The record's target action hash is
    // at +0x14.
    constexpr uintptr_t kRva_BranchCheck = 0x232E3D0;
    constexpr uint8_t kBranchCheckHead[] = { 0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x10, 0x4C, 0x89, 0x48, 0x20, 0x44,
                                             0x88, 0x40, 0x18, 0x48, 0x89, 0x48, 0x08 };

    typedef uint64_t (*FnCheck)(uintptr_t, uint32_t*, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
                                uintptr_t, uintptr_t, uintptr_t, uintptr_t);
    FnCheck g_original = nullptr;

    struct Last { uintptr_t comp; uint8_t layer; uint32_t target; };
    constexpr int kSlots = 64;
    Last g_last[kSlots] = {};
    int g_used = 0;
    SRWLOCK g_lock = SRWLOCK_INIT;
    volatile LONG g_lines = 0;
    bool g_log = false;

    // The rider fix: the two default branches of Kliff's lower dispatchers,
    // by their offset in ride_test3_lower.paac, with the shipped target and
    // the broom state they take while Broomy is ridden.
    struct Swap { uint32_t offset; uint32_t shipped; uint32_t broomy; };
    constexpr Swap kSwaps[] = {
        { 0x6F3F1 + 0x14, 0x07407E6A, 0x588C2001 },   // branch 397, ground states -> broom idle
        { 0x6F48D + 0x14, 0x90337E5C, 0xAD3E621B },   // branch 400, air states -> broom air idle
    };
    constexpr DWORD kRiddenMs = 3000;
    volatile uintptr_t g_lowerBase = 0;
    volatile uint32_t g_lowerSize = 0;
    volatile uintptr_t g_rideOnBase = 0;
    volatile uint32_t g_rideOnSize = 0;
    volatile LONG g_rideOnAt = 0;
    volatile LONG g_swapped = 0;   // 1 while the broom targets are in
    SRWLOCK g_swapLock = SRWLOCK_INIT;

    bool In(uintptr_t a, uintptr_t base, uint32_t size) { return base && a >= base && a < base + size; }

    void SetSwap(bool on)
    {
        AcquireSRWLockExclusive(&g_swapLock);
        const uintptr_t base = g_lowerBase;
        if (base && (g_swapped != 0) != on)
        {
            bool ok = true;
            for (const Swap& w : kSwaps)
            {
                uint32_t* at = reinterpret_cast<uint32_t*>(base + w.offset);
                const uint32_t want = on ? w.broomy : w.shipped, was = on ? w.shipped : w.broomy;
                if (*at == was) *at = want;
                else if (*at != want) ok = false;
            }
            InterlockedExchange(&g_swapped, on ? 1 : 0);
            LOG(ok ? "[rider] Kliff's lower chart %s." : "[rider] Kliff's lower chart %s, but a branch held other bytes.",
                on ? "takes his broom states (Broomy is ridden)" : "is back to its shipped targets");
        }
        ReleaseSRWLockExclusive(&g_swapLock);
    }

    uint64_t CheckDetour(uintptr_t comp, uint32_t* err, uintptr_t layer, uintptr_t a4, uintptr_t a5, uintptr_t a6,
                         uintptr_t a7, uintptr_t a8, uintptr_t a9, uintptr_t a10, uintptr_t a11, uintptr_t a12)
    {
        uintptr_t pre = 0;
        if (a8 && bm::mem::ReadPtr(a8, &pre))
        {
            if (In(pre, g_rideOnBase, g_rideOnSize)) InterlockedExchange(&g_rideOnAt, static_cast<LONG>(GetTickCount()));
            else if (In(pre, g_lowerBase, g_lowerSize))
            {
                const LONG at = g_rideOnAt;
                const bool ridden = at && GetTickCount() - static_cast<DWORD>(at) < kRiddenMs;
                if (ridden != (g_swapped != 0)) SetSwap(ridden);
            }
        }
        const uint64_t r = g_original(comp, err, layer, a4, a5, a6, a7, a8, a9, a10, a11, a12);
        if (!g_log) return r;
        uintptr_t rec = 0;
        uint32_t code = 1, target = 0;
        if (!a8 || !err || !bm::mem::Read32(reinterpret_cast<uintptr_t>(err), &code) || code != 0) return r;
        if (!bm::mem::ReadPtr(a8, &rec) || !bm::mem::Read32(rec + 0x14, &target)) return r;
        const uint8_t l = static_cast<uint8_t>(layer);
        bool fresh = false;
        AcquireSRWLockExclusive(&g_lock);
        int i = 0;
        while (i < g_used && !(g_last[i].comp == comp && g_last[i].layer == l)) ++i;
        if (i == g_used && g_used < kSlots) g_last[g_used++] = { comp, l, 0 };
        if (i < g_used && g_last[i].target != target)
        {
            g_last[i].target = target;
            fresh = true;
        }
        ReleaseSRWLockExclusive(&g_lock);
        if (fresh && InterlockedIncrement(&g_lines) <= 20000)
            LOG("[state] comp %p layer %u -> %08X (record %p)", reinterpret_cast<void*>(comp), l, target,
                reinterpret_cast<void*>(rec));
        return r;
    }

    void Seen(const char* path, const uint8_t* data, uint32_t size)
    {
        const size_t n = strlen(path);
        if (n < 5 || _stricmp(path + n - 5, ".paac") != 0) return;
        if (!strstr(path, "ride") && !strstr(path, "riding")) return;
        if (g_log) LOG("[state] chart %s at %p (%u bytes)", path, data, size);
        if (!strstr(path, "/ride_test3_lower.paac")) return;
        bool shipped = true;
        for (const Swap& w : kSwaps)
            shipped = shipped && w.offset + 4 <= size &&
                      *reinterpret_cast<const uint32_t*>(data + w.offset) == w.shipped;
        AcquireSRWLockExclusive(&g_swapLock);
        g_lowerBase = shipped ? reinterpret_cast<uintptr_t>(data) : 0;
        g_lowerSize = size;
        InterlockedExchange(&g_swapped, 0);
        ReleaseSRWLockExclusive(&g_swapLock);
        if (!shipped) LOG_ERR("[rider] ride_test3_lower.paac is not the shipped file, so Kliff keeps the dragon pose.");
    }
}

namespace bm::riderfix
{
    void NoteServed(const char* path, uintptr_t base, uint32_t size)
    {
        if (!strstr(path, "m0002_armadillo.paac")) return;
        g_rideOnSize = size;
        g_rideOnBase = base;
    }

    bool Install(bool logStates)
    {
        g_log = logStates;
        const uintptr_t at = bm::mem::Game().base + kRva_BranchCheck;
        uint8_t head[sizeof kBranchCheckHead] = {};
        if (!bm::mem::ReadBytes(at, head, sizeof head) || memcmp(head, kBranchCheckHead, sizeof head) != 0)
        {
            LOG_ERR("[rider] the branch check is not at +0x%llX on this exe.", static_cast<unsigned long long>(kRva_BranchCheck));
            return false;
        }
        char why[160] = {};
        if (!bm::farhook::Install("branch check", at, reinterpret_cast<void*>(&CheckDetour),
                              reinterpret_cast<void**>(&g_original), why, sizeof why))
        {
            LOG_ERR("[rider] could not hook the branch check: %s", why);
            return false;
        }
        bm::gamefile::Observe(&Seen);
        LOG(logStates ? "[rider] Kliff rides Broomy in his broom poses; each chart layer's next action is logged."
                      : "[rider] Kliff rides Broomy in his broom poses.");
        return true;
    }
}
