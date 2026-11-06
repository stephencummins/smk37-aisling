# AISLING licensing

AISLING is free software under the GNU General Public License, version 3 only
(`GPL-3.0-only`, full text in `LICENSE`). It is a fork of [NACHTFAHRT](https://github.com/FartyPants/fm1-nachtfahrt),
itself a fork of [DUB-1](https://github.com/sbegueria/fm1-dub), a fork of
[AMB-1](https://github.com/charlesvestal/fm1-amb) by Charles Vestal (GPL-3.0-only), which is built on
Felucca's platform layer (through X0X and FoMni), and keeps their licence. The same tree also builds for the
M-VAVE SMK-37 Elite (`AISLING_BOARD_SMK37`); its board layer is credited below. If you distribute AISLING, or firmware
derived from it, you must give your recipients its complete corresponding source under the same licence.

## Where the code comes from

| What | Origin | Licence |
| --- | --- | --- |
| Platform: `firmware/hal/`, `firmware/loader/`, `firmware/src/{libc,lcd,gfx,usb,storage,ota,midi_uart}.c`, `tools/` (build, package, install, rescue), `web/fm1*.js` | [Felucca](https://github.com/hugelton/Felucca), Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments, as changed for [X0X](https://github.com/charlesvestal/fm1-x0x) and FoMni, then AMB-1, DUB-1, NACHTFAHRT and AISLING | GPL-3.0-only |
| `firmware/src/app/{panel,plat_fm1,main_fm1}.c`, `host/amb_host.c`, `web/emu/`, `tools/{make_pages,manual_html}.py`, `tools/publish_pages.sh` | adapted from X0X (itself from Felucca's `panel.c`, `main.c`, `audio.c`) | GPL-3.0-only |
| Dattorro's plate reverb in `firmware/src/dsp/amb.c` | from [FoMni](https://github.com/charlesvestal/fm1-fomni) | GPL-3.0-only |
| Shimmer's octave shifter (a rotating delay) in `firmware/src/dsp/amb.c` | after OCT UP and its SHIMMER in [Felucca](https://github.com/hugelton/Felucca)'s `perform.c` (Leo Kuroshita, Hügelton Instruments; also in [Skiff](https://github.com/illuminus5/skiff)), written again in floating point | GPL-3.0-only |
| Choir, the Drone's formant choir in `firmware/src/dsp/amb.c` (`choir`, `CH_F`, `CH_B`) | after VOICE, `eng_formant.c` in [Felucca](https://github.com/hugelton/Felucca) (Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments; the same file in [Skiff](https://github.com/illuminus5/skiff)): Klatt's (1980) resonators in cascade, written again in floating point; the vowels' formants (`CH_F`) are Felucca's table (`VOWEL_F`, after Hillenbrand et al.'s 1995 published means), reordered | GPL-3.0-only |
| The Atmospheres' pink and brown noise (Wind, Surf), Wind's band whose level follows its wandering, and Rain's drops (and Fire's pops, timed the same way) in `firmware/src/dsp/amb.c` (`render_atmos`, `rain_drop`, `fire_pop`) | after NOISE, `eng_noise.c` in [Skiff](https://github.com/illuminus5/skiff) (Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments): its COLR tilt (pink: Paul Kellet's three-pole "economy" filter, the same constants; brown: a leaky pole), DRFT and DUST, written again in floating point | GPL-3.0-only |
| Pluck, the sequences' plucked string (Karplus-Strong) in `firmware/src/dsp/amb.c` (`ks_strike`, `render_ks`) | after `third_party/synth/pluck.c` in M-VAVE's [fm1_open](https://github.com/m-vave/fm1_open) (Copyright (c) 2020 Electrosmith, Corp, Emilie Gillet; 2026 M-VAVE: an FM1 adaptation of [DaisySP](https://github.com/daisyaudio/DaisySP)'s String), written again in this engine's style; its notice and the MIT permission text are kept in the code, and below | MIT (the adapted code, in this GPL-3.0-only work) |
| The SMK-37 Elite board layer: `firmware/hal/smk37/`, `firmware/hal/fl_map.h`, `firmware/src/board_caps.h`, `firmware/src/app/master.h`, the code behind `AISLING_BOARD_SMK37`, `tests/host/smk_*`, `tests/smk_flash_test.c`, `tests/pin_guard_test.py`, `tests/smk37_scenario.py`, `tests/scenarios_smk37/` | after the SMK-37 ports on the `smk37` branches of [fm1-amb](https://github.com/genkigenki/fm1-amb/tree/smk37) and [fm1-fomni](https://github.com/genkigenki/fm1-fomni/tree/smk37) (SMK-37 port: Copyright (C) 2026 M. Geisler (genkigenki/makkuro)): the board behind the FM-1's `hal/` interfaces, one switch for both boards and the FM-1 build unchanged. Written again for AISLING on the Elite, not copied; their files were read, never built or run | GPL-3.0-only |
| The SMK-37 Elite's hardware facts: the pins, the key and button matrix, the 595 word, the LED groups, the LCD's wiring and the analog mux (cited in each `firmware/hal/smk37/` file and its test) | [FM-1 to SMK-37 Pro: porting custom firmware](https://github.com/genkigenki/fm1-to-smk37) by M. Geisler (makkuro), cited, not copied: the facts are stated again in this tree's own code and words, and none of the guide's text or figures is reproduced | the guide: [CC BY-NC 4.0](https://creativecommons.org/licenses/by-nc/4.0/) |
| The SMK-37 Elite's flash regions (`firmware/hal/smk37/smk_flash_map.h`, `tools/fm1pkg_make.py` BOARDS) and how the tools recognise its stock package | read from M-VAVE's SMK-37 Elite V016 update package; no part of the package is in this tree | facts only |
| The idea of a browser emulator for FM-1 firmware (`web/emu/`) | [Groove OS](https://www.groove-os.com/) by Peter Gombos, whose site runs its firmware in the browser; no code from it | credit only |
| Everything else in `firmware/src/{app,dsp}`, `tests/` | AMB-1, as changed for DUB-1, then NACHTFAHRT, then AISLING | GPL-3.0-only |
| AISLING's changes: its name and identity, its pieces, voices, ornaments and modes | Copyright (C) 2026 Stephen Cummins | GPL-3.0-only |

## Third-party material

| What | Licence | Where |
| --- | --- | --- |
| Barlow Semi Condensed (The Barlow Project Authors), the UI face | SIL OFL 1.1 | `assets/fonts/BarlowSemiCondensed-*.ttf`, `assets/fonts/Barlow-OFL.txt` |
| Terminus (Dimitar Toshkov Zhekov), an alternative font set | SIL OFL 1.1 | `assets/fonts/ter-u*.bdf`, `assets/fonts/Terminus-LICENSE.txt` |
| JieLi AC79 SDK: `uboot.boot`, `cfg_tool.bin`, `eq_cfg_hw.bin` are read from your SDK checkout at build time and placed in the package; no SDK files are in this tree | Apache-2.0 | <https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK> |
| The emulator's screen reader on the site: `words.js` and `tts.js` from [fm1-reader](https://github.com/charlesvestal/fm1-reader) (Copyright (c) 2026 Charles Vestal) | MIT | `emu/reader/` on the site, with fm1-reader's `LICENSE`; not in this tree: `tools/get_reader.sh` fetches the commit `web/emu/reader.pin` names, and `web/emu/build.sh` checks each file against the pin's sums |
| The emulator's voice on the site: [eSpeak NG](https://github.com/espeak-ng/espeak-ng), English only, compiled to WebAssembly, with fm1-reader's `tts_worker.js`, its wrapper `tts_wrap.c` and its build script | GPL-3.0-or-later | `emu/reader/tts_worker.js` and `emu/reader/espeak/` on the site, with eSpeak NG's licence; `emu/reader/SOURCE.txt` names the commits of both sources. Fetched and checked as above |

## MIT notice (Pluck)

```text
pluck.c: Copyright (c) 2020 Electrosmith, Corp, Emilie Gillet; 2026 M-VAVE.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## Trademarks

"Felucca" and "Hügelton Instruments" are names of Hügelton Instruments.
"M-VAVE", "FM-1" and "SMK-37" are trademarks of their respective owners. AISLING is independent firmware; it is
not affiliated with, endorsed by or supported by any of them.

## Radio

NACHTFAHRT, like DUB-1 and AMB-1, never enables the Bluetooth / Wi-Fi radio of the hardware.
