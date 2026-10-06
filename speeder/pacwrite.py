"""Write a one-part, one-bone .pac mesh in the layout of the broom's.

The layout is CD Animator's formats/pac.py read backwards. Section 0 is
rebuilt from the broom's own file: its flags, part and material names, the
two unread floats and the Havok data after the bounding box are kept, and
only the offsets, counts, bounding boxes and the palette change. With the
names the same length, section 0 keeps its size. Sections 1 to 4 hold the
detail levels, least detailed first.

A level is (positions, normals, uvs, triangles) in game space: Y up, metres,
UV with V down, triangles wound as the game winds them. Every vertex follows
one bone, palette slot 0, fully.
"""

import struct

STRIDE = 40


def _name(s, off):
    n = s[off]
    return off + 1 + n


def _qpos(p, mn, ext):
    return tuple(max(0, min(32767, round((p[i] - mn[i]) / ext[i] * 32767))) if ext[i] else 0 for i in range(3))


def _normal(n):
    x = max(0, min(1023, round((n[0] + 1.0) * 511.5)))
    y = max(0, min(1023, round((n[1] + 1.0) * 511.5)))
    return (x << 10) | (y << 20) | (0x40000000 if n[2] < 0 else 0)


def write_pac(template, levels, bone_hash):
    """The new file's bytes. `template` is the broom's .pac, plain (every
    section's stored size 0). `levels` holds 4 levels, most detailed first."""
    if template[:4] != b"PAR " or any(struct.unpack_from("<I", template, 0x10 + 8 * i)[0] for i in range(8)):
        raise ValueError("the template must be a plain PAR file")
    if len(levels) != 4:
        raise ValueError("four detail levels are needed")
    size0 = struct.unpack_from("<I", template, 0x14)[0]
    s = bytearray(template[0x50:0x50 + size0])
    if struct.unpack_from("<I", s, 0)[0] != 0 or s[4] != 4:
        raise ValueError("the template is not a flag-free four-level mesh")
    if struct.unpack_from("<H", s, 37)[0] != 1:
        raise ValueError("the template has more than one part")

    allp = [p for lv in levels for p in lv[0]]
    mn = tuple(min(p[i] for p in allp) for i in range(3))
    mx = tuple(max(p[i] for p in allp) for i in range(3))
    ext = tuple(mx[i] - mn[i] for i in range(3))

    # Geometry sections, least detailed level first.
    blobs = []
    counts = []
    for positions, normals, uvs, tris in levels:
        if len(positions) > 65535:
            raise ValueError(f"{len(positions)} vertices do not fit u16 indices")
        vb = bytearray()
        for p, n, uv in zip(positions, normals, uvs):
            q = _qpos(p, mn, ext)
            vb += struct.pack("<3HH2eHHI", q[0], q[1], q[2], 0, uv[0], uv[1], 0, 0x3C00, _normal(n))
            vb += struct.pack("<II", 0, 0)                     # palette slot 0 in every slot
            vb += bytes((255, 0, 0, 0, 0, 0, 0, 0))            # all on the first
            vb += b"\xff\xff\xff\xff"                          # held by the skeleton (63)
        ib = struct.pack(f"<{3 * len(tris)}H", *[i for t in tris for i in t])
        blobs.append((bytes(vb), ib))
        counts.append((len(positions), 3 * len(tris)))

    # File offsets: header, section 0, then levels 3, 2, 1, 0.
    vstart, istart = [0] * 4, [0] * 4
    off = 0x50 + size0
    for lv in (3, 2, 1, 0):
        vstart[lv] = off
        istart[lv] = off + len(blobs[lv][0])
        off = istart[lv] + len(blobs[lv][1])

    struct.pack_into("<4I", s, 5, *vstart)
    struct.pack_into("<4I", s, 21, *istart)
    o = _name(s, _name(s, 39))
    o += 3
    struct.pack_into("<6f", s, o + 8, *mn, *ext)
    o += 32
    if s[o] != 4:
        raise ValueError("the template's part does not have four levels")
    o += 5
    struct.pack_into("<4H", s, o, *[c[0] for c in counts])
    struct.pack_into("<4I", s, o + 8, *[c[1] for c in counts])
    o += 24
    if struct.unpack_from("<H", s, o)[0] != 1:
        raise ValueError("the template's palette is not one bone")
    struct.pack_into("<I", s, o + 2, bone_hash)
    struct.pack_into("<6f", s, o + 6, *mn, *mx)

    head = bytearray(template[:0x50])
    sizes = [size0] + [len(blobs[lv][0]) + len(blobs[lv][1]) for lv in (3, 2, 1, 0)]
    for i in range(8):
        struct.pack_into("<II", head, 0x10 + 8 * i, 0, sizes[i] if i < 5 else 0)
    out = bytes(head) + bytes(s) + b"".join(blobs[lv][0] + blobs[lv][1] for lv in (3, 2, 1, 0))
    assert len(out) == off
    return out
