# Broom Mount

A plugin for Crimson Desert that lets Kliff ride a flying broom.

Pearl Abyss built a broom mount and left it out of the game. The archives
hold its model, a skeleton with two rider seats, eleven animations each for
the broom and for Kliff, and a `Broom_Ride` interaction, all unused. The
plugin wires them up as Broomy, a mount with its own wedge on the Character
radial. The Wyvern and every other mount stay as they are.

Target: Crimson Desert 2.03.02 (exe 1.0.0.2976). Players should read
`mod/README.md`, which ships with the plugin and covers installing, settings
and the known limits.

## How it works

The plugin is the whole install. There's no pack group and no `0.papgt`
edit. Everything Broomy needs that the game doesn't ship comes from memory:

- Broomy's rows in eight static tables: stringinfo, characterinfo,
  mercenaryinfo, mercenarygroupinfo, reserveslot, vehicleinfo,
  characterappearanceindexinfo and quickslotinfo. The game reads its own
  file and the plugin swaps in its built copy.
- Charts made from the Wyvern's riding charts, with every animation path
  pointed at the broom's clips, the sounds and camera shakes cut, and the
  speeds taken from the ini.
- A pose modifier that turns the broom's body toward the camera in flight.

A few seconds after a save loads, the plugin checks the roster. A save
without Broomy gets one by hiring a wild horse under Broomy's character row
on the game server's own thread. A hook on the call key's branch lets
Broomy's wedge play Kliff's flying call. Another scales Broomy's speed by the
left stick.

## Building

MSVC Build Tools 2022, with the CMake and Ninja they bundle.

    mod\build.bat

The plugin lands in `mod\dist\BroomMount.asi`. It needs Ultimate ASI Loader
in the game's `bin64`.

## License

MIT. See LICENSE and THIRD_PARTY_NOTICES.md.
