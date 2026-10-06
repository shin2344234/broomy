# Broomy

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

The full account of how the mod was made, with the addresses, formats and
everything that was tried and dropped, is in
[docs/how-broomy-was-made.md](docs/how-broomy-was-made.md).

## The speeder bike fork

This branch turns the broom into a speeder bike called Speeder Bike. The
mount's rows, charts and flight are Broomy's; what changes is what the
plugin hands the game for the broom's mesh, its material and textures, and
Kliff's riding clips, plus an engine sound of the plugin's own.

The mesh is skinned to `B_Rider_01`, the seat bone, which stays level in
every riding clip while the broom tips under it, so the speeder flies
level and still bobs and turns with the body. Kliff's clips keep his
spine and head from the broom's, with his pelvis held on the saddle, his
hands on the grips and his feet on the footrests by IK, and a forward lean
that grows with speed. The engine is the speeder bike's own sound from
Return of the Jedi, cut from the Star Wars SFX Archive's "Speeder Bike.wav"
by `speeder/make_engine.py`: idle, close engine and boost loops, and the
acceleration, boost start, boost stop, start-up and shutdown takes. The
start-up plays as Kliff gets on, unless he rode in the last 5 seconds, and
the shutdown as he gets off. The plugin plays them
through XAudio2 while the speeder is ridden. The idle crosses into the
close engine and the pitch rises with the speed, the boost has its own
loop with its start and stop, and a jump in speed of 12 m/s or more plays
the acceleration, or the boost's stop, quieter, when slowing down.
`EngineVolume` in the ini sets the volume, and `build.bat soundcheck`
plays a fake ride through it all. Without the WAV the script makes a
synthesised hum and no one-shots.

`speeder/` has the scripts that make the files in `mod/assets`. They need
the model from Sketchfab (THIRD_PARTY_NOTICES.md) unpacked into
`speeder/src`, Blender 5.2 and CD Animator:

    py -3 speeder/make_engine.py ["Speeder Bike.wav"]
    py -3 speeder/make_textures.py <broom textures, unpacked>
    blender -b --factory-startup --python speeder/build_mesh.py -- <broom .pac, plain>
    blender -b --factory-startup --python speeder/build_rider.py
    py -3 speeder/pack_assets.py <broom .pac_xml, unpacked>

`speeder/check_pacwrite.py` checks the mesh writer against the broom's own
mesh.

## Building

MSVC Build Tools 2022, with the CMake and Ninja they bundle.

    mod\build.bat

The plugin lands in `mod\dist\Broomy.asi`. It needs Ultimate ASI Loader
in the game's `bin64`.

## License

MIT. See LICENSE and THIRD_PARTY_NOTICES.md.
