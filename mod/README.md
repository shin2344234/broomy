# Broomy 0.12.32

For Crimson Desert 2.03.02, exe 1.0.0.2976.

Pearl Abyss built a flying broom mount and never switched it on. The model,
its skeleton and its animations are all in the game's archives. This plugin
turns it into Broomy, a mount of its own with a wedge on the Character
radial. No other mount is changed.

## Installing

Copy `Broomy.asi` into the game's `bin64` folder, next to
`CrimsonDesert.exe`, with the game closed. You need an ASI loader there
already. Ultimate ASI Loader installed as `winmm.dll` is what most Crimson
Desert setups use, and the Definitive Mod Manager installs one for you.

No game file is modified. The first time it runs, the plugin writes
`Broomy.ini` beside itself with every setting at its default. An ini you
already have is left alone.

## Getting Broomy

Load a save. A few seconds later the plugin looks for Broomy among your
mounts, and if it is missing, a wild horse the game has loaded joins your
roster as Broomy. It happens once per save. A save that has Broomy already is
left alone.

Broomy's wedge takes the place of the fourth companion wedge on the Character
radial. Pick it and Kliff calls Broomy the way he calls the Wyvern.

## Riding

Mount from either side. On the ground Broomy hovers and moves about. Jump to
take off. In the air the nose follows the camera, the run key boosts, and
letting go of the stick holds Broomy in place. The flying controls are the
Wyvern's.

On a controller the speed follows how far the left stick is pushed. The
boost and the keyboard always move at full speed.

## Settings

All of them live in `Broomy.ini` and are read when the game starts.

    GiveBroomy=1       give Broomy to a save that does not have it
    GroundSpeed=250    speed on the ground, percent of the Wyvern's
    FlightSpeed=250    speed in the air, percent of the Wyvern's
    BoostSpeed=200     the boost, percent of Broomy's flight speed
    ClimbSpeed=200     climbing and diving, percent of the Wyvern's
    SlowestPush=15     speed at the lightest stick push, percent of full

At the defaults Broomy cruises at 62.5 m/s and boosts at 125. Set
`SlowestPush=100` for one speed whatever the push.

INI Master (https://www.nexusmods.com/crimsondesert/mods/3578) can edit these
with a label and range for each.

## Uninstalling

Delete the `Broomy` files from `bin64`. A save that has Broomy still
loads, and Broomy leaves the radial. If you save without the plugin, that
save loses Broomy, and putting the plugin back gives you a new one.

## Known limits

- The radial wedge is the only way to call Broomy.
- A fourth companion's wedge can't be reached while the plugin is installed,
  because Broomy's wedge sits in its place. The radial holds eight wedges,
  and a ninth blanks it.
- Some places refuse every summon, the Abyss Nexus among them. That is the
  game's own rule.
