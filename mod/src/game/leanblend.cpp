#include "game/leanblend.h"

#include <Windows.h>
#include <cstdint>

#include "core/log.h"
#include "game/broomblend.h"
#include "game/farhook.h"
#include "game/mem.h"
#include "game/signatures.h"

using namespace bm::sig;

namespace
{
    typedef float (*FnUpdate)(uintptr_t blend, float dt, const float* measured, float scale, uint8_t flag);
    FnUpdate g_original = nullptr;

    // Kliff's lean space, once seen (the space's +0x28, as a blend holds it).
    volatile uintptr_t g_space = 0;

    // The lean blends updated lately, so a blend's first update is known.
    // Kliff's dispatchers start his broom idle again each time Broomy's state
    // changes, and the motion it fades from is still updated for 10 frames.
    constexpr int kSeen = 8;
    uintptr_t g_seen[kSeen] = {};
    int g_next = 0;
    SRWLOCK g_lock = SRWLOCK_INIT;

    bool IsLean(uintptr_t ref)
    {
        if (!ref) return false;
        if (ref == g_space) return true;
        const uintptr_t space = ref - kOff_Blend_Space;
        uint32_t n = 0, t0 = 0, t1 = 0;
        uintptr_t dims = 0, d0 = 0, d1 = 0;
        float s = 0;
        if (!bm::mem::Read32(space + kOff_Space_DimCount, &n) || n != 2 ||
            !bm::mem::ReadPtr(space + kOff_Space_Dims, &dims) || !bm::mem::ReadPtr(dims, &d0) ||
            !bm::mem::ReadPtr(dims + 8, &d1) || !bm::mem::Read32(d0 + kOff_Dim_Type, &t0) ||
            !bm::mem::Read32(d1 + kOff_Dim_Type, &t1) || t0 != kDim_SpeedForwardInLocal || t1 != kDim_SpeedUpInWorld ||
            !bm::mem::ReadBytes(d0 + kOff_Dim_Smoothing, &s, 4) || s != bm::broomchart::kRiderSpeedSmoothing)
            return false;
        g_space = ref;
        return true;
    }

    // True on a blend's first update.
    bool Remember(uintptr_t blend)
    {
        for (uintptr_t b : g_seen)
            if (b == blend) return false;
        g_seen[g_next] = blend;
        g_next = (g_next + 1) % kSeen;
        return true;
    }

    float Detour(uintptr_t blend, float dt, const float* measured, float scale, uint8_t flag)
    {
        uintptr_t ref = 0;
        if (!bm::mem::ReadPtr(blend + kOff_Blend_Space, &ref) || !IsLean(ref))
            return g_original(blend, dt, measured, scale, flag);

        AcquireSRWLockExclusive(&g_lock);
        const bool first = Remember(blend);
        ReleaseSRWLockExclusive(&g_lock);
        uintptr_t from = 0;
        bm::mem::ReadPtr(blend + kOff_Blend_From, &from);
        // After its first update the blend evaluates its own weights.
        auto* link = reinterpret_cast<uintptr_t*>(blend + kOff_Blend_From);
        const bool own = !first && from;
        if (own) *link = 0;
        // His broom idle's motion is not set to loop, so with the time moving
        // it ran to the end of its 100 frames and the update stopped until
        // his state began again, which letting go of the stick does (Seth,
        // 4 October: "it get stuck sitting up or hunched over until i let go
        // of the stick"). The flag is the loop byte the time step wraps on.
        const float t = g_original(blend, dt, measured, scale, 1);
        if (own) *link = from;
        return t;
    }
}

namespace bm::leanblend
{
    bool Install()
    {
        const uintptr_t at = bm::mem::Find(kSig_BlendUpdate);
        char why[128] = {};
        if (!at || !bm::farhook::Install("blend update", at, reinterpret_cast<void*>(&Detour),
                                         reinterpret_cast<void**>(&g_original), why, sizeof why))
        {
            LOG_ERR("[lean] the blend update %s, so Kliff's lean changes only when Broomy's state does.",
                    at ? why : "was not found");
            return false;
        }
        LOG("[lean] Kliff's lean follows Broomy's speed every frame, eased at %.2f a second (+0x%llX).",
            bm::broomchart::kRiderSpeedSmoothing, static_cast<unsigned long long>(bm::mem::Rva(at)));
        return true;
    }
}
