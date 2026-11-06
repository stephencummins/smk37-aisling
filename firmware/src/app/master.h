/* SPDX-License-Identifier: GPL-3.0-only */
/* MASTER's knob (10 bits) to the engine's gain (Q12).
 * (0.5) A cube, so the quiet end has room: the old square's 15% of the knob (91/4096, about -33 dB) now sits at 28%,
 * and full is where it was (4084, against 4088).
 * A test build (--release X.Y-quiet) makes no sound whatever the knob says: the engine still runs, so its CPU figures
 * are real. */
#ifndef AISLING_MASTER_H
#define AISLING_MASTER_H
#include <stdint.h>
#ifdef AISLING_QUIET_TEST
#define MASTER_START 0u
static inline uint32_t master_curve(uint32_t k10) { (void)k10; return 0u; }
#else
#define MASTER_START 2048u
static inline uint32_t master_curve(uint32_t k10) { return (k10 * k10 * k10) >> 18; }
#endif
/* A MASTER that is an encoder (the SMK-37's K1): it starts at the knob position nearest MASTER_START from below
 * (812 for 2048), and a detent moves it MASTER_ENC_STEP of 1023 (43 detents end to end) */
#define MASTER_ENC_STEP 24
static inline uint32_t master_enc_start(void)
{
    uint32_t k = 1023u;
    while (k && master_curve(k) > MASTER_START)
        k--;
    return k;
}
static inline uint32_t master_enc_turn(uint32_t k10, int32_t detents)
{
    int32_t k = (int32_t)k10 + detents * MASTER_ENC_STEP;
    return k < 0 ? 0u : k > 1023 ? 1023u : (uint32_t)k;
}
#endif
