/* SPDX-License-Identifier: GPL-3.0-only */
/* What the board has, so that features are written against what is there rather than which board it is.
 *
 *   AISLING_BOARD_SMK37   0: the FM-1 (the default; its build is byte-identical to one without this file)
 *                         1: the M-VAVE SMK-37 Elite (tools/build.py --board smk37)
 *
 * With a CAP_ at 0 the compiler drops the code behind it, so the FM-1 build carries nothing of the SMK's.
 * The counts are the boards' controls (the SMK-37 Elite's from genkigenki's fm1-to-smk37 guide; LICENSING.md). */
#ifndef BOARD_CAPS_H
#define BOARD_CAPS_H

#ifndef AISLING_BOARD_SMK37
#define AISLING_BOARD_SMK37 0
#endif

#if AISLING_BOARD_SMK37
#define CAP_KEYS        37      /* C3..C6; AISLING plays the middle 27 (F3..G5) */
#define CAP_OUTER_KEYS  10      /* the 5 below and 5 above those 27 */
#define CAP_PADS        16      /* the effects grid */
#define CAP_FADERS      4       /* the mix */
#define CAP_WHEELS      2       /* pitch (bends the Lead), mod (opens the filters) */
#define CAP_RGB         1       /* the pads' WS2812 LEDs */
#else
#define CAP_KEYS        27
#define CAP_OUTER_KEYS  0
#define CAP_PADS        0
#define CAP_FADERS      0
#define CAP_WHEELS      0
#define CAP_RGB         0
#endif

#if CAP_KEYS - CAP_OUTER_KEYS != 27
#error "board_caps.h: AISLING plays 27 keys"
#endif

#endif
