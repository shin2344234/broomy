#pragma once

// Kliff's lean on Broomy follows the speed every frame. A blend motion's
// update (+0x2EA9F90) takes the weights of the motion it started from when
// both blends have the same axes, and evaluates its own only when it has no
// such motion, so his broom_rider_move held the lean it began with until his
// state started again, and then jumped to the lean for the speed of that
// moment. _parameterSmoothingFactor is applied only where the blend is
// evaluated, so the 1.5 in broom_blend.py did nothing (Seth, 4 October:
// "still snapping"). Here his lean blend takes over the weights, the speed
// and the phase of the motion it started from on its first update, as the
// game does, and evaluates its own from then on. Its time loops, since his
// broom idle's motion is not set to and would otherwise stop at its end.
// Every other blend runs as shipped. KNOWLEDGE.md, "Nexus reports and
// 1.0.3".
namespace bm::leanblend
{
    // From the worker. Hooks the blend motion update.
    bool Install();
}
