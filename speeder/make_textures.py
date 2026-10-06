"""The speeder's three textures, as the game's material wants them.

    py -3 speeder/make_textures.py <broom texture folder>

The folder holds the broom's cd_t0000_broom_0001.dds and _n.dds unpacked,
whose headers are the templates. Writes into speeder/out/files under the
game paths the plugin serves:

    _0001.dds     base colour, the model's own with its ambient occlusion
                  multiplied in, 1024 px DXT1
    _0001_sp.dds  material map: R specular level, G roughness, B metal,
                  512 px DXT1 (CD Animator's NOTES.md has the channels)
    _0001_n.dds   a flat normal map, 64 px BC5. The model's normal map needs
                  tangents, and the vertex record's tangent bytes are not
                  decoded, so the speeder goes without.

Mips go down to 4 px. Every reserved word stays 0, which the game reads as
plain levels.
"""

import io
import os
import struct
import sys

from PIL import Image, ImageChops

HERE = os.path.dirname(os.path.abspath(__file__))
TEX = os.path.join(HERE, "src", "textures", "SpeederBikeForSketch_")
OUT = os.path.join(HERE, "out", "files", "character", "texture")
STEM = "cd_r0032_00_speeder_0001"
SPECULAR = 200


def dxt1_levels(img):
    out = []
    while True:
        b = io.BytesIO()
        img.save(b, "DDS", pixel_format="DXT1")
        out.append(b.getvalue()[128:])
        if img.width <= 4:
            return out
        img = img.resize((img.width // 2, img.height // 2), Image.LANCZOS)


def header(template, size, levels, top):
    h = bytearray(template[:128])
    struct.pack_into("<5I", h, 12, size, size, top, 0, levels)
    h[0x20:0x4C] = bytes(0x4C - 0x20)
    return bytes(h)


def main():
    broom = sys.argv[1]
    base_t = open(os.path.join(broom, "cd_t0000_broom_0001.dds"), "rb").read()
    norm_t = open(os.path.join(broom, "cd_t0000_broom_0001_n.dds"), "rb").read()
    assert base_t[84:88] == b"DXT1" and norm_t[84:88] == b"BC5U"
    os.makedirs(OUT, exist_ok=True)

    color = Image.open(TEX + "BaseColor.png").convert("RGB")
    ao = Image.open(TEX + "ao.png").convert("L")
    color = ImageChops.multiply(color, Image.merge("RGB", (ao, ao, ao))).resize((1024, 1024), Image.LANCZOS)
    lv = dxt1_levels(color)
    with open(os.path.join(OUT, STEM + ".dds"), "wb") as f:
        f.write(header(base_t, 1024, len(lv), len(lv[0])) + b"".join(lv))

    rough = Image.open(TEX + "Roughness.png").convert("L").resize((512, 512), Image.LANCZOS)
    metal = Image.open(TEX + "Metalness.png").convert("L").resize((512, 512), Image.LANCZOS)
    sp = Image.merge("RGB", (Image.new("L", (512, 512), SPECULAR), rough, metal))
    lv = dxt1_levels(sp)
    with open(os.path.join(OUT, STEM + "_sp.dds"), "wb") as f:
        f.write(header(base_t, 512, len(lv), len(lv[0])) + b"".join(lv))

    # BC5: two BC4 blocks per 4x4, each both endpoints 128 and every index 0.
    flat = bytes((128, 128, 0, 0, 0, 0, 0, 0)) * 2
    lv, size = [], 64
    while size >= 4:
        lv.append(flat * (size // 4) ** 2)
        size //= 2
    with open(os.path.join(OUT, STEM + "_n.dds"), "wb") as f:
        f.write(header(norm_t, 64, len(lv), len(lv[0])) + b"".join(lv))
    for n in ("", "_sp", "_n"):
        p = os.path.join(OUT, STEM + n + ".dds")
        print(p, os.path.getsize(p))


main()
