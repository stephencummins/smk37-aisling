/* SPDX-License-Identifier: GPL-3.0-only */
/* The M-VAVE SMK-37 Elite's SARADC: the three analog muxes' outputs, polled, 10 bit.
 *
 * From genkigenki's fm1-to-smk37 guide, 01-hardware.md (CC BY-NC 4.0; LICENSING.md): mux A on PA8 (ch1), mux B on
 * PC9 (ch8), mux C on PA10 (ch2), all three addressed by bits 5..3 of the 595 word (smk_input.h's smk_mux). Mux C:
 * 0..3 the faders, 4 the pitch wheel, 5 the mod wheel, 6 the pedal, 7 the battery. And the timing: CON 0x400D, where
 * fm1_adc.h's 0xF04E takes ~200 us a conversion, too slow for the pads at one a 100 us tick. The read sequence is
 * fm1_adc.h's (Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments) with that CON.
 *
 * Stage 4: the scan. All three muxes at all eight addresses, from the 10 kHz timer ISR around the matrix scan
 * (main_fm1.c): four ticks an address, 32 a scan (3.2 ms). The tick that latches a new address starts nothing (the
 * muxes settle); each of the next three starts one conversion AFTER the 595 word is out, and the tick after reads
 * it BEFORE its own word goes out. So no conversion ever spans a change of address, and the ISR never waits.
 *   address:  0      1      2      3      4       5      6      7
 *   mux A:    pad 1  pad 2  pad 3  pad 0  pad 4   pad 7  pad 5  pad 6     (the top row, left to right 0..7)
 *   mux B:    pad 9  pad 10 pad 11 pad 8  pad 12  pad 15 pad 13 pad 14    (the bottom row, 8..15)
 *   mux C:    fdr 1  fdr 2  fdr 3  fdr 0  pitch   mod    pedal  battery
 * A pad: its resting level is learned (the first readings, then tracked slowly while it is up); down at rest + 24,
 * up after two readings under rest + 12 (the guide's numbers). smk_pads_rest() learns every pad's rest again and
 * lets them all go: the way out if one ever sticks. A fader or wheel: the median of three, smoothed, +-1 held.
 * The pins are inputs only (PA8, PC9, PA10, analog): nothing here drives a pin. The pads' LEDs (PC10; SPI1 group
 * B, whose clock would come out on PC9, mux B's output) are smk_rgb.h's: PC9 stays this header's analog input.
 * PB1 (the charger's status) is no ADC input here and is never touched.
 *
 *   smk_adc_init()      PA8, PC9, PA10 analog inputs, no pulls
 *   smk_an_pre()        (ISR, before fm1_input_tick) takes the last reading; returns the address to latch
 *   smk_an_post()       (ISR, after it) starts the next
 *   smk_an              the readings: pads held, faders, wheels, pedal, battery
 *   smk_adc_read(ch)    0..1023, -1 on timeout: waits; not for use once the scan runs
 *
 * Host tests replace the hardware access: SMK_GPIO(port, reg), SMK_REG(address) (smk_reg.h). */
#pragma once
#include <stdint.h>
#include "smk_reg.h"
#include "smk_pins.h"

#define SMK_ADC_CON SMK_REG(0x13100u)
#define SMK_ADC_RES SMK_REG(0x13104u)
#define SMK_WLA_CON0 SMK_REG(0x11900u)
enum { SMK_ADC_MUXA = 1, SMK_ADC_MUXC = 2, SMK_ADC_MUXB = 8 };            /* channels */
enum { SMK_MUX_PITCH = 4, SMK_MUX_MOD = 5, SMK_MUX_PEDAL = 6, SMK_MUX_BATT = 7 };   /* mux C's addresses */

static void smk_adc_init(void) { smk_pins_setup(SMK_OWN_ADC); }

static int32_t smk_adc_read(uint32_t ch)
{
    uint32_t t, v;
    SMK_ADC_CON = 0;
    if (SMK_WLA_CON0 & (1u << 14))
        SMK_WLA_CON0 &= ~(1u << 14);
    SMK_ADC_CON = 0x400Du | ((ch & 0xFu) << 8);
    SMK_ADC_CON |= 0x10u;
    SMK_ADC_CON |= 0x40u;
    for (t = 0; t < 20000u; t++)
        if (SMK_ADC_CON & 0x80u)
            break;
    v = SMK_ADC_RES & 0x3FFu;
    SMK_ADC_CON = 0x40u;
    SMK_ADC_CON = 0;
    return t == 20000u ? -1 : (int32_t)v;
}

/* ------------------------------------------------------------------ stage 4: the scan --- */
#define SMK_NPAD 16u
#define SMK_NFADER 4u
#define SMK_PAD_ON 24                      /* over its rest: down */
#define SMK_PAD_OFF 12                     /* under rest + this, twice: up */
#define SMK_AN_SKIP 4u                     /* scans before anything is believed (the first conversions) */
enum { SMK_AN_PITCH = 4, SMK_AN_MOD = 5, SMK_AN_NFILT = 6 };   /* smk_an.pos: the faders 0..3, then the wheels */
static const uint8_t SMK_AN_CH[3] = {SMK_ADC_MUXA, SMK_ADC_MUXB, SMK_ADC_MUXC};
static const uint8_t SMK_AN_LINE[8] = {1, 2, 3, 0, 4, 7, 5, 6};   /* the pad of its row (the fader, 0..3) at an address */

static volatile struct {
    uint16_t raw[3][8];                    /* the last reading: mux A, B, C by address */
    uint16_t pads;                         /* down: bit p = pad p (0..7 the top row, 8..15 the bottom) */
    uint16_t pos[SMK_AN_NFILT];            /* 0..1023, smoothed: faders 0..3, pitch, mod */
    uint16_t pedal, batt;                  /* as read */
    uint32_t scans, misses;                /* whole scans; conversions not done in their tick */
} smk_an;
static struct {
    int32_t rest16[SMK_NPAD];              /* a pad's resting level, x16 */
    uint8_t low[SMK_NPAD], known[SMK_NPAD];
    uint16_t h[SMK_AN_NFILT][3];           /* the last three readings */
    int32_t f16[SMK_AN_NFILT];
    uint8_t hn[SMK_AN_NFILT];
    uint8_t step;                          /* 0..31: address << 2 | phase */
    uint8_t busy;                          /* 1 + mux << 3 + address of the conversion started (0: none) */
} smk__an;

static void smk__pad(uint32_t p, int32_t v)
{
    int32_t *r = &smk__an.rest16[p];
    const uint16_t m = (uint16_t)(1u << p);
    if (!smk__an.known[p]) {
        *r = v * 16;
        smk__an.known[p] = 1;
        smk_an.pads &= (uint16_t)~m;
    } else if (!(smk_an.pads & m)) {
        if (v * 16 >= *r + SMK_PAD_ON * 16) {
            smk_an.pads |= m;
            smk__an.low[p] = 0;
        } else {
            *r += (v * 16 - *r) / 64;
        }
    } else if (v * 16 < *r + SMK_PAD_OFF * 16) {
        if (++smk__an.low[p] >= 2u)
            smk_an.pads &= (uint16_t)~m;
    } else {
        smk__an.low[p] = 0;
    }
}

static void smk__pos(uint32_t i, uint32_t v)
{
    uint16_t *h = smk__an.h[i];
    uint32_t a, b, c, med;
    int32_t out;
    h[0] = h[1], h[1] = h[2], h[2] = (uint16_t)v;
    if (smk__an.hn[i] < 3u) {              /* the first three: it starts where it is */
        if (++smk__an.hn[i] == 3u) {
            smk__an.f16[i] = (int32_t)v * 16;
            smk_an.pos[i] = (uint16_t)v;
        }
        return;
    }
    a = h[0], b = h[1], c = h[2];
    med = a > b ? (b > c ? b : a > c ? c : a) : (a > c ? a : b > c ? c : b);
    smk__an.f16[i] += ((int32_t)med * 16 - smk__an.f16[i]) / 4;
    out = (smk__an.f16[i] + 8) / 16;
    if (out > (int32_t)smk_an.pos[i] + 1 || out + 1 < (int32_t)smk_an.pos[i] || out == 0 || out == 1023)
        smk_an.pos[i] = (uint16_t)out;
}

/* every pad's rest learned again from its next reading, and all let go */
static void smk_pads_rest(void)
{
    uint32_t p;
    for (p = 0; p < SMK_NPAD; p++)
        smk__an.known[p] = 0;
    smk_an.pads = 0;
}

static uint32_t smk_an_pre(void)
{
    if (smk__an.busy) {
        const uint32_t mux = (smk__an.busy - 1u) >> 3, a = (smk__an.busy - 1u) & 7u;
        smk__an.busy = 0;
        if (SMK_ADC_CON & 0x80u) {
            const uint32_t v = SMK_ADC_RES & 0x3FFu;
            smk_an.raw[mux][a] = (uint16_t)v;
            if (smk_an.scans >= SMK_AN_SKIP) {
                if (mux < 2u)
                    smk__pad(SMK_AN_LINE[a] + 8u * mux, (int32_t)v);
                else if (a < 4u)
                    smk__pos(SMK_AN_LINE[a], v);
                else if (a < 6u)
                    smk__pos(a, v);
                else if (a == SMK_MUX_PEDAL)
                    smk_an.pedal = (uint16_t)v;
                else
                    smk_an.batt = (uint16_t)v;
            }
        } else {
            smk_an.misses++;
        }
        SMK_ADC_CON = 0x40u;
        SMK_ADC_CON = 0;
    }
    return smk__an.step >> 2;
}

static void smk_an_post(void)
{
    const uint32_t a = smk__an.step >> 2, ph = smk__an.step & 3u;
    if (ph) {
        SMK_ADC_CON = 0;
        if (SMK_WLA_CON0 & (1u << 14))
            SMK_WLA_CON0 &= ~(1u << 14);
        SMK_ADC_CON = 0x400Du | ((uint32_t)SMK_AN_CH[ph - 1u] << 8);
        SMK_ADC_CON |= 0x10u;
        SMK_ADC_CON |= 0x40u;
        smk__an.busy = (uint8_t)(1u + ((ph - 1u) << 3) + a);
    }
    smk__an.step = (uint8_t)((smk__an.step + 1u) & 31u);
    if (!smk__an.step)
        smk_an.scans++;
}
