#include "game/aimrate.h"

#include <Windows.h>
#include <timeapi.h>
#pragma comment(lib, "winmm.lib")
#include <cstdint>
#include <cstring>

#include "core/log.h"
#include "game/farhook.h"
#include "game/hash.h"
#include "game/mem.h"

namespace
{
    // In the AimIK apply (+0x1119B60), with rdi the modifier, xmm7 the target
    // distance and xmm8 the smoothing rate (the event's, read from [rdi+0x84]
    // and reset there to -1 at once, or 10 when it holds nothing):
    //   +0x111A0C5  vxorps  xmm1, xmm1, xmm1
    //   +0x111A0C9  vcomiss xmm7, xmm1
    //   +0x111A0CD  jbe     +0x111A10F        ; no distance: rate stays as it is
    //   +0x111A0CF  vcomiss xmm8, xmm9        ; ... rate += min(|lag|, 8) / 8 * (4 - rate)
    //   +0x111A10F  mov     r9, r14           ; the solve, which smooths at rate xmm8
    // [rdi+0x60] is the hash of the aim's reference bone (looked up at
    // +0x1119DB7). For the broom it is B_Body_00, which ikpatches.h puts in
    // the RideOn and body charts in place of the Wyvern's B_IK_Head_00. The
    // broom's aim gets kRate on every frame and skips the pull; every other
    // aim IK runs the shipped code.
    constexpr uintptr_t kRva_Site = 0x111A0C5;
    constexpr uintptr_t kRva_Pull = 0x111A0CF;
    constexpr uintptr_t kRva_Solve = 0x111A10F;
    constexpr unsigned char kSite[] = { 0xC5, 0xF0, 0x57, 0xC9, 0xC5, 0xF8, 0x2F, 0xF9, 0x76, 0x40 };
    constexpr unsigned char kPullHead[] = { 0xC4, 0x41, 0x78, 0x2F, 0xC1 };
    constexpr float kRate = 1.0f;   // per second: a 90 degree swing is 90 percent done in 2.3 s

    // Filled by the code below: how often the broom's aim ran, and the rate
    // the game had for it the last time.
    volatile uint32_t* g_count = nullptr;
    volatile float* g_seen = nullptr;
    volatile uintptr_t* g_mod = nullptr;    // the broom's modifier (rdi) the last time
    volatile uintptr_t* g_ctx = nullptr;    // and its context (r14), position at +0x158
    volatile uint32_t* g_other = nullptr;   // the reference bone hash of the last other aim

    unsigned AbsJump(unsigned char* p, uintptr_t to)
    {
        p[0] = 0xFF; p[1] = 0x25; p[2] = p[3] = p[4] = p[5] = 0;
        memcpy(p + 6, &to, 8);
        return 14;
    }
    void Rel32(unsigned char* code, unsigned at, unsigned next, unsigned target)
    {
        const int32_t d = static_cast<int32_t>(target) - static_cast<int32_t>(next);
        memcpy(code + at, &d, 4);
    }

    // A trace for diagnosis, about 60 lines a second while the broom's aim
    // runs: the target point (+0x74), the lag S (+0x80), the broom's position
    // (context +0x158) and the first stored aim entry (array at +0x98, 20
    // bytes each), as Broomy.log's [aimlog] lines.
    DWORD WINAPI Report(LPVOID)
    {
        uint32_t last = 0;
        LARGE_INTEGER f, t0, t;
        QueryPerformanceFrequency(&f);
        QueryPerformanceCounter(&t0);
        timeBeginPeriod(1);
        uint32_t others[64] = {};
        int otherN = 0;
        for (int lines = 0; lines < 40000;)
        {
            Sleep(15);
            const uint32_t o = *g_other;
            bool known = o == 0;
            for (int i = 0; i < otherN && !known; ++i) known = others[i] == o;
            if (!known && otherN < 64)
            {
                others[otherN++] = o;
                LOG("[aim] another aim IK runs, reference bone hash %08X.", o);
            }
            const uint32_t n = *g_count;
            if (n == last) continue;
            last = n;
            ++lines;
            float tgt[3] = {}, lag = 0, pos[3] = {}, q[5] = {};
            uintptr_t arr = 0;
            const uintptr_t mod = *g_mod, ctx = *g_ctx;
            bm::mem::ReadBytes(mod + 0x74, tgt, sizeof tgt);
            bm::mem::ReadBytes(mod + 0x80, &lag, sizeof lag);
            bm::mem::ReadBytes(ctx + 0x158, pos, sizeof pos);
            if (bm::mem::ReadPtr(mod + 0x98, &arr)) bm::mem::ReadBytes(arr, q, sizeof q);
            QueryPerformanceCounter(&t);
            const double s = static_cast<double>(t.QuadPart - t0.QuadPart) / static_cast<double>(f.QuadPart);
            LOG("[aimlog] %.3f n %u rate %.2f tgt %.2f %.2f %.2f lag %.2f pos %.2f %.2f %.2f q %.4f %.4f %.4f %.4f %.4f",
                s, n, *g_seen, tgt[0], tgt[1], tgt[2], lag, pos[0], pos[1], pos[2], q[0], q[1], q[2], q[3], q[4]);
        }
        timeEndPeriod(1);
        return 0;
    }
}

namespace bm::aimrate
{
    bool Install()
    {
        const uintptr_t base = bm::mem::Game().base;
        unsigned char pull[sizeof kPullHead] = {};
        if (!bm::mem::ReadBytes(base + kRva_Pull, pull, sizeof pull) || memcmp(pull, kPullHead, sizeof pull) != 0)
        {
            LOG_ERR("[aim] the aim IK's rate code is not at +0x%llX on this exe.", static_cast<unsigned long long>(kRva_Pull));
            return false;
        }
        const uint32_t body = bm::hash::Little("b_body_00");
        const uint32_t bodyAsWritten = bm::hash::Little("B_Body_00");
        uint32_t rate;
        memcpy(&rate, &kRate, 4);

        unsigned char c[160];
        memset(c, 0xCC, sizeof c);
        unsigned n = 0;
        // The name as the charts spell it, B_Body_00, or in lower case.
        const unsigned char head[] = {
            0xC5, 0xF0, 0x57, 0xC9,                     // vxorps  xmm1, xmm1, xmm1
            0x81, 0x7F, 0x60, 0, 0, 0, 0,               // cmp     dword [rdi+0x60], hash(B_Body_00)
            0x74, 9,                                    // je      broom
            0x81, 0x7F, 0x60, 0, 0, 0, 0,               // cmp     dword [rdi+0x60], hash(b_body_00)
            0x75, 0,                                    // jne     other
        };
        memcpy(c, head, sizeof head);
        memcpy(c + 7, &bodyAsWritten, 4);
        memcpy(c + 16, &body, 4);
        n = sizeof head;
        const unsigned jneAt = n - 1;
        // the broom
        const unsigned incAt = n;
        c[n++] = 0xF0; c[n++] = 0xFF; c[n++] = 0x05; n += 4;                  // lock inc dword [rip+count]
        const unsigned saveAt = n;
        c[n++] = 0xC5; c[n++] = 0x7A; c[n++] = 0x11; c[n++] = 0x05; n += 4;   // vmovss [rip+seen], xmm8
        const unsigned modAt = n;
        c[n++] = 0x48; c[n++] = 0x89; c[n++] = 0x3D; n += 4;                  // mov     [rip+mod], rdi
        const unsigned ctxAt = n;
        c[n++] = 0x4C; c[n++] = 0x89; c[n++] = 0x35; n += 4;                  // mov     [rip+ctx], r14
        c[n++] = 0xB8; memcpy(c + n, &rate, 4); n += 4;                      // mov     eax, kRate
        c[n++] = 0xC5; c[n++] = 0x79; c[n++] = 0x6E; c[n++] = 0xC0;          // vmovd   xmm8, eax
        n += AbsJump(c + n, base + kRva_Solve);
        // other: the shipped instructions
        c[jneAt] = static_cast<unsigned char>(n - (jneAt + 1));
        c[n++] = 0x8B; c[n++] = 0x47; c[n++] = 0x60;                         // mov     eax, [rdi+0x60]
        const unsigned otherAt = n;
        c[n++] = 0x89; c[n++] = 0x05; n += 4;                                // mov     [rip+other], eax
        c[n++] = 0xC5; c[n++] = 0xF8; c[n++] = 0x2F; c[n++] = 0xF9;          // vcomiss xmm7, xmm1
        c[n++] = 0x76; c[n++] = 14;                                         // jbe     solve
        n += AbsJump(c + n, base + kRva_Pull);
        n += AbsJump(c + n, base + kRva_Solve);
        n = (n + 7) & ~7u;
        const unsigned modSlot = n; memset(c + n, 0, 8); n += 8;
        const unsigned ctxSlot = n; memset(c + n, 0, 8); n += 8;
        const unsigned countAt = n; memset(c + n, 0, 4); n += 4;
        const unsigned seenAt = n;  memset(c + n, 0, 4); n += 4;
        const unsigned otherSlot = n; memset(c + n, 0, 4); n += 4;
        Rel32(c, otherAt + 2, otherAt + 6, otherSlot);
        Rel32(c, incAt + 3, incAt + 7, countAt);
        Rel32(c, saveAt + 4, saveAt + 8, seenAt);
        Rel32(c, modAt + 3, modAt + 7, modSlot);
        Rel32(c, ctxAt + 3, ctxAt + 7, ctxSlot);

        uintptr_t placed = 0;
        char why[160] = {};
        if (!bm::farhook::InstallBranch("aim rate", base + kRva_Site, kSite, sizeof kSite, c, n, &placed, why, sizeof why))
        {
            LOG_ERR("[aim] could not patch the aim IK's rate: %s", why);
            return false;
        }
        g_count = reinterpret_cast<volatile uint32_t*>(placed + countAt);
        g_seen = reinterpret_cast<volatile float*>(placed + seenAt);
        g_mod = reinterpret_cast<volatile uintptr_t*>(placed + modSlot);
        g_ctx = reinterpret_cast<volatile uintptr_t*>(placed + ctxSlot);
        g_other = reinterpret_cast<volatile uint32_t*>(placed + otherSlot);
        if (HANDLE t = CreateThread(nullptr, 0, &Report, nullptr, 0, nullptr)) CloseHandle(t);
        LOG("[aim] the broom's aim (B_Body_00, hash %08X or %08X) smooths at %.2f per second, without the distance pull.",
            bodyAsWritten, body, kRate);
        return true;
    }
}
