#pragma once
#include <cstdint>

// Broomy's speed follows how far the left stick is pushed. The Wyvern's
// states have one speed whatever the stick does, and the game latches that
// speed when movement starts, so the plugin scales each frame's speed record
// for Broomy alone and lets the latch fall to it. Seth, 2 October: "pushed
// slightly is slow and all the way is full speed". The boost is left at full
// speed ("make the boost always full speed").
namespace bm::analogspeed
{
    // From the chart patch: where the game holds Broomy's chart `index`. The
    // movement code reads Broomy's speed records straight from these
    // buffers, which is how the hook tells Broomy from every other actor.
    void NoteChart(int index, uintptr_t data, uint32_t size);

    // The speed at the lightest push, in percent of the full one (ini
    // SlowestPush); 100 leaves the speed to the game. Before Install.
    void SetSlowest(int percent);

    // From the worker. Hooks the one call of the movement update and starts
    // reading the left stick.
    bool Install();
    void Stop();

    // Broomy's speed in m/s at its last movement update, with how many
    // milliseconds ago that was in `age` and whether it was the boost in
    // `boost`; for the engine sound.
    float LastSpeed(uint32_t* age, bool* boost);
}
