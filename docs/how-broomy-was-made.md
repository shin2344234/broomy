# Broomy: making Crimson Desert's cut flying broom a mount

Seth Walker (Shin234), October 2026. Crimson Desert 2.03.02, `CrimsonDesert.exe` 1.0.0.2976.
Every address below is an RVA in that exe, image base 0x140000000.

## Abstract

Crimson Desert ships a complete flying broom mount that nothing in the game
uses: a model, a skeleton with two rider bones, eleven animations for the
broom, eleven for the player character riding it, three blend spaces and an
interaction that seats the rider. It has no character entry, no vehicle entry,
no action chart and no appearance file. Broomy is an ASI plugin that supplies
every missing piece from memory, as the game reads its files, and turns the
broom into a mount with its own wedge on the Character radial. No game file
on disk changes. This paper covers what the game ships and how a plugin adds a mount
without a pack mod. It then follows the new mount into a save, explains why
its radial wedge did nothing for most of the project, and shows how its
flight was tuned away from the Wyvern it borrows from, including what was
tried and dropped. It ends with the file formats that had to be decoded and the
addresses the plugin depends on.

## 1. The question

On 27 September 2026 I wanted to know whether a broom could be a flying
mount in Crimson Desert. There were two ways to try. One was to fake it:
put a broom model on an existing flying mount. The other was to find out
whether Pearl Abyss had started a broom mount and left it out, and wire up
what they left. The second turned out to be possible, and the goal became a
mount of its own, named Broomy, with its own radial wedge, that takes nothing
away from the Wyvern or any other mount.

A second rule came a day later. The release has to work like a clean
install for a new player. That means the plugin alone, with no pack group,
no `0.papgt` edit and nothing that depends on my save. That rule shaped
everything after it.

## 2. What the game ships

All of it is in pack group 0009 unless noted.

    character/cd_r0032_00_broom_0001.pac             mesh, 103,536 bytes
    character/cd_r0032_00_broom.pab                  skeleton
    character/cd_r0032_00_broom_0001.hkx             physics
    character/cd_r0032_00_broom_0001.prefab          skinned mesh component, tagged Nude
    character/cd_r0032_00_broom_0001.prefabdata_xml  names the skeleton
    character/cd_r0032_00_broom_0001.pac_xml         one submesh, the sweeping broom's textures

The skeleton has nine bones: `Bip01`, `B_TL_Position_02`, `B_TL_Position_01`,
`B_MoveControl_01`, `B_Body_00`, `B_EnemyCatch_00`, `B_CatchMe_00`,
`B_Rider_01` and `B_Rider_02`. Both rider bones are children of `B_Body_00`,
so a rider tilts with the broom.

The broom's animations are `cd_rd_broom_basic_00_00_nor_*`: idle, mount and
dismount on each side, walk, walk fast, run, run fast, and a 75 degree climb
and dive. The player's are the same eleven under
`1_pc/1_phm/00_riding/cd_phm_rd_broom_basic_00_00_nor_*`, male player only.
Three blend spaces go with them: `broom_riding_move` for the broom,
`broom_rider_move` and `broom_rider_move_2cycle` for the rider. Group 0010
holds a `.paa_metabin` for every clip. The player's own riding chart,
`ride_upper.paac`, already has a block for the broom.

The static tables name the broom once. `interactioninfo` row 1000068 is
`Broom_Ride`, with two seat pivots on `B_Rider_01`. No character, vehicle,
item or skill points at any of it, and the broom has no action chart, no
characterinfo row, no appearance file and no string for its paths.

The sweeping broom you can pick up in the game (`Equip_Broom`, model
`cd_t0000_broom_0001`) is a separate asset and system. It shares textures
with the mount, and its item icon is what Broomy uses for a portrait.

## 3. Constraints

### 3.1 Why a pack mod was dropped

The first build added files as a pack group. Three things count against
that for a release.

1. A new pack group has to be listed in `meta/0.papgt`, the one file every
   group hangs off. Definitive Mod Manager writes it, a game update writes
   it, and a Steam file check writes it, and each drops the entry. A bad
   `0.papgt` stops the game loading.
2. A group can add a file only to a folder some shipped group already has.
   New folders do not load.
3. A static table cannot come from a pack group at all. Under its shipped
   name an edited copy loses to group 0008. Under a new name, the table
   loader's existence check fails and the table is dropped without a word.

So the plugin has to serve everything from memory.

### 3.2 What a plugin can serve

The game reads files through `pa::ResourceLoader` (vtable +0x571BE40). The
path argument is always an engine string whose first pointer leads to the
characters.

| Slot | Address | What it does |
|---|---|---|
| 3 | +0x12D00B0 | finds the pack source for a path |
| 8 | +0x12D0350 | Read: calls slot 3, then the source's handler |
| 9 | +0x12D03E0 | async load |
| 10, 11 | +0x12D2510, +0x12D2310 | list a folder |
| 13 | +0x12D1560 | existence check |

Read carries static tables, `.paloc` strings, UI XML (the portrait registry
included), `.paac` charts, `.paa_metabin`, `.paatt` and character descriptor
XML. The async load carries appearance files, prefab data and other streamed
assets, about 27,500 paths by the main menu. On a job thread it queues a
task that never reaches Read; otherwise it calls Read itself.

Five hooks cover what Broomy needs.

- The Read hook lets the game read its own file, check the bytes are the ones
  Broomy's version was built from, then put Broomy's bytes in the same
  buffer. The buffer is `{u8* data; u32 capacity = size + 2; u32 size; i8
  tag}`, allocated through the game's own allocator pair (picked by a TLS
  byte, +0x47EFA78 or +0x47EF9B0), with the tag's byte count moved to match.
  A mismatch means the game is a different build or another mod has changed
  that file, so the plugin leaves it alone and logs why.
- The table loader formats `"%s%s.staticinfobody"`
  at +0x25C53FA and `"%s%s.staticinfoheader"` at +0x25C576B. Hooking both
  tells the plugin which table the next Read on that thread is for. A
  table's header is parsed before its body is read, so on the first half the
  plugin reads the other half through the game's own Read and builds both.
- The appearance loader (+0x2439D90) takes a task whose +0x28 field
  is a buffer holding the decrypted XML. Pointing it at the plugin's buffer
  for the one call serves an appearance file from memory.
- In the async load, clearing the job pointer in the thread block for one
  call (`[gs:0x58]` slot 0, +0x1E0) makes it call Read instead of queueing a
  task, so the Read hook can supply the file.
- In find and exists (slots 3 and 13), sending a new path to an existing
  file as a stand-in lets a chart name a clip no pack holds.

Some files are found by listing a folder, so a new name in that folder would
never be read. Broomy reuses two shipped files that no table names: the
Phoenix's appearance file `cd_m0004_00_phoenix_0001_00000.app_xml`, whose
text is swapped for the broom's, and `animal_seal.xml`, served as the
Wyvern's gameplay description without its foot IK and climbing. 544 of the
5,667 shipped appearance files are named by no table, which leaves room for
more.

## 4. Broomy's rows

| Table | Key | Name | Built from |
|---|---|---|---|
| characterinfo | 1900001 | Riding_Broomy_1, "Broomy" | the Alpine Ibex's row (7040), patched by value |
| mercenaryinfo | 87 | Vehicle_Broom | Vehicle_Horse (78) |
| mercenarygroupinfo | Vehicle | gains list 87 | the shipped row |
| reserveslot | 1000032 | VehicleSlot_Broomy, list 87 | VehicleSlot_Dragon (1000020) |
| vehicleinfo | 20000 | Broom | AlpineIbex (16993) |
| quickslotinfo | 16962 | the Character radial | the dragon wedge's item, pointed at Broomy's slot, in place of companion wedge 3 |
| characterappearanceindexinfo | (1900001, -2) | the broom's appearance | the ibex's |
| stringinfo | | the paths Broomy's row names | |

The runtime gives Broomy characterinfo row 7250 and mercenary list index 21.

A few format facts made the rows possible.

- A table header is a u16 count, then a key and a u32 offset per entry, the
  key 1, 2, 4 or 8 bytes wide. `characterappearanceindexinfo` is wider,
  with a u32 count and 12 byte entries of key, tag and offset.
- A characterinfo row's localized strings are `u8 3, u32 sub, u32 key, u32
  length` and then the decimal digits of `(key << 32) | sub`, with sub 0x30
  for the name, 0x31 the description and 0x32 the hire message. The text
  itself goes in a `.paloc`.
- Seven asset strings run together in a characterinfo row: upper chart,
  lower chart, gameplay data, appearance, unset, skeleton, unset. Strings
  are shared between rows. The Wyvern's appearance string is used by two
  rows, its skeleton by six, its upper chart by nine. Broomy's row points at
  new strings and never edits a shared one.
- The Character radial has eight items. A ninth blanks the whole radial, so
  Broomy takes the fourth companion wedge, which was empty in my game.
- Giving a shipped mercenary list to a second reserve slot crashed the game
  at boot, twice. Broomy has a list of its own.

## 5. Getting Broomy into a save

### 5.1 The hire

A mount in your roster is a hired mercenary. The server's hire (+0x2BADC00)
takes the clan, a u32 error out, the actor id, 0 and a byte, and it reads the
hired actor's characterinfo row. Swap an unowned wild horse's row to
Broomy's for the length of that one call and the horse joins the roster as
Broomy. The first build did this through the client request
`TrocTrHireMercenaryToTargetReq` (0x0B8F), and it worked on 28 September.

It only worked for a horse standing next to the player. For any other horse
the server dropped the request before its handler (+0x2A2F160) ran, which is
why a save loaded from inside the game was never granted. The handler does
nothing more than call the hire with the sender's clan, so the plugin now
makes that call itself, on the server's own thread, from the handler of the
client's movement request 0x0B0C (+0x29784C0). That request arrives about 37
times a second on the thread that also runs the mount call. Any wild horse
the server has loaded will do.

### 5.2 When to check

The game sends two loading-complete requests (0x098D) per load, and wild
horses exist only after the second. Horses seen while the save loads are kept
as candidates, so the hire lands about 0.2 seconds after loading ends. Three
seconds after a save loads the plugin reads both rosters for Broomy's row and
hires only when it is missing. Loading a save from inside the game sends no
loading-complete, so the plugin also watches the rosters it last read and
checks again when one moves.

### 5.3 The actor field

After a hire, Broomy's server roster entry names the hired horse as the actor
it has out in the world (entry +0x50). The server accepts a call for a mount
whose entry names an actor and spawns nothing, so a freshly granted Broomy
could not be called. The plugin clears that field right after the hire. A
summoned Broomy gets a new actor id there the normal way.

### 5.4 Roster memory

The server clan component is `ServerMercenaryClanActorComponent` (vtable
+0x5B2C6C8), the client's `ClientMercenaryClanActorComponent` (+0x55BE900).
At +0x18 a map holds every owned mercenary: count at +0x2C, element pointers
at +0x40, each element an id at +8 and an entry at +0x10, each entry a row
u16 at +0x20 and the spawned actor id at +0x50. At +0x58 and +0x60 a map
holds one element per mercenary list. At +0x118 are the ids of what is out.

### 5.5 Uninstalling

A save with Broomy loads without the plugin, and Broomy leaves the radial.
Saved without the plugin, the save loses Broomy, and the plugin grants a new
one when it comes back.

## 6. Making the wedge call

For a long stretch on 28 September Broomy was in the roster, its wedge
showed with its portrait, and choosing it did nothing. This took more work
than anything else in the project, and most of it went into ruling things out.

### 6.1 What a working call looks like

Choosing a wedge sends `TrocTrChangeUseItemReserveSlotReq` (0x0AC1); the
server side is +0x2AF0140, and for a vehicle slot the slot data's u16 at
+0xD8 is the mercenary list index. About 1.15 seconds later a frame event in
the player's call animation sends `TrocTrFrameEventCallMercenaryReq`
(0x0876). The client event is `ClientFrameEventCallMercenaryReservedSlot`
(execute at +0x7FCD90). The client's call checks run inside that event:
+0x9DD480 for vehicles (indoors, region, airborne, ground, water, altitude)
and +0x9E1020 for mercenaries.

For a working mount, the player's action changes in the same frame as the
0x0AC1, and the client sends 0x08AF with the call action and the frame event
A6C6F154 (the Wyvern's flying call) or 1E16DA95 (the horse whistle). For
Broomy, nothing changed. The call animation never started.

### 6.2 What it was not

Each of these was tested and ruled out, in about this order.

- The rows. Broomy's match their sources field for field.
- The rosters and the per-list map. Broomy's entries look like other stored
  mounts'.
- Filling the slot data at +0xD8. The client already sends Broomy's list.
- Making Broomy the main mount with `TrocTrSwapOwnedMercenaryReq` (0x0A95).
  It swaps two owned mounts and cannot make a lone one main.
- The mercenaryinfo manager's cached list indices. Hardware read watches on
  all 32 bytes saw no read in the three seconds after any press.
- The 417 ConditionData classes. Slot 21 of each is the check made in play;
  patching all of them and logging for 1.5 seconds after each press showed
  the Wyvern's press and Broomy's run the same kinds of condition, and none
  passes for one and fails for the other.
- Sending 0x0876 by hand. The server's mount call (+0x2BA9950) refuses it
  with `eErrCantAccessActionFrameEvent`: it looks for the request's frame
  event among the frame events of the action the player is playing. The
  server only calls from inside the call animation.
- The wedge commit (+0xEFE330), the radial close (+0xB156C50) and the
  client's slot apply (+0x99CBA0). None of them starts the call.

### 6.3 What it was

Releasing the call key raises the input `Key_CallVehicle`. The player's
on-foot chart `common_upper_branchset.paac` has one call branch per reserve
slot. Each branch is a 52 byte record whose condition asks token 0x1D1, with
the slot key as its argument, whether the chosen slot is another one. The
branches cover VehicleSlot (1000006), VehicleSlot_Dragon (1000020) and
VehicleSlot_Mechanic (1000019). None covers 1000032, a slot that did not
exist when the game shipped, so no branch passes for Broomy.

The call animation's frame event also names the slot it calls. The frame
event lookup (+0x1F78A20) leaves the event's data at out+8, and a call
event's data holds the slot key at +0x0C. The server reads the slot there,
so the animation decides which mount comes, whatever the radial chose.

The fix has two halves. For 500 ms after Broomy's 0x0AC1, token 0x1D1 for
VehicleSlot answers "this one" on the client, so the general vehicle branch
passes. A longer window refires the branch every two seconds. And while
Broomy's wedge is the one chosen, a frame event whose data names VehicleSlot
is handed a copy naming Broomy's slot. The player plays the flying call and
Broomy spawns. Token dispatch for ids 0x83 and up goes through the chart
component's vtable slot 78 (+0x360BE0 on the client), which is where the
plugin answers.

The Wyvern's own branch was a tempting alternative. Condition token 0xED is a
skill check against `SkillInfoManager` (global +0x6D69AF0), and the dragon
branch needs Skill_CallDragon (1506) where the general one needs
Skill_CallVehicle (1505). A player without the dragon skill could never call
Broomy through it, so Broomy stays on the general branch.

## 7. Charts

### 7.1 Borrowing the Wyvern's

A mount needs an upper and a lower action chart. Broomy's row names the
packages of GoldStar (`CD_M0004_Ride_GoldStar_Upper` and `_Lower`), a cut
dragon. The plugin serves `m0004_ride_goldstar_upper.paac` as the Wyvern's
`m0004_dragon_upper`, `m0004_ride_goldstar_lower.paac` as
`m0004_ride_dragon_lower`, and the armadillo's chart as the Wyvern's riding
sub-chart `m0004_ride_dragon_upper`, with the attack tables to match. The
package list gives GoldStar's upper group sub-package 180, RideOnDragon,
naming the armadillo path. GoldStar's own charts never move.

Every animation path in those charts is rewritten to the broom's real path,
with the rest of the Wyvern path's bytes zeroed and the length byte left as
it was. The game reads chart paths up to the first zero, and every broom
path fits with 13 bytes to spare. Blend paths become
`broom_riding_move.motionblending`.

The chart loader frees a chart's buffer with the plain heap free (+0x47F02BC),
so a chart buffer the plugin allocates crashes it. Charts are read through
Read on the source path into the game's own buffer and patched in place.

### 7.2 The summon crash

Early summons crashed nine times on 28 September, under every chart
arrangement tried. The cause turned out to be Broomy's equipment. Its
equipment list (0xD0 byte entries, item key first, slot i16 at +0xC8) holds
one blank entry, item FFFFFFFF in slot 2. Two copies place each entry at
`out[entry+0xC8]` in an array sized by the list's equipslotinfo row: the
server component's setup (+0x2AD0FD0) and a copy into a given array
(+0xE2AFC40, from the server's mount call). Slot 2 lies outside Broomy's
array, and the copy clears its destination first, so it freed whatever was
there. The plugin leaves trailing blank entries out of both copies.

Crash dumps are at `%LOCALAPPDATA%\Pearl Abyss\DumpCache\reports\*.dmp`,
stacks only, and that is where this one was found.

### 7.3 Underground

Before the broom had charts of its own it spawned under the ground. The
Wyvern's idle, arrival and mount clips played on the broom's skeleton draw it
below the surface, and the quadruped foot IK in the borrowed description
pulled it half under too. The broom's own clips and a description without
foot IK fixed both.

## 8. Riding

### 8.1 Mounting

Under `Broom_Ride` the player's `ride_upper.paac` runs its broom block, and
the lower chart is `ride_test3_lower.paac`, which has `broom_rider_move`.
Broomy moves only once it enters its own riding sub-chart, and it enters only
when its chart has an action keyed by the pivot the player mounted at. Each
pivot in interactioninfo ends with its mount action key: 262C63D3 for the
right (the player's `mount_r`), A272684B for the left. The Wyvern's riding
chart keys one action by the Wyvern's second pivot.

Actions in that chart are 0x114 bytes, sorted by key at +0x18, starting at
the end of the included names. One action, a fireball nothing branches to,
sorts exactly where 262C63D3 belongs, so it becomes a copy of the mount
action under the right pivot's key. The other is rekeyed to A272684B. The
player mounts from either side.

### 8.2 Taking off nose up

The takeoff first pointed the broom into the ground. The RideOn chart leaves
its ground idle for its flight idle on condition `121(0,40,1,0)`. Token 0x121
(handler +0x369990) tests one bit of another layer's current action. Its
second argument is the bit, counted from action +0x08, its third the layer.
Bit 40 marks the airborne states, and the lower chart's takeoff had it set,
so the RideOn chart switched to flight the moment the takeoff began. The
flight idle's aim IK then turned the broom toward a camera that was looking
down at the player. Clearing bit 40 on the takeoff alone keeps the ground
idle, without aim IK, until flight starts, and the broom climbs nose up.

## 9. Flight

### 9.1 Reading the Wyvern's charts

In the riding lower chart, the ground idle is F5150415, walk 082F0420, run
E587FB26, takeoff 117F8F51 on jump, flight E066175D with its cruise and turn
states, hold jump to descend 608F3105, and the landing 0B5A6D35 below four
metres. Token 0x26E checks a game setting: `26E(7,0,14)` is the flying
control option set to Manual. The Wyvern's air boost (0E23DEE4) exists only
under Manual; under Camera, the run key in cruise only dives, and only when
pitched steeply down. Broomy sends that branch to the boost with no condition.

### 9.2 Speeds

Each node's `moveSpeedInfo` records (104 bytes) hold the speed in m/s at
word 6: walk 5.6, run 11, cruise 25, dive 50. Word 7 is a rate and word 18 a
further air speed. `verticalMoveSpeedInfo` (24 bytes) holds vertical speed
and rate at words 2 and 3. The generator keeps each Wyvern value with its
kind, ground or flight by the same bit 40, boost, or climb, and the plugin
multiplies by the ini's percentages as it builds the chart. At the defaults
Broomy cruises at 62.5 m/s and boosts at 125.

The Wyvern's "acceleration" state flies at cruise speed and climbs at 20 m/s,
so it is no boost. Broomy's flies at twice its cruise and does not climb on
its own. Action byte +0x5D at 1 holds a state to a level plane, and the
boost has it cleared so it turns and climbs like the cruise. Letting go
after a boost snapped to the hover, because two shared branches had a
crossfade of 0; they get 15 frames.

### 9.3 Speed that follows the stick

The Wyvern has one speed whatever the stick does, and the game latches that
speed when movement starts. The movement update (+0xAB8FC0) has one caller
(+0xAB96F0, the call at +0xAB99A7) and fills a speed record whose word 6 at
+0x18 is the speed. The caller latches the speed it started with at movement
object +0x114, -1 while stopped. It takes the record's speed whenever that is
higher or the latch is -1, and lowers the latch only on a timer that never
runs out in flight. That is why edits to the record took effect only after
the stick went back to centre.

The plugin hooks that one call, scales word 6 by the left stick's push read
through XInput in a thread of its own, and resets the latch to -1 when it
sits above the scaled speed. Broomy is told apart from every other actor by
the node header the record comes from,
`[[[[[[[self+0x18]+0x30]]+0x68]+0x40] + idx*8 + 0x88]+0x18]+0x38` with idx the
byte at `[[...+0x40]+0x30]+0x1B`. That header lies inside Broomy's own chart
buffer. The boost's two nodes are skipped, so the boost stays at full speed.

### 9.4 Quiet

The Wyvern's sounds came along with its charts. The frame event factory
(+0x4EDC40, jump table +0x4F3EF4, types 0 to 0xB0) builds type 0x41 as
`ClientFrameEventSoundEvent` with the sound's hash at word 48, 0x42 as
`ClientFrameEventVoiceSound` with the hash at word 2, and 0x43 as
`ClientFrameEventKillSound`. Zeroing all 95 sound and voice hashes in
Broomy's three charts silences the Wyvern. The walk also fired a camera shake
and a rumble on each of four footfalls per stride, and the landing added blur
and impact effects. All 785 instances of Effect, EffectSwitch, CameraShake,
CameraShakeSwitch, Blur and Vibrate are pointed at a sound event that is
already zeroed.

Rolls and glides went the same way. A branch's frame tag byte (+0x2D) is
compared only with the tags of the node's frame tag windows (+0x232E8D9), so
tag 0x27, which no shipped node carries, means a branch never fires. The 12
branches into roll and glide states get it.

### 9.5 The nose follows the camera

I wanted the broom's nose to point where the camera points, straight down
included. The first attempt went through the blend space. A rebuilt
`broom_riding_move` laid the clips out on rays at Broomy's speeds, and a
probe on the blend evaluator (+0x2EA98C0) showed why it could not work. The
flight path is capped at 45 degrees, so a nose that follows the path never
points down, and the blend is evaluated only in two to four second bursts as
a state starts, then frozen. No blend can follow the camera.

Pose modifiers can. They adjust bones after animation, are set per skeleton
in `character/descriptors/posemodifierdata/posemodifierdata.xml`, and the
Wyvern's AimIK turns its neck toward the aim. Broomy adds an AimIK for
`cd_r0032_00_broom.pab` with one aim bone, `B_Body_00`, and a maximum
rotation of 90 degrees. The RideOn chart's flight states already fire
`ClientFrameEventAimIK`, but its payload names a reference bone,
`B_IK_Head_00`, through string list 6. The broom has no such bone. The AimIK
apply (+0x1119B60) looks it up each frame, finds nothing and gives up, and
with every rotation at identity it switches itself off. Renaming the
reference to `B_Body_00` inside the same length makes the broom turn with the
camera.

## 10. Formats decoded on the way

### 10.1 `.paac` action charts

A chart is a flat list of count-prefixed arrays with no offsets and no
compression, read by the loader at +0x1F67A00. Reading the reader, rather
than comparing files, settled the whole layout. A parser written from it
round-trips 556 of the 558 `.paac` files byte for byte, and the game's own
deserializer, run offline in an emulator against the exe image, accepts
every one of them and a chart edited at tree level. The other two files are
not action charts.

The layout is 30 sections: nodes (one per animation clip with ten timed
arrays such as frame tags, move speeds and vertical speeds), ten string
tables (names, `.paa` paths, effects, `.motionblending` paths, bone names and
more), actions (276 bytes), branches (52 bytes), slots (28 bytes),
condition tokens and frame events. An action's key is
`hashlittle(name, 0xC5EDE)`, and so are input names. Conditions are prefix
expressions of tokens; ids 0x275 and 0x276 are the binary operators, 0x277 is
NOT, 0x83 means the animation ended and 0xE9 is a key event.

### 10.2 `.motionblending` blend spaces

A `.motionblending` file is a reflection-serialised
`ParameterizedMotionSpace`, which picks and mixes clips from one to three
measured values such as speeds and angles. The file carries its own schema.
Element footers equal their field byte count plus four, element headers carry
a type index, and a value's mask bit is set when it differs from the class
default, which the constructors at +0x2EB1B90 and +0x2EB6990 write. All 1,688
files in group 0009 round-trip byte for byte, and rescaling a blend space
keeps its triangulation valid. Dimension types are registered at +0x3151340:
1 MovingSpeed, 4 MovingSpeedUpInWorld, 6 MovingSpeedForwardInLocal, 70
AimPitch, up to 79.

### 10.3 `.paa` clips and `.paa_metabin`

The broom's and the player's 22 full clips and 22 LOD clips round-trip byte
for byte from decoded keys, with bones named from the skeletons. 31,723
`.paa_metabin` files in group 0010 round-trip too. A plugin can deliver a
replacement clip with no pack at all. With the async load sent through Read,
a broom idle pitched 20 degrees nose up played in the game. A chart can also
name a clip no pack holds; the game then asks for its metadata, LOD and full
clip by path, and the find and exists hooks point each at a stand-in.

### 10.4 Static tables and error codes

Error codes are `hashlittle(name, 0xC5EDE)`, each held in a global set up at
start, registered as `lea rcx, global` followed by the name. Request
descriptors are static objects with the kind at +8 (4 request, 1 ack) and the
id at +0xC; slot 2 of the live vtable is the server handler.

## 11. Method and tools

Everything was found by reading the exe and watching the live game.

- The exe's section names are scrambled, so the exception directory (data directory 3, at +0x170DE000)
  is the reliable list of functions. Small Python tools disassemble at an
  RVA, find cross references and strings, list RTTI vtables and their slots,
  and name the function holding an address.
- The game's own loaders document their records. Each field a loader reads
  leaves a `lea rdx,[rsi+off]` and a `mov r8d,size` before a call, with the
  field's failure message on the branch after it. Walking a loader once
  prints the record layout, names included.
- The chart deserializer runs offline under Unicorn against
  the exe image, with the allocator stubbed, which proved the chart format
  without starting the game.
- Early probe builds of the plugin hooked the resource loader,
  every condition class, the chart reads and the blend evaluator, and wrote
  what they saw to the log. Guarding a small chart's pages hung the save
  load, because those buffers share pages with other files. They are gone
  from the release.
- Frida read the live game. It is safe for reading, but
  detaching from a running game closed the game twice, and a memory-access
  monitor on a busy page and a broad scan over every writable range each
  crashed it.
- Crash dumps were read with the `minidump` package for their stacks.

## 12. Ruled out

- Repointing the Wyvern's or the ibex's row in memory, and the horse-tame
  swap. They work as test benches but take over another mount.
- New table file names. The loader drops them.
- A ninth radial item. It blanks the radial.
- A shipped mercenary list for Broomy's slot. The game crashes at boot.
- Forcing flight from the moment of mounting. Broomy took off at once, could
  not boost and could not be dismounted, because dismounting needs a ground
  state.
- Giving Broomy the Wyvern's vehicle type. It changed nothing.
- Any blend space for the nose, for the reasons in 9.5.

## 13. Limits and open questions

- The radial wedge is the only way to call Broomy. A quick press of the call
  key and a call from the riding chart, which has its own `key_callvehicle`,
  go through branches the plugin does not answer.
- Broomy's wedge takes the fourth companion wedge.
- Some places refuse every summon. The "You cannot do that here." on the
  Abyss Nexus comes from the summon validator's overlap check (+0x39A730,
  called at +0x9DDA0B), for every mount. Flight Freedom 1.1.8 lifts it with
  its BlockedSummon setting.
- Broomy flies on the Wyvern's movement, so its turns, its hold-to-descend
  and its landing behave like the Wyvern's.
- The takeoff has no push-off. The player would need a new clip.
- Most of an action record's 276 bytes and of a node header's 156 bytes are
  still unnamed.

## Appendix: addresses

Build 1.0.0.2976. The plugin finds code by byte signature where it can, and
checks every file it changes, so a different build fails safe.

| Address | What |
|---|---|
| +0x571BE40 | `pa::ResourceLoader` vtable |
| +0x12D0350 | Read |
| +0x12D03E0 | async load |
| +0x12D00B0, +0x12D1560 | find, exists |
| +0x25C53FA, +0x25C576B | table body and header formatter calls |
| +0x2439D90 | appearance XML loader |
| +0x1F67A00 | chart loader |
| +0x47F02BC | plain heap free used by the chart loader |
| +0x2BADC00 | server hire |
| +0x2A2F160 | 0x0B8F hire request handler |
| +0x29784C0 | 0x0B0C movement request handler |
| +0x2A32440 | 0x0876 call request handler |
| +0x2AF0140 | 0x0AC1 reserve slot change, server side |
| +0x2BA9950 | server mount call |
| +0x7FCD90 | `ClientFrameEventCallMercenaryReservedSlot` execute |
| +0x9DD480 | client vehicle call checks |
| +0x39A730 | summon overlap check |
| +0x1F78A20 | frame event lookup |
| +0x360BE0 | client condition token dispatch |
| +0x369990 | token 0x121 handler |
| +0x6D69AF0 | `SkillInfoManager` global |
| +0x2AD0FD0, +0xE2AFC40 | the two equipment copies |
| +0xAB8FC0 | movement update, called once at +0xAB99A7 |
| +0x2EA98C0 | blend evaluator |
| +0x1119B60 | AimIK apply |
| +0x4EDC40 | frame event factory |

## Source

The plugin is MIT licensed, at https://github.com/shin2344234/broomy. The
release is on Nexus Mods at https://www.nexusmods.com/crimsondesert/mods/3639.
