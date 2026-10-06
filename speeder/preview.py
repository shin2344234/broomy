"""A look at the result: Kliff in his outfit on the speeder, playing the
exported idle (or the clip named on the command line), from three sides.

    blender -b --factory-startup --python speeder/preview.py -- [suffix]
"""

import os
import sys

import bpy

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, r"C:\working\cd mods\CD animator")
import cd_animator  # noqa: E402
from cd_animator import anim  # noqa: E402

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
SUFFIX = argv[0] if argv else "nor_std_idle_01"
OUT = os.path.join(HERE, "out")
ASSETS = os.path.join(HERE, "..", "mod", "assets")
PAC = os.path.join(ASSETS, "character", "model", "4_riding", "cd_r0032_00_broom", "cd_r0032_00_broom_0001.pac")
CLIP = os.path.join(ASSETS, "character", "motion", "1_pc", "1_phm", "00_riding",
                    f"cd_phm_rd_broom_basic_00_00_{SUFFIX}.paa")

cd_animator.register()
for o in list(bpy.data.objects):
    bpy.data.objects.remove(o)
bpy.ops.cd_animator.game_skeleton(skeleton="4_riding/cd_r0032_00_broom/cd_r0032_00_broom")
broom = bpy.context.object
bpy.ops.cd_animator.game_clip(clip=f"4_riding/cd_r0032_00_broom/cd_rd_broom_basic_00_00_{SUFFIX}")
bpy.ops.cd_animator.import_pac(filepath=PAC, textures=False)
mesh = next(o for o in bpy.data.objects if o.type == "MESH" and o.parent is broom)
mat = mesh.data.materials[0]
mat.use_nodes = True
tex = mat.node_tree.nodes.new("ShaderNodeTexImage")
tex.image = bpy.data.images.load(os.path.join(HERE, "src", "textures", "SpeederBikeForSketch_BaseColor.png"))
bsdf = next(n for n in mat.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
mat.node_tree.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])

bpy.ops.cd_animator.game_character(gear="AWAY", cloth=False, sway=False, voice=False)
kliff = next(o for o in bpy.data.objects if o.type == "ARMATURE" and o is not broom)
bpy.context.view_layer.objects.active = kliff
bpy.ops.cd_animator.import_paa(filepath=CLIP)
anim.seat_rider(kliff, broom)

scene = bpy.context.scene
scene.frame_set(0)
cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
scene.collection.objects.link(cam)
scene.camera = cam
cam.data.lens = 50
scene.render.engine = "BLENDER_WORKBENCH"
scene.display.shading.color_type = "TEXTURE"
scene.display.shading.light = "STUDIO"
scene.render.resolution_x, scene.render.resolution_y = 1200, 800
for name, loc, rot in (("quarter", (3.6, -4.2, 2.6), (1.25, 0, 0.71)),
                       ("side", (6.5, 0.2, 1.6), (1.5708, 0, 1.5708)),
                       ("back", (-2.6, 4.6, 2.4), (1.25, 0, 3.66))):
    cam.location, cam.rotation_euler = loc, rot
    scene.render.filepath = os.path.join(OUT, f"preview_{SUFFIX}_{name}.png")
    bpy.ops.render.render(write_still=True)
sys.stdout.flush()
os._exit(0)
