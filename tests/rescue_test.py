#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""tools/fm1_rescue.py against a simulated FM-1 in the chip's update mode (no hardware, no pyusb).
The fake answers the loader's commands on a 1 MiB flash; the stock packages are stand-ins with
V15's and V16's sizes and two different heads. Run from the repo root:
  python3 tests/rescue_test.py
Also checks M-VAVE's real V15 and V16 in ~/fm1-rescue (where tools/fm1_rescue.sh keeps them;
skipped otherwise), and its SMK-37 Elite_016.fwsc when $SMK_ELITE_FWSC names it (run python3 -I then).
The SMK-37 Elite's stand-ins share V15's head, as M-VAVE's real packages do."""
import hashlib
import os
import re
import struct
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import fm1_rescue as R  # noqa: E402
import fm1pkg_make as P  # noqa: E402

failed = 0


def ok(cond, what):
    global failed
    print(f"{what:<64} {'ok' if cond else 'FAIL'}")
    failed += not cond


# ---------------------------------------------------------- simulated FM-1 ---

class FakeBot:
    """The loader's side of the bulk-only transport: one command per xfer, on a flash bytearray."""

    def __init__(self, flash, flash_id=0x856014):
        self.flash, self.erased, self.programmed, self.ran = bytearray(flash), [], [], False
        self.flash_id = flash_id

    def xfer(self, cdb, data_out=None, n_in=0, timeout=5000):
        cdb = bytes(cdb)
        if cdb[0] == 0x12:                                   # inquiry
            return b"\0" * 8 + b"WL82    " + b"UBOOT1.00".ljust(16) + b"\0" * 4
        cmd = struct.unpack_from(">H", cdb)[0]
        if cmd in (R.CMD_WRITE_MEMORY, R.CMD_WRITE_FLASH):
            addr, n = struct.unpack_from(">IH", cdb, 2)
            assert n == len(data_out) and struct.unpack_from("<H", cdb, 9)[0] == R.crc16(data_out)
            if cmd == R.CMD_WRITE_FLASH:
                assert all(b == 0xFF for b in self.flash[addr:addr + n]), "programmed without an erase"
                self.flash[addr:addr + n] = data_out
                self.programmed.append(addr)
            return b""
        if cmd == R.CMD_READ_FLASH:
            addr, n = struct.unpack_from(">IH", cdb, 2)
            return bytes(self.flash[addr:addr + n])
        reply = bytearray(16)
        struct.pack_into(">H", reply, 0, cmd)
        if cmd == R.CMD_READ_KEY:
            k = bytearray([0x0F, 0x98])                      # key 980F, ciphered as the chip sends it
            R.crc_cipher(k)
            reply[2 + 5], reply[2 + 4] = k[0], k[1]
        elif cmd == R.CMD_GET_ONLINE_DEVICE:
            reply[2] = 3
            struct.pack_into("<I", reply, 4, self.flash_id)
        elif cmd == R.CMD_ERASE_SECTOR:
            addr = struct.unpack_from(">I", cdb, 2)[0]
            self.flash[addr:addr + R.SECTOR] = b"\xFF" * R.SECTOR
            self.erased.append(addr)
        elif cmd == R.CMD_RUN_APP:
            self.ran = True
        return bytes(reply[:n_in])


def image(seed, size):
    """a stand-in package image: a head and an app of their own"""
    out = bytearray()
    while len(out) < size:
        out += hashlib.sha256(f"{seed}{len(out)}".encode()).digest()
    return bytes(out[:size])


def flash_with(head_of, app_seed, tail_seed):
    """an FM-1: a stock head, some other firmware in the app area, its own data above"""
    f = bytearray(image(tail_seed, R.FLASH_SIZE))
    f[0:R.WRITE_MIN] = head_of[0:R.WRITE_MIN]
    f[R.WRITE_MIN:0x93000] = image(app_seed, 0x93000)[R.WRITE_MIN:]
    return f


def run(flash, stock, write=True, confirm=None, flash_id=0x856014, board="fm1"):
    bot = FakeBot(flash, flash_id)
    said = []
    with tempfile.TemporaryDirectory() as d:
        try:
            r = R.rescue(R.Uboot(bot), stock, b"loader", write, d,
                         say=lambda *a, **k: said.append(" ".join(map(str, a))), confirm=confirm, board=board)
        except R.Fail as e:
            r = e
        backups = list(Path(d).glob("*-backup-*.bin"))
        backup = backups[0].read_bytes() if len(backups) == 1 else None
        if backup is not None:
            said.append("backup file " + backups[0].name)
    return r, bot, backup, "\n".join(said)


V15 = image("V15", 0x93000)
V16 = image("V16", 0xA9000)
STOCK = [("V16", V16), ("V15", V15)]


def writes():
    before = flash_with(V16, "AISLING", "data")
    r, bot, backup, said = run(before, STOCK)
    ok(r is True and bot.ran, "V16's head: written, restarted")
    ok(backup == bytes(before), "the backup is the flash as found")
    ok(bot.flash == V16 + before[0xA9000:], "flash = V16's app over the FM-1's own head and data")
    ok(min(bot.erased) >= R.WRITE_MIN and max(bot.erased) < 0xA9000, "only [0x4000, 0xA9000) erased")
    ok(bot.erased == sorted(bot.erased, reverse=True), "sectors written high to low")
    ok("head is V16's" in said and "holds V16" in said, "says which stock it found and wrote")

    before = flash_with(V15, "AISLING", "data")
    r, bot, _, said = run(before, STOCK)
    ok(r is True and bot.flash == V15 + before[0x93000:], "V15's head: V15 written, even with V16 given")
    ok(max(bot.erased) < 0x93000, "  nothing at or above 0x93000 touched")

    same = bytearray(V16 + before[0xA9000:])
    same[0x50000:0x51000] = b"\0" * R.SECTOR
    r, bot, _, _ = run(same, STOCK)
    ok(r is True and bot.erased == [0x50000], "only sectors that differ are written")


def refusals():
    before = flash_with(V16, "AISLING", "data")
    r, bot, backup, said = run(before, STOCK, write=False)
    ok(r is False and not bot.erased and backup == bytes(before), "check only: backed up, nothing written")
    r, bot, _, _ = run(before, STOCK, write=False, confirm=lambda: False)
    ok(r is False and not bot.erased, "answer other than yes: nothing written")

    r, bot, backup, _ = run(before, [("V15", V15)])
    ok(isinstance(r, R.Fail) and "not writing" in str(r) and not bot.erased,
       "V16's head, only V15 given: refused")
    ok("V15 differs in sectors 0x0000 0x1000 0x2000 0x3000" in str(r), "  names the sectors")
    ok(backup == bytes(before), "  backup still saved")

    odd = flash_with(V16, "AISLING", "data")
    odd[0x3F00] ^= 1
    r, bot, _, _ = run(odd, STOCK)
    ok(isinstance(r, R.Fail) and "matches no stock package" in str(r) and not bot.erased,
       "a head matching neither: refused")

    dev = R.Uboot(FakeBot(bytes(R.FLASH_SIZE)))
    for addr, end in ((0x3000, 0xA9000), (0x93000, 0x93000), (0xA9000, 0xA9000)):
        try:
            dev.write_sector(addr, b"\0" * R.SECTOR, end)
            ok(False, f"write_sector refuses {addr:#x} (app ends {end:#x})")
        except R.Fail:
            ok(True, f"write_sector refuses {addr:#x} (app ends {end:#x})")


def flash_chips():
    before = flash_with(V16, "AISLING", "data")
    r, bot, backup, said = run(before, STOCK, flash_id=0xB36014)
    ok(r is True and bot.ran and bot.flash == V16 + before[0xA9000:], "the newer flash (B36014): written, restarted")
    ok(backup == bytes(before) and "id B36014" in said, "  backed up first, and says which it found")

    r, bot, backup, _ = run(before, STOCK, flash_id=0xEF4014)
    ok(isinstance(r, R.Fail) and "nothing written" in str(r) and not bot.erased and backup is None,
       "any other flash: refused before reading or writing")

    # the firmware's check and this tool's must not drift apart (0x856014 is search-and-replace bait)
    hal = (ROOT / "firmware/hal/fm1_flash.h").read_text()
    ids = {int(x, 16) for x in re.findall(r"0x([0-9A-F]{6})u", re.search(r"#define FL_JEDEC_OK\(id\) (.+)", hal)[1])}
    ok(ids == set(R.FLASH_IDS), "the firmware's FL_JEDEC_OK accepts the same flash IDs")
    app = (ROOT / "firmware/src/app/main_fm1.c").read_text()
    ok("FL_JEDEC_OK(" in app and "856014" not in app, "the app checks through FL_JEDEC_OK")
    loader = [p.name for p in (ROOT / "firmware/loader").iterdir() if p.is_file()
              and "856014" in p.read_text(errors="ignore")]
    ok(not loader, "the update loader has no flash ID check of its own")


# ------------------------------------------------------------ SMK-37 Elite ---

SMK_ID = 0xEB6014
E16 = V15[:R.WRITE_MIN] + image("E16", 0x9D000)[R.WRITE_MIN:]    # V15's head, as M-VAVE's Elite_016 has
ELITE = [("SMK-37 Elite 016", E16)]


def smk_flash(head_of=E16, trim=True):
    """an SMK-37 Elite: a stock head, some other firmware in the app area, its calibration and data above"""
    f = bytearray(image("smkdata", R.FLASH_SIZE))
    f[0:R.WRITE_MIN] = head_of[0:R.WRITE_MIN]
    f[R.WRITE_MIN:0x9D000] = image("AISLING-SMK", 0x9D000)[R.WRITE_MIN:]
    if trim:
        f[R.TRIM_AT:R.TRIM_AT + len(R.TRIM_MARK)] = R.TRIM_MARK
    return f


def smk37():
    before = smk_flash()
    r, bot, backup, said = run(before, ELITE, flash_id=SMK_ID, board="smk37")
    ok(r is True and bot.ran and bot.flash == E16 + before[0x9D000:], "SMK-37 Elite: Elite_016's app written, restarted")
    ok(min(bot.erased) >= R.WRITE_MIN and max(bot.erased) < 0x9D000 and bot.programmed and
       min(bot.programmed) >= R.WRITE_MIN and max(bot.programmed) < 0x9D000, "  only [0x4000, 0x9D000) erased and written")
    ok(bot.flash[0xC0000:0xC2000] == before[0xC0000:0xC2000] and bot.flash[0xF4000:] == before[0xF4000:],
       "  calibration, resources and key as they were")
    ok(backup == bytes(before) and "backup file smk37-backup-" in said and "Restarting the SMK-37 Elite" in said,
       "  backed up first (smk37-backup-*.bin), says SMK-37 Elite")

    r, bot, backup, said = run(smk_flash(trim=False), ELITE, flash_id=SMK_ID, board="smk37")
    ok(isinstance(r, R.Refuse) and "lacks an SMK-37's velocity calibration" in str(r) and "trimdat" in str(r) and not bot.erased
       and backup is not None, "no calibration at 0xC1224: refused after the backup, nothing written")

    r, bot, backup, _ = run(smk_flash(), ELITE, flash_id=0x856014, board="smk37")
    ok(isinstance(r, R.Refuse) and "not the SMK-37 Elite's chip and flash" in str(r) and "id EB6014)" in str(r)
       and not bot.erased and backup is None, "an FM-1's flash ID with the Elite's package: refused before reading")
    r, bot, backup, _ = run(flash_with(V15, "AISLING", "data"), ELITE, flash_id=SMK_ID, board="smk37")
    ok(isinstance(r, R.Refuse) and not bot.erased, "an FM-1 flash (no calibration) with the SMK's ID: refused")

    # an SMK whose flash has an FM-1's ID, given FM-1 V15 (the same head): the calibration stops it
    before = smk_flash(head_of=V15)
    r, bot, backup, _ = run(before, STOCK, flash_id=0x856014)
    ok(isinstance(r, R.Refuse) and "holds an SMK-37's velocity calibration" in str(r) and not bot.erased
       and backup == bytes(before), "FM-1 V15 on an SMK-37 (same head, FM-1 flash ID): refused, backed up")

    dev = R.Uboot(FakeBot(bytes(R.FLASH_SIZE)))
    never = R.BOARDS["smk37"]["never"]
    for addr in (0x3000, 0xC0000, 0xC1000, 0xF4000, 0xFF000):
        try:
            dev.write_sector(addr, b"\0" * R.SECTOR, R.FLASH_SIZE, never)
            ok(False, f"write_sector refuses {addr:#x} on the SMK, whatever the app's end")
        except R.Fail:
            ok(True, f"write_sector refuses {addr:#x} on the SMK, whatever the app's end")
    try:
        dev.write_sector(0xC2000, b"\0" * R.SECTOR, R.FLASH_SIZE, never)
        ok(True, "  ... and writes 0xC2000 (not protected) when the app reaches it")
    except R.Fail:
        ok(False, "  ... and writes 0xC2000 (not protected) when the app reaches it")
    r, bot, _, _ = run(smk_flash(), [("long", E16 + image("x", 0x30000))], flash_id=SMK_ID, board="smk37")
    ok(isinstance(r, R.Refuse) and "never written" in str(r) and not bot.erased,
       "a package whose app reaches the calibration: refused before any write")

    # the tool's SMK settings and the firmware's flash map must not drift apart
    m = (ROOT / "firmware/hal/smk37/smk_flash_map.h").read_text()
    mac = {k: int(v, 16) for k, v in re.findall(r"#define (SMK_\w+|FL_APP_HI)\s+0x([0-9A-F]+)u", m)}
    ok(tuple(never) == tuple((mac[f"SMK_{k}_LO"], mac[f"SMK_{k}_HI"]) for k in ("HEAD", "CAL", "TOP")),
       "never-written ranges == smk_flash_map.h's SMK_HEAD, SMK_CAL, SMK_TOP")
    ids = {int(x, 16) for x in re.findall(r"0x([0-9A-F]{6})u", re.search(r"#define FL_JEDEC_OK\(id\) (.+)", m)[1])}
    ok(ids == set(R.BOARDS["smk37"]["flash_ids"]), "flash IDs == smk_flash_map.h's FL_JEDEC_OK")
    ends = {end for _, end, b in R.STOCK.values() if b == "smk37"}
    ok(ends == {mac["FL_APP_HI"]} == {P.BOARDS["smk37"]["flash_size"]}, "Elite_016's app ends at FL_APP_HI (0x9D000)")
    ok("0xC1224" in m and R.TRIM_AT == 0xC1224, "the calibration marker's address is smk_flash_map.h's")


def packages():
    """package_flash_image and load_stock on packages written by tools/fm1pkg_make.py (20 and 36 blocks)"""
    with tempfile.TemporaryDirectory() as d:
        paths, saved = {}, dict(R.STOCK)
        for name, board, end, product in (("fm1", "fm1", 0x93000, "FM-1_015"),
                                          ("smk", "smk37", 0x9D000, "SMK-37 Elite_016")):
            flash = image(name, end)
            raw = P.ufw(flash, image("ota" + name, 0x4E01), product, board)
            ok(R.package_flash_image(raw) == flash,
               f"package_flash_image: {board}, {P.BOARDS[board]['blocks']} marker blocks")
            paths[name] = Path(d) / f"{name}.fwsc"
            paths[name].write_bytes(raw)
            R.STOCK[hashlib.sha256(raw).hexdigest()] = (name, end, board)
        try:
            stock, board = R.load_stock([str(paths["smk"])])
            ok(board == "smk37" and stock[0][0] == "smk", "load_stock: the Elite's package -> board smk37")
            try:
                R.load_stock([str(paths["fm1"]), str(paths["smk"])])
                ok(False, "load_stock: FM-1 and SMK-37 packages together refused")
            except R.Fail as e:
                ok("together" in str(e), "load_stock: FM-1 and SMK-37 packages together refused")
            bad = bytearray(paths["fm1"].read_bytes())
            bad[5] ^= 1                                                  # the header's CRC fails
            try:
                R.package_flash_image(bytes(bad))
                ok(False, "package_flash_image: a damaged header refused")
            except R.Fail:
                ok(True, "package_flash_image: a damaged header refused")
        finally:
            R.STOCK.clear()
            R.STOCK.update(saved)


class FakeUsbDev:
    def __init__(self, bus, ports):
        self.bus, self.port_numbers = bus, ports


def locations():
    hub, direct = FakeUsbDev(1, (1, 2)), FakeUsbDev(0, (1,))
    ok(R.location_of(hub) == 0x01120000 and R.location_of(direct) == 0x00100000,
       "location: bus and hub ports, as ioreg shows them")
    ok(R.location_of(FakeUsbDev(0, None)) is None and R.location_of(FakeUsbDev(None, (1,))) is None and
       R.location_of(FakeUsbDev(0, (16,))) is None, "  no ports, no bus or a port over 15 -> unknown")
    ok(R.pick_device([]) is None and R.pick_device([], 0x01120000) is None, "  nothing there yet -> wait")
    ok(R.pick_device([hub]) is hub, "  one device, no --location -> that one")
    try:
        R.pick_device([hub, direct])
        ok(False, "  two devices, no --location -> refused")
    except R.Refuse as e:
        ok("0x01120000" in str(e) and "0x00100000" in str(e), "  two devices, no --location -> refused, both listed")
    ok(R.pick_device([hub, direct], 0x01120000) is hub and R.pick_device([direct, hub], 0x00100000) is direct,
       "  two devices and --location -> the one plugged in there")
    for devs, what in (([direct], "the only device is somewhere else"),
                       ([FakeUsbDev(0, None)], "the only device has no known location")):
        try:
            R.pick_device(devs, 0x01120000)
            ok(False, f"  --location, {what} -> refused")
        except R.Refuse:
            ok(True, f"  --location, {what} -> refused")


def real_packages():
    pk = {n: Path.home() / "fm1-rescue" / f"FM-1-{n}.fwsc" for n in ("V15", "V16")}
    if not all(p.exists() for p in pk.values()):
        print("  skip real packages (no ~/fm1-rescue/FM-1-V15.fwsc and FM-1-V16.fwsc)")
        return
    images = {}
    for n, p in pk.items():
        raw = p.read_bytes()
        name, end, board = R.STOCK.get(hashlib.sha256(raw).hexdigest(), (None, None, None))
        images[n] = R.package_flash_image(raw)
        ok(name == n and len(images[n]) == end and board == "fm1", f"M-VAVE's {n}: known, flash entry {end:#x}")
    ok(images["V15"][:R.WRITE_MIN] != images["V16"][:R.WRITE_MIN], "V15's and V16's heads differ")
    elite = os.environ.get("SMK_ELITE_FWSC")
    if not elite:
        print("  skip M-VAVE's SMK-37 Elite_016 (set SMK_ELITE_FWSC)")
        return
    raw = Path(elite).read_bytes()
    name, end, board = R.STOCK.get(hashlib.sha256(raw).hexdigest(), (None, None, None))
    im = R.package_flash_image(raw)
    ok(board == "smk37" and len(im) == end == 0x9D000, f"M-VAVE's SMK-37 Elite_016: known, flash entry {end:#x}")
    ok(im[:R.WRITE_MIN] == images["V15"][:R.WRITE_MIN], "  its head is V15's (so the board comes from the package)")
    ok(R.TRIM_MARK not in im, "  the package carries no calibration (the device's own)")


writes()
refusals()
flash_chips()
smk37()
packages()
locations()
real_packages()
print("rescue: " + ("all ok" if not failed else f"{failed} FAILED"))
sys.exit(1 if failed else 0)
