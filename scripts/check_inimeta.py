"""Hold the INI Master metadata up against the code that reads the ini.

Broomy embeds its own mod/Broomy.ini as the INIMETA resource (see
mod/src/resources.rc): the file embed_ini.cmake turns into the default the
plugin writes on first run and the ;@ directives INI Master reads are the
same text. What can still drift is the ;@ lines against the code: a changed
clamp or fallback that nobody copied into the ini.

The code is the authority. For every key mod/src/core/mod.cpp reads with
ReadSetting():

  - it must appear in Broomy.ini's [settings] section with a ;@ type
    directive, and every key carrying one must be a key the code reads;
  - a key read as ReadSetting(...) != 0 must be type=bool true=1 false=0,
    and any other key type=int;
  - the ini's own value must match the fallback the code passes, which for
    the four speeds is the initialiser in struct Speeds (game/broomchart.h);
  - the ;@ min/max must match the clamp the code applies: Clamp() in
    EarlyInstall for the speeds, SetSlowest() in game/analogspeed.cpp for
    SlowestPush;
  - the ;@mod line says live=0, since nothing rereads the ini while the game
    runs, and its name and author match release.json and Seth.

Exits non-zero on any mismatch, so it can gate a release.

    py -3 scripts/check_inimeta.py
"""

import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..")
INI = os.path.join(ROOT, "mod", "Broomy.ini")
MOD_CPP = os.path.join(ROOT, "mod", "src", "core", "mod.cpp")
CHART_H = os.path.join(ROOT, "mod", "src", "game", "broomchart.h")
ANALOG_CPP = os.path.join(ROOT, "mod", "src", "game", "analogspeed.cpp")
RELEASE_JSON = os.path.join(ROOT, "release.json")


def read(p):
    with open(p, encoding="utf-8") as f:
        return f.read()


def parse_speed_defaults(h):
    m = re.search(r"struct Speeds\s*\{(.*?)\};", h, re.S)
    if not m:
        raise ValueError("struct Speeds not found in broomchart.h")
    return {name: int(v) for name, v in re.findall(r"int\s+(\w+)\s*=\s*(-?\d+);", m.group(1))}


def parse_reader(cpp, speeds):
    """key -> {kind, default, min, max} from every ReadSetting() call."""
    out = {}
    for m in re.finditer(
            r'Clamp\(ReadSetting\(L"(\w+)",\s*speeds\.(\w+)\),\s*(-?\d+),\s*(-?\d+)\)', cpp):
        key, field, lo, hi = m.group(1), m.group(2), int(m.group(3)), int(m.group(4))
        if field not in speeds:
            raise ValueError("%s falls back to speeds.%s, which struct Speeds does not have" % (key, field))
        out[key] = {"kind": "int", "default": speeds[field], "min": lo, "max": hi}
    for m in re.finditer(r'ReadSetting\(L"(\w+)",\s*(-?\d+)\)(\s*!=\s*0)?', cpp):
        key, default, bang = m.group(1), int(m.group(2)), m.group(3)
        if key in out:
            continue
        out[key] = {"kind": "bool" if bang else "int", "default": default, "min": None, "max": None}
    if not out:
        raise ValueError("mod.cpp reads no keys; the parser is probably out of date")
    return out


def parse_slowest_clamp(cpp):
    m = re.search(r"percent < (\d+) \? \1 : percent > (\d+) \? \2", cpp)
    if not m:
        raise ValueError("SetSlowest()'s clamp not found in analogspeed.cpp")
    return int(m.group(1)), int(m.group(2))


def parse_mod_line(text):
    m = re.search(r"^;@mod\s+(.*)$", text, re.M)
    if not m:
        return {}
    return {fm.group(1): fm.group(2) if fm.group(2) is not None else fm.group(3)
            for fm in re.finditer(r'(\w+)=(?:"([^"]*)"|(\S+))', m.group(1))}


def parse_ini(text):
    """key -> {directive fields..., 'value': ini literal} for every key under
    [settings] with a ;@ line in the block above it."""
    out = {}
    section = None
    pending = {}
    for line in text.splitlines():
        s = line.strip()
        sec = re.match(r"^\[(\w+)\]$", s)
        if sec:
            section, pending = sec.group(1), {}
            continue
        if s.startswith(";@") and not s.startswith(";@mod"):
            for m in re.finditer(r'(\w+)=("[^"]*"|\S+)|(\w+)', s[2:]):
                if m.group(1):
                    pending[m.group(1)] = m.group(2).strip('"')
                else:
                    pending[m.group(3)] = True
            continue
        if not s:
            pending = {}
            continue
        if s.startswith(";"):
            continue
        kv = re.match(r"^(\w+)\s*=\s*(.*)$", s)
        if kv and section == "settings" and pending:
            pending["value"] = kv.group(2).strip()
            out[kv.group(1)] = pending
        pending = {}
    return out


def main():
    mod_cpp = read(MOD_CPP)
    ini_text = read(INI)
    release = json.loads(read(RELEASE_JSON))
    reader = parse_reader(mod_cpp, parse_speed_defaults(read(CHART_H)))
    slow_lo, slow_hi = parse_slowest_clamp(read(ANALOG_CPP))
    if "SlowestPush" in reader:
        reader["SlowestPush"].update(min=slow_lo, max=slow_hi)
    ini_keys = parse_ini(ini_text)
    mod_line = parse_mod_line(ini_text)
    errors = []

    if not mod_line:
        errors.append(";@mod: no ;@mod line in Broomy.ini")
    else:
        if mod_line.get("name") != release.get("name"):
            errors.append(";@mod name=%r, release.json says %r" % (mod_line.get("name"), release.get("name")))
        if mod_line.get("author") != "Seth":
            errors.append(";@mod author=%r, expected Seth" % mod_line.get("author"))
        if mod_line.get("live") != "0":
            errors.append(";@mod live=%r, expected 0: nothing in mod.cpp rereads the ini while the game runs"
                          % mod_line.get("live"))

    for k in sorted(set(reader) - set(ini_keys)):
        errors.append("%s: mod.cpp reads it and the ini has no ;@ directive for it" % k)
    for k in sorted(set(ini_keys) - set(reader)):
        errors.append("%s: has a ;@ directive and mod.cpp never reads it" % k)

    for k, want in reader.items():
        spec = ini_keys.get(k)
        if spec is None:
            continue
        if spec.get("type") != want["kind"]:
            errors.append("%s: ;@ type=%s, the code reads it as %s" % (k, spec.get("type"), want["kind"]))
        if want["kind"] == "bool":
            if spec.get("true") != "1" or spec.get("false") != "0":
                errors.append("%s: ;@ should say true=1 false=0, the code treats anything but 0 as on" % k)
            want_value = "1" if want["default"] else "0"
        else:
            want_value = str(want["default"])
        if spec.get("value") != want_value:
            errors.append("%s: ini value %s, the code falls back to %s" % (k, spec.get("value"), want_value))
        for field in ("min", "max"):
            if want[field] is None:
                continue
            got = spec.get(field)
            if got is None or int(got) != want[field]:
                errors.append("%s: ;@ %s=%s, the code clamps to %s" % (k, field, got, want[field]))
        if want["min"] is not None and not (want["min"] <= want["default"] <= want["max"]):
            errors.append("%s: default %s falls outside the %s..%s the code clamps to"
                          % (k, want["default"], want["min"], want["max"]))

    for line in errors:
        print("error  " + line)
    print("%d keys read by mod.cpp, %d described in the ini, %d errors" % (len(reader), len(ini_keys), len(errors)))
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
