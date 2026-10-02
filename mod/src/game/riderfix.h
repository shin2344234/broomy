#pragma once
#include <cstdint>

// Kliff's riding pose on Broomy. His lower riding chart, ride_test3_lower,
// enters the state with the same key as the mount's current state. Broomy's
// charts keep the Wyvern's state keys, and from those the lower chart's
// branch 397 (ground) and branch 400 (air) send every mount that is not a
// Wyvern to the big dragon's stand idle, a full body crouch that hides the
// broom idle his upper chart plays. While Broomy is ridden the two branches
// point at Kliff's broom states instead, 588C2001 and AD3E621B, his broom
// idle on broom_rider_move. Broomy counts as ridden while its RideOn chart
// (the served armadillo chart) has been checked in the last 3 seconds; that
// chart runs only while it is ridden. Otherwise the branches keep their
// shipped targets, so riding the dragon is unchanged.
//
// Both go through the branch check (+0x232E3D0). Test builds can also log
// each chart layer's next action (LogStates=1).
namespace bm::riderfix
{
    bool Install(bool logStates);
    // From broomy.cpp's chart patch: where each of Broomy's served charts is.
    void NoteServed(const char* path, uintptr_t base, uint32_t size);
}
