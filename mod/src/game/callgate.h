#pragma once
#include <cstdint>

// Broomy is one of the saddle wedge's mounts: VehicleSlot (1000006) takes
// its mercenary list as a third, after the horses and Vehicle_Special. With
// Broomy chosen there, Kliff's on-foot chart takes its own VehicleSlot call
// branch and the server calls Broomy, so a call needs nothing from the
// plugin. The plugin notes when the chosen wedge is the saddle wedge with
// Broomy, from the server's side of a wedge press.
//
// Kliff's falling chart calls Blackstar from the air with an event naming
// VehicleSlot_Dragon and an air summon-and-ride type (6, where a ground call
// has 3), whichever wedge is chosen. Handing that event VehicleSlot left
// Broomy flying 14.3 m under Kliff wherever he went, out of sight in the
// ground. Seth, 4 October: "disable calling broomy while falling instead of
// switching back to blackstar". So while Broomy is chosen, the client's
// chart leaf check says Kliff has not learned Skill_CallDragon, which every
// falling call branch asks, and a call while falling does nothing.
// KNOWLEDGE.md, "Broomy in the saddle wedge".
namespace bm::callgate
{
    // From the worker. Hooks the server's reserve slot change and the
    // client's chart leaf check.
    bool Install();
    // Whether the chosen wedge is the saddle wedge, holding Broomy.
    bool BroomyChosen();
}
