#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""tools/fm1pkg_make.py for each board, without the SDK (stand-ins for its files, at their real sizes):
flash.bin, the app area's entries and the .fwsc's marker blocks. For the SMK-37 Elite the region
descriptors are compared with M-VAVE's own SMK-37 Elite_016.fwsc. Run from the repo root:
  python3 tests/pkg_test.py"""
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import fm1pkg_make as P  # noqa: E402

failed = 0


def ok(cond, what):
    global failed
    print(f"{what:<72} {'ok' if cond else 'FAIL'}")
    failed += not cond


def raises(f):
    try:
        f()
    except SystemExit:
        return True
    return False


# stand-ins for the SDK's files, at AC79NN_SDK_V1.2.1's sizes
SDK = {"uboot.boot": 14536, "cfg_tool.bin": 383, "cfg/eq_cfg_hw.bin": 655}
P.sdk_file = lambda rel: bytes((i * 13 + len(rel)) & 0xFF for i in range(SDK[rel]))

# M-VAVE's SMK-37 Elite_016.fwsc (sha256 b2ee38ab...): its app area's region descriptors, decrypted
# (entries 3..8: VM, PRCT, BTIF, USRTRIM, USRFLASH, USR)
ELITE_016 = [bytes.fromhex(h) for h in (
    "bf0cffff00d009000030020012810000564d00ffffffffffffffffff00800000",
    "1d3cffff0000000000d00900928200005052435400ffffffffffffff434f4445",
    "6400ffff00000c0000100000928200004254494600ffffffffffffff4155544f",
    "1804ffff00100c0000100000928100005553525452494d00ffffffffffffffff",
    "cbd0ffff00200c000090020092810000555352464c41534800ffffffffffffff",
    "0498ffff00400f0000a000009281010055535200ffffffffffffffffffffffff")]
IDENTITY = {"fm1": "FM-1_353010000", "smk37": "SMK-37 Elite_353010000"}


def area_entries(flash):
    a = bytearray(flash[0x4000:0x4120])
    P.sfc(a, 0, len(a), 0, P.KEY)
    return [bytes(a[o:o + 32]) for o in range(0, 0x120, 32)]


def unpack(raw):
    """(marker blocks, identity, logical image), the block count taken from the file as a reader takes it:
    its length less the total in the header (the header is logical 0..0x3F, in the first two blocks)"""
    hdr = bytearray(raw[0:0x2F] + raw[0x30:0x41])
    P.enc(hdr, 0, 0x40)
    n = len(raw) - struct.unpack_from("<I", hdr, 4)[0]
    marks = [raw[i * 0x30 + 0x2F] for i in range(n)]
    k = marks.index(0x7D) if 0x7D in marks else n
    ident = "".join(chr((m - i - 1) & 0xFF) for i, m in enumerate(marks[:k]))
    tail_ok = all(m == 0x7D for m in marks[k:])
    logical = b"".join(raw[i * 0x30:i * 0x30 + 0x2F] for i in range(n)) + raw[n * 0x30:]
    return n, ident if tail_ok else None, logical


for board, b in P.BOARDS.items():
    slot, size = b["app_slot"], b["flash_size"]
    ok(slot == size - 0x4000 - 0x120 - 0xF24, f"{board}: app slot {slot:#x} = app area less entries and cfg blocks")
    ok(raises(lambda: P.flash_image(bytes(slot + 1), P.KEY, board)), f"{board}: an app over the slot is refused")
    app = bytes((i * 7 + 1) & 0xFF for i in range(slot))
    flash = P.flash_image(app, P.KEY, board)
    ok(len(flash) == size, f"{board}: flash.bin is {len(flash):#x} B")
    ents = area_entries(flash)[:3 + len(b["regions"])]     # (then blank: 0xFF)
    ok(all(struct.unpack_from("<H", e)[0] == P.crc16(e[2:]) for e in ents), f"{board}: app area entries' CRCs")
    name = [e[16:32].split(b"\0")[0].decode("latin-1") for e in ents]
    ok(name[:3] == ["app_area_head", "app.bin", "cfg_tool.bin"], f"{board}: app_area_head, app.bin, cfg_tool.bin")
    off, ln = struct.unpack_from("<II", ents[1], 4)
    ok((off, ln) == (0x120, slot), f"{board}: app.bin at 0x120, {slot:#x} B")
    regions = [struct.unpack_from("<IIBBH", e, 4) for e in ents[3:3 + len(b["regions"])]]
    ok(regions == [r[:5] for r in b["regions"]], f"{board}: region descriptors as in BOARDS")
    ok((0, size) in [(r[0], r[1]) for r in regions], f"{board}: PRCT covers flash.bin")
    if board == "smk37":
        ok(ents[3:9] == ELITE_016, "smk37: region descriptors == M-VAVE's SMK-37 Elite_016")

    ota = bytes(range(256)) * 25
    raw = P.ufw(flash, ota, IDENTITY[board], board)
    n, ident, logical = unpack(raw)
    ok(n == b["blocks"], f"{board}: {n} marker blocks, from the file's length and header")
    ok(ident == IDENTITY[board], f"{board}: identity {ident!r}")
    hdr = bytearray(logical[:0x40])
    P.enc(hdr, 0, 0x40)
    ok(struct.unpack_from("<H", hdr)[0] == P.crc16(hdr[2:0x40]), f"{board}: package header CRC")
    ok(logical[0x400:0x400 + size] == flash, f"{board}: flash.bin at 0x400 of the logical image")
    ok(raises(lambda: P.ufw(flash, ota, "X" * (b["blocks"] + 1), board)),
       f"{board}: an identity over {b['blocks']} characters is refused")

ok(raises(lambda: P.ufw(bytes(16), bytes(16), IDENTITY["smk37"], "fm1")), "fm1: the 22-character SMK identity is refused")

# the firmware's map (hal/fl_map.h, hal/smk37/smk_flash_map.h): the app area the loader writes ends where
# the package's flash.bin does
for board, hdr in (("fm1", "firmware/hal/fl_map.h"), ("smk37", "firmware/hal/smk37/smk_flash_map.h")):
    m = re.search(r"^#define FL_APP_HI\s+(0x[0-9A-Fa-f]+)u", (ROOT / hdr).read_text(), re.M)
    ok(m and int(m.group(1), 16) == P.BOARDS[board]["flash_size"], f"{board}: {hdr} FL_APP_HI = flash.bin's size")
print("package: " + ("all ok" if not failed else f"{failed} FAILED"))
sys.exit(1 if failed else 0)
