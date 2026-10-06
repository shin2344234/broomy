#pragma once
#include <Windows.h>

// The speeder's engine. The game has no sound for it, so the plugin plays
// two loops of its own (resources 301 and 302, from speeder/make_engine.py)
// through XAudio2: they fade in while Broomy is ridden, cross from the idle
// to the close engine and rise in pitch with Broomy's speed, and fade out on
// dismount, in a menu or when the game is not the foreground window.
namespace bm::speedersound
{
    // From the worker, after riderfix and analogspeed are installed.
    // `module` holds the loops; `volume` is the ini's EngineVolume, 0 to
    // 100, and 0 plays nothing.
    bool Start(HMODULE module, int volume);
    void Stop();
}
