#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The emulator's home-screen icons: the site's harp (web/brand/favicon.svg, 16 x 16 pixels), enlarged, as PNGs.
   tools/make_app_icons.py OUTDIR  ->  touch-180.png, icon-192.png, icon-512.png, maskable-512.png
Standard library only. The maskable one keeps the harp inside the middle 80% (a launcher may cut the rest off)."""
import re, struct, sys, zlib
from pathlib import Path

SVG = Path(__file__).resolve().parent.parent / "web" / "brand" / "favicon.svg"

def harp():
    svg = SVG.read_text(encoding="utf-8")
    rgb = lambda h: tuple(int(h[i:i + 2], 16) for i in (1, 3, 5))
    ground = rgb(re.search(r"<rect[^>]*fill='(#[0-9a-f]{6})'", svg).group(1))
    px = [[ground] * 16 for _ in range(16)]
    for fill, d in re.findall(r"<path fill='(#[0-9a-f]{6})' d='([^']*)'", svg):
        for x, y, w, h in re.findall(r"M(\d+) (\d+)h(\d+)v(\d+)H\d+z", d):
            for yy in range(int(y), int(y) + int(h)):
                for xx in range(int(x), int(x) + int(w)):
                    px[yy][xx] = rgb(fill)
    return ground, px

def png(path, size, cell, ground, px):
    off = (size - 16 * cell) // 2
    rows = []
    for y in range(size):
        r = bytearray([0])
        sy = (y - off) // cell
        for x in range(size):
            sx = (x - off) // cell
            r += bytes(px[sy][sx] if 0 <= sx < 16 and 0 <= sy < 16 else ground)
        rows.append(bytes(r))
    chunk = lambda t, d: struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d))
    path.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 2, 0, 0, 0))
                     + chunk(b"IDAT", zlib.compress(b"".join(rows), 9)) + chunk(b"IEND", b""))

if __name__ == "__main__":
    out = Path(sys.argv[1])
    out.mkdir(parents=True, exist_ok=True)
    ground, px = harp()
    for name, size, cell in (("touch-180.png", 180, 9), ("icon-192.png", 192, 10), ("icon-512.png", 512, 28), ("maskable-512.png", 512, 24)):
        png(out / name, size, cell, ground, px)
