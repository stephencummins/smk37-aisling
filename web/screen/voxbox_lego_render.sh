#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Rebuild the VOXBOX Lego tray: the .ldr, the BrickLink wanted list, parts.json and the ten pictures.
# Usage: ./render.sh [work dir]   (needs OpenSCAD and Python with Pillow and numpy)
# The model, the checks and the scenes come from voxbox_lego.py; this only renders and trims the pictures.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
WORK=${1:-$(mktemp -d)}
SC=${OPENSCAD:-/Applications/OpenSCAD.app/Contents/MacOS/OpenSCAD}
python3 "$HERE/voxbox_lego.py" "$WORK"
cd "$WORK"
r() { "$SC" -o "$1.png" --imgsize=2800,2000 --camera="$2" --viewall --colorscheme=Tomorrow "$1.scad" >/dev/null 2>&1 || { echo "render failed: $1"; exit 1; }; }
for n in 1 2 3 4 5 6; do r step$n "-25,-120,215,52,40,8" & done; wait
r step7 "-40,-150,170,52,40,12" &
r empty "-60,-170,150,52,45,18" &
r fitted "-60,-170,150,52,45,18" &
r back "150,270,130,52,45,18" & wait
python3 - <<'EOF'
from PIL import Image
import numpy as np, os
for n in ["step1", "step2", "step3", "step4", "step5", "step6", "step7", "empty", "fitted", "back"]:
    a = np.array(Image.open(n + ".png").convert("RGB"))
    clear = np.abs(a.astype(int) - a[2, 2].astype(int)).sum(axis=2) <= 3      # the background goes clear
    im = Image.fromarray(np.dstack([a, np.where(clear, 0, 255).astype(np.uint8)]), "RGBA")
    box = im.getchannel("A").getbbox()
    if box[0] <= 1 or box[1] <= 1 or box[2] >= im.width - 1 or box[3] >= im.height - 1: raise SystemExit(n + " touches the edge")
    im = im.crop((max(0, box[0] - 30), max(0, box[1] - 30), min(im.width, box[2] + 30), min(im.height, box[3] + 30)))
    im = im.resize((im.width // 2, im.height // 2), Image.LANCZOS)
    im.save(n + ".webp", quality=86, method=6)
    print(n, im.size, os.path.getsize(n + ".webp"))
EOF
cp voxbox-lego-tray.ldr voxbox-lego-tray-bricklink.xml parts.json step?.webp empty.webp fitted.webp back.webp "$HERE/"
echo "written to $HERE"
