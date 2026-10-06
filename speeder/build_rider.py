"""Kliff's riding clips for the speeder: hands on the grips, feet on the
footrests.

    blender -b --factory-startup --python speeder/build_rider.py -- [--only SUFFIX]

Each of his broom riding clips plays with the broom's clip of the same name,
the pairing Pearl Abyss made them in, and keeps everything but his limbs:
IK pins his wrists to the grips and his ankles to the footrests, frame by
frame in the frame of B_Rider_01, the bone the speeder is skinned to
(speeder/out/fit.json, from build_mesh.py). His spine leans a little
further forward so the grips are in reach. Export bakes the IK.

His takeoff is a clip of the plugin's own: the speeder idle's first frame
held for 136 frames, the length riderfix.cpp gives the takeoff action.
The mount and dismount clips stay the game's.

Writes the clip and its LOD under speeder/out/files at the game's paths, and
speeder/out/rider_*.png, a side view of each clip's frame 0 on the speeder.
"""

import json
import math
import os
import shutil
import sys

import bpy
from mathutils import Quaternion, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, r"C:\working\cd mods\CD animator")
import cd_animator  # noqa: E402
from cd_animator import anim, clip, export, packs  # noqa: E402

OUT = os.path.join(HERE, "out")
FILES = os.path.join(OUT, "files")
PAC = os.path.join(FILES, "character", "model", "4_riding", "cd_r0032_00_broom", "cd_r0032_00_broom_0001.pac")
BROOM = "4_riding/cd_r0032_00_broom/"
RIDER = "1_pc/1_phm/00_riding/"
SUFFIXES = ("nor_std_idle_01", "nor_move_walk_f_ing_00", "nor_move_walkfast_f_ing_00", "nor_move_run_f_ing_00",
            "nor_move_runfast_f_ing_00", "nor_move_walkfast_f_75u_ing_00", "nor_move_walkfast_f_75d_ing_00")
TAKEOFF = "nor_std_takeoff_00"
TAKEOFF_FRAMES = 136
SEAT = "B_Rider_01"
SIDES = ("L", "R")
# From a grip's centre to the wrist, and from a footrest's top to the
# ankle, in B_Rider_01's frame (x lateral, y up, z forward).
WRIST = Vector((0.0, 0.03, -0.08))
ANKLE = Vector((0.0, 0.09, -0.07))
# Degrees of extra forward lean per clip, split over Spine1 and Spine2:
# enough to reach the grips at rest, and more the faster he goes.
LEAN = {"nor_std_idle_01": 10, "nor_move_walk_f_ing_00": 10, "nor_move_walkfast_f_ing_00": 14,
        "nor_move_run_f_ing_00": 22, "nor_move_runfast_f_ing_00": 28, "nor_move_walkfast_f_75u_ing_00": 6,
        "nor_move_walkfast_f_75d_ing_00": 16, "nor_std_takeoff_00": 10}


def update(f):
    bpy.context.scene.frame_set(f)
    bpy.context.view_layer.update()


def frames(action):
    return range(0, round(action["cd_duration"] * anim.FPS) + 1)


def game_path(kind, suffix):
    leaf = f"cd_phm_rd_broom_basic_00_00_{suffix}"
    if kind == "clip":
        return os.path.join(FILES, "character", "motion", *RIDER.split("/"), leaf + ".paa")
    return os.path.join(FILES, "character", "motion", "motion_lod__", *RIDER.split("/"), leaf + "_lod.paa")


def lean(kliff, action, degrees):
    """`degrees` more forward lean on Spine1 and Spine2, on every key."""
    bag = export._channelbag(kliff, action)
    for name in ("Bip01 Spine1", "Bip01 Spine2"):
        pb = kliff.pose.bones[name]
        update(0)
        # The bone's local axis that is world X (Kliff's lateral axis) at frame 0.
        m = pb.matrix.to_3x3() @ pb.matrix_basis.to_3x3().inverted()
        axis = (m.inverted() @ Vector((1.0, 0.0, 0.0))).normalized()
        turn = Quaternion(axis, math.radians(degrees / 2))
        # Forward is the sign that moves the head toward -Y.
        head = kliff.pose.bones["Bip01 Head"]
        before = (kliff.matrix_world @ head.head).y
        base = pb.rotation_quaternion.copy()
        pb.rotation_quaternion = turn @ base
        bpy.context.view_layer.update()
        if (kliff.matrix_world @ head.head).y > before:
            turn = turn.inverted()
        pb.rotation_quaternion = base
        path = f'pose.bones["{name}"].rotation_quaternion'
        curves = [fc for fc in bag.fcurves if fc.data_path == path]
        keys = sorted({p.co[0] for fc in curves for p in fc.keyframe_points}) or [0]
        values = {}
        for f in keys:
            update(int(f))
            values[f] = turn @ pb.rotation_quaternion
        for fc in curves:
            bag.fcurves.remove(fc)
        for f, q in values.items():
            pb.rotation_quaternion = q
            pb.keyframe_insert("rotation_quaternion", frame=f)


def hips_on_seat(kliff, broom):
    """His pelvis in B_Rider_01's frame on the current frame."""
    seat = broom.matrix_world @ broom.pose.bones[SEAT].matrix
    return seat.inverted() @ kliff.matrix_world @ kliff.pose.bones["Bip01 Pelvis"].matrix


def seat_hips(kliff, broom, hips, rng):
    """Key his pelvis on every frame where the idle's first frame has it in
    B_Rider_01's frame, turn included. On the broom he kneels lower in the
    runs and lies back along it in the climb; the speeder stays level, so
    he sits as he does at rest and his spine and limbs keep the clip's
    motion."""
    seat = broom.pose.bones[SEAT]
    pelvis = kliff.pose.bones["Bip01 Pelvis"]
    for f in rng:
        update(f)
        pelvis.matrix = kliff.matrix_world.inverted() @ broom.matrix_world @ seat.matrix @ hips
        bpy.context.view_layer.update()
        pelvis.keyframe_insert("location", frame=f)
        pelvis.keyframe_insert("rotation_quaternion", frame=f)


def pin_limbs(kliff, broom, fit, rng):
    """IK on both arms and legs, targets keyed on every frame."""
    seat = broom.pose.bones[SEAT]
    made = []
    for s in SIDES:
        for bone, point, offset in ((f"Bip01 {s} Forearm", fit["grips"][s], WRIST),
                                    (f"Bip01 {s} Calf", fit["pegs"][s], ANKLE)):
            t = bpy.data.objects.new(f"target {bone}", None)
            bpy.context.scene.collection.objects.link(t)
            local = Vector(point) + offset
            for f in rng:
                update(f)
                t.location = broom.matrix_world @ (seat.matrix @ local)
                t.keyframe_insert("location", frame=f)
            ik = kliff.pose.bones[bone].constraints.new("IK")
            ik.target, ik.chain_count = t, 2
            made.append((kliff.pose.bones[bone], ik, t))
    return made


def unpin(made):
    for pb, ik, t in made:
        pb.constraints.remove(ik)
        bpy.data.objects.remove(t)


def render(name):
    scene = bpy.context.scene
    scene.render.filepath = os.path.join(OUT, f"rider_{name}.png")
    update(0)
    bpy.ops.render.render(write_still=True)


def setup_scene(broom):
    bpy.context.view_layer.objects.active = broom
    bpy.ops.cd_animator.import_pac(filepath=PAC, textures=False)
    scene = bpy.context.scene
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    scene.collection.objects.link(cam)
    scene.camera = cam
    cam.data.type = "ORTHO"
    cam.data.ortho_scale = 3.2
    cam.location, cam.rotation_euler = (5.0, 0.3, 1.3), (1.5708, 0, 1.5708)
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.render.resolution_x, scene.render.resolution_y = 800, 600


def write(kliff, action, suffix):
    path = game_path("clip", suffix)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    written, problems = export.export_with_companions(kliff, action, path)
    lod = game_path("lod", suffix)
    os.makedirs(os.path.dirname(lod), exist_ok=True)
    shutil.move(path[:-4] + "_lod.paa", lod)
    os.remove(path[:-4] + ".paa_metabin")   # the events are the template's; the game keeps its own
    for p in problems:
        print("  note:", p)
    print("wrote", os.path.relpath(path, OUT), "and its LOD")


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    only = argv[argv.index("--only") + 1] if "--only" in argv else None
    with open(os.path.join(OUT, "fit.json")) as f:
        fit = json.load(f)
    cd_animator.register()
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o)
    bpy.ops.cd_animator.game_skeleton(skeleton=BROOM + "cd_r0032_00_broom")
    broom = bpy.context.object
    setup_scene(broom)
    bpy.ops.cd_animator.game_skeleton(skeleton="1_pc/1_phm/phm_01")
    kliff = bpy.context.object
    bpy.ops.cd_animator.game_mesh(mesh="nude/cd_phm_00_nude_00_0001", textures=False, cloth=False, sway=False)
    bpy.context.view_layer.objects.active = kliff

    # Where the idle seats him, the reference for every clip.
    bpy.context.view_layer.objects.active = broom
    bpy.ops.cd_animator.game_clip(clip=BROOM + "cd_rd_broom_basic_00_00_nor_std_idle_01")
    bpy.context.view_layer.objects.active = kliff
    bpy.ops.cd_animator.game_clip(clip=RIDER + "cd_phm_rd_broom_basic_00_00_nor_std_idle_01")
    anim.seat_rider(kliff, broom)
    update(0)
    hips = hips_on_seat(kliff, broom)

    for suffix in SUFFIXES + (TAKEOFF,):
        if only and suffix != only:
            continue
        print("clip", suffix)
        bpy.context.view_layer.objects.active = broom
        broom_suffix = "nor_std_idle_01" if suffix == TAKEOFF else suffix
        bpy.ops.cd_animator.game_clip(clip=BROOM + f"cd_rd_broom_basic_00_00_{broom_suffix}")
        bpy.context.view_layer.objects.active = kliff
        if suffix == TAKEOFF:
            idle = packs.fetch_clip(packs.index(), RIDER + "cd_phm_rd_broom_basic_00_00_nor_std_idle_01")
            action = clip.new_clip(bpy.context, kliff, idle, f"cd_phm_rd_broom_basic_00_00_{TAKEOFF}",
                                   TAKEOFF_FRAMES, hold=True)
            rng = range(0, TAKEOFF_FRAMES + 1)
        else:
            bpy.ops.cd_animator.game_clip(clip=RIDER + f"cd_phm_rd_broom_basic_00_00_{suffix}")
            action = kliff.animation_data.action
            rng = frames(action)
        anim.seat_rider(kliff, broom)
        if suffix == TAKEOFF:
            seat_hips(kliff, broom, hips, range(0, 1))
            # The clip holds one pose, so frame 0's pelvis holds to the end.
            pelvis = kliff.pose.bones["Bip01 Pelvis"]
            pelvis.keyframe_insert("location", frame=TAKEOFF_FRAMES)
            pelvis.keyframe_insert("rotation_quaternion", frame=TAKEOFF_FRAMES)
        else:
            seat_hips(kliff, broom, hips, rng)
        lean(kliff, action, LEAN[suffix])
        # The takeoff holds the idle's first frame, so its targets do too.
        made = pin_limbs(kliff, broom, fit, rng if suffix != TAKEOFF else range(0, 1))
        render(suffix)
        write(kliff, action, suffix)
        unpin(made)
    sys.stdout.flush()
    os._exit(0)


main()
