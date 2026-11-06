/* SPDX-License-Identifier: GPL-3.0-only */
/* The M-VAVE SMK-37 Elite's keys, buttons, knobs and button LEDs: hal/fm1_input.h's interface on the SMK's
 * matrix, so the app above it (panel.c, plat_fm1.c, main_fm1.c) runs unchanged.
 *
 * The hardware facts below (the matrix codes, the pins, the 595 word, the LED groups) are from genkigenki's
 * fm1-to-smk37 guide, 01-hardware.md (CC BY-NC 4.0; LICENSING.md). The quadrature decoder is fm1_input.h's
 * (Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments), typed again here unchanged but for
 * the frame count, so the FM-1's build stays as it was.
 *
 * The matrix: 16 columns by 8 rows. A code is col << 4 | row. Four 74HC595s hold a 32-bit word:
 *   bits 31..24  columns 0..7, active low, one at a time
 *   bits 23..16  columns 8..15
 *   bits 15..0   the button LED lines, active low, except bits 5..3: the analog mux's address
 * clocked out on PA4 (data) and PA3 (clock), latched by a pulse on PA1; PA0 (the 595s' OE) is held low.
 * Rows 0..7 = PA2 PA5 PA6 PA7 PA9 PH6 PH8 PH9, pulled up, low = closed: rows 0-4 the keys, 5-6 the buttons,
 * 7 the knobs. The rows are only ever inputs; PB1 (the charger's status: held low 8 s it resets the chip)
 * and port C are never touched here. The pins are set up from smk_pins.h's table, and named from it.
 *
 * Ids, chosen so the FM-1's panel map (panel.c PANEL_DEFAULT) and its OCT- + OCT+ checks hold as they are:
 *   buttons 0..13  the FM-1's matrix ids: the SMK button that does that job (SMK_BTN below)
 *   buttons 14..17 the SMK's four more (KNOB BANK, FADER BANK, <, >): fm1_in.extra, never in fm1_in.buttons
 *   notes   bit n = key 5 + n: F3..G5, the 27 the app plays (fm1_in.notes, as the FM-1's)
 *   outer   the 5 keys below and the 5 above those: fm1_in.outer, bits 0..4 = C3..E3, 5..9 = G#5..C6; silent
 *   encoders 0..6  the FM-1's encoder ids (K2..K8), 7 = K1 = MASTER (SMK_ENC_MASTER), outside the panel's
 *
 * A key has two contacts: c1 closes early in the stroke, c2 near the bottom. It sounds when c2 closes and
 * stops when c1 opens (no velocity). Each contact and button: an integrating debounce of SMK_DEBOUNCE frames.
 *
 *   fm1_input_init();
 *   fm1_input_tick() from the 10 kHz timer ISR: one column a tick, a frame (16 columns) every 1.6 ms
 *
 * Button LEDs: 19, on two commons (the second USB PHY's pads, as GPIO: SMK_LED_COM), 16 lines each in the
 * word's low half. The groups take turns a frame each. smk_leds() lights them by button id; a dim one is lit on
 * one tick in fm1_led_dim_div, inside its group's frame (so ~2.5 kHz, no flicker).
 *
 * NOT YET CHECKED ON THE DEVICE (stage 3; each is one place below):
 *   - the order the word's bits go out (smk__sr_bit): the guide's DMA sends its 4 bytes from memory, not
 *     saying which end the 595s take first. Wrong: columns read keys from other columns, or nothing.
 *   - which way a knob turns + (SMK_ENC_FLIP).
 *   - the LED commons' register and its other bits (SMK_LED_COM, SMK_COM_BASE), and the LED table, which the
 *     guide marks verified only for the LEDs its own port drives.
 *
 * The analog muxes (smk_adc.h) take their address from the word too: smk_mux, latched with every column. It stays 7
 * (mux C's battery) until stage 4 reads the pads, faders and wheels.
 *
 * Host tests replace the hardware access: SMK_GPIO(port, reg), SMK_SR_PUT(v), SMK_LED_COM, SMK_IRQ_OFF/ON. */
#pragma once
#include <stdint.h>
#include "smk_reg.h"
#include "smk_pins.h"

#ifndef SMK_SR_PUT                         /* port A's output, the 595 pins (only this file writes port A) */
#define SMK_SR_PUT(v) (SMK_GPIO(FM1_PA, FM1_OUT) = (v))
#endif
#ifndef SMK_LED_COM                        /* bit 17 = group 0's common, bit 18 = group 1's, low = on */
#define SMK_LED_COM (*(volatile uint32_t *)0x16A0Cu)
#endif
#ifndef SMK_IRQ_OFF
#define SMK_IRQ_OFF() __asm__ volatile("cli" ::: "memory")
#define SMK_IRQ_ON() __asm__ volatile("csync\n\tsti" ::: "memory")
#endif
#ifndef SMK_ENC_FLIP
#define SMK_ENC_FLIP 0                     /* 1: swap every knob's A and B (+ the other way) */
#endif

#define SMK_COM_BASE 0x1000Fu              /* the register's other bits, kept as the guide writes them */
#define SMK_COM_OFF (SMK_COM_BASE | 3u << 17)
#define SMK_COM_ON(g) (SMK_COM_BASE | ((g) ? 1u << 17 : 1u << 18))

#define SMK_NCOL 16u
#define SMK_NROW 8u
#define SMK_DEBOUNCE 4u                    /* frames (1.6 ms each) */
#define SMK_REST_FRAMES 28u                /* ~45 ms still = a detent position (the FM-1's 40 of 1.1 ms) */
#define SMK_NKEY 37u                       /* C3..C6 */
#define SMK_NOTE_LO 5u                     /* key 5, F3, is note 0 */
#define SMK_NBTN 18u
#define SMK_NSW (SMK_NBTN + 2u * SMK_NKEY) /* debounced switches: buttons, then every key's c1, then c2 */
#define FM1_NENC 8u
#define SMK_ENC_MASTER 7u

/* the buttons by id: 0..13 the FM-1's matrix ids (the FM-1 label each stands for, which PANEL_DEFAULT maps) */
static const uint8_t SMK_BTN[SMK_NBTN] = {
    0x66,   /*  0 OCTAVE-      OCT- */
    0x36,   /*  1 OCTAVE+      OCT+ */
    0x85,   /*  2 FX           FX */
    0x25,   /*  3 SCALE        SEL */
    0x45,   /*  4 CHORD        ENV */
    0x65,   /*  5 NOTE REPEAT  LFO */
    0x55,   /*  6 PARA         EDIT */
    0x15,   /*  7 GLOBE        GLO */
    0x95,   /*  8 PATCH        HOME */
    0x86,   /*  9 BT           SAVE */
    0x75,   /* 10 ARP          ARP */
    0x35,   /* 11 SEQ          SEQ */
    0x16,   /* 12 SEQ PLAY     PLAY */
    0x46,   /* 13 SEQ REC      REC */
    0x26,   /* 14 KNOB BANK */
    0x76,   /* 15 FADER BANK */
    0x96,   /* 16 < */
    0x56,   /* 17 > */
};
/* each button's LED: group << 4 | line (bit of the word) */
static const uint8_t SMK_BTN_LED[SMK_NBTN] = {
    0x0D, 0x1D, 0x18, 0x09, 0x08, 0x01, 0x19, 0x0B, 0x11, 0x0A, 0x02, 0x1A, 0x1C, 0x1E, 0x0C, 0x12, 0x00, 0x10,
};
/* key k (0 = C3 .. 36 = C6): c1, c2 */
static const uint8_t SMK_KEY[SMK_NKEY][2] = {
    {0x10, 0x52}, {0x20, 0x72}, {0x30, 0x92}, {0x40, 0xB2}, {0x50, 0xD2}, {0x60, 0xF2}, {0x70, 0xE2},
    {0x80, 0xC2}, {0x90, 0xA2}, {0xA0, 0x82}, {0xC0, 0x62}, {0xD0, 0x42}, {0xE0, 0x22}, {0xF0, 0x12},
    {0xB0, 0x32}, {0x11, 0x33}, {0x21, 0x43}, {0x31, 0x53}, {0x41, 0x63}, {0x51, 0x73}, {0x61, 0x83},
    {0x71, 0x93}, {0x81, 0xA3}, {0x91, 0xB3}, {0xA1, 0xC3}, {0xB1, 0xD3}, {0xC1, 0xE3}, {0xD1, 0x23},
    {0xE1, 0x13}, {0xF1, 0x14}, {0x64, 0x24}, {0x74, 0x34}, {0x84, 0x44}, {0x94, 0x54}, {0xA4, 0xD4},
    {0xB4, 0xF4}, {0xC4, 0xE4},
};
/* encoder e: A, B (row 7) */
static const uint8_t SMK_ENC[FM1_NENC][2] = {
    {0x37, 0x47},   /* 0 K2  SELECT */
    {0x17, 0x27},   /* 1 K3  ALGORITHM */
    {0x57, 0x67},   /* 2 K5  KNOB 1 */
    {0x77, 0x87},   /* 3 K6  KNOB 2 */
    {0xD7, 0xE7},   /* 4 K7  KNOB 3 */
    {0xF7, 0x07},   /* 5 K8  KNOB 4 */
    {0xB7, 0xC7},   /* 6 K4  PRESETS */
    {0x97, 0xA7},   /* 7 K1  MASTER */
};

static volatile struct {
    uint32_t notes;              /* debounced: bit n = note n (0 = F3 .. 26 = G5) */
    uint32_t buttons;            /* debounced: bit i = button i (0..13) */
    uint32_t pressed, released;  /* button edges since the last fm1_input_edges() */
    uint32_t notes_pressed;      /* note press edges since the last fm1_input_note_edges() */
    uint32_t outer;              /* the outer keys, held */
    uint32_t extra;              /* buttons 14..17, held: bit i - 14 */
    uint32_t sw[(SMK_NSW + 31u) / 32u];   /* debounced switch states */
    uint8_t raw[SMK_NCOL];       /* last frame, rows, 1 = closed */
    uint8_t cnt[SMK_NSW];
    uint8_t enc_prev[FM1_NENC], enc_last[FM1_NENC];
    uint8_t enc_rest[FM1_NENC];  /* learned rest (detent) states, bit per state */
    uint8_t enc_still[FM1_NENC]; /* frames since the last state change */
    int8_t enc_sub[FM1_NENC];    /* net transitions since the last rest state */
    int8_t enc_dir[FM1_NENC];    /* the last valid transition: +1 / -1 (0: none yet) */
    int16_t enc_steps[FM1_NENC]; /* + = clockwise */
    uint32_t frames;
} fm1_in;
static volatile uint16_t smk_led[2], smk_led_dim[2];   /* per group: bit = line, 1 = lit (smk_leds) */
static volatile uint8_t fm1_led_dim_div;
static volatile uint8_t smk_mux = 7u;   /* the analog muxes' address, latched with the next column (smk_adc.h) */
static uint8_t smk__col;         /* the column latched by the last tick: its rows are read on this one */
static uint8_t smk__grp;         /* the LED group lit this frame */
static uint8_t smk__dim;         /* ticks to the next one that lights the dim LEDs */

/* the word for column c, LED lines lit (bit = line), mux address */
static uint32_t smk__word(uint32_t c, uint32_t lit, uint32_t mux)
{
    uint32_t col = c < 8u ? 0x01000000u << c : 0x00010000u << (c - 8u);
    return (0xFFFF0000u & ~col) | (~lit & 0xFFC7u) | (mux & 7u) << 3;
}

/* bit i (0 first) of the word as it goes out: its four bytes from the lowest (the guide's DMA sends the word
 * from memory, little-endian), each MSB first. ASSUMED (see the top). */
static uint32_t smk__sr_bit(uint32_t w, uint32_t i)
{
    return (w >> ((i & ~7u) + 7u - (i & 7u))) & 1u;
}

/* shift the word out and latch it. Plain writes, no read-modify-write: nothing else writes port A after
 * fm1_input_init, and this ISR is the highest, so one read at the start holds (and it is a third the SFR
 * traffic: 32 bits a tick, twice the FM-1's) */
static void smk__sr_word(uint32_t w)
{
    uint32_t o = SMK_GPIO(FM1_PA, FM1_OUT) &
                 ~(1u << SMK_SR_OE | 1u << SMK_SR_LATCH | 1u << SMK_SR_CLK | 1u << SMK_SR_DATA), i;
    for (i = 0; i < 32u; i++) {
        uint32_t d = o | smk__sr_bit(w, i) << SMK_SR_DATA;
        SMK_SR_PUT(d);                                 /* data, clock low */
        SMK_SR_PUT(d | 1u << SMK_SR_CLK);              /* the 595s shift on the rising edge */
    }
    SMK_SR_PUT(o | 1u << SMK_SR_LATCH);                /* latch */
    SMK_SR_PUT(o);
}

/* the rows, 1 = closed: bit r = row r */
static uint32_t smk__rows(void)
{
    uint32_t a = SMK_GPIO(FM1_PA, FM1_IN), h = SMK_GPIO(FM1_PH, FM1_IN);
    return ~((a >> SMK_ROW0 & 1u) | (a >> SMK_ROW1 & 1u) << 1 | (a >> SMK_ROW2 & 1u) << 2 | (a >> SMK_ROW3 & 1u) << 3 |
             (a >> SMK_ROW4 & 1u) << 4 | (h >> SMK_ROW5 & 1u) << 5 | (h >> SMK_ROW6 & 1u) << 6 |
             (h >> SMK_ROW7 & 1u) << 7) & 0xFFu;
}

static void fm1_input_init(void)
{
    uint32_t i;
    smk_pins_setup(SMK_OWN_INPUT);                     /* the rows inputs, pulled up; then the 595 pins, low */
    SMK_LED_COM = SMK_COM_OFF;
    smk__sr_word(0xFFFFFFFFu & ~(7u << 3));            /* no column, no LED */
    smk__col = (uint8_t)(SMK_NCOL - 1u);               /* the first tick latches column 0 */
    for (i = 0; i < FM1_NENC; i++)
        fm1_in.enc_prev[i] = fm1_in.enc_last[i] = 0xFF;   /* seeded by the first frame */
}

static uint32_t smk__closed(uint32_t code)
{
    return (fm1_in.raw[code >> 4] >> (code & 15u)) & 1u;
}

/* switch s, debounced: 1 = closed */
static uint32_t smk__sw(uint32_t s, uint32_t closed)
{
    volatile uint8_t *c = &fm1_in.cnt[s];
    uint32_t m = 1u << (s & 31u);
    if (closed) {
        if (*c < SMK_DEBOUNCE)
            (*c)++;
    } else if (*c) {
        (*c)--;
    }
    if (*c == SMK_DEBOUNCE)
        fm1_in.sw[s >> 5] |= m;
    else if (*c == 0)
        fm1_in.sw[s >> 5] &= ~m;
    return (fm1_in.sw[s >> 5] & m) != 0;
}

static void smk__frame(void)
{
    uint32_t i, k, e, extra = 0;
    for (i = 0; i < SMK_NBTN; i++) {
        uint32_t on = smk__sw(i, smk__closed(SMK_BTN[i]));
        if (i >= 14u)
            extra |= on << (i - 14u);
        else if (on != ((fm1_in.buttons >> i) & 1u)) {
            fm1_in.buttons ^= 1u << i;
            if (on)
                fm1_in.pressed |= 1u << i;
            else
                fm1_in.released |= 1u << i;
        }
    }
    fm1_in.extra = extra;
    for (k = 0; k < SMK_NKEY; k++) {
        uint32_t c1 = smk__sw(SMK_NBTN + k, smk__closed(SMK_KEY[k][0]));
        uint32_t c2 = smk__sw(SMK_NBTN + SMK_NKEY + k, smk__closed(SMK_KEY[k][1]));
        uint32_t n = k - SMK_NOTE_LO;                  /* (wraps below F3) */
        if (n < 27u) {
            uint32_t m = 1u << n, held = (fm1_in.notes & m) != 0;
            if (c2 && !held) {
                fm1_in.notes |= m;
                fm1_in.notes_pressed |= m;
            } else if (!c1 && !c2 && held) {
                fm1_in.notes &= ~m;
            }
        } else {
            uint32_t m = 1u << (k < SMK_NOTE_LO ? k : k - 27u), held = (fm1_in.outer & m) != 0;
            if (c2 && !held)
                fm1_in.outer |= m;
            else if (!c1 && !c2 && held)
                fm1_in.outer &= ~m;
        }
    }
    for (e = 0; e < FM1_NENC; e++) {               /* fm1_input.h's quadrature decoder + detents, unchanged */
        uint32_t a = smk__closed(SMK_ENC[e][SMK_ENC_FLIP]), b = smk__closed(SMK_ENC[e][1 - SMK_ENC_FLIP]);
        uint32_t cur = a << 1 | b;
        uint32_t idx;
        volatile int8_t *sub = &fm1_in.enc_sub[e];
        if (cur != fm1_in.enc_last[e]) {
            fm1_in.enc_last[e] = (uint8_t)cur;
            fm1_in.enc_still[e] = 0;
        }
        if (fm1_in.enc_prev[e] == 0xFF) {          /* first frame: the knob rests here */
            fm1_in.enc_prev[e] = (uint8_t)cur;
            fm1_in.enc_rest[e] = (uint8_t)(1u << cur);
        }
        if (fm1_in.enc_still[e] < 255u && ++fm1_in.enc_still[e] == SMK_REST_FRAMES) {
            /* learn detent states: one state, or a complementary pair (00/11 or 01/10) */
            uint32_t r = fm1_in.enc_rest[e], bit = 1u << cur, comp = 1u << (cur ^ 3u);
            if (!(r & bit))
                fm1_in.enc_rest[e] = (uint8_t)(r == comp ? (r | bit) : bit);
        }
        if (cur == fm1_in.enc_prev[e])
            continue;
        idx = (uint32_t)fm1_in.enc_prev[e] << 2 | cur;
        if ((0x4182u >> idx) & 1u) {
            (*sub)++;
            fm1_in.enc_dir[e] = 1;
        } else if ((0x2814u >> idx) & 1u) {
            (*sub)--;
            fm1_in.enc_dir[e] = -1;
        } else {                                   /* both lines changed: a state skipped (a flick) */
            *sub = (int8_t)(*sub + 2 * fm1_in.enc_dir[e]);
        }
        fm1_in.enc_prev[e] = (uint8_t)cur;
        if ((fm1_in.enc_rest[e] >> cur) & 1u) {    /* back on a detent */
            if (*sub >= 2)
                fm1_in.enc_steps[e]++;
            else if (*sub <= -2)
                fm1_in.enc_steps[e]--;
            *sub = 0;
        }
    }
    fm1_in.frames++;
}

/* one column per call, from the timer ISR: read the rows of the column latched last tick, latch the next.
 * At column 0 the other LED group takes over: both commons off while its lines go out, then its own on. */
static void fm1_input_tick(void)
{
    uint32_t p = smk__col, n = p + 1u == SMK_NCOL ? 0u : p + 1u, div = fm1_led_dim_div, lit;
    fm1_in.raw[p] = (uint8_t)smk__rows();
    if (n == 0u) {
        smk__grp ^= 1u;
        SMK_LED_COM = SMK_COM_OFF;
    }
    lit = smk_led[smk__grp];
    if (div == 0u)
        smk__dim = 0;
    else if (smk__dim == 0u) {
        lit |= smk_led_dim[smk__grp];
        smk__dim = (uint8_t)(div - 1u);
    } else
        smk__dim--;
    smk__sr_word(smk__word(n, lit, smk_mux));
    if (n == 0u)
        SMK_LED_COM = SMK_COM_ON(smk__grp);
    smk__col = (uint8_t)n;
    if (n == 0u)
        smk__frame();
}

/* light buttons by id (bit i = button i, 0..17): lit, else dim if in `dim`, on one tick in div (0: none) */
static void smk_leds(uint32_t lit, uint32_t dim, uint32_t div)
{
    uint16_t nl[2] = {0, 0}, dl[2] = {0, 0};
    uint32_t i;
    for (i = 0; i < SMK_NBTN; i++) {
        uint32_t q = SMK_BTN_LED[i];
        uint16_t m = (uint16_t)(1u << (q & 15u));
        if ((lit >> i) & 1u)
            nl[q >> 4] |= m;
        else if ((dim >> i) & 1u)
            dl[q >> 4] |= m;
    }
    smk_led[0] = nl[0];                            /* built off-line, then copied: no dark gap to catch */
    smk_led[1] = nl[1];
    smk_led_dim[0] = dl[0];
    smk_led_dim[1] = dl[1];
    fm1_led_dim_div = (uint8_t)div;
}

/* main-loop critical section against fm1_input_tick (main loop only: it re-enables interrupts) */
static inline uint32_t fm1__lock(void)
{
    SMK_IRQ_OFF();
    return 0;
}
static inline void fm1__unlock(uint32_t v)
{
    (void)v;
    SMK_IRQ_ON();
}

/* detent steps turned since the last call, + = clockwise */
static int32_t fm1_enc_take(uint32_t e)
{
    uint32_t k = fm1__lock();
    int32_t s = fm1_in.enc_steps[e];
    fm1_in.enc_steps[e] = 0;
    fm1__unlock(k);
    return s;
}

static uint32_t fm1_input_edges(uint32_t *released)
{
    uint32_t k = fm1__lock();
    uint32_t p = fm1_in.pressed;
    if (released)
        *released = fm1_in.released;
    fm1_in.pressed = fm1_in.released = 0;
    fm1__unlock(k);
    return p;
}

static uint32_t fm1_input_note_edges(void)
{
    uint32_t k = fm1__lock();
    uint32_t p = fm1_in.notes_pressed;
    fm1_in.notes_pressed = 0;
    fm1__unlock(k);
    return p;
}
