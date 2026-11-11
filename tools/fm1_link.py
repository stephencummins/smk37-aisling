#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Talk to a real FM-1 running AISLING 0.5 or later, over its USB link.

The same wire as web/editor/link.js and firmware/src/app/link.c: frames are
F0 7D 41 4C cmd seq args F7, one request out at a time, resent until it is
answered. The reader's SAY lines (F0 7D 53 52, announce.c) tell us what the
screen said.

  fm1_link.py state                 HELLO, then one state frame
  fm1_link.py play | stop           start or stop the music (and say what it's doing)
  fm1_link.py backup DIR            every piece, the project and the saved bytes
  fm1_link.py verify DIR            the saved bytes against that backup
  fm1_link.py restore DIR           the saved bytes back, at rest, then verify
  fm1_link.py test DIR              the firmware's own paths, on the hardware
  fm1_link.py soak [--minutes N]    CPU and dropouts, per piece

`test` drives the firmware with virtual buttons: SEL in and out, hold SAVE,
shake then undo, a write read back. It needs a backup in DIR made first, and
puts the saved bytes back when it ends, however it ends.

`test` and `soak` refuse to run unless master reads 0 (a -quiet build), or with
--audible. Needs mido with python-rtmidi. Exit codes: 0 done, 1 a check failed,
2 bad arguments or backup, 3 no FM-1, or no link on it, 4 master isn't 0.
"""
import argparse
import os
import queue
import re
import struct
import sys
import threading
import time
from pathlib import Path

H = (0x7D, 0x41, 0x4C)
SR = (0x7D, 0x53, 0x52)
PORT_RE = re.compile(r"FM-1|DUB-1", re.I)     # by name: nothing goes to any other device

HELLO, BYE, BEAT, SET, VIEW, PIECE, PLAY, HOLD, PERF, FORGET, KEYS, BUTTONS = range(1, 13)
UNDO, REDO, SAVE, VOICE, COPY, SWAP, RESET, OPEN, READ, BEGIN, WRITE, COMMIT = range(13, 25)
R_HELLO, R_ACK, R_STATE, R_VALS, R_CHUNK = 65, 66, 67, 68, 69
ST_OK, ST_BAD, ST_REFUSED, ST_SUM, ST_BUSY, ST_NONE = range(6)
ST_NAME = ["ok", "bad", "refused", "sum", "busy", "none"]
GAVE_UP = -1
SAY = 16

A_PROJ, A_SLOT, A_FLASH = 0, 1, 9             # area 1 + piece: a piece
VER, CHUNK = 1, 128
RESEND, TRIES = 0.2, 25
NPIECES, NPARS, PROJ_FORMAT, PROJ_LEN, SLOT_LEN = 8, 133, 3, 3112, 352
XRUNS_FULL = 16383
PAR_AT = 8                                    # the project's own settings: where they start

B = dict(FX=0, SEL=1, ENV=2, LFO=3, EDIT=4, GLO=5, HOME=6, SAVE=7, ARP=8, SEQ=9, PLAY=10, REC=11, OCTDN=12, OCTUP=13)
V_DRONE, V_PERFORM, V_SPACE = 1, 17, 5
P_ECHO = 8                                    # a knob on a page, with room to move: Space's Echo
WHITE_K = [0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26]
PIECE_NAME = ["Skellig", "Burren", "Aran", "Carolan", "Blasket", "Samhain", "Connemara", "Tir na nOg"]


def put14(v):
    return [v & 0x7F, v >> 7 & 0x7F]


def get14u(a, i):
    return a[i] | a[i + 1] << 7


def get14s(a, i):
    v = get14u(a, i)
    return v - 16384 if v >= 8192 else v


def lk_sum(p):
    h = 2166136261
    for b in p:
        h = ((h ^ b) * 16777619) & 0xFFFFFFFF
    return (h ^ h >> 21) & 0x1FFFFF


def pack(s):
    o = []
    for i in range(0, len(s), 7):
        part = s[i:i + 7]
        hi = 0
        for j, v in enumerate(part):
            hi |= (v >> 7) << j
        o.append(hi)
        o.extend(v & 0x7F for v in part)
    return o


def unpack(s):
    o = bytearray()
    i = 0
    while i < len(s):
        hi = s[i]
        i += 1
        for j in range(7):
            if i >= len(s):
                break
            o.append(s[i] | (hi >> j & 1) << 7)
            i += 1
    return bytes(o)


def parse_hello(a):
    n = a[9]
    return dict(ver=a[0], format=a[1], proj=get14u(a, 2), slot=get14u(a, 4), npars=get14u(a, 6), npieces=a[8],
                version=bytes(a[10:10 + n]).decode("ascii", "replace"))


def parse_state(a):
    f = a[1]
    return dict(view=a[0], playing=bool(f & 1), frozen=bool(f & 2), dirty=bool(f & 4), perf=bool(f & 8),
                saving=bool(f & 16), piece=a[2], moves=get14u(a, 3), act=list(a[5:10]), muted=a[10], solo=a[11],
                sounds=a[12], un=a[13], rn=a[14], cpu=a[15], peak=a[16], xruns=get14u(a, 17), master=get14u(a, 19),
                raw=get14u(a, 21), lit=get14u(a, 23), keys=get14u(a, 25) | get14u(a, 27) << 14, gen=a[29],
                fx=get14u(a, 30) | get14u(a, 32) << 14 if len(a) >= 34 else 0)   # (1.3: the effects sounding)


def pars_of(project):
    return struct.unpack_from("<%dh" % NPARS, project, PAR_AT)


class Link:
    """One FM-1's MIDI ports, and who said what on them."""

    def __init__(self, port=None):
        import mido
        self.mido = mido
        ins = list(dict.fromkeys(mido.get_input_names()))
        outs = list(dict.fromkeys(mido.get_output_names()))
        match = lambda n: re.search(port, n, re.I) if port else PORT_RE.search(n)
        i = [n for n in ins if match(n)]
        o = [n for n in outs if match(n)]
        if not i or not o:
            raise NoFM1("no FM-1 on USB (inputs: %s)" % (ins,))
        self.q = queue.Queue()
        self.lock = threading.Lock()
        self.hello = None
        self.state = None
        self.nstates = 0
        self.vals = {}
        self.said = []                         # (time, text)
        self.link_frames = self.reader_frames = 0
        self.notes_on = self.notes_off = 0
        self.seq = 0
        self.beat_link = self.beat_reader = False
        self.stop = False
        self.inp = mido.open_input(i[0], callback=self._rx)
        self.out = mido.open_output(o[0])
        time.sleep(0.3)                        # CoreMIDI may drop a message sent just after opening
        threading.Thread(target=self._beats, daemon=True).start()

    # ---------- the wire ----------
    def _rx(self, m):
        if m.type == "note_on" and m.velocity:
            self.notes_on += 1
        elif m.type == "note_off" or m.type == "note_on":
            self.notes_off += 1
        if m.type != "sysex":
            return
        d = m.data
        if tuple(d[:3]) == SR:
            self.reader_frames += 1
            if len(d) > 4 and d[3] == SAY:
                self.said.append((time.monotonic(), bytes(d[5:]).decode("ascii", "replace")))
            return
        if tuple(d[:3]) != H or len(d) < 5:
            return
        self.link_frames += 1
        cmd, seq, a = d[3], d[4], d[5:]
        if cmd == R_HELLO:
            self.hello = parse_hello(a)
        elif cmd == R_STATE:
            self.state = parse_state(a)
            self.nstates += 1
        elif cmd == R_VALS:
            at = 0
            while at + 5 <= len(a):
                k, v, n = get14u(a, at), get14s(a, at + 2), a[at + 4]
                at += 5
                self.vals[k] = (v, bytes(a[at:at + n]).decode("ascii", "replace"))
                at += n
        elif cmd in (R_ACK, R_CHUNK):
            self.q.put((cmd, seq, a))

    def _send(self, *data):
        with self.lock:
            self.out.send(self.mido.Message("sysex", data=list(data)))

    def _beats(self):                          # a frame arriving while the last is handled is dropped: requests are resent
        t_link = t_reader = -1e9               # (due at once: this clock can start near 0)
        while not self.stop:
            t = time.monotonic()
            if self.beat_link and t - t_link >= 1.0:
                t_link = t
                self._send(*H, BEAT)
            elif self.beat_reader and t - t_reader >= 2.0:     # (a pass later, never right behind the beat: it would be lost)
                t_reader = t
                self._send(*SR, HELLO, VER)
            time.sleep(0.05)

    def close(self):
        self.stop = True
        for p in (self.inp, self.out):
            try:
                p.close()
            except Exception:
                pass

    # ---------- opening ----------
    def open(self, ms=3000, reader=True):
        self.hello = None
        end = time.monotonic() + ms / 1000
        while self.hello is None and time.monotonic() < end:
            self._send(*H, HELLO, VER)
            wait = time.monotonic() + 0.5
            while self.hello is None and time.monotonic() < wait:
                time.sleep(0.01)
        if self.hello is None:
            return None
        self.beat_link = True
        if reader:
            n = self.reader_frames
            self.beat_reader = True
            until(lambda: self.reader_frames > n, 3.0)     # (lines said before it listens are lost)
        until(lambda: self.state is not None, 2.0)
        return self.hello

    def bye(self):
        self.beat_link = self.beat_reader = False
        self._send(*H, BYE)
        time.sleep(0.06)
        self._send(*H, BYE)                    # (twice: one right behind another frame can be dropped)
        self._send(*SR, BYE)
        time.sleep(0.06)
        self._send(*SR, BYE)

    # ---------- requests ----------
    def req(self, cmd, args=(), tries=TRIES):
        """One request, resent with its seq until answered. (status, extra); the status is GAVE_UP if never."""
        self.seq = self.seq % 127 + 1
        seq = self.seq
        while not self.q.empty():
            self.q.get_nowait()
        off = get14u(args, 0) if cmd == READ else None
        for _ in range(tries):
            self._send(*H, cmd, seq, *args)
            end = time.monotonic() + RESEND
            while (left := end - time.monotonic()) > 0:
                try:
                    r, s, a = self.q.get(timeout=left)
                except queue.Empty:
                    break
                if s != seq:
                    continue
                if r == R_ACK:
                    return a[0], bytes(a[1:])
                if r == R_CHUNK and cmd == READ and get14u(a, 0) == off:
                    return ST_OK, unpack(a[2:])
        return GAVE_UP, b""

    def read_area(self, area):
        st, x = self.req(OPEN, [area])
        if st != ST_OK or len(x) != 5:
            return None
        n, want = get14u(x, 0), x[2] | x[3] << 7 | x[4] << 14
        b = bytearray()
        for off in range(0, n, CHUNK):
            st, c = self.req(READ, put14(off))
            if st != ST_OK:
                return None
            b += c
        b = bytes(b[:n])
        return b if lk_sum(b) == want and len(b) == n else None

    def write_area(self, area, data, badsum=False, rest_wait=0):
        """0 taken; else why not. rest_wait: seconds to keep asking while it answers busy."""
        st, _ = self.req(BEGIN, [area])
        if st != ST_OK:
            return st
        for off in range(0, len(data), CHUNK):
            st, _ = self.req(WRITE, put14(off) + pack(data[off:off + CHUNK]))
            if st != ST_OK:
                return st
        s = lk_sum(data) ^ (1 if badsum else 0)
        args = [s & 0x7F, s >> 7 & 0x7F, s >> 14 & 0x7F]
        end = time.monotonic() + rest_wait
        while True:
            st, _ = self.req(COMMIT, args)     # (a new seq each time: a repeated one replays the old answer)
            if st != ST_BUSY or time.monotonic() >= end:
                return st
            time.sleep(0.5)

    # ---------- the panel ----------
    def buttons(self, *names):
        m = 0
        for n in names:
            m |= 1 << B[n]
        return self.req(BUTTONS, put14(m))[0]

    def tap(self, name, ms=90):
        self.buttons(name)
        time.sleep(ms / 1000)
        self.buttons()
        time.sleep(0.05)

    def hold(self, name, ms):
        self.buttons(name)
        time.sleep(ms / 1000)
        self.buttons()
        time.sleep(0.05)

    def undo_gesture(self, redo=False):        # hold HOME, tap OCT- (undo) or OCT+ (redo)
        other = "OCTUP" if redo else "OCTDN"
        self.buttons("HOME")
        time.sleep(0.08)
        self.buttons("HOME", other)
        time.sleep(0.09)
        self.buttons("HOME")
        time.sleep(0.08)
        self.buttons()
        time.sleep(0.05)

    def keys(self, m):
        return self.req(KEYS, [m & 0x7F, m >> 7 & 0x7F, m >> 14 & 0x7F, m >> 21 & 0x7F])[0]

    def set_par(self, k, v):
        return self.req(SET, put14(k) + put14(v & 0x3FFF))[0]

    def view(self, v, knob=127):
        return self.req(VIEW, [v, knob])[0]

    def piece(self, i):
        return self.req(PIECE, [i])[0]

    def set_playing(self, on, wait=8.0):
        if self.state and self.state["playing"] == on:
            return True
        self.req(PLAY)
        return until(lambda: self.state["playing"] == on, wait)

    def heard(self, text, since, wait=1.5):
        """Was TEXT said (it must be in a line) since that time?"""
        end = time.monotonic() + wait
        while True:
            for t, s in list(self.said):
                if t >= since and text in s:
                    return s
            if time.monotonic() >= end:
                return None
            time.sleep(0.02)


class NoFM1(Exception):
    pass


def until(cond, secs):
    end = time.monotonic() + secs
    while True:
        if cond():
            return True
        if time.monotonic() >= end:
            return False
        time.sleep(0.02)


def show_state(s):
    return ("view %d  %s%s%s  piece %d (%s)  cpu %d/%d  dropouts %d  master %d  knob %d  undo %d/%d" % (
        s["view"], "playing" if s["playing"] else "resting", " dirty" if s["dirty"] else "",
        " perform" if s["perf"] else "", s["piece"], PIECE_NAME[s["piece"] % NPIECES], s["cpu"], s["peak"], s["xruns"],
        s["master"], s["raw"], s["un"], s["rn"]))


def connect(a):
    lk = Link(a.port)
    if not lk.open(reader=a.what == "test"):  # (the screen reader's traffic only where its SAY lines are checked)
        lk.close()
        raise NoFM1("the FM-1 doesn't answer on the link: it needs AISLING 0.5 or later")
    return lk


def need_quiet(lk, a):
    if lk.state is None or (lk.state["master"] != 0 and not a.audible):
        print("master reads %s, not 0: stopping" % (lk.state and lk.state["master"]))
        if lk.state and lk.state["playing"]:
            lk.set_playing(False)
        return False
    return True


# ---------------------------------------------------------------- backup ---

def area_file(d, area):
    return Path(d) / ("flash.bin" if area == A_FLASH else "project.bin" if area == A_PROJ else "piece%d.bin" % (area - A_SLOT))


def cmd_backup(a, lk):
    d = Path(a.dir)
    d.mkdir(parents=True, exist_ok=True)
    if area_file(d, A_FLASH).exists() and not a.force:
        print("%s already holds a backup: not overwriting it" % d)
        return 2
    # the bytes saved in flash first: they're the ones that matter
    for area in [A_FLASH, A_PROJ] + list(range(A_SLOT, A_SLOT + NPIECES)):
        b = lk.read_area(area)
        if b is None:
            print("area %d: not read" % area + (" (nothing saved yet)" if area == A_FLASH else ""))
            if area != A_FLASH:
                return 1
            continue
        area_file(d, area).write_bytes(b)
        print("%-12s %5d B  sum %06x" % (area_file(d, area).name, len(b), lk_sum(b)))
    (d / "firmware.txt").write_text("%s format %d\n" % (lk.hello["version"], lk.hello["format"]))
    return 0


def cmd_verify(a, lk):
    want = area_file(a.dir, A_FLASH)
    if not want.exists():
        print("no flash.bin in %s" % a.dir)
        return 2
    b = lk.read_area(A_FLASH)
    if b is None:
        print("the saved bytes weren't read")
        return 1
    w = want.read_bytes()
    same = b == w
    print("saved bytes: %s (%d B)" % ("byte-identical to the backup" if same else "DIFFER from the backup", len(b)))
    if not same:
        return 1
    bad = 0
    for i in range(NPIECES):                  # what's in RAM, each piece, against those saved bytes
        s = lk.read_area(A_SLOT + i)
        at = 280 + i * SLOT_LEN
        if lk.state["playing"] and i == lk.state["piece"]:
            print("  %s is playing, and its settings move as it plays: not compared" % PIECE_NAME[i])
            continue
        if s is None or s != w[at:at + SLOT_LEN]:
            bad += 1
            print("  %s in RAM differs from the saved copy" % PIECE_NAME[i])
    print("pieces in RAM: %s" % ("all as saved" if not bad else "%d differ" % bad))
    return 0


def cmd_restore(a, lk):
    want = area_file(a.dir, A_FLASH)
    if not want.exists():
        print("no flash.bin in %s" % a.dir)
        return 2
    lk.set_playing(False)
    lk.buttons()
    lk.keys(0)
    st = lk.write_area(A_FLASH, want.read_bytes(), rest_wait=90)
    print("restore: %s" % (ST_NAME[st] if st >= 0 else "no answer"))
    if st != ST_OK:
        return 1
    return cmd_verify(a, lk)


# ------------------------------------------------------------------ test ---

class Checks:
    def __init__(self):
        self.failed = 0
        self.passed = 0

    def __call__(self, ok, what, more=""):
        print("%s %s%s" % ("ok  " if ok else "FAIL", what, "  (%s)" % more if more else ""))
        sys.stdout.flush()
        if ok:
            self.passed += 1
        else:
            self.failed += 1
        return ok


def cmd_test(a, lk):
    c = Checks()
    want = area_file(a.dir, A_FLASH)
    if not want.exists():
        print("no flash.bin in %s: back up first" % a.dir)
        return 2
    saved = want.read_bytes()
    if lk.read_area(A_FLASH) != saved:
        print("the saved bytes aren't the backup's: not testing over them")
        return 2
    h = lk.hello
    c(h["format"] == PROJ_FORMAT and h["proj"] == PROJ_LEN and h["slot"] == SLOT_LEN and h["npars"] == NPARS and h["npieces"] == NPIECES,
      "HELLO's sizes", "%s, format %d, %d/%d B" % (h["version"], h["format"], h["proj"], h["slot"]))
    n0 = lk.nstates
    time.sleep(1.0)
    c(5 <= lk.nstates - n0 <= 15, "state frames at about 10 a second", "%d in a second" % (lk.nstates - n0))
    c(lk.state["master"] == 0, "master is 0 at the engine", "knob %d" % lk.state["raw"])
    x0 = lk.state["xruns"]
    print("    %s" % show_state(lk.state))
    try:
        run_tests(lk, c)
    finally:                                  # whatever happened: nothing held, the saved bytes back
        lk.buttons()
        lk.keys(0)
        if lk.state["perf"]:
            lk.tap("SEL")
        lk.set_playing(False)
        st = lk.write_area(A_FLASH, saved, rest_wait=90)
        c(st == ST_OK, "the saved bytes put back", ST_NAME[st] if st >= 0 else "no answer")
        c(lk.read_area(A_FLASH) == saved, "the saved bytes are byte-identical to the backup")
        for i in range(NPIECES):
            at = 280 + i * SLOT_LEN
            s = lk.read_area(A_SLOT + i)
            c(s == saved[at:at + SLOT_LEN], "%s in RAM as saved" % PIECE_NAME[i])
    print("dropouts over the test: %d" % (lk.state["xruns"] - x0))
    # goodbye, and silence
    lk.bye()
    time.sleep(0.4)
    n = lk.link_frames + lk.reader_frames
    time.sleep(1.5)
    c(lk.link_frames + lk.reader_frames == n, "no frames at all after BYE")
    print("%d passed, %d failed" % (c.passed, c.failed))
    return 1 if c.failed else 0


def run_tests(lk, c):
    # ---- pages: what the panel's buttons do, with the reader listening
    lk.set_playing(True)
    c(lk.state["playing"], "playing, at master 0")

    t = time.monotonic()
    lk.view(V_DRONE)
    c(until(lambda: lk.state["view"] == V_DRONE, 1.0), "the link moves to the Drone page")
    t = time.monotonic()
    lk.tap("SEL")
    c(until(lambda: lk.state["perf"] and lk.state["view"] == V_PERFORM, 1.0), "SEL goes to PERFORM")
    c(lk.heard("PERFORM ON", t), "the screen said so", "PERFORM ON")
    t = time.monotonic()
    lk.tap("SEL")
    c(until(lambda: not lk.state["perf"] and lk.state["view"] == V_DRONE, 1.0), "SEL again comes back to the Drone page",
      "view %d" % lk.state["view"])
    c(lk.heard("PERFORM OFF", t), "the screen said so", "PERFORM OFF")

    was = lk.state["view"]
    lk.tap("ENV")
    c(until(lambda: lk.state["view"] != was, 1.0), "a page button changes page", "view %d" % lk.state["view"])
    lk.view(V_DRONE)

    # ---- notes: a virtual key, and MIDI out
    on0, off0 = lk.notes_on, lk.notes_off
    lk.keys(1 << WHITE_K[3])
    time.sleep(0.25)
    lk.keys(0)
    time.sleep(0.4)
    print("    a key: %d notes on, %d off, on MIDI out" % (lk.notes_on - on0, lk.notes_off - off0))

    # ---- hold SAVE: playing, then the flash's bytes show it
    b = lk.read_area(A_PROJ)
    cur = pars_of(b)[P_ECHO]
    new = cur + 7 if cur <= 90 else cur - 7
    lk.view(V_SPACE)
    c(lk.set_par(P_ECHO, new) == ST_OK, "a knob set over the link", "Echo %d to %d" % (cur, new))
    c(until(lambda: lk.vals.get(P_ECHO, (None,))[0] == new, 1.0), "and its value came back with its words",
      str(lk.vals.get(P_ECHO)))
    c(lk.state["dirty"], "the piece is marked changed")
    x = lk.state["xruns"]
    t = time.monotonic()
    lk.hold("SAVE", 700)
    c(lk.heard("SAVED", t), "hold SAVE: the screen said SAVED")
    c(until(lambda: not lk.state["dirty"], 1.0), "and the piece isn't marked changed any more")
    f = lk.read_area(A_FLASH)
    c(f is not None and pars_of(f)[P_ECHO] == new, "the flash holds the new value", f and str(pars_of(f)[P_ECHO]))
    print("    dropouts across that save, playing: %d" % (lk.state["xruns"] - x))
    lk.req(UNDO)
    time.sleep(0.15)
    c(pars_of(lk.read_area(A_PROJ))[P_ECHO] == cur, "the link's undo takes the knob back")

    # ---- a tap of SAVE isn't a save
    t = time.monotonic()
    lk.tap("SAVE")
    c(lk.heard("SAVED", t, 0.6) is None, "a short tap of SAVE doesn't save")

    # ---- shake, undo, redo, at rest: nothing else moves the settings then
    c(lk.set_playing(False), "stopped")
    before = lk.read_area(A_PROJ)
    t = time.monotonic()
    lk.hold("HOME", 1650)
    c(lk.heard("SHAKE EVERYTHING", t), "hold HOME: the screen said SHAKE EVERYTHING")
    shaken = lk.read_area(A_PROJ)
    moved = sum(1 for x, y in zip(pars_of(before), pars_of(shaken)) if x != y)
    c(moved > 0, "and settings moved", "%d of %d" % (moved, NPARS))
    c(until(lambda: lk.state["un"] > 0, 1.0), "undo has something to take back", "%d" % lk.state["un"])
    t = time.monotonic()
    lk.undo_gesture()
    said = lk.heard("UNDO", t)
    c(said, "HOME + OCT-: the screen said so", said or "")
    after = lk.read_area(A_PROJ)
    diff = [k for k, (x, y) in enumerate(zip(pars_of(before), pars_of(after))) if x != y]
    c(not diff, "and every setting is back", "differ: %s" % diff if diff else "")
    t = time.monotonic()
    lk.undo_gesture(redo=True)
    said = lk.heard("REDO", t)
    c(said, "HOME + OCT+: the screen said so", said or "")
    again = lk.read_area(A_PROJ)
    c(pars_of(again) == pars_of(shaken), "and the shake is back")
    lk.undo_gesture()
    c(pars_of(lk.read_area(A_PROJ)) == pars_of(before), "undo again: back to before the shake")

    # ---- a piece written and read back
    lk.set_playing(True)
    piece = lk.state["piece"]
    other = (piece + 1) % NPIECES
    orig = lk.read_area(A_SLOT + other)
    c(orig is not None and len(orig) == SLOT_LEN, "%s read" % PIECE_NAME[other])
    edit = bytearray(orig)
    v = struct.unpack_from("<h", edit, P_ECHO * 2)[0]
    struct.pack_into("<h", edit, P_ECHO * 2, v + 5 if v <= 90 else v - 5)
    edit = bytes(edit)
    c(lk.write_area(A_SLOT + other, edit, badsum=True) == ST_SUM, "a wrong sum is refused")
    c(lk.read_area(A_SLOT + other) == orig, "and it changed nothing")
    c(lk.write_area(A_SLOT + other, edit) == ST_OK, "a changed piece is written")
    c(lk.read_area(A_SLOT + other) == edit, "and reads back byte for byte")
    c(lk.write_area(A_SLOT + other, orig) == ST_OK, "the piece put back")
    c(lk.read_area(A_SLOT + other) == orig, "and reads back byte for byte")
    c(lk.write_area(A_FLASH, b"\0" * PROJ_LEN) in (ST_BUSY, ST_REFUSED), "the saved bytes can't be written over while playing, or with junk")

    # ---- the whole project, out and back
    p = lk.read_area(A_PROJ)
    c(lk.write_area(A_PROJ, p) == ST_OK, "the whole project written back")
    q = lk.read_area(A_PROJ)
    diff = [k for k, (x, y) in enumerate(zip(p, q)) if x != y]
    c(len(diff) <= 2 * NPARS, "and read back", "%d bytes moved while playing" % len(diff))

    # ---- pieces: over and back, still playing
    was = lk.state["piece"]
    lk.piece(other)
    c(until(lambda: lk.state["piece"] == other, 1.5), "a piece change", PIECE_NAME[other])
    lk.piece(was)
    c(until(lambda: lk.state["piece"] == was, 1.5), "and back", PIECE_NAME[was])

    # ---- the editor vanishes with a key held: everything lets go, nothing stays stuck
    on0, off0 = lk.notes_on, lk.notes_off
    lk.keys(1 << WHITE_K[4])
    time.sleep(0.2)
    t = time.monotonic()
    lk.beat_link = False
    gone = lk.heard("EDITOR GONE", t, 7.5)
    c(gone, "no heartbeat: gone after 5 s", gone or "")
    n = lk.nstates
    time.sleep(0.5)
    c(lk.nstates == n, "and the state frames stop")
    print("    the key that was held: %d on, %d off" % (lk.notes_on - on0, lk.notes_off - off0))
    c(lk.notes_on - on0 <= lk.notes_off - off0, "and no note left on")
    t = time.monotonic()
    lk.open()
    c(lk.hello is not None and lk.heard("EDITOR CONNECTED", t), "HELLO brings it back", "EDITOR CONNECTED")


# ------------------------------------------------------------------ soak ---

class Tally:
    def __init__(self):
        self.cpu = []
        self.peak = 0
        self.x0 = self.x1 = None

    def add(self, s):
        self.cpu.append(s["cpu"])
        self.peak = max(self.peak, s["peak"])
        if self.x0 is None:
            self.x0 = s["xruns"]
        self.x1 = s["xruns"]

    def line(self):
        if not self.cpu:
            return "no state frames"
        s = "cpu mean %5.1f  max %3d  peak %3d  dropouts %d" % (
            sum(self.cpu) / len(self.cpu), max(self.cpu), self.peak, self.x1 - self.x0)
        if self.x1 >= XRUNS_FULL:             # (the link's count stops there, until the FM-1 restarts)
            s += " (the count is full: this figure means nothing)"
        return s


def soak_phase(lk, secs, poll, log, label):
    t = Tally()
    end = time.monotonic() + secs
    n0 = lk.nstates
    reads = bad = 0
    gaps = []
    last = -1
    while time.monotonic() < end:
        if poll:                              # an editor at its busiest: the whole project, over and over
            t0 = time.monotonic()
            if lk.read_area(A_PROJ) is None:
                bad += 1
            else:
                reads += 1
                gaps.append(time.monotonic() - t0)
        else:
            time.sleep(0.25)
        if lk.nstates != last and lk.state:
            last = lk.nstates
            t.add(lk.state)
            if log:
                log.write("%.1f,%s,%d,%d,%d,%d,%d\n" % (time.time(), label, lk.state["piece"], lk.state["playing"], lk.state["cpu"], lk.state["peak"], lk.state["xruns"]))
    s = t.line()
    if poll:
        s += "  reads %d (%d failed), %.2f s each" % (reads, bad, sum(gaps) / len(gaps) if gaps else 0)
    s += "  states %d" % (lk.nstates - n0)
    return s


def cmd_soak(a, lk):
    pieces = [int(x) for x in a.pieces.split(",")] if a.pieces else list(range(NPIECES))
    log = open(a.log, "a") if a.log else None
    start_piece = lk.state["piece"]
    print("boot: %s" % show_state(lk.state))
    lk.set_playing(True)
    if a.nolink:                              # nothing on the wire at all: dropouts counted from outside it
        x = lk.state["xruns"]
        lk.bye()
        print("no link for %d min..." % a.nolink)
        sys.stdout.flush()
        time.sleep(a.nolink * 60)
        lk.open(reader=False)
        print("no link, %d min, on %s: dropouts %d, now %s" % (a.nolink, PIECE_NAME[lk.state["piece"]], lk.state["xruns"] - x, show_state(lk.state)))
        sys.stdout.flush()
    for i in pieces:
        lk.piece(i)
        until(lambda: lk.state["piece"] == i, 2.0)
        lk.set_playing(True)
        print("%-11s idle:    %s" % (PIECE_NAME[i], soak_phase(lk, a.minutes * 60, False, log, "idle")))
        sys.stdout.flush()
        if a.poll:
            print("%-11s polling: %s" % (PIECE_NAME[i], soak_phase(lk, a.poll * 60, True, log, "poll")))
            sys.stdout.flush()
        if log:
            log.flush()
    lk.piece(start_piece)
    print("end: %s" % show_state(lk.state))
    return 0


# ------------------------------------------------------------------ main ---

def main():
    p = argparse.ArgumentParser(description="AISLING's USB link, on a real FM-1")
    p.add_argument("--port", help="a part of the port's name")
    p.add_argument("--audible", action="store_true", help="test or soak even though master isn't 0")
    sub = p.add_subparsers(dest="what", required=True)
    sub.add_parser("state")
    sub.add_parser("play")
    sub.add_parser("stop")
    for name in ("backup", "verify", "restore", "test"):
        s = sub.add_parser(name)
        s.add_argument("dir")
        if name == "backup":
            s.add_argument("--force", action="store_true", help="overwrite a backup already there")
    s = sub.add_parser("soak")
    s.add_argument("--minutes", type=float, default=10, help="minutes a piece, link idle")
    s.add_argument("--poll", type=float, default=0, help="then minutes a piece, an editor reading all it can")
    s.add_argument("--nolink", type=float, default=0, help="first, minutes with nothing on the wire")
    s.add_argument("--pieces", help="which, as 0,3,5")
    s.add_argument("--log", help="append every sample to this CSV")
    a = p.parse_args()

    try:
        lk = connect(a)
    except NoFM1 as e:
        print(e)
        sys.stdout.flush()
        os._exit(3)
    code = 0
    try:
        print("%s, format %d  |  %s" % (lk.hello["version"], lk.hello["format"], lk.state and show_state(lk.state)))
        sys.stdout.flush()
        if a.what in ("test", "soak") and not need_quiet(lk, a):
            code = 4
        elif a.what in ("play", "stop"):
            code = 0 if lk.set_playing(a.what == "play") else 1
            print(show_state(lk.state))
        elif a.what == "backup":
            code = cmd_backup(a, lk)
        elif a.what == "verify":
            code = cmd_verify(a, lk)
        elif a.what == "restore":
            code = cmd_restore(a, lk)
        elif a.what == "test":
            code = cmd_test(a, lk)
        elif a.what == "soak":
            code = cmd_soak(a, lk)
        if lk.beat_link:
            lk.bye()
    except KeyboardInterrupt:
        code = 1
    sys.stdout.flush()
    os._exit(code)                            # (mido can hang at exit with both ports open)


if __name__ == "__main__":
    main()
