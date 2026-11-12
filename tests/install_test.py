#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""tools/fm1_install.py against a simulated FM-1 (no hardware, no mido).
The fake device is the one in web/test_web.mjs: identity on the handshake,
then "device asks, host answers" reads of the logical image. Run from the repo root:
  python3 tests/install_test.py
Also checks logical_image/product_of against web/fm1pkg.js (needs node; the
built packages, build/amb.fwsc and build/smk37/*.fwsc, when they are there).
Test packages are written by tools/fm1pkg_make.py: the FM-1's with 20 marker
blocks, the SMK-37 Elite's with 36."""
import io
import json
import os
import queue
import shutil
import subprocess
import sys
import tempfile
import threading
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import fm1_install as I  # noqa: E402
import fm1pkg_make as P  # noqa: E402

I.DELAY.update(open=0, start=0.01, reply=0, loader=0.01, reboot=0.01, retry=0.02, poll=0.05, hs=0.1,
               idle_check=0.4, idle_write=0.4, wait_loader=2, wait_reboot=2, uboot=0.05)
failed = 0


def ok(cond, what):
    global failed
    print(f"{what:<64} {'ok' if cond else 'FAIL'}")
    failed += not cond


# ---------------------------------------------------------- simulated FM-1 ---

class FakeLink:
    def __init__(self, dev, gen):
        self.dev, self.gen, self.q, self.closed = dev, gen, queue.Queue(), False

    @property
    def lost(self):
        return self.gen != self.dev.gen or not self.dev.connected

    def send(self, pkt):
        if self.lost:
            return False
        self.dev.sent.append(bytes(pkt))
        self.dev.rx(bytes(pkt))
        return True

    def read(self, timeout):
        try:
            return self.q.get(timeout=max(timeout, 0))
        except queue.Empty:
            return None

    def drain(self):
        while not self.q.empty():
            self.q.get_nowait()

    def close(self):
        self.closed = True


def ident_frame(text, chip):
    """the 0x11 answer with the flash's JEDEC ID in bytes 24..26 (AISLING 1.1's answer to the body 01)"""
    t = text.encode()
    return b"\xF0" + I.pack7(bytes([0, 0x59, 0x11, 0, 0, 0]) + t + bytes(24 - len(t)) + chip.to_bytes(3, "big") +
                             bytes(1)) + b"\xF7"


class FakeFM1:
    """a MIDI backend with one FM-1 on it"""

    def __init__(self, image, identity="FM-1_015", name="FM-1", unplug_after=None, after_write="FM-1_900",
                 stall_after=None, bad_addr=None, loader="ota-FM-1_900", chip=None):
        self.image, self.unplug_after, self.after_write, self.loader = image, unplug_after, after_write, loader
        self.chip = chip                           # JEDEC ID: answers the handshake with the body 01 (else silent)
        self.stall_after, self.bad_addr = stall_after, bad_addr
        self.served = self.bad = self.upgrades = self.uboot_keys = 0
        self.deaf = False                          # a loader that ignores the UBOOT key
        self.sent, self.links, self.gen, self.lock = [], [], 0, threading.Lock()
        self.boot(identity, name)

    def boot(self, identity, name):
        with self.lock:
            self.gen += 1
            self.identity, self.name, self.connected = identity, name, True
            self.waiting, self.queue = None, []

    def input_names(self):
        return [self.name] if self.connected else []

    output_names = input_names

    def open(self, in_name, out_name):
        if in_name != self.name or not self.connected:
            raise IOError("no such port")
        link = FakeLink(self, self.gen)
        self.links.append(link)
        return link

    def tx(self, pkt):
        for link in self.links:
            if link.gen == self.gen and not link.closed:
                link.q.put(bytes(pkt))

    def rx(self, d):
        if d == I.HS_QUERY:
            t = self.identity.encode()
            self.tx(b"\xF0" + I.pack7(bytes([0, 0x59, 0x11, 0, 0, 0]) + t + bytes(28 - len(t))) + b"\xF7")
        elif d == I.HS_WHOLE:
            if self.chip is not None:
                self.tx(ident_frame(self.identity, self.chip))
        elif d == I.UBOOT_KEY:
            self.uboot_keys += 1
            if self.identity.startswith("ota-") and not self.deaf:
                self.connected = False             # the loader detaches: the chip's ROM takes the USB
        elif d == I.UPGRADE:
            self.upgrades += 1
            first = self.bad_addr if self.bad_addr is not None else 0
            self.queue = ([(first + k * 512, 512) for k in range(6)] + [(I.FINISH_WRITE, 8)]
                          if self.identity.startswith("ota-")
                          else [(0, 64), (0x40, 160), (0x1000, 512), (I.FINISH_CHECK, 8)])
            self.next()
        elif self.waiting:
            u = I.unpack7(d[1:-1])
            addr, n = self.waiting
            fin = addr >= I.FINISH_CHECK
            want = b"success\0" if fin else self.image[addr:addr + n]
            if u[14:14 + len(want)] != want or len(u) != 15 + len(want):
                self.bad += 1
            self.waiting = None
            self.served += 1
            if self.unplug_after and self.served >= self.unplug_after:
                self.connected = False
                return
            if self.stall_after and self.served >= self.stall_after:
                return
            if addr == I.FINISH_CHECK:
                threading.Timer(0.05, self.boot, (self.loader, "Felucca Update")).start()
            elif addr == I.FINISH_WRITE:
                threading.Timer(0.05, self.boot, (self.after_write, "Felucca")).start()
            else:
                self.next()

    def next(self):
        if not self.queue:
            return
        self.waiting = addr, n = self.queue.pop(0)
        u = bytearray([0, 0x59, 0x30, 0, 0, 0, 0]) + addr.to_bytes(4, "little") + bytes([n & 0xFF, n >> 8, 0])
        u.append(~sum(u[6:14]) & 0xFF)
        self.tx(b"\xF0" + I.pack7(u) + b"\xF7")


# ----------------------------------------------------------------- helpers ---

def package(product="FM-1_900", marker=True, size=0x2000, board=None):
    """a package as tools/fm1pkg_make.py writes it: the FM-1's 20 marker blocks, an SMK-37's 36"""
    board = board or ("smk37" if product.startswith("SMK-37") else "fm1")
    flash = bytes((i * 7) & 0xFF for i in range(size))
    ota = bytearray((i * 13 + 5) & 0xFF for i in range(0x800))
    ota[0x100:0x110] = I.LOADER_MARK if marker else bytes(16)
    return P.ufw(flash, bytes(ota), product, board)


TMP = Path(tempfile.mkdtemp(prefix="felucca-install-"))


def pkgfile(name, raw):
    p = TMP / name
    p.write_bytes(raw)
    return str(p)


def cli(args, dev, answer=True):
    out, err = io.StringIO(), io.StringIO()
    old, sys.stderr = sys.stderr, err
    try:
        rc = I.main(args, backend=dev, out=out, ask=lambda _p: answer)
    finally:
        sys.stderr = old
    return rc, out.getvalue(), err.getvalue()


# ------------------------------------------------------------------- tests ---

def wire():
    data = bytes(range(256)) * 3
    ok(I.unpack7(I.pack7(data))[:len(data)] == data, "pack7 / unpack7 round trip")
    pkt = I.response(0x12345, b"\x01\x02\x03", fl=5)
    u = I.unpack7(pkt[1:-1])
    ok(u[:3] == b"\x00\x59\x30" and int.from_bytes(u[7:11], "little") == 0x12345 and u[14:17] == b"\x01\x02\x03"
       and ~sum(u[6:17]) & 0xFF == u[17], "response: header, address, data, checksum")
    raw = package("FM-1_906")
    ok(I.marker_blocks(raw) == 20 and I.product_of(raw) == "FM-1_906" and len(I.logical_image(raw)) == len(raw) - 20,
       "product_of / logical_image (synthetic, 20 marker blocks)")
    i = I.parse_identity(ident_frame("FM-1_353010100", 0xB36014))
    ok(i is not None and i.text == "FM-1_353010100" and i.version == 353010100 and i.chip == 0xB36014,
       "parse_identity: the text up to its first zero, the chip ID from bytes 24..26")
    i = I.parse_identity(ident_frame("FM-1_353010000", 0))
    ok(i is not None and i.text == "FM-1_353010000" and i.chip == 0, "parse_identity: no chip ID -> 0")
    raw = package("FM-1_9001000")                  # X0X from 0.10: major, minor, patch, two digits each
    ok(I.product_of(raw) == "FM-1_9001000", "product_of: a 12-character identity")


def installs():
    raw = package()
    image = I.logical_image(raw)
    p = pkgfile("ok.fwsc", raw)

    dev = FakeFM1(image)
    rc, out, err = cli([p, "--yes"], dev)
    ok(rc == 0 and dev.bad == 0 and dev.upgrades == 2 and dev.served == 11 and "done: the FM-1 runs FM-1_900" in out
       and "100%" in out, f"install: running -> loader -> Felucca ({dev.served} reads)")

    raw10 = package("FM-1_9001000")
    dev = FakeFM1(I.logical_image(raw10), identity="FM-1_909", after_write="FM-1_9001000")
    rc, out, err = cli([pkgfile("x10.fwsc", raw10), "--yes"], dev)
    ok(rc == 0 and dev.bad == 0 and "done: the FM-1 runs FM-1_9001000" in out,
       "install: 0.9 (FM-1_909) -> 0.10 (FM-1_9001000), the longer identity verified after")

    dev = FakeFM1(image, identity="ota-FM-1_900", name="Felucca Update")
    rc, out, err = cli([p, "--yes"], dev)
    ok(rc == 0 and dev.bad == 0 and dev.upgrades == 1 and "update mode" in out, "install: device already in update mode -> finishes the write")

    dev = FakeFM1(image)
    rc, out, err = cli([p], dev, answer=False)
    ok(rc == 1 and dev.upgrades == 0 and "cancelled" in out, "install: answer no -> nothing sent but the handshake")

    dev = FakeFM1(image, identity="FM-1_905", name="Felucca")
    rc, out, err = cli(["--info"], dev)
    ok(rc == 0 and "FM-1_905" in out and "running" in out and dev.upgrades == 0 and "flash chip" not in out,
       "--info: identity of the connected FM-1 (no chip ID: none shown)")

    dev = FakeFM1(image, identity="FM-1_353010100", name="FM-1", chip=0xB36014)
    rc, out, err = cli(["--info"], dev)
    ok(rc == 0 and "FM-1_353010100" in out and "flash chip: JEDEC ID B36014" in out and dev.upgrades == 0,
       "--info: the flash chip, from the answer to the body 01")

    dev = FakeFM1(image, identity="ota-FM-1_900", name="Felucca Update")
    rc, out, err = cli(["--info"], dev)
    ok(rc == 0 and "update loader" in out, "--info: device in update mode")


def errors():
    raw = package()
    image = I.logical_image(raw)
    p = pkgfile("ok.fwsc", raw)

    rc, out, err = cli([p, "--yes"], FakeFM1(image, name="IAC Driver Bus 1"))
    ok(rc == 3 and "not found" in err and "IAC Driver" in err, "no FM-1 port -> exit 3, lists the MIDI inputs")
    dev = FakeFM1(image)
    rc, out, err = cli([p, "--yes", "--port", "Felucca"], dev)
    ok(rc == 3 and dev.upgrades == 0, "--port that matches nothing -> exit 3")
    dev = FakeFM1(image, name="My Interface")
    rc, out, err = cli(["--info", "--port", "my int"], dev)
    ok(rc == 0 and "FM-1_015" in out, "--port picks a port the default match skips")

    t0 = time.monotonic()
    rc, out, err = cli([p, "--yes"], FakeFM1(image, unplug_after=2))
    ok(rc == 4 and "disconnected" in err and "nothing was written" in err and time.monotonic() - t0 < 3,
       "unplugged in step 1 -> exit 4 'lost', nothing written")
    t0 = time.monotonic()
    rc, out, err = cli([p, "--yes"], FakeFM1(image, identity="ota-FM-1_900", name="Felucca Update", unplug_after=3))
    ok(rc == 4 and "run the install again" in err and time.monotonic() - t0 < 3, "unplugged during the write -> exit 4 at once")
    rc, out, err = cli([p, "--yes"], FakeFM1(image, identity="ota-FM-1_900", name="Felucca Update", stall_after=3))
    ok(rc == 4 and "stopped answering" in err, "loader stops answering -> exit 4 after the idle time")
    rc, out, err = cli([p, "--yes"], FakeFM1(image, identity="ota-FM-1_900", name="Felucca Update", bad_addr=len(image)))
    ok(rc == 4 and "outside the package" in err, "read past the package -> exit 4")

    rc, out, err = cli([p, "--yes"], FakeFM1(image, after_write="FM-1_015"))
    ok(rc == 6 and "reports FM-1_015" in err, "another identity after the restart -> exit 6")
    dev = FakeFM1(image, identity="XY-9_001", name="usb-midi")
    rc, out, err = cli([p, "--yes"], dev)
    ok(rc == 6 and dev.upgrades == 0, "another model -> exit 6, nothing sent")

    class Stuck(FakeFM1):          # the loader never shows up
        def boot(self, identity, name):
            super().boot(identity, name)
            if identity.startswith("ota-"):
                self.connected = False
    rc, out, err = cli([p, "--yes"], Stuck(image))
    ok(rc == 5 and "loader did not appear" in err, "no loader after step 1 -> exit 5")

    plain = pkgfile("plain.fwsc", package(marker=False))
    dev = FakeFM1(image)
    rc, out, err = cli([plain, "--yes"], dev)
    ok(rc == 2 and "no Felucca update loader" in err and dev.sent == [], "package without the loader marker -> exit 2, no MIDI")
    dev = FakeFM1(I.logical_image(package(marker=False)))
    rc, out, err = cli([plain, "--yes", "--force"], dev)
    ok(rc == 0 and dev.bad == 0, "... installs with --force")
    rc, out, err = cli([pkgfile("short.fwsc", b"\0" * 100), "--yes"], FakeFM1(image))
    ok(rc == 2 and "too short" in err, "not a package -> exit 2")
    rc, out, err = cli([str(TMP / "missing.fwsc"), "--yes"], FakeFM1(image))
    ok(rc == 2, "missing file -> exit 2")


def smk37():
    """the SMK-37 Elite: its packages have 36 marker blocks, and no package goes to another model"""
    raw = package("SMK-37 Elite_900")
    image = I.logical_image(raw)
    ok(I.marker_blocks(raw) == 36 and I.product_of(raw) == "SMK-37 Elite_900" and len(image) == len(raw) - 36,
       "SMK-37 Elite package: 36 marker blocks, identity and logical image")
    short = package("FM-1_900", board="smk37")
    ok(I.marker_blocks(short) == 36 and I.product_of(short) == "FM-1_900",
       "  the count comes from the header, not the identity's length")
    p = pkgfile("smk.fwsc", raw)
    fm1 = pkgfile("fm1.fwsc", package())
    pro = pkgfile("pro.fwsc", package("SMK-37 Pro_016"))

    dev = FakeFM1(image, identity="SMK-37 Elite_015", name="SMK-37 Elite", loader="ota-SMK-37 Elite_900",
                  after_write="SMK-37 Elite_900")
    rc, out, err = cli([p, "--yes"], dev)
    ok(rc == 0 and dev.bad == 0 and dev.upgrades == 2 and "done: the FM-1 runs SMK-37 Elite_900" in out,
       "install on an SMK-37 Elite: running -> loader -> AISLING")

    dev = FakeFM1(image)
    rc, out, err = cli([p, "--yes"], dev)
    ok(rc == 6 and dev.upgrades == 0 and "the package is for SMK-37 Elite_900" in err,
       "SMK-37 Elite package on an FM-1 -> exit 6, nothing sent")
    for path, what in ((fm1, "an FM-1"), (pro, "an SMK-37 Pro")):
        dev = FakeFM1(image, identity="SMK-37 Elite_015", name="SMK-37 Elite")
        rc, out, err = cli([path, "--yes"], dev)
        ok(rc == 6 and dev.upgrades == 0, f"{what} package on an SMK-37 Elite -> exit 6, nothing sent")
    dev = FakeFM1(image, identity="ota-SMK-37 Elite_900", name="Felucca Update")
    rc, out, err = cli([fm1, "--yes"], dev)
    ok(rc == 6 and dev.upgrades == 0, "an FM-1 package on an SMK-37 Elite's loader (update mode) -> exit 6")
    dev = FakeFM1(image, identity="ota-SMK-37 Elite_900", name="Felucca Update", after_write="SMK-37 Elite_900")
    rc, out, err = cli([p, "--yes"], dev)
    ok(rc == 0 and dev.bad == 0 and dev.upgrades == 1, "  ... and its own package finishes the write")

    # --uboot: a backup package's loader, then the UBOOT key; never a write request
    dry = pkgfile("smk-dryloader.fwsc", raw)
    dev = FakeFM1(image, identity="SMK-37 Elite_016", name="SMK-37 Elite", loader="ota-SMK-37 Elite_900")
    rc, out, err = cli([dry, "--uboot", "--yes"], dev)
    ok(rc == 0 and dev.bad == 0 and dev.upgrades == 1 and dev.uboot_keys == 1 and dev.served == 4 and
       "Nothing was written" in out,
       "--uboot on a running SMK-37 Elite: step 1, then the key to the loader; no write request")
    class Beside(FakeFM1):         # a second board on USB the whole time (an FM-1 in use)
        probes = 0

        def input_names(self):
            return super().input_names() + ["DUB-1 FM-1"]

        output_names = input_names

        def open(self, in_name, out_name):
            if in_name == "DUB-1 FM-1":
                Beside.probes += 1
                raise IOError("busy")
            return super().open(in_name, out_name)
    dev = Beside(image, identity="SMK-37 Elite_016", name="SMK-37 Elite", loader="ota-SMK-37 Elite_900")
    rc, out, err = cli([dry, "--uboot", "--yes", "--port", "SMK-37 Elite"], dev)
    ok(rc == 0 and dev.uboot_keys == 1 and Beside.probes == 0,
       "  with an FM-1 beside it and --port: the FM-1's port is never opened")
    dev = FakeFM1(image, identity="SMK-37 Elite_016", name="SMK-37 Elite", loader="ota-SMK-37 Elite_900")
    rc, out, err = cli([p, "--uboot", "--yes"], dev)
    ok(rc == 2 and dev.upgrades == 0 and dev.uboot_keys == 0 and "not a backup package" in err,
       "  a package that is not NAME-dryloader.fwsc -> exit 2, nothing sent")
    dev = FakeFM1(image, identity="ota-SMK-37 Elite_900", name="Felucca Update")
    rc, out, err = cli([p, "--uboot", "--yes"], dev)
    ok(rc == 0 and dev.upgrades == 0 and dev.uboot_keys == 1, "  a device already in update mode: the key only")
    dev = FakeFM1(image, identity="SMK-37 Elite_016", name="SMK-37 Elite", loader="ota-SMK-37 Elite_900")
    rc, out, err = cli([dry, "--uboot"], dev, answer=False)
    ok(rc == 1 and dev.upgrades == 0 and dev.uboot_keys == 0, "  answered no -> nothing sent")
    dev = FakeFM1(image)
    rc, out, err = cli([dry, "--uboot", "--yes"], dev)
    ok(rc == 6 and dev.upgrades == 0 and dev.uboot_keys == 0, "  on an FM-1 -> exit 6, nothing sent")
    dev = FakeFM1(image, identity="ota-SMK-37 Elite_900", name="Felucca Update")
    dev.deaf = True
    rc, out, err = cli([dry, "--uboot", "--yes"], dev)
    ok(rc == 4 and "did not take the UBOOT key" in err, "  a loader that stays after the key -> exit 4")

    bad = bytearray(raw)
    bad[5] ^= 1                                    # in the header: its CRC no longer matches
    rc, out, err = cli([pkgfile("bad.fwsc", bytes(bad)), "--yes"], FakeFM1(image))
    ok(rc == 2 and "no valid header" in err, "a damaged package header -> exit 2")
    rc, out, err = cli([pkgfile("long.fwsc", raw + bytes(100)), "--yes"], FakeFM1(image))
    ok(rc == 2 and "no valid header" in err, "a package with bytes added -> exit 2 (the count is out of range)")

    elite = os.environ.get("SMK_ELITE_FWSC")
    if elite:
        e = Path(elite).read_bytes()
        ok(I.marker_blocks(e) == 20 and I.product_of(e) == "SMK-37 Elite_016",
           "M-VAVE's SMK-37 Elite_016: 20 marker blocks, identity SMK-37 Elite_016")


def against_js():
    if not shutil.which("node"):
        print("logical image vs fm1pkg.js: skipped (needs node)")
        return
    js = ("import { logicalImage, productOf, markerBlocks } from %r; import { readFileSync } from 'node:fs';"
          "const p = readFileSync(process.argv[1]);"
          "try { const n = markerBlocks(p); process.stderr.write(n + ' ' + productOf(p)); "
          "process.stdout.write(logicalImage(p)); } catch (e) { process.stderr.write('E ' + e.message); }") \
        % str(TMP / "fm1pkg.mjs")
    # (an .mjs copy: Node before 22 reads a plain .js with no package.json as CommonJS, and its named exports fail)
    shutil.copyfile(ROOT / "web/fm1pkg.js", TMP / "fm1pkg.mjs")

    def js_read(path):
        r = subprocess.run(["node", "--input-type=module", "-e", js, str(path)], capture_output=True, check=True)
        return r.stdout, r.stderr.decode()

    built = [ROOT / "build/amb.fwsc"] + sorted((ROOT / "build/smk37").glob("*.fwsc"))
    made = {"fm1.fwsc": package("FM-1_906"), "smk.fwsc": package("SMK-37 Elite_901"),
            "short.fwsc": package("FM-1_900", board="smk37")}
    for name, raw in made.items():
        built.append(Path(pkgfile(name, raw)))
    for path in built:
        if not path.exists():
            print(f"logical image vs fm1pkg.js: {path.relative_to(ROOT)} skipped (not built)")
            continue
        raw = path.read_bytes()
        out, err = js_read(path)
        n, product = I.marker_blocks(raw), I.product_of(raw)
        ok(I.logical_image(raw) == out and err == f"{n} {product}",
           f"logical_image / product_of == fm1pkg.js ({path.name}, {product}, {n} blocks)")
        if path.parent != TMP:
            ok(I.LOADER_MARK in raw, f"  {path.name} carries the Felucca loader marker")
    bad = bytearray(made["smk.fwsc"])
    bad[5] ^= 1
    _, err = js_read(pkgfile("bad.fwsc", bytes(bad)))
    ok(err == "E not an FM-1 package (no valid header)", "fm1pkg.js refuses a damaged header too")
    _, err = js_read(pkgfile("short2.fwsc", b"\0" * 100))
    ok(err == "E not an FM-1 package (too short)", "  ... and a file that is too short")


def identity_js():
    """web/fm1ota.js reads the identity as the installer does: up to the first zero, the chip from bytes 24..26"""
    if not shutil.which("node"):
        print("parseIdentity in fm1ota.js: skipped (needs node)")
        return
    shutil.copyfile(ROOT / "web/fm1ota.js", TMP / "fm1ota.mjs")
    js = ("import { parseIdentity } from %r;"
          "const r = process.argv.slice(1).map((h) => parseIdentity(Array.from(Buffer.from(h, 'hex'))));"
          "process.stdout.write(JSON.stringify(r));") % str(TMP / "fm1ota.mjs")
    plain = b"\xF0" + I.pack7(bytes([0, 0x59, 0x11, 0, 0, 0]) + b"FM-1_353010000" + bytes(14)) + b"\xF7"
    frames = [ident_frame("FM-1_353010100", 0xB36014), plain]
    r = subprocess.run(["node", "--input-type=module", "-e", js] + [f.hex() for f in frames],
                       capture_output=True, check=True)
    a, b = json.loads(r.stdout)
    ok(a == {"text": "FM-1_353010100", "model": "FM-1", "version": 353010100, "chip": 0xB36014} and
       b == {"text": "FM-1_353010000", "model": "FM-1", "version": 353010000, "chip": 0},
       "fm1ota.js parseIdentity: the text up to its first zero, the chip ID")


def chip_js():
    """web/fm1ota.js asks for the chip as the installer does: the handshake with the body 01, the chip from the answer"""
    if not shutil.which("node"):
        print("flashChip in fm1ota.js: skipped (needs node)")
        return
    shutil.copyfile(ROOT / "web/fm1ota.js", TMP / "fm1ota.mjs")
    js = ("import { flashChip } from %r;"
          "const ask = async (h) => { const sent = [], q = h === '-' ? [] : [Array.from(Buffer.from(h, 'hex'))];"
          "  const link = { drain() {}, send(b) { sent.push(Buffer.from(b).toString('hex')); return true; },"
          "                 async read(ms) { return q.shift() || null; } };"
          "  return { chip: await flashChip(link), sent }; };"
          "const out = []; for (const h of process.argv.slice(1)) out.push(await ask(h));"
          "process.stdout.write(JSON.stringify(out));") % str(TMP / "fm1ota.mjs")
    r = subprocess.run(["node", "--input-type=module", "-e", js, ident_frame("FM-1_353010100", 0xB36014).hex(), "-"],
                       capture_output=True, check=True)
    a, b = json.loads(r.stdout)
    hs = I.HS_WHOLE.hex()
    ok(a == {"chip": 0xB36014, "sent": [hs]} and b == {"chip": 0, "sent": [hs]},
       "fm1ota.js flashChip: the same handshake as fm1_install.py, the chip from the answer, 0 with no answer")


wire()
installs()
errors()
smk37()
against_js()
identity_js()
chip_js()
shutil.rmtree(TMP, ignore_errors=True)
print(f"INSTALL TESTS FAILED ({failed})" if failed else "install tests passed")
sys.exit(1 if failed else 0)
