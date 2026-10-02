#pragma once
#include <Windows.h>

namespace bm::Mod
{
    // From DllMain, synchronously: hooks that must be in before the game's
    // own start-up code runs. Keep it small.
    void EarlyInstall(HMODULE module);
    void Initialize(HMODULE module);
    void Shutdown(bool processExiting);
}
