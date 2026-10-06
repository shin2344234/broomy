"""The speeder as the broom's mesh, fitted under Kliff.

    blender -b --factory-startup --python speeder/build_mesh.py -- <plain broom .pac>

Kliff sits on B_Rider_01, which stays level in every riding clip while the
broom tips under him (CD Animator's NOTES.md), so the speeder is skinned to
that bone and stays level too. It is placed with Kliff seated in the broom's
idle at frame 0: its saddle under his hips, its nose where the broom's tip
points.

Writes speeder/out/files/character/model/.../cd_r0032_00_broom_0001.pac,
speeder/out/fit.json (the grips and footrests in B_Rider_01's frame, for
build_rider.py) and speeder/out/mesh_*.png, the written mesh imported back
onto the broom with Kliff seated.
"""

import json
import os
import sys

import bpy
import bmesh
from mathutils import Matrix, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, r"C:\working\cd mods\CD animator")
import cd_animator  # noqa: E402
from cd_animator import anim, convert  # noqa: E402
from cd_animator.formats.paa_codec import bone_hash  # noqa: E402
import pacwrite  # noqa: E402

OBJ = os.path.join(HERE, "src", "source", "SpeederBikeForSketch.obj")
OUT = os.path.join(HERE, "out")
PAC = os.path.join(OUT, "files", "character", "model", "4_riding", "cd_r0032_00_broom", "cd_r0032_00_broom_0001.pac")
BROOM = "4_riding/cd_r0032_00_broom/cd_r0032_00_broom"
KLIFF_IDLE = "1_pc/1_phm/00_riding/cd_phm_rd_broom_basic_00_00_nor_std_idle_01"
BROOM_IDLE = "4_riding/cd_r0032_00_broom/cd_rd_broom_basic_00_00_nor_std_idle_01"
SEAT_BONE = "B_Rider_01"

# The model is in centimetres, nose along +X, up +Z. At 0.85 it is 3.2 m
# long, the 74-Z's length.
SCALE = 0.0085
# Points on the model, in its own centimetres.
SADDLE = Vector((8.0, 0.0, 18.0))
GRIPS = {"L": Vector((83.0, 24.0, 49.0)), "R": Vector((83.0, -24.0, 49.0))}
PEGS = {"L": Vector((-12.0, 30.0, -47.0)), "R": Vector((-12.0, -30.0, -47.0))}
# Kliff's pelvis bone sits this far above the saddle's top.
HIPS_UP = 0.10
LOD_RATIOS = (1.0, 0.5, 0.25, 0.1)


def to_world(m, offset):
    """A model point (cm) to Blender world space at the idle's frame 0:
    model +X (nose) to -Y, where Kliff faces."""
    return Vector((m.y, -m.x, m.z)) * SCALE + offset


def level_data(obj):
    """(positions, normals, uvs, triangles) of a mesh object in game space,
    one game vertex per distinct corner."""
    me = obj.data
    bm = bmesh.new()
    bm.from_mesh(me)
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    bm.to_mesh(me)
    bm.free()
    me.update()
    uv = me.uv_layers.active.data
    normals = me.corner_normals
    keys, positions, nrm, uvs, tris = {}, [], [], [], []
    for poly in me.polygons:
        corners = []
        for li in poly.loop_indices:
            vi = me.loops[li].vertex_index
            n = normals[li].vector
            u, v = uv[li].uv
            key = (vi, round(u, 5), round(v, 5), round(n.x, 3), round(n.y, 3), round(n.z, 3))
            if key not in keys:
                keys[key] = len(positions)
                positions.append(convert.vec_out(obj.matrix_world @ me.vertices[vi].co))
                nrm.append(convert.vec_out(n.normalized()))
                uvs.append((u, 1.0 - v))
            corners.append(keys[key])
        a, b, c = corners
        tris.append((a, c, b))   # the Y-Z swap mirrors, so two corners trade
    return positions, nrm, uvs, tris


def main():
    template = open(sys.argv[sys.argv.index("--") + 1], "rb").read()
    cd_animator.register()
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o)

    bpy.ops.cd_animator.game_skeleton(skeleton=BROOM)
    broom = bpy.context.object
    bpy.ops.cd_animator.game_clip(clip=BROOM_IDLE)
    bpy.ops.cd_animator.game_character(clip=KLIFF_IDLE, gear="NONE", cloth=False, sway=False, voice=False)
    kliff = next(o for o in bpy.data.objects if o.type == "ARMATURE" and o is not broom)
    anim.seat_rider(kliff, broom)
    bpy.context.scene.frame_set(0)
    bpy.context.view_layer.update()

    hips = kliff.matrix_world @ kliff.pose.bones["Bip01 Pelvis"].head
    offset = hips - Vector((0.0, 0.0, HIPS_UP)) - Vector((SADDLE.y, -SADDLE.x, SADDLE.z)) * SCALE
    seat_pose = broom.matrix_world @ broom.pose.bones[SEAT_BONE].matrix
    seat_rest = broom.matrix_world @ broom.data.bones[SEAT_BONE].matrix_local
    to_rest = seat_rest @ seat_pose.inverted()
    to_seat = seat_pose.inverted()
    fit = {"grips": {s: list(to_seat @ to_world(p, offset)) for s, p in GRIPS.items()},
           "pegs": {s: list(to_seat @ to_world(p, offset)) for s, p in PEGS.items()},
           "hips": list(to_seat @ hips)}
    os.makedirs(OUT, exist_ok=True)
    with open(os.path.join(OUT, "fit.json"), "w") as f:
        json.dump(fit, f, indent=1)
    print("fit", json.dumps(fit))

    bpy.ops.wm.obj_import(filepath=OBJ)
    src = bpy.data.objects["SpeederBikeForSketch"]
    place = to_rest @ Matrix.Translation(offset) @ Matrix.Rotation(-1.5707963, 4, "Z") @ Matrix.Scale(SCALE, 4)
    src.matrix_world = place @ src.matrix_world   # the importer's Y-up turn is on the object
    levels = []
    for r in LOD_RATIOS:
        o = src.copy()
        o.data = src.data.copy()
        bpy.context.scene.collection.objects.link(o)
        if r < 1.0:
            mod = o.modifiers.new("lod", "DECIMATE")
            mod.ratio = r
            bpy.context.view_layer.objects.active = o
            bpy.ops.object.modifier_apply(modifier=mod.name)
        lv = level_data(o)
        print(f"level ratio {r}: {len(lv[0])} vertices, {len(lv[3])} triangles")
        levels.append(lv)
        bpy.data.objects.remove(o)
    bpy.data.objects.remove(src)

    data = pacwrite.write_pac(template, levels, bone_hash(SEAT_BONE))
    os.makedirs(os.path.dirname(PAC), exist_ok=True)
    with open(PAC, "wb") as f:
        f.write(data)
    print("wrote", PAC, len(data))

    # The written file back on the broom, as the game will skin it.
    for o in [o for o in bpy.data.objects if o.parent is broom and o.type == "MESH"]:
        bpy.data.objects.remove(o)
    bpy.context.view_layer.objects.active = broom
    broom.select_set(True)
    bpy.ops.cd_animator.import_pac(filepath=PAC, textures=False)
    mesh = next(o for o in bpy.data.objects if o.type == "MESH" and o.parent is broom)
    mat = mesh.data.materials[0]
    mat.use_nodes = True
    tex = mat.node_tree.nodes.new("ShaderNodeTexImage")
    tex.image = bpy.data.images.load(os.path.join(HERE, "src", "textures", "SpeederBikeForSketch_BaseColor.png"))
    bsdf = next(n for n in mat.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    mat.node_tree.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])

    scene = bpy.context.scene
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    scene.collection.objects.link(cam)
    scene.camera = cam
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.color_type = "TEXTURE"
    scene.render.resolution_x, scene.render.resolution_y = 1000, 700
    cam.data.type = "ORTHO"
    for name, loc, rot, ortho in (("side", (5, 0.0, 1.3), (1.5708, 0, 1.5708), 3.8),
                                  ("front", (0, -5, 1.4), (1.5708, 0, 0), 2.4),
                                  ("quarter", (3.0, -3.0, 2.8), (1.1, 0, 0.785), 4.0)):
        cam.location, cam.rotation_euler, cam.data.ortho_scale = loc, rot, ortho
        scene.render.filepath = os.path.join(OUT, f"mesh_{name}.png")
        bpy.ops.render.render(write_still=True)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, "mesh.blend"))
    sys.stdout.flush()
    os._exit(0)


main()
