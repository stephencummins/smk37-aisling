#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""An FM-1 scenario (tests/scenarios/*.amb) with the SMK-37 Elite's control names, for build/host/amb_host_smk37: the
same presses, turns and keys on the controls that do the same jobs, so the same expectations must hold. A scenario
that sets the MASTER pot is refused (the SMK's MASTER is K1, an encoder: tests/scenarios_smk37/master.amb).

  python3 tests/smk37_scenario.py tests/scenarios/pages.amb > build/scenarios_smk37/pages.amb

Exit 3: the scenario cannot be played on the SMK-37 Elite (it names the reason)."""
import sys

# the FM-1's names (host/amb_host.c BTN_N, ENC_N) and the SMK's for the same job (hal/smk37/smk_input.h)
BTN = dict(zip("FX SEL ENV LFO EDIT GLO HOME SAVE ARP SEQ PLAY REC OCT- OCT+".split(),
               "FX SCALE CHORD NOTE_REPEAT PARA GLOBE PATCH BT ARP SEQ SEQ_PLAY SEQ_REC OCT- OCT+".split()))
ENC = dict(zip("SELECT ALGO PRESET K1 K2 K3 K4".split(), "K2 K3 K4 K5 K6 K7 K8".split()))
# the FM-1's keys 0..26 are the SMK's 5..31 (F3..G5): its white key N is the SMK's N+3 (C D E below), black N, N+2
KEY_LO, WHITE_LO, BLACK_LO = 5, 3, 2


class Refuse(Exception):
    pass


def key(k):
    if k[0] == "w":
        return f"w{(int(k[1:]) & 15) + WHITE_LO}"          # (amb_host's key_of: w & 15, b % 11)
    if k[0] == "b":
        return f"b{int(k[1:]) % 11 + BLACK_LO}"
    if not 0 <= int(k) < 27:
        raise Refuse(f"key {k} is not one of the FM-1's 27")
    return str(int(k) + KEY_LO)


def line(s, n):
    w = s.split()
    if not w or s.startswith("#"):
        return s
    cmd = w[0]
    if cmd in ("press", "release", "tap"):
        w[1] = BTN[w[1]]
    elif cmd in ("turn", "spin"):
        w[1] = ENC[w[1]]
    elif cmd in ("key", "tapkey"):
        w[1] = key(w[1])
    elif cmd == "strum":
        for i in (1, 2):
            if not 0 <= int(w[i]) < 16:
                raise Refuse(f"line {n}: strum past the FM-1's 16 white keys")
            w[i] = str(int(w[i]) + WHITE_LO)
    elif cmd == "master":
        raise Refuse(f"line {n}: sets the MASTER pot (the SMK's MASTER is K1, an encoder)")
    else:
        return s
    return " ".join(w) + "\n"


def main(path):
    out = []
    with open(path) as f:
        for n, s in enumerate(f, 1):
            out.append(line(s, n))
    sys.stdout.write("".join(out))


if __name__ == "__main__":
    try:
        main(sys.argv[1])
    except Refuse as e:
        print(f"{sys.argv[1]}: not for the SMK-37 Elite: {e}", file=sys.stderr)
        sys.exit(3)
