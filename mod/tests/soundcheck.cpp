// The offline check of the speeder's engine sounds: build.bat soundcheck.
// A fake ride through stand-ins for riderfix and analogspeed, at 1% volume:
// mount, stand, the hover's walk, run, boost, back to cruise, stop, get off.
// It fails unless the sounds that play are, in order, the start-up,
// acceleration (walk), acceleration (run), boost start, boost stop, the
// deceleration (stop) and the shutdown.
#include <Windows.h>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>

#include "game/speedersound.h"

static std::vector<std::string> g_lines;
static ULONGLONG g_start;
static float Now() { return (GetTickCount64() - g_start) / 1000.0f; }

namespace bm::Log
{
    void Write(const char*, const char* fmt, ...)
    {
        char line[512];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(line, sizeof line, fmt, ap);
        va_end(ap);
        printf("%5.2f  %s\n", Now(), line);
        g_lines.push_back(line);
    }
}

namespace bm::riderfix
{
    uint32_t MsSinceRidden() { return Now() < 13.0f ? 0 : 0xFFFFFFFFu; }
}

namespace bm::analogspeed
{
    // (from second, speed, boost)
    struct Step { float at, speed; bool boost; };
    constexpr Step kRide[] = { { 0, 0, false }, { 1, 14, false }, { 4.5f, 27.5f, false }, { 6.5f, 125, true },
                               { 8.5f, 62.5f, false }, { 12, 0, false } };
    float LastSpeed(uint32_t* age, bool* boost)
    {
        const Step* s = kRide;
        for (const Step& k : kRide) if (Now() >= k.at) s = &k;
        *age = s->speed > 0 ? 0 : 1000;
        *boost = s->boost;
        return s->speed;
    }
}

int main()
{
    g_start = GetTickCount64();
    if (!bm::speedersound::Start(GetModuleHandleW(nullptr), 1)) return 1;
    Sleep(15000);
    bm::speedersound::Stop();
    const char* want[] = { "startup.",    "accelerate.", "accelerate.", "boost start.",
                           "boost stop.", "boost stop.", "shutdown." };
    const size_t n = sizeof want / sizeof want[0];
    std::vector<std::string> got;
    for (const std::string& l : g_lines)
        if (l.rfind("[sound] ", 0) == 0 && l.find("engine") == std::string::npos) got.push_back(l.substr(8));
    bool ok = got.size() == n;
    for (size_t i = 0; ok && i < n; ++i) ok = got[i] == want[i];
    printf(ok ? "PASS\n" : "FAIL: %zu sounds\n", got.size());
    return ok ? 0 : 1;
}
