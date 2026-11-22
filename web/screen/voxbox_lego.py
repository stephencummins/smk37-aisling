#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""VOXBOX: a Lego tray for the screen. Writes the LDraw file, the parts list and OpenSCAD scenes, and checks
that nothing overlaps, everything is held on by studs, and the measured boards fit.

Second design, 15 Nov 2026. The first assumed 14 mm under the screen's board and 7 mm above it. Photos of the
real box, scaled by the 2.54 mm pin spacing, give about 19.5 mm under and 4.6 mm above, so the pillars are one
plate taller (22.4 mm) and the low rest is now a single tile that only stops the corner dipping.

Grid: col 0..12 left to right, row 0..9 front to back, level in plates (3.2 mm) with 0 the underside of the floor.
"""
import json, sys, os
from collections import Counter, defaultdict

OUT = sys.argv[1]
os.makedirs(OUT, exist_ok=True)

# id: (name, studs along X in LDraw's own orientation, studs along Z, height in plates, studs on top)
PARTS = {
    "3033":  ("Plate 6 x 10", 10, 6, 1, True),
    "4477":  ("Plate 1 x 10", 10, 1, 1, True),
    "3795":  ("Plate 2 x 6",   6, 2, 1, True),
    "3460":  ("Plate 1 x 8",   8, 1, 1, True),
    "3623":  ("Plate 1 x 3",   3, 1, 1, True),
    "3068b": ("Tile 2 x 2",    2, 2, 1, False),
    "3001":  ("Brick 2 x 4",   4, 2, 3, True),
    "3002":  ("Brick 2 x 3",   3, 2, 3, True),
    "3003":  ("Brick 2 x 2",   2, 2, 3, True),
    "3004":  ("Brick 1 x 2",   2, 1, 3, True),
    "3622":  ("Brick 1 x 3",   3, 1, 3, True),
    "3009":  ("Brick 1 x 6",   6, 1, 3, True),
    "3008":  ("Brick 1 x 8",   8, 1, 3, True),
    "3245b": ("Brick 1 x 2 x 2", 2, 1, 6, True),
}
# LDraw colour codes and the same for the pictures
COL = {71: ("light grey", [0.66, 0.68, 0.70]), 72: ("dark grey", [0.39, 0.41, 0.42]),
       0: ("black", [0.13, 0.13, 0.14]), 25: ("orange", [0.93, 0.47, 0.10])}

model = []   # dicts: part, col, row, w (cols), d (rows), z (bottom, plates), h, colour, step

def put(step, part, col, row, z, colour, along="x"):
    name, lx, lz, h, _ = PARTS[part]
    w, d = (lx, lz) if along == "x" else (lz, lx)
    model.append(dict(part=part, col=col, row=row, w=w, d=d, z=z, h=h, colour=colour, step=step, along=along))

STEPS = [
    "The floor",
    "The first course of the wall",
    "The second course",
    "Three pillars and the stop",
    "The first layer of plates on the wall",
    "The second layer of plates",
    "The foot under the back edge",
]
PILLAR_AT = ((1, 2), (10, 7), (1, 7))
PILLAR_PLATES = 7      # brick, brick, tile
STOP_AT = (10, 3)
STOP_PLATES = 1        # one tile

# 1. Floor, 13 x 10: two 6 x 10 plates and a 1 x 10 down the right edge.
put(1, "3033", 0, 0, 0, 71, "z")
put(1, "3033", 6, 0, 0, 71, "z")
put(1, "4477", 12, 0, 0, 71, "z")

# 2. First course. Front wall two studs deep; sides; two stubs at the back, the cable gap between them.
for c in (0, 4, 8): put(2, "3001", c, 0, 1, 72)
put(2, "3004", 12, 0, 1, 72, "z")
put(2, "3008", 0, 2, 1, 72, "z")
put(2, "3008", 12, 2, 1, 72, "z")
put(2, "3004", 1, 9, 1, 72)
put(2, "3004", 10, 9, 1, 72)

# 3. Second course, its joins away from the first's, turning each corner.
put(3, "3622", 0, 0, 4, 72, "z")
put(3, "3622", 12, 0, 4, 72, "z")
put(3, "3001", 1, 0, 4, 72)
put(3, "3002", 5, 0, 4, 72)
put(3, "3001", 8, 0, 4, 72)
put(3, "3009", 0, 3, 4, 72, "z")
put(3, "3009", 12, 3, 4, 72, "z")
put(3, "3622", 0, 9, 4, 72)
put(3, "3622", 10, 9, 4, 72)

# 4. Pillars: brick, brick, tile = 22.4 mm. The stop under the small board: one tile = 3.2 mm.
for (c, r) in PILLAR_AT:
    put(4, "3003", c, r, 1, 25)
    put(4, "3003", c, r, 4, 25)
    put(4, "3068b", c, r, 7, 25)
put(4, "3068b", STOP_AT[0], STOP_AT[1], 1, 25)

# 5. Plates, first layer.
put(5, "3795", 0, 0, 7, 0)
put(5, "3795", 6, 0, 7, 0)
put(5, "4477", 12, 0, 7, 0, "z")
put(5, "3460", 0, 2, 7, 0, "z")
put(5, "3460", 1, 9, 7, 0)
put(5, "3623", 9, 9, 7, 0)

# 6. Plates, second layer, the other way round.
put(6, "4477", 0, 0, 8, 0, "z")
put(6, "3795", 1, 0, 8, 0)
put(6, "3795", 7, 0, 8, 0)
put(6, "3460", 12, 2, 8, 0, "z")
put(6, "3623", 1, 9, 8, 0)
put(6, "3460", 4, 9, 8, 0)

# 7. Foot under the back two rows, two bricks deep.
for c in (1, 5, 9): put(7, "3001", c, 8, -3, 72)
put(7, "3003", 1, 8, -6, 72)
put(7, "3001", 3, 8, -6, 72)
put(7, "3001", 7, 8, -6, 72)
put(7, "3003", 11, 8, -6, 72)
# Stephen's, from the tray he built (15 Nov 2026): a tall brick at the left end, so the foot runs the whole width.
put(7, "3245b", 0, 8, -6, 72, "z")

# ---------------------------------------------------------------- checks
fails = []
def cells(p):
    return {(c, r) for c in range(p["col"], p["col"] + p["w"]) for r in range(p["row"], p["row"] + p["d"])}

occ = {}
for i, p in enumerate(model):
    for (c, r) in cells(p):
        if not (0 <= c <= 12 and 0 <= r <= 9): fails.append(f"part {i} {p['part']} off the 13 x 10 grid at {c},{r}")
        for z in range(p["z"], p["z"] + p["h"]):
            if (c, r, z) in occ: fails.append(f"part {i} {p['part']} overlaps part {occ[(c, r, z)]} at {c},{r},{z}")
            occ[(c, r, z)] = i

# stud joins: a part with studs, and whatever sits directly on those studs
links = defaultdict(set)
for i, p in enumerate(model):
    top = p["z"] + p["h"]
    for (c, r) in cells(p):
        j = occ.get((c, r, top))
        if j is not None:
            if not PARTS[p["part"]][4]: fails.append(f"part {j} sits on the tile {i}")
            links[i].add(j); links[j].add(i)
seen, todo = {0}, [0]
while todo:
    for j in links[todo.pop()]:
        if j not in seen: seen.add(j); todo.append(j)
if len(seen) != len(model): fails.append(f"{len(model) - len(seen)} parts are not held on by studs: "
                                         + ", ".join(f"{i}:{model[i]['part']}" for i in range(len(model)) if i not in seen))

# in building order, each part must hold on to something already there
done = set()
for i, p in enumerate(model):
    if p['step'] > 1 and not (links[i] & done): fails.append(f"step {p['step']}: part {i} {p['part']} has nothing to hold on to yet")
    done.add(i)

# the floor is whole
for c in range(13):
    for r in range(10):
        if (c, r, 0) not in occ: fails.append(f"hole in the floor at {c},{r}")
# the floor's two seams (between cols 5|6 and 11|12) are each crossed above and below
for seam in (5, 11):
    for where, zs in (("above", [1, 4]), ("below", [-1])):
        if not any(occ.get((seam, r, z)) is not None and occ.get((seam, r, z)) == occ.get((seam + 1, r, z))
                   for r in range(10) for z in zs):
            fails.append(f"floor seam after col {seam} is not crossed {where}")

# the pocket: cols 1..11, rows 2..8 clear of wall; the cable gap cols 3..9 of row 9 clear for both brick courses
POCKET = [(c, r) for c in range(1, 12) for r in range(2, 9)]
PILLARS = {(c + dc, r + dr) for (c, r) in PILLAR_AT for dc in (0, 1) for dr in (0, 1)}
STOP = {(STOP_AT[0] + dc, STOP_AT[1] + dr) for dc in (0, 1) for dr in (0, 1)}
for (c, r) in POCKET:
    tall = max((z + 1 for z in range(1, 12) if (c, r, z) in occ), default=1) - 1   # plates above the floor's top
    want = PILLAR_PLATES if (c, r) in PILLARS else STOP_PLATES if (c, r) in STOP else 0
    if tall != want: fails.append(f"pocket {c},{r}: {tall} plates high, expected {want}")
for c in range(3, 10):
    for z in range(1, 7):
        if (c, 9, z) in occ: fails.append(f"cable gap blocked at col {c} level {z}")
    if (c, 9, 7) not in occ or (c, 9, 8) not in occ: fails.append(f"no lintel over the gap at col {c}")
# the wall: every border cell reaches level 9, except the gap's brick courses
for c in range(13):
    for r in range(10):
        border = c in (0, 12) or r in (0, 1, 9)
        if border and (c, r, 8) not in occ: fails.append(f"wall short at {c},{r}")
        if border and not (r == 9 and 3 <= c <= 9):
            for z in range(1, 9):
                if (c, r, z) not in occ: fails.append(f"hole in the wall at {c},{r},{z}")

# sizes in mm. The heights are read from photos of the real box, scaled by the 2.54 mm pin spacing (the small
# board came out at 65.2 mm against its real 65.0, and two photos agree within about 0.5 mm). The outline is
# the screen's nominal size; the photos agree with it to about 1 mm.
P, S, STUD = 3.2, 8.0, 1.7
SIZES = dict(board_l=85.5, board_w=56.0, pcb=1.6,
             under_face=17.9,   # screen board's underside down to the small board's outer face
             under_tips=19.5,   # ... down to the tips of the small board's soldered pins, its lowest point
             above=4.6,         # screen board's top up to the glass
             gap=16.4,          # between the two boards
             plug_h=6.4)        # a USB-C plug's body
SIZES_FROM = ("Heights read from photos of the real box, scaled by the 2.54 mm pin spacing: good to about "
              "0.7 mm. Outline is the screen's nominal size; the photos agree with it to about 1 mm. "
              "Not yet checked with a ruler. Stephen built a prototype from loose bricks on 15 Nov 2026: "
              "\"it worked well\".")
pillar, stop, wall = PILLAR_PLATES * P, STOP_PLATES * P, 8 * P
port_mid = pillar - SIZES["gap"] + 1.6          # middle of a USB-C socket, above the floor's top
facts = dict(
    outer=(13 * S, 10 * S), pocket=(11 * S, 7 * S), pillar=pillar, wall=wall, wall_studs=wall + STUD,
    floor_studs=STUD, stop=stop,
    small_board_low=pillar - SIZES["under_tips"],           # pin tips above the floor's top
    small_board_underside=pillar - SIZES["under_face"],     # the small board's face above the floor's top
    pins_clear_of_studs=pillar - SIZES["under_tips"] - STUD,
    stop_gap=pillar - SIZES["under_face"] - stop,           # how far the corner can dip before the stop takes it
    screen_face=pillar + SIZES["pcb"] + SIZES["above"],
    glass_over_wall_studs=pillar + SIZES["pcb"] + SIZES["above"] - (wall + STUD),
    plug_low=port_mid - SIZES["plug_h"] / 2, plug_high=port_mid + SIZES["plug_h"] / 2, cable_gap=6 * P,
    foot=6 * P, parts=len(model))
facts = {k: (tuple(round(x, 1) for x in v) if isinstance(v, tuple) else round(v, 1) if isinstance(v, float) else v)
         for k, v in facts.items()}
if facts["pins_clear_of_studs"] < 1.0: fails.append("small board's pins under 1 mm above the floor's studs")
if SIZES["board_l"] > 11 * S or SIZES["board_w"] > 7 * S + 0.2: fails.append("board bigger than the pocket")
if facts["stop"] > facts["small_board_underside"]: fails.append("the stop is taller than the small board's underside")
if facts["pillar"] + SIZES["pcb"] > facts["wall"]: fails.append("the board's edge stands above the wall, nothing holds it sideways")
if facts["plug_low"] < STUD + 0.5: fails.append("a plug would rub the floor's studs")
if facts["plug_high"] > facts["cable_gap"] - 0.5: fails.append("a plug would not pass under the lintel")

# ---------------------------------------------------------------- LDraw
def ldraw_line(p):
    x = (p["col"] + p["w"] / 2) * 20 - 130
    zc = (p["row"] + p["d"] / 2) * 20 - 100
    y = -(p["z"] + p["h"]) * 8
    m = "1 0 0 0 1 0 0 0 1" if p["along"] == "x" else "0 0 1 0 1 0 -1 0 0"
    return f"1 {p['colour']} {x:g} {y:g} {zc:g} {m} {p['part']}.dat"

lines = ["0 VOXBOX: Lego tray for the screen", "0 Name: voxbox-lego-tray.ldr", "0 Author: Stephen Cummins",
         f"0 13 x 10 studs. Pocket 11 x 7 studs (88 x 56 mm). Pillars {pillar:.1f} mm. Front of the model is -Z."]
for s, title in enumerate(STEPS, 1):
    lines.append(f"0 // Step {s}: {title}")
    lines += [ldraw_line(p) for p in model if p["step"] == s]
    lines.append("0 STEP")
open(os.path.join(OUT, "voxbox-lego-tray.ldr"), "w", newline="").write("\r\n".join(lines) + "\r\n")

# ---------------------------------------------------------------- parts list
by_part = Counter(p["part"] for p in model)
rows = [dict(part=k, name=PARTS[k][0], count=n) for k, n in sorted(by_part.items(), key=lambda kv: (PARTS[kv[0]][3] < 3, -PARTS[kv[0]][1] * PARTS[kv[0]][2]))]
per_step = [dict(step=s, title=t, parts=[dict(part=k, name=PARTS[k][0], count=n) for k, n in
            Counter(p["part"] for p in model if p["step"] == s).items()]) for s, t in enumerate(STEPS, 1)]
json.dump(dict(parts=rows, steps=per_step, facts=facts, sizes=SIZES, sizes_from=SIZES_FROM),
          open(os.path.join(OUT, "parts.json"), "w"), indent=1)

# ---------------------------------------------------------------- BrickLink wanted list
# One line for each part in each colour, in BrickLink's own numbers. Colours: light bluish grey 86, dark bluish
# grey 85, black 11, orange 4. BrickLink's upload takes the 2 x 2 tile as 3068; Studio gives the tall brick as 3245b.
BL_COLOUR = {71: 86, 72: 85, 0: 11, 25: 4}
BL_ITEM = {"3068b": "3068"}
wanted = Counter((BL_ITEM.get(p["part"], p["part"]), BL_COLOUR[p["colour"]]) for p in model)
open(os.path.join(OUT, "voxbox-lego-tray-bricklink.xml"), "w", newline="").write(
    "<INVENTORY>\n" + "".join(
        f"<ITEM><ITEMTYPE>P</ITEMTYPE><ITEMID>{item}</ITEMID><COLOR>{colour}</COLOR><MINQTY>{n}</MINQTY></ITEM>\n"
        for (item, colour), n in sorted(wanted.items())) + "</INVENTORY>\n")

# ---------------------------------------------------------------- OpenSCAD
def scad_part(p, colour=None, alpha=1.0):
    rgb = colour or COL[p["colour"]][1]
    x, y, z = p["col"] * S, p["row"] * S, p["z"] * P
    studs = "true" if PARTS[p["part"]][4] else "false"
    return f"color([{rgb[0]},{rgb[1]},{rgb[2]},{alpha}]) translate([{x:g},{y:g},{z:g}]) piece({p['w']},{p['d']},{p['h']},{studs});"

HEAD = """$fn = 28;
module piece(w, d, h, studs) {
  translate([0.1, 0.1, 0]) cube([w*8 - 0.2, d*8 - 0.2, h*3.2 - 0.06]);
  if (studs) for (i = [0:w-1]) for (j = [0:d-1]) translate([i*8 + 4, j*8 + 4, h*3.2 - 0.06]) cylinder(d = 4.8, h = 1.76);
}
"""
BOARD_Z = P + pillar          # the screen board's underside, from the underside of the floor
GAP = SIZES["gap"]
SCREEN = f"""
module screen() {{
  // from the photos: board 85.5 x 56 x 1.6, glass 4.6 mm above the board's top, 16.4 mm between the boards,
  // small board's face 17.9 mm and its pin tips 19.5 mm under the screen board
  translate([8 + 1.2, 16 + 0.1, {BOARD_Z:g}]) {{
    color([0.10, 0.25, 0.55]) cube([85.5, 55.8, 1.6]);
    color([0.10, 0.10, 0.11]) translate([0.8, 0.8, 1.6]) cube([83.9, 54.2, 0.9]);
    color([0.75, 0.76, 0.78]) translate([0.5, 0.5, 2.5]) cube([84.5, 54.8, 2.7]);
    color([0.05, 0.05, 0.06]) translate([0.5, 0.5, 5.2]) cube([84.5, 54.8, 1.0]);
    color([0.62, 0.42, 0.72]) translate([6, 4, 6.2]) cube([73.5, 49, 0.05]);
    color([0.93, 0.62, 0.30]) translate([6, 20, 6.22]) cube([73.5, 14, 0.05]);
    color([0.86, 0.40, 0.55]) translate([6, 4, 6.24]) cube([73.5, 9, 0.05]);
    color([0.08, 0.08, 0.09]) translate([85.5 - 7.1 - 33, 0.9, -13.8]) cube([33, 5.1, 13.8]);
    color([0.08, 0.08, 0.09]) translate([85.5 - 7.1 - 50.8, 0.9, -{GAP:g}]) cube([50.8, 5.1, 2.6]);
    color([0.10, 0.25, 0.55]) translate([20.5, 0, -{GAP + 1.6:g}]) cube([65, 30, 1.6]);
    color([0.85, 0.85, 0.87]) translate([85.5 - 7.1 - 50.2, 2.2, -{GAP + 3.2:g}]) cube([49.6, 2.6, 1.6]);
    color([0.8, 0.8, 0.82]) for (x = [44.1, 31.5]) translate([x - 4.5, 23, -{GAP:g}]) cube([9, 7.4, 3.2]);
    color([0.8, 0.8, 0.82]) translate([73.1 - 5.6, 23.5, -{GAP:g}]) cube([11.2, 6.9, 3.3]);
  }}
}}
module plugs() {{
  color([0.95, 0.80, 0.15]) for (x = [44.1, 31.5]) translate([8 + 1.2 + x, 16 + 30.4, {BOARD_Z - GAP + 1.6:g}]) {{
    translate([-6, 0, -3.2]) cube([12, 24, 6.4]);
    translate([0, 24, 0]) rotate([-90, 0, 0]) cylinder(d = 4, h = 26);
  }}
}}
"""
def scene(name, body, tilt=False, extra=""):
    inner = "\n".join(body) + "\n" + extra
    if tilt: inner = "rotate([16.7, 0, 0]) {\n" + inner + "\n}\ncolor([0.80, 0.74, 0.66]) translate([-22, -22, -1.0]) cube([148, 134, 1.0]);"
    open(os.path.join(OUT, name + ".scad"), "w").write(HEAD + SCREEN + inner + "\n")

GHOST = [0.86, 0.86, 0.84]
for s in range(1, len(STEPS) + 1):
    body = [scad_part(p, None if p["step"] == s else GHOST) for p in model if p["step"] <= s]
    if s == 4:   # the front wall hides the front pillar: draw that wall see-through, and last so it blends
        front = lambda p: p["step"] in (2, 3) and p["row"] + p["d"] <= 2
        body = ([scad_part(p, None if p["step"] == s else GHOST) for p in model if p["step"] <= s and not front(p)]
                + [scad_part(p, GHOST, 0.28) for p in model if front(p)])
    if s == 7:   # the foot goes on from underneath: show it upside down
        body = ["translate([0, 80, 28.8]) rotate([180, 0, 0]) {"] + body + ["}"]
    scene(f"step{s}", body)
scene("empty", [scad_part(p) for p in model], tilt=True)
scene("fitted", [scad_part(p) for p in model], tilt=True, extra="screen(); plugs();")
scene("back", [scad_part(p) for p in model], tilt=True, extra="screen(); plugs();")
# a cut through the fitted tray, looking from the right end, to check the heights by eye
scene("section", ["difference() { union() {"] + [scad_part(p) for p in model if p["step"] < 7],
      extra="screen(); plugs(); }\n  translate([92, -50, -50]) cube([200, 300, 200]); }")

print(f"{len(model)} parts in {len(STEPS)} steps; {len(by_part)} kinds; {len(wanted)} lines on the wanted list")
for r in rows: print(f"  {r['count']:2d} x {r['name']:13s} ({r['part']})")
for k, v in facts.items(): print(f"  {k}: {v}")
print("checks:", "all passed" if not fails else f"{len(fails)} FAILED")
for f in fails[:30]: print("  FAIL", f)
sys.exit(1 if fails else 0)
