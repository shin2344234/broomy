"""Round trip: the broom's mesh through CD Animator's reader and pacwrite
must give the shipped file back, apart from the vertex bytes pacwrite does
not carry (+6, +12 and the normal's low 10 bits).

    py -3 speeder/check_pacwrite.py <plain broom .pac>
"""
import struct
import sys

sys.path.insert(0, r"C:/working/cd mods/CD animator/cd_animator")
from formats import pac  # noqa: E402

import pacwrite  # noqa: E402

src = open(sys.argv[1], "rb").read()
levels = []
for lv in range(4):
    sm = pac.read_pac(src, lv).submeshes[0]
    levels.append((sm.positions, sm.normals, sm.uvs, sm.triangles))
hash_ = pac.read_pac(src, 0).palette[0]
out = pacwrite.write_pac(src, levels, hash_)
assert len(out) == len(src), (len(out), len(src))
s0 = struct.unpack_from("<I", src, 0x14)[0]
diff = [i for i in range(0x50 + s0) if out[i] != src[i]]
print("section 0 bytes that differ:", len(diff), [hex(i) for i in diff[:12]])
# Vertices: positions requantise against the same box, so they match, and
# normals match outside the low 10 bits.
bad = 0
for lv in range(4):
    a = pac.read_pac(out, lv).submeshes[0]
    b = pac.read_pac(src, lv).submeshes[0]
    for p, q in zip(a.positions, b.positions):
        if max(abs(p[i] - q[i]) for i in range(3)) > 1e-4: bad += 1
    for p, q in zip(a.normals, b.normals):
        if max(abs(p[i] - q[i]) for i in range(3)) > 1e-3: bad += 1
    assert a.triangles == b.triangles and a.uvs == b.uvs
print("vertices off:", bad)
assert bad == 0 and len(diff) <= 24, "round trip failed"
print("PASS")
