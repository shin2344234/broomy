#pragma once

// Keeps Broomy's summon from crashing in the server's equipment copies.
// Broomy's equipment list (0xD0-byte entries at +0x08, count +0x10, item key
// first, slot i16 at +0xC8) holds one blank entry, item FFFFFFFF in slot 2.
// Two copies place each entry into slot (i16)entry+0xC8 of an array sized
// by the list's equipslotinfo row: the component's setup (+0x2AD0FD0,
// ServerEquipSlotActorComponent, list at +0x90) and a copy into a given
// array (+0xE2AFC40, behind the thunk at +0x20CEB90) that the server's mount
// call (+0x2BAEF5F) makes. Broomy's row has no slot 2, so the blank entry
// lands outside the array, and the copy, which clears its destination
// first, frees whatever memory is there: a crash unless the leftovers
// happen to be harmless (nine crashes on 28 September through +0x240B070,
// two summons that held). Blank entries at the end of the list are left
// out of both copies.
namespace bm::equipfix
{
    bool Install();
}
