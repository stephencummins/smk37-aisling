#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""A sound pack from its source: packs/NAME.json -> build/packs/NAME.aispack (firmware/src/app/pack.h has the bytes)
and build/packs/NAME.json (what the Packs page shows and sends: the source's words, the pack as base64).

    tools/make_pack.py packs/saints-sinners.json [-o build/packs]

A piece may have an "icon" for the page: an SVG path (its d) on a 24 by 24 grid, drawn as a line. A recipe's keys are the engine's parameters as amb.h names them, less the P_ (TEMPO, LVOICE); a value a number, a
hex string ("0x49": a pattern's steps), or the name the screen shows for it ("Dorian", "Bodhran"). The ranges and
names come from the engine itself (host/en_params.c, built here), so a pack cannot ask for what it has not.
"""
import base64, json, os, re, struct, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PK_FORMAT, PK_NPIECES, PK_MAX, PK_NAME, PK_PNAME = 1, 8, 3840, 20, 16


def engine_table():
    """[(lo, hi, default, name, texts)], the P_ names in order, the engine's level, the pack's air's number"""
    exe = os.path.join(ROOT, "build", "host", "en_params")
    os.makedirs(os.path.dirname(exe), exist_ok=True)
    subprocess.check_call(["cc", "-O1", "-DEN_HOST", "-I" + os.path.join(ROOT, "firmware/src/dsp"), "-o", exe,
                           os.path.join(ROOT, "host/en_params.c"), os.path.join(ROOT, "firmware/src/dsp/amb.c"), "-lm"])
    lines = subprocess.check_output([exe], text=True).splitlines()
    m = re.match(r"level (\d+) airs (\d+)", lines[0])
    level, airs = int(m.group(1)), int(m.group(2))
    table = []
    for ln in lines[1:]:
        m = re.match(r'(\d+) (-?\d+) (-?\d+) (-?\d+) "([^"]*)"(.*)', ln)
        table.append((int(m.group(2)), int(m.group(3)), int(m.group(4)), m.group(5), re.findall(r'"([^"]*)"', m.group(6))))
    src = open(os.path.join(ROOT, "firmware/src/dsp/amb.h")).read()
    src = re.sub(r"/\*.*?\*/", "", src, flags=re.S)
    m = re.search(r"enum\s*\{([^}]*\bP_NPARAMS\b[^}]*)\}", src)
    names = [x.strip() for x in m.group(1).split(",") if x.strip()]
    names = names[:names.index("P_NPARAMS")]
    if len(names) != len(table) or any(not n.startswith("P_") or "=" in n for n in names):
        sys.exit("amb.h's parameters and the engine's table disagree (%d names, %d entries)" % (len(names), len(table)))
    return table, [n[2:] for n in names], level, airs


def value(key, v, lo, hi, texts, where):
    if isinstance(v, str):
        if re.fullmatch(r"0[xX][0-9a-fA-F]+", v):
            v = int(v, 16)
        else:
            hit = [i for i, t in enumerate(texts) if t.lower() == v.lower()]
            if not hit:
                sys.exit("%s: %s has no value \"%s\"%s" % (where, key, v, " (it has: %s)" % ", ".join(texts) if texts else ""))
            v = lo + hit[0]
    if not isinstance(v, int) or isinstance(v, bool) or not lo <= v <= hi:
        sys.exit("%s: %s = %r is outside %d..%d" % (where, key, v, lo, hi))
    return v


def name_bytes(s, n, where):
    b = s.encode("ascii", "strict") if all(32 <= ord(c) < 127 for c in s) else None
    if not b or len(b) >= n:
        sys.exit("%s: the name \"%s\" must be 1 to %d plain characters" % (where, s, n - 1))
    return b.ljust(n, b"\0")


def build(src):
    table, names, level, airs = engine_table()
    idx = {n: i for i, n in enumerate(names)}
    need = int(src.get("level", level))
    if need > level:
        sys.exit("the pack asks for engine level %d; this engine is level %d" % (need, level))
    pieces = src["pieces"]
    if len(pieces) != PK_NPIECES:
        sys.exit("a pack has %d pieces; this has %d" % (PK_NPIECES, len(pieces)))
    body = b""
    shown = []
    for k, pc in enumerate(pieces):
        where = "piece %d (%s)" % (k + 1, pc.get("name", "?"))
        side = pc.get("side", "sinner")
        if side not in ("saint", "sinner", "light", "dark"):
            sys.exit("%s: side is \"saint\" (the white screen) or \"sinner\" (the black)" % where)
        light = side in ("saint", "light")
        air = pc.get("air") or {}
        notes, lens = air.get("notes", []), air.get("lengths", [])
        if len(notes) != len(lens) or len(notes) > 16:
            sys.exit("%s: an air is up to 16 notes, each with a length" % where)
        if any(not -24 <= n <= 36 for n in notes) or any(not 1 <= d <= 255 for d in lens):
            sys.exit("%s: an air's notes are -24..36 semitones over the root, their lengths 1..255 sixteenths" % where)
        pairs = b""
        full = [t[2] for t in table]
        for key, v in pc["recipe"].items():
            if key.startswith("_"):
                continue
            if key not in idx:
                sys.exit("%s: no parameter %s" % (where, key))
            if key == "AIR":
                sys.exit("%s: the air is the piece's \"air\", not a parameter" % where)
            i = idx[key]
            lo, hi, _, _, texts = table[i]
            full[i] = value(key, v, lo, hi, texts, where)
            pairs += struct.pack("<Bh", i, full[i])
        if len(pairs) // 3 > 255:
            sys.exit("%s: too many parameters" % where)
        body += name_bytes(pc["name"], PK_PNAME, where) + bytes([1 if light else 0, len(notes), len(pairs) // 3, 0])
        body += struct.pack("%db" % len(notes), *notes) + bytes(lens) + pairs
        icon = pc.get("icon", "")              # (the page's: an SVG path on a 24 by 24 grid, drawn as a line)
        if not re.fullmatch(r"[MmLlHhVvCcSsQqTtAaZz0-9 .,\-]{0,400}", icon):
            sys.exit("%s: its icon is an SVG path's d, up to 400 characters" % where)
        shown.append({"name": pc["name"], "icon": icon, "side": "saint" if light else "sinner", "about": pc.get("about", ""),
                      "tempo": full[idx["TEMPO"]], "key": table[idx["ROOT"]][4][full[idx["ROOT"]]],
                      "mode": table[idx["MODE"]][4][full[idx["MODE"]]]})
    total = 32 + len(body)
    if total > PK_MAX:
        sys.exit("the pack is %d bytes; %d is the most" % (total, PK_MAX))
    head = b"APK1" + bytes([PK_FORMAT, need, PK_NPIECES, 0]) + struct.pack("<HH", total, int(src.get("version", 1)))
    blob = head + name_bytes(src["name"], PK_NAME, "the pack") + body
    # the update loader erases any sector whose bytes 6..7 at 0xF00 read "AT" under a good CRC (ldr_core.c): the
    # pack's payload starts 256 bytes into its sector, so that is the pack's 0xE06; never let it read so
    if len(blob) > 0xE08 and blob[0xE06:0xE08] == b"AT":
        sys.exit("the pack's bytes at 0xE06 would read as the update loader's mark: reorder a recipe")
    return blob, shown, need


def main():
    args = sys.argv[1:]
    out = os.path.join(ROOT, "build", "packs")
    if "-o" in args:
        out = args[args.index("-o") + 1]
        del args[args.index("-o"):args.index("-o") + 2]
    if len(args) != 1:
        sys.exit(__doc__)
    src = json.load(open(args[0]))
    blob, shown, need = build(src)
    stem = os.path.splitext(os.path.basename(args[0]))[0]
    os.makedirs(out, exist_ok=True)
    open(os.path.join(out, stem + ".aispack"), "wb").write(blob)
    json.dump({"format": "aisling-pack", "id": stem, "name": src["name"], "version": int(src.get("version", 1)),
               "level": need, "needs": src.get("needs", ""), "about": src.get("about", ""), "size": len(blob), "pieces": shown,
               "data": base64.b64encode(blob).decode()},
              open(os.path.join(out, stem + ".json"), "w"), indent=1)
    print("%s: %d bytes of %d, level %d" % (stem, len(blob), PK_MAX, need))


if __name__ == "__main__":
    main()
