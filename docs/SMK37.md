# AISLING on the SMK-37 Elite

**Status.** AISLING builds for the M-VAVE SMK-37 Elite and passes its tests on a computer, but it has not yet run on
an Elite. There is nothing to install: no package will be offered until one has been installed, played and taken off
again on an Elite, with the way back to M-VAVE's firmware proven. This page says what the code does and what its tests
check; section 12 lists what only the hardware can settle.

The Elite has the FM-1's chip and its audio path, so AISLING's whole app runs on it as it is: the same engine, the same
eight pieces, the same pages and screen, saved the same way. What differs is the panel: 37 keys, more buttons, knobs
that turn without end, pads, faders and wheels. One switch, `AISLING_BOARD_SMK37`, puts a board layer for the Elite
under the FM-1's interfaces. Without it the same tree builds the FM-1's firmware exactly as before, to the byte.

It is for the **SMK-37 Elite** only, its layout read from M-VAVE's V016 firmware for it: not the SMK-37 Pro, and not
the MKE-P37.

The site for the Elite is [stephencummins.github.io/smk37-aisling](https://stephencummins.github.io/smk37-aisling/),
with the [controls](https://stephencummins.github.io/smk37-aisling/controls/) on a page of their own. Everything
AISLING does is in the FM-1 version's [manual](https://aisling.stephen8n.com/manual/).

---

## Contents

1. What differs from the FM-1
2. The controls
3. Volume
4. The keys
5. Not used yet
6. The flash
7. The pins
8. Building
9. Testing
10. The simulator
11. Installing, and going back
12. Open until it runs on an Elite
13. The files
14. Credits

---

## 1. What differs from the FM-1

| | FM-1 | SMK-37 Elite |
| --- | --- | --- |
| Keys | 27, lit | 37, unlit; AISLING plays the middle 27, F3 to G5 |
| MASTER | a pot | K1, a knob that turns without end: half volume at each power-on |
| Knobs | 7 more | K2 to K8, the same 7 jobs |
| Buttons | 14 | 14 doing the FM-1's jobs, 4 more that do nothing yet |
| Pads | none | 16, not used yet (section 5) |
| Faders | none | 4, not used yet |
| Wheels | none | pitch and mod, not used yet |
| MIDI in | USB and the TRS jack | USB only |
| Screen | 240×240 | the same picture, wired differently; the backlight always on |
| Calibration | the panel's | none: the Elite's buttons have fixed codes |
| Identity | `FM-1_353XXYYZZ` | `SMK-37 Elite_353XXYYZZ` |

## 2. The controls

Each of the Elite's buttons does the job of an FM-1 button. The manual, the cheat sheet and the screen use the FM-1's
names; this is what they mean on the Elite:

| Elite | FM-1 |
| --- | --- |
| PATCH | HOME |
| SCALE | SEL |
| CHORD | ENV |
| NOTE REPEAT | LFO |
| PARA | EDIT |
| GLOBE | GLO |
| BT | SAVE |
| SEQ PLAY | PLAY |
| SEQ REC | REC |
| ARP, FX, SEQ | ARP, FX, SEQ |
| OCTAVE− and OCTAVE+ | OCT− and OCT+ |
| KNOB BANK, FADER BANK, <, > | nothing yet |
| K1 | MASTER |
| K2, K3, K4 | SELECT, ALGORITHM, PRESETS |
| K5 to K8 | K1 to K4 |

Two buttons held together work as on the FM-1, by their Elite names: OCTAVE− with OCTAVE+ for five seconds, for
instance, is the update mode. The screen's hints still name the FM-1's buttons: TAP REC on the effects page means SEQ
REC.

The site's [controls page](https://stephencummins.github.io/smk37-aisling/controls/) has what each one does.

## 3. Volume

The FM-1's MASTER is a pot, which stays where it was left. The Elite's K1 turns without end, so AISLING keeps the level
itself. At each power-on it is half (−6 dB). One click of the knob moves it a
step, and 43 clicks take it from silence to full. The steps follow the same curve as the FM-1's pot, so the quiet end
has room. The level isn't saved.

## 4. The keys

The Elite's 37 keys run from C3 to C6. AISLING plays the middle 27, F3 to G5, which are the FM-1's 27: the white keys
give the Lead, and the black keys do what the FM-1's do. The five below them (C3 to E3) and the five above (G♯5 to
C6) are silent for now.

A key sounds near the bottom of its travel and stops when it is nearly back up. AISLING doesn't use velocity, as on
the FM-1.
The Elite's keys have no lights, so the FM-1's lit key for the note the Lead sounds has no place to show.

## 5. Not used yet

These wait until AISLING has run on an Elite. The plan for each:

- **The 16 pads:** the effects grid, an effect each while held, lit in its colour.
- **The 4 faders:** the mix (the Drone, the Sequence, Sequence 2 and the Lead, left to right), each taking over
  only once it is moved.
- **The pitch wheel:** bends the Lead, up to two semitones each way.
- **The mod wheel:** opens the filters by degrees, toward where Rise takes them.
- **The outer white keys** (C3, D3, E3 and A5, B5, C6): the Lead's scale, further down and up.

KNOB BANK, FADER BANK, < and >, the outer black keys and the pedal have no job yet. TRS MIDI out stays off.

## 6. The flash

The Elite's flash holds 1 MiB. Its regions are M-VAVE's own, read from M-VAVE's SMK-37 Elite V016 update package.
AISLING writes three parts of it and never writes the rest:

| Range | What it holds | AISLING |
| --- | --- | --- |
| 0x00000–0x03FFF | the head: the first-stage loader, its settings, the chip's key | never written |
| 0x04000–0x9CFFF | the app | the app |
| 0x9D000–0xBFFFF | the stock firmware's settings | an update's staging, in the top five sectors (0xBB000–0xBFFFF) |
| 0xC0000–0xC0FFF | Bluetooth pairings | never written |
| 0xC1000–0xC1FFF | the keys' velocity calibration, made for each unit | never written |
| 0xC2000–0xEAFFF | the stock presets | AISLING's saved pieces and panel, 0xC2000–0xC5FFF |
| 0xEB000–0xF3FFF | (not in the package) | not written |
| 0xF4000–0xFDFFF | the stock firmware's resources | never written |
| 0xFE000–0xFFFFF | (not in the package), and the key and address at 0xFF000 | never written |

The velocity calibration matters most: it was made for each Elite in the factory, and without a backup of that unit it
can't be put back. AISLING's saved pieces sit over the start of the stock presets, so a full backup taken before the
first install is what restores those: `tools/fm1_rescue.py --write --presets BACKUP.bin` (section 11).

The map is in `firmware/hal/smk37/smk_flash_map.h`. Its `_Static_assert`s keep every window the firmware writes off the
protected ranges; at run time the flash driver refuses a write that would touch one; and `tests/smk_flash_test.c`
checks the saved pieces, the update and the update loader against each range, byte by byte. The FM-1's own map would
cross three of those ranges on an Elite, which is why the Elite has its own.

## 7. The pins

The Elite wires the same chip's pins to other things: PH8, the FM-1's TRS MIDI in, is a row of the Elite's button
matrix, for instance, so the Elite has no MIDI in on its jack. The FM-1's drivers would set pins up wrongly here, so:

- The Elite build never touches PB1 (the charger's status line: held low for eight seconds, it resets the chip), PC7
  (TRS MIDI out) or PC10 (the pads' lights, for later). PA8, PC9 and PA10, the pads', faders' and wheels' three
  analog lines, are inputs only.
- Every pin it does set up is in one table, `firmware/hal/smk37/smk_pins.h`. Each driver sets up its own rows from it,
  inputs first, and an output's level before its direction.
- The build fails if any of the FM-1's hardware headers (`fm1_input.h`, `fm1_adc.h`, `fm1_lcd_hw.h`, `fm1_uart.h`) is
  among the includes of any part of the Elite's firmware, by any path.

## 8. Building

The toolchain and the SDK are the FM-1's ([BUILDING.md](../BUILDING.md)). Then:

```
./build.sh --board smk37                    # build/smk37/amb.fwsc
./build.sh --board smk37 --release 1.0      # build/smk37/aisling-smk37-elite-1.0.fwsc
```

The default board is the FM-1: `./build.sh` makes the same bytes it always has, in `build/`. The Elite's build goes
into `build/smk37/` with the same file names (`amb.bin`, `loader/ota.bin`, the package), and its own checks: the app
must fit the Elite's app area (0x4000–0x9CFFF), leave at least 2 KiB of RAM and 8 KiB of the pool spare, and pass the
pin guard. A release's identity is `SMK-37 Elite_353` and the version's three numbers, two digits each.

`tools/mini_build.sh --board smk37` does the same on another Mac over ssh and fetches the package back.

After a change to shared code, build the FM-1 too and check that `build/amb.bin`, `build/loader/ota.bin` and its
package are unchanged.

## 9. Testing

`tests/run_tests.sh` runs the Elite's tests with the FM-1's. Each control must fail:

| Test | What it checks |
| --- | --- |
| `usb-desc-smk` | the Elite's USB names |
| `smk-flash` | the flash map's guards; the control builds it with the FM-1's map |
| `smk-input` | the key and button matrix, simulated: every key, button and knob by its code; the control swaps OCTAVE− and OCTAVE+ |
| `smk-hw` | the screen's and the battery's setup, on fake registers |
| `smk-pins` | every driver's pin setup, from four starting states; the control rotates the ports |
| `pin-guard` | that the build's guard catches an FM-1 header by any path |
| `package` | the Elite's package format, against M-VAVE's own |
| `installer` | that the installers refuse one model's package on the other |
| `ota-entry-smk37` | the update's way in, with an Elite package (needs a `build/smk37` package) |
| `update-loader-smk37` | the update loader, staging and record on the Elite's map (also needs the SDK) |
| `smk37-<scenario>` | all 34 of the FM-1's simulator scenarios, renamed to the Elite's controls, must give the FM-1's screens and sound byte for byte |
| `smk37-own-*` | the Elite's own scenarios (`tests/scenarios_smk37/`): MASTER on K1, the outer keys, the four more buttons, the pads and faders, the battery |
| `smk37-control` | an FM-1 script, and a script that sets MASTER, must be refused by the Elite's simulator |

## 10. The simulator

`sh host/build_host.sh` builds `build/host/amb_host_smk37` beside the FM-1's `amb_host`. It runs the same scripts (the
command list is at the top of `host/amb_host.c`), with the Elite's controls by their printed names, underscores for
spaces:

- **Buttons:** FX, SCALE, CHORD, NOTE_REPEAT, PARA, GLOBE, PATCH, BT, ARP, SEQ, SEQ_PLAY, SEQ_REC, OCT- and OCT+, and
  KNOB_BANK (nothing yet), FADER_BANK, < and >.
- **Pads and faders:** `pad N down`, `pad N up`, `tappad N` (0 to 7 the top row, 8 to 15 the bottom); `fader N V`
  (0 to 3, V 0 to 1023).
- **Knobs:** K1 to K8. K1 is MASTER, turned like the others (`turn`, `spin`), so `master N` is refused.
- **Keys:** 0 to 36 (C3 to C6), or w0 to w21 for the white keys and b0 to b14 for the black.
- **The battery:** `battery N` sets its reading.

`tests/smk37_scenario.py` renames an FM-1 script for it:

```
sh host/build_host.sh
python3 tests/smk37_scenario.py tests/scenarios/pages.amb > build/pages_smk37.amb
build/host/amb_host_smk37 build/pages_smk37.amb build/pages_smk37
```

## 11. Installing, and going back

Not yet. The tools are ready for it:

- `tools/fm1_install.py` and the web installer (`web/fm1ota.js`) find an Elite by its USB name, read the package's
  layout from its header, and refuse a package made for the other model: an FM-1 package on an Elite, or the reverse.
- The Elite's stock update loader is the FM-1's official one, so AISLING's way back to M-VAVE's firmware is the FM-1's.
- `tools/fm1_rescue.py` takes M-VAVE's own SMK-37 Elite V016 package (it knows it by its checksum). Before it writes,
  it checks the flash chip's ID and that the backup holds the velocity calibration. Then it writes only the app area,
  0x4000–0x9CFFF.
- With `--presets BACKUP.bin` it also puts the four preset sectors AISLING uses, 0xC2000–0xC5FFF, back from a backup
  it saved before the first install. It takes only that unit's own backup: the backup's bootloader area and velocity
  calibration must equal the flash's. Without `--presets` the stock firmware starts with AISLING's data in the first
  of its presets.

The plan, on an Elite: first a backup of the whole flash, read twice and compared; then the first install, with the way
back proven before anything else.

> Building AISLING for the Elite yourself and installing it is at your own risk, and not yet wise: each question in
> section 12 can leave the keys, knobs or lights not working, and the way back is untried on an Elite.

## 12. Open until it runs on an Elite

Each is one place in the code:

- **The order the shift registers take their bits** (`smk__sr_bit` in `smk_input.h`). The four 74HC595s that scan the
  matrix take a 32-bit word, and which end goes first isn't known. Wrong, and a key reads as another, or as nothing.
- **Which way a knob turns up** (`SMK_ENC_FLIP`).
- **The button lights' two commons:** their register (`SMK_LED_COM`, 0x16A0C) and its other bits, and which light is
  which.
- **The debounce,** 4 scans of 1.6 ms, and how long a knob rests before it counts as a click, 28 scans.
- **The update record:** whether the Elite's first-stage loader finds it at the top of the settings area (0xBFF00). If
  not, an update keeps its record in RAM only (`FELUCCA_OTA_RAMONLY`), and a cable pulled mid-update can't resume.
- **The stock loader's USB name,** `OTA_LDR_NAME` in `firmware/src/ota.c`: "ota-SMK-37 Elite_016". Stock's own update
  record, read in a backup of an Elite on V016, has that name; AISLING hasn't yet started that loader with it.
- **The flash chip's ID:** read on an Elite on V016, it is 856014, the part of the first FM-1s. EB6014 was read on an
  SMK-37 Pro. Both are on the list; any other part runs from RAM and saves nothing until it has been read and added.

## 13. The files

| File | What |
| --- | --- |
| `firmware/src/board_caps.h` | what each board has (keys, outer keys, pads, faders, wheels), so the Elite's paths compile away on the FM-1 |
| `firmware/src/app/master.h` | MASTER's curve, and MASTER on a knob that turns without end |
| `firmware/hal/fl_map.h` | the flash map, per board |
| `firmware/hal/smk37/smk_flash_map.h` | the Elite's flash, and the ranges never written |
| `firmware/hal/smk37/smk_input.h` | the keys, buttons, knobs and button lights, behind `fm1_input.h`'s interface |
| `firmware/hal/smk37/smk_lcd_hw.h` | the screen's wiring |
| `firmware/hal/smk37/smk_adc.h` | the analog multiplexers: the battery, and later the pads, faders and wheels |
| `firmware/hal/smk37/smk_pins.h` | every pin the Elite build sets up, in one table |
| `firmware/hal/smk37/smk_reg.h` | the register access, so the tests can fake it |
| `tools/build.py` | the build, per board (`--board`) |
| `tools/fm1pkg_make.py` | the package, per board (`BOARDS`) |
| `tools/make_pages_smk37.py` | the Elite's site |
| `tools/publish_pages_smk37.sh` | puts the site on GitHub Pages |

## 14. Credits

AISLING is by CU_STE ([www.stephen8n.com](https://www.stephen8n.com)). Its board layer for the Elite follows M.
Geisler's (genkigenki) SMK-37 ports on the `smk37` branches of [fm1-amb](https://github.com/genkigenki/fm1-amb/tree/smk37)
and [fm1-fomni](https://github.com/genkigenki/fm1-fomni/tree/smk37) (GPL-3.0), written again for AISLING. The
hardware's facts are from M. Geisler's guide, [FM-1 to SMK-37 Pro](https://github.com/genkigenki/fm1-to-smk37) (CC BY-NC
4.0), cited in each file and not copied. The flash regions were read from M-VAVE's own SMK-37 Elite V016 package, none
of which is in this tree. [LICENSING.md](../LICENSING.md) has the rest.

M-VAVE, FM-1 and SMK-37 are trademarks of their owners. AISLING is not affiliated with M-VAVE, or with the authors of
the ports it follows.
