#pragma once
#include <cstdint>

// Lets Broomy's wedge start Kliff's call animation. Kliff's on-foot chart
// (common_upper_branchset) has one call branch per reserve slot it knows:
// VehicleSlot (1000006), VehicleSlot_Dragon (1000020) and
// VehicleSlot_Mechanic (1000019). Each branch's condition asks leaf 0x1D1
// whether its slot is the one chosen, and no branch names Broomy's slot
// (1000032), so a Broomy press never calls. While Broomy's wedge is the one
// chosen, the plugin answers leaf 0x1D1 for VehicleSlot as if VehicleSlot
// were chosen, for half a second after the wedge request; the chart takes
// the general call branch. That branch's call event names VehicleSlot as the
// slot to call, so while Broomy's wedge is the one chosen, the plugin hands
// the lookup of that event a copy of its data naming Broomy's slot, and the
// server calls Broomy. KNOWLEDGE.md, "How Broomy's wedge calls".
namespace bm::callgate
{
    // From the worker. Patches the client chart component's leaf check.
    bool Install();
    // From the request sender: the slot a wedge request chose.
    void Chosen(uint32_t slotKey);
}
