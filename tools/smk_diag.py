#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Stage 3's readout from an SMK-37 Elite on AISLING (a build with AISLING_SMK_DIAG): the key matrix as the
firmware last scanned it, and the pins as they read. It only asks; nothing on the Elite changes.

  smk_diag.py [--port NAME] [--seconds N]
  smk_diag.py --analog [--seconds N]      the pads, faders and wheels as the scan reads them (stage 4)

Hold a button, a key or turn a knob while it runs: every change is printed, as col<<4|row codes.
"""
import argparse
import sys
import time

sys.path.insert(0, __import__("os").path.dirname(__file__))
import fm1_link as L

DIAG, R_DIAG = 40, 70
DIAG_AN, R_DIAG_AN = 41, 71
# what sits at each address of each mux (the guide's map)
AN_NAME = ([f"pad{p}" for p in (1, 2, 3, 0, 4, 7, 5, 6)], [f"pad{p}" for p in (9, 10, 11, 8, 12, 15, 13, 14)],
           ["fader1", "fader2", "fader3", "fader0", "pitch", "mod", "pedal", "battery"])
PORTS, REGS = "ABCH", ("OUT", "IN", "DIR", "DIE", "PU", "PD")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", default="SMK-37 Elite")
    ap.add_argument("--seconds", type=float, default=20)
    ap.add_argument("--analog", action="store_true", help="the pads, faders and wheels instead of the matrix")
    a = ap.parse_args()
    lk = L.Link(a.port)
    got = []
    rx = lk._rx

    def both(m):
        if m.type == "sysex" and tuple(m.data[:3]) == L.H and len(m.data) > 4 and m.data[3] == (R_DIAG_AN if a.analog else R_DIAG):
            got.append(m.data[5:])
        else:
            rx(m)
    lk.inp.callback = both
    if lk.open(reader=False) is None:
        print("no link on it")
        return 3
    seen, regs_seen, union = None, None, set()
    end = time.monotonic() + a.seconds
    seq = 0
    try:
        while time.monotonic() < end:
            seq = seq % 126 + 1
            lk._send(*L.H, DIAG_AN if a.analog else DIAG, seq)
            time.sleep(0.03)
            while got:
                d = got.pop(0)
                if a.analog:
                    if len(d) < 56:
                        continue
                    v = [d[2 * i] | d[2 * i + 1] << 7 for i in range(28)]
                    raw, pads = v[:24], v[24] | v[25] << 8
                    if regs_seen is None:
                        regs_seen = raw
                        for m in range(3):
                            print("mux " + "ABC"[m], " ".join(f"{AN_NAME[m][x]}={raw[m * 8 + x]}" for x in range(8)))
                        print(f"scans {v[26]}, conversions missed {v[27]}")
                    moved = [f"{AN_NAME[i >> 3][i & 7]}={raw[i]}" for i in range(24)
                             if abs(raw[i] - regs_seen[i]) >= 12 and i != 23]
                    key = (pads, tuple(moved))
                    if key != seen:
                        seen = key
                        union.update(p for p in range(16) if pads >> p & 1)
                        print(f"{time.strftime('%H:%M:%S')} pads down:",
                              " ".join(str(p) for p in range(16) if pads >> p & 1) or "-",
                              "| moved:", " ".join(moved) or "-", flush=True)
                    continue
                if len(d) < 32 + 24 * 5:
                    continue
                raw = [d[2 * i] | d[2 * i + 1] << 7 for i in range(16)]
                reg = [sum(d[32 + 5 * i + k] << 7 * k for k in range(5)) & 0xFFFFFFFF for i in range(24)]
                closed = sorted(c << 4 | r for c in range(16) for r in range(8) if raw[c] >> r & 1)
                if regs_seen is None:
                    regs_seen = reg
                    for p in range(4):
                        print("P" + PORTS[p], " ".join(f"{REGS[k]}={reg[p * 6 + k]:04X}" for k in range(6)))
                ins = (reg[1], reg[7], reg[13], reg[19])
                if closed != seen:
                    seen = closed
                    union.update(closed)
                    print(f"{time.strftime('%H:%M:%S')} closed:", " ".join(f"{c:02X}" for c in closed) or "-",
                          "| IN A,B,C,H:", " ".join(f"{v:04X}" for v in ins), flush=True)
    finally:
        lk.bye()
        lk.close()
    if a.analog:
        print("every pad seen down:", " ".join(str(p) for p in sorted(union)) or "-")
        return 0
    print("every code seen closed:", " ".join(f"{c:02X}" for c in sorted(union)) or "-")
    return 0


if __name__ == "__main__":
    sys.exit(main())
