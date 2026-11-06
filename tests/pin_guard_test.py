#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""tools/build.py's pin guard, on dependency lists written here as the compiler writes them (-MD): the SMK-37 Elite's
build fails if any unit includes one of the FM-1's hardware headers, by any path, and if a list is missing or does
not name what it must (so an unwritten or unparsed list cannot pass). The FM-1's commands get no -MD. Run from the
repo root:
  python3 tests/pin_guard_test.py"""
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import build as B  # noqa: E402

failed = 0


def ok(cond, what):
    global failed
    print(f"{what:<72} {'ok' if cond else 'FAIL'}")
    failed += not cond


MAIN_NEED = ("smk_pins.h", "smk_input.h", "smk_lcd_hw.h", "smk_adc.h")
CLEAN = """build/smk37/main.o: firmware/src/main.c firmware/hal/board_caps.h \\
  firmware/hal/fm1_time.h firmware/hal/smk37/smk_input.h \\
  firmware/hal/smk37/smk_reg.h firmware/hal/fm1_gpio.h \\
  firmware/hal/smk37/smk_pins.h firmware/hal/fm1_audio.h \\
  firmware/hal/smk37/smk_adc.h firmware/hal/smk37/smk_lcd_hw.h \\
  firmware/src/libc.c firmware/src/lcd.c firmware/src/usb.c
"""

with tempfile.TemporaryDirectory() as t:
    t = Path(t)

    def guard(text, src="main.c", need=MAIN_NEED, name="main.d"):
        d = t / name
        d.write_text(text)
        return B.pin_guard([(d, src, need)])

    (t / "x.d").write_text(CLEAN)
    names = [Path(f).name for f in B.deps(t / "x.d")]
    ok(len(names) == 13 and names[0] == "main.c" and names[-1] == "usb.c" and "main.o" not in names,
       "deps: every file after the target, across continuation lines")
    ok(guard(CLEAN) == [], "a clean SMK list passes")
    for h in B.FM1_PIN_HEADERS:
        e = guard(CLEAN.replace("firmware/src/usb.c", f"firmware/src/usb.c \\\n  firmware/hal/{h}"))
        ok(len(e) == 1 and h in e[0], f"{h} in the list fails")
    e = guard(CLEAN.replace("firmware/hal/fm1_time.h", "firmware/src/../hal/./fm1_input.h firmware/hal/fm1_time.h"))
    ok(len(e) == 1 and "fm1_input.h" in e[0], "fm1_input.h by another path fails")
    ok(guard(CLEAN.replace("firmware/src/usb.c", "firmware/src/my_fm1_input.h firmware/src/fm1_input.hpp")) == [],
       "a name that only contains fm1_input.h passes")
    e = guard(CLEAN.replace("main.o: firmware/src/main.c", "main.o:"))
    ok(len(e) == 1 and "main.c" in e[0], "a list without its own source fails")
    e = guard(CLEAN.replace("firmware/hal/smk37/smk_pins.h ", ""))
    ok(len(e) == 1 and "smk_pins.h" in e[0], "a list without smk_pins.h fails")
    ok(len(guard("")) == 1 + len(MAIN_NEED), "an empty list fails")
    e = B.pin_guard([(t / "none.d", "main.c", ())])
    ok(len(e) == 1 and "no dependency list" in e[0], "a missing list fails")
    (t / "main.d").write_text(CLEAN)
    two = B.pin_guard([(t / "main.d", "main.c", MAIN_NEED), (t / "none.d", "loader.c", ())])
    ok(len(two) == 1, "every unit is checked")
    e = guard("build/smk37/loader/loader.o: firmware/loader/loader.c \\\n  firmware/hal/fm1_uart.h\n",
              src="loader.c", need=(), name="loader.d")
    ok(len(e) == 1 and "loader.c includes fm1_uart.h" in e[0], "the loader's list is checked too")

    B.BOARD = B.BOARDS["fm1"]
    ok(B.deps_flags(t / "main.d") == [], "the FM-1: no -MD (its commands as they were)")
    B.BOARD = B.BOARDS["smk37"]
    ok(B.deps_flags(t / "main.d") == ["-MD", "-MF", t / "main.d"], "the SMK-37 Elite: -MD -MF <list>")

print("pin guard:", "ok" if not failed else f"{failed} FAILED")
sys.exit(1 if failed else 0)
