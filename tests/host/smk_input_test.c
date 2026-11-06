/* SPDX-License-Identifier: GPL-3.0-only */
/* hal/smk37/smk_input.h, the SMK-37 Elite's matrix, on a simulated board: four 74HC595s fed by the header's own
 * shift-out, and keys, buttons and knobs closing contacts at their printed positions.
 *
 * The expected tables here are typed again from genkigenki's fm1-to-smk37 guide (01-hardware.md), by printed
 * name, not copied from the header: a typo in either shows as a mismatch. They check:
 *   - every code is in the matrix and none is used twice
 *   - each printed button reaches the app's B_* through panel.c's PANEL_DEFAULT, the four extras reach none
 *   - each knob reaches its EN_* role (K1: MASTER, outside the panel), clockwise +
 *   - keys: sound on the second contact, stop when the first opens; the middle 27 are notes, the outer 10 not
 *   - a contact closed for less than the debounce does nothing
 *   - the word: one column low at a time, LED lines active low, the mux address in bits 5..3
 *   - the bit order out (ASSUMED in the header until stage 3), and that every word is 32 bits and latched
 *   - the LEDs: each button's lit in its group's frames only, the groups alternating at column 0
 *   - pins never driven: the rows' direction, port B (PB1: the charger's status), port C, the rest of A and H
 *   - MASTER as an encoder (master.h): starts at 812, 24 a detent, clamped
 * -DCONTROL swaps OCT-'s and OCT+'s expected codes: that build must fail. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

static uint32_t fake[8][8];                  /* port, register / 4 */
static uint32_t fake_com;
static void sr_put(uint32_t v);
#define SMK_GPIO(p, r) fake[p][(r) >> 2]
#define SMK_SR_PUT(v) sr_put(v)
#define SMK_LED_COM fake_com
#define SMK_IRQ_OFF() ((void)0)
#define SMK_IRQ_ON() ((void)0)
#include "smk37/smk_input.h"
#include "master.h"
#include "plat.h"
#define section(x) aligned(4)                /* panel.c's .noinit: not a Mach-O section name */
#include "panel.c"
#undef section

static int fails;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL " __VA_ARGS__); printf("\n"); fails++; } } while (0)

/* ---- the board ---- */
static uint8_t mat[16];                      /* closed contacts: bit r of column c */
static uint32_t chain, latched = 0xFFFFFFFFu, nclk, sent[40], nlat, oe_high, bad_words, two_cols;
static int col_now = -1;

static int word_col(uint32_t w)              /* the column driven, -1: none */
{
    int c, found = -1, n = 0;
    for (c = 0; c < 16; c++)
        if (!((w >> (c < 8 ? 24 + c : 16 + c - 8)) & 1u)) {
            found = c;
            n++;
        }
    if (n > 1)
        two_cols++;
    return found;
}

static void inputs(void)                     /* the row pins for the column driven: pulled up, low = closed */
{
    static const uint8_t PIN_A[5] = {2, 5, 6, 7, 9}, PIN_H[3] = {6, 8, 9};
    uint32_t r = col_now < 0 ? 0u : mat[col_now], i;
    uint32_t a = (nlat & 1u) ? 0xFFFFFFFFu : 0u, h = (nlat & 1u) ? 0u : 0xFFFFFFFFu;  /* other bits: noise */
    for (i = 0; i < 5; i++)
        a = (r >> i) & 1u ? a & ~(1u << PIN_A[i]) : a | 1u << PIN_A[i];
    for (i = 0; i < 3; i++)
        h = (r >> (5 + i)) & 1u ? h & ~(1u << PIN_H[i]) : h | 1u << PIN_H[i];
    fake[FM1_PA][FM1_IN >> 2] = a;
    fake[FM1_PH][FM1_IN >> 2] = h;
}

/* the 595s: data PA4 shifts in on PA3's rising edge, PA1's rising edge latches; PA0 (OE) must stay low */
static void sr_put(uint32_t v)
{
    uint32_t old = fake[FM1_PA][0];
    fake[FM1_PA][0] = v;
    if (v & 1u)
        oe_high++;
    if (!(old & 8u) && (v & 8u)) {
        if (nclk < 40u)
            sent[nclk] = (v >> 4) & 1u;
        nclk++;
    }
    if (!(old & 2u) && (v & 2u)) {
        /* the assumption, put another way: the word's bytes from the lowest, each from its top bit */
        uint32_t w = 0, i = 0, b, bit;
        if (nclk != 32u)
            bad_words++;
        for (b = 0; b < 4u; b++)
            for (bit = 8; bit-- > 0;)
                w |= (i < 40u ? sent[i++] : 0u) << (8u * b + bit);
        latched = w;
        chain = w;
        nclk = 0;
        nlat++;
        col_now = word_col(w);
        inputs();
    }
}

static void press(uint32_t code) { mat[code >> 4] |= (uint8_t)(1u << (code & 15u)); inputs(); }
static void release(uint32_t code) { mat[code >> 4] &= (uint8_t)~(1u << (code & 15u)); inputs(); }
static void ticks(int n) { while (n-- > 0) fm1_input_tick(); }
static void frames(int n) { ticks(16 * n); }

static void boot(void)
{
    memset((void *)&fm1_in, 0, sizeof fm1_in);
    memset(mat, 0, sizeof mat);
    smk_leds(0, 0, 0);
    fm1_input_init();
    ticks(1);                                /* the first frame: from here, frames() ends on one */
}

/* ---- the guide's tables, by printed name ---- */
struct btn { const char *name; uint32_t code; int role; uint32_t led; };   /* role -1: none; led = group << 4 | line */
static const struct btn BTN[18] = {
#ifdef CONTROL
    {"OCT-", 0x36, B_OCTDN, 0x0D}, {"OCT+", 0x66, B_OCTUP, 0x1D},
#else
    {"OCT-", 0x66, B_OCTDN, 0x0D}, {"OCT+", 0x36, B_OCTUP, 0x1D},
#endif
    {"ARP", 0x75, B_ARP, 0x02},          {"NOTE REPEAT", 0x65, B_LFO, 0x01}, {"SCALE", 0x25, B_SEL, 0x09},
    {"CHORD", 0x45, B_ENV, 0x08},        {"GLOBE", 0x15, B_GLO, 0x0B},       {"PATCH", 0x95, B_HOME, 0x11},
    {"PARA", 0x55, B_EDIT, 0x19},        {"FX", 0x85, B_FX, 0x18},           {"SEQ", 0x35, B_SEQ, 0x1A},
    {"BT", 0x86, B_SAVE, 0x0A},          {"SEQ PLAY", 0x16, B_PLAY, 0x1C},   {"SEQ REC", 0x46, B_REC, 0x1E},
    {"KNOB BANK", 0x26, -1, 0x0C},       {"FADER BANK", 0x76, -1, 0x12},     {"<", 0x96, -1, 0x00},
    {">", 0x56, -1, 0x10},
};
struct knob { const char *name; uint32_t a, b; int role; };   /* role: EN_*, or NE for MASTER */
static const struct knob KNOB[8] = {
    {"K1", 0x97, 0xA7, NE},       {"K2", 0x37, 0x47, EN_SELECT}, {"K3", 0x17, 0x27, EN_ALGO},
    {"K4", 0xB7, 0xC7, EN_PRESET}, {"K5", 0x57, 0x67, EN_K1},     {"K6", 0x77, 0x87, EN_K2},
    {"K7", 0xD7, 0xE7, EN_K3},     {"K8", 0xF7, 0x07, EN_K4},
};
/* key 0 (C3) .. 36 (C6): first contact / second, as the guide lists them */
static const char KEYS[] =
    "10/52 20/72 30/92 40/b2 50/d2 60/f2 70/e2 80/c2 90/a2 a0/82 c0/62 d0/42 e0/22 f0/12 b0/32 11/33 21/43 "
    "31/53 41/63 51/73 61/83 71/93 81/a3 91/b3 a1/c3 b1/d3 c1/e3 d1/23 e1/13 f1/14 64/24 74/34 84/44 94/54 "
    "a4/d4 b4/f4 c4/e4";
static uint32_t key_c[37][2];

static void parse_keys(void)
{
    const char *p = KEYS;
    int k;
    for (k = 0; k < 37; k++) {
        char *e;
        key_c[k][0] = (uint32_t)strtoul(p, &e, 16);
        key_c[k][1] = (uint32_t)strtoul(e + 1, &e, 16);
        p = e;
    }
}

/* the button id the header gives a printed button: found by pressing it */
static int hal_id(uint32_t code, uint32_t *extra)
{
    uint32_t b, i;
    int id = -1;
    press(code);
    frames(6);
    b = fm1_in.buttons;
    *extra = fm1_in.extra;
    release(code);
    frames(6);
    for (i = 0; i < 14u; i++)
        if ((b >> i) & 1u)
            id = id < 0 ? (int)i : 99;
    return id;
}

static void test_tables(void)
{
    static uint8_t seen[256];
    uint32_t i, all[128], n = 0;
    for (i = 0; i < 18u; i++)
        all[n++] = BTN[i].code;
    for (i = 0; i < 8u; i++) {
        all[n++] = KNOB[i].a;
        all[n++] = KNOB[i].b;
    }
    for (i = 0; i < 37u; i++) {
        all[n++] = key_c[i][0];
        all[n++] = key_c[i][1];
    }
    CHECK(n == 108u, "108 contacts, %u", (unsigned)n);
    for (i = 0; i < n; i++) {
        CHECK(all[i] < 0x100u && (all[i] & 15u) < 8u, "code %02X outside the matrix", (unsigned)all[i]);
        CHECK(!seen[all[i] & 0xFFu], "code %02X used twice", (unsigned)all[i]);
        seen[all[i] & 0xFFu] = 1;
    }
    for (i = 0; i < 37u; i++)                /* the guide's rows: keys 0-4, buttons 5-6, knobs 7 */
        CHECK((key_c[i][0] & 15u) <= 4u && (key_c[i][1] & 15u) <= 4u, "key %u off rows 0-4", (unsigned)i);
    for (i = 0; i < 18u; i++)
        CHECK((BTN[i].code & 15u) == 5u || (BTN[i].code & 15u) == 6u, "%s off rows 5-6", BTN[i].name);
    /* the header's tables, the same contacts */
    for (i = 0; i < 37u; i++)
        CHECK(SMK_KEY[i][0] == key_c[i][0] && SMK_KEY[i][1] == key_c[i][1], "key %u: header %02X/%02X", (unsigned)i,
              SMK_KEY[i][0], SMK_KEY[i][1]);
}

static void test_buttons(void)
{
    uint32_t i, extras = 0;
    panel_init();
    CHECK(memcmp(&panel, &PANEL_DEFAULT, sizeof panel) == 0, "panel_init: not PANEL_DEFAULT");
    boot();
    for (i = 0; i < 18u; i++) {
        uint32_t ex = 0, r, roles = 0;
        int id = hal_id(BTN[i].code, &ex);
        if (BTN[i].role < 0) {
            CHECK(id < 0, "%s: reached button id %d", BTN[i].name, id);
            CHECK(ex != 0u && !(ex & extras) && !(ex & (ex - 1u)), "%s: extra bits %X", BTN[i].name, (unsigned)ex);
            extras |= ex;
            continue;
        }
        CHECK(ex == 0u, "%s: an extra (%X)", BTN[i].name, (unsigned)ex);
        CHECK(id >= 0 && id < 14, "%s: button id %d", BTN[i].name, id);
        if (id < 0 || id >= 14)
            continue;
        for (r = 0; r < NB; r++)             /* plat_buttons: the roles whose panel entry is this id */
            if (panel.btn[r] == (uint8_t)id)
                roles |= 1u << r;
        CHECK(roles == 1u << BTN[i].role, "%s: roles %X, want B_%d", BTN[i].name, (unsigned)roles, BTN[i].role);
    }
    CHECK(extras == 0xFu, "extras %X", (unsigned)extras);
    /* edges: one press, one release, and the OCT- + OCT+ pair main_fm1.c watches */
    {
        uint32_t rel = 0, p;
        fm1_input_edges(0);
        press(0x66);
        press(0x36);
        frames(6);
        p = fm1_input_edges(&rel);
        CHECK(p == 3u && rel == 0u, "OCT- + OCT+: pressed %X released %X", (unsigned)p, (unsigned)rel);
        CHECK((fm1_in.buttons & 3u) == 3u, "OCT- + OCT+ held: %X", (unsigned)fm1_in.buttons);
        release(0x66);
        release(0x36);
        frames(6);
        p = fm1_input_edges(&rel);
        CHECK(p == 0u && rel == 3u, "released: pressed %X released %X", (unsigned)p, (unsigned)rel);
    }
}

static void test_keys(void)
{
    uint32_t k;
    boot();
    for (k = 0; k < 37u; k++) {
        uint32_t c1 = key_c[k][0], c2 = key_c[k][1], want = k >= 5u && k < 32u ? 1u << (k - 5u) : 0u;
        uint32_t wo = k < 5u ? 1u << k : k >= 32u ? 1u << (k - 27u) : 0u;
        fm1_input_note_edges();
        press(c1);                           /* part way down: nothing */
        frames(8);
        CHECK(fm1_in.notes == 0u && fm1_in.outer == 0u, "key %u, first contact: notes %X outer %X", (unsigned)k,
              (unsigned)fm1_in.notes, (unsigned)fm1_in.outer);
        press(c2);                           /* down */
        frames(8);
        CHECK(fm1_in.notes == want && fm1_in.outer == wo, "key %u down: notes %X outer %X", (unsigned)k,
              (unsigned)fm1_in.notes, (unsigned)fm1_in.outer);
        CHECK(fm1_input_note_edges() == want, "key %u: press edge", (unsigned)k);
        release(c2);                         /* rising, still past the first contact: holds */
        frames(8);
        CHECK(fm1_in.notes == want && fm1_in.outer == wo, "key %u half up: notes %X", (unsigned)k,
              (unsigned)fm1_in.notes);
        press(c2);                           /* back down from there: the same note, no new edge */
        frames(8);
        CHECK(fm1_input_note_edges() == 0u, "key %u: re-pressed half way, a new edge", (unsigned)k);
        release(c2);
        release(c1);                         /* up */
        frames(8);
        CHECK(fm1_in.notes == 0u && fm1_in.outer == 0u, "key %u up: notes %X outer %X", (unsigned)k,
              (unsigned)fm1_in.notes, (unsigned)fm1_in.outer);
        CHECK(fm1_in.buttons == 0u && fm1_in.extra == 0u, "key %u pressed a button", (unsigned)k);
    }
    /* debounce, 4 frames (6.4 ms): closed for 3 it is not there; for 4, it is */
    press(key_c[10][0]);
    press(key_c[10][1]);
    frames(3);
    release(key_c[10][1]);
    release(key_c[10][0]);
    frames(8);
    CHECK(fm1_in.notes == 0u && fm1_input_note_edges() == 0u, "a 3-frame glitch sounded");
    press(key_c[10][0]);
    press(key_c[10][1]);
    frames(4);
    CHECK(fm1_in.notes == 1u << 5, "held 4 frames: notes %X", (unsigned)fm1_in.notes);
    release(key_c[10][1]);
    release(key_c[10][0]);
    frames(8);
}

static void knob_state(const struct knob *kn, uint32_t st)
{
    if (st & 2u) press(kn->a); else release(kn->a);
    if (st & 1u) press(kn->b); else release(kn->b);
}

static void test_knobs(void)
{
    static const uint32_t CW[4] = {1, 3, 2, 0}, CCW[4] = {2, 3, 1, 0};
    uint32_t i, e;
    boot();
    frames(40);                              /* at rest: learned */
    for (i = 0; i < 8u; i++) {
        const struct knob *kn = &KNOB[i];
        int id = -1, d, k, per;
        for (e = 0; e < FM1_NENC; e++)
            fm1_enc_take(e);
        for (d = 0; d < 3; d++)
            for (k = 0; k < 4; k++) {
                knob_state(kn, CW[k]);
                frames(2);
            }
        frames(40);
        for (e = 0; e < FM1_NENC; e++) {
            int32_t s = fm1_enc_take(e);
            if (s) {
                CHECK(id < 0 && s == 3, "%s: encoder %u turned %d", kn->name, (unsigned)e, (int)s);
                id = (int)e;
            }
        }
        if (kn->role == NE)
            CHECK(id == (int)SMK_ENC_MASTER, "%s: encoder %d, want MASTER (%u)", kn->name, id, SMK_ENC_MASTER);
        else
            CHECK(id >= 0 && PANEL_DEFAULT.enc[kn->role] == id && PANEL_DEFAULT.dir[kn->role] == 1,
                  "%s: encoder %d, the panel's for role %d is %d", kn->name, id, kn->role,
                  PANEL_DEFAULT.enc[kn->role]);
        if (id < 0)
            continue;
        for (per = 1; per <= 4; per *= 2) {  /* anticlockwise, fast and slow */
            for (d = 0; d < 5; d++)
                for (k = 0; k < 4; k++) {
                    knob_state(kn, CCW[k]);
                    frames(per);
                }
            frames(40);
            {
                int32_t s = fm1_enc_take((uint32_t)id);
                CHECK(s == -5, "%s: 5 detents back, %d frames a state: %d", kn->name, per, (int)s);
            }
        }
        knob_state(kn, 1);                   /* a one-frame glitch at rest: nothing */
        frames(1);
        knob_state(kn, 0);
        frames(40);
        CHECK(fm1_enc_take((uint32_t)id) == 0, "%s: a glitch turned it", kn->name);
    }
}

static void test_word(void)
{
    uint32_t c, i, w;
    for (c = 0; c < 16u; c++) {
        w = smk__word(c, 0, 0);
        CHECK((w >> 16) == (0xFFFFu & ~(c < 8u ? 1u << (8 + c) : 1u << (c - 8u))), "column %u: %08X", (unsigned)c,
              (unsigned)w);
        CHECK((w & 0xFFFFu) == 0xFFC7u, "column %u, nothing lit: low half %04X", (unsigned)c, (unsigned)(w & 0xFFFFu));
    }
    for (i = 0; i < 16u; i++) {
        w = smk__word(3, 1u << i, 0);
        if (i >= 3u && i <= 5u)
            CHECK((w & 0xFFFFu) == 0xFFC7u, "line %u is the mux's: lit anyway", (unsigned)i);
        else
            CHECK((w & 0xFFFFu) == (0xFFC7u & ~(1u << i)), "line %u lit: %04X", (unsigned)i, (unsigned)(w & 0xFFFFu));
    }
    for (i = 0; i < 8u; i++)
        CHECK(((smk__word(5, 0xFFFFu, i) >> 3) & 7u) == i && (smk__word(5, 0xFFFFu, i) & 0xFFC7u) == 0u,
              "mux address %u", (unsigned)i);
    for (i = 0; i < 32u; i++)                /* bit i out: byte i / 8 from the lowest, its top bit first */
        CHECK(smk__sr_bit(1u << (8u * (i / 8u) + 7u - i % 8u), i) == 1u && smk__sr_bit(~(1u << (8u * (i / 8u) +
              7u - i % 8u)), i) == 0u, "bit %u out", (unsigned)i);
}

static void test_leds(void)
{
    uint32_t i;
    boot();
    for (i = 0; i < 18u; i++) {
        int id = -1, r, t, on[2] = {0, 0}, other = 0, frames_seen[2] = {0, 0};
        uint32_t g = BTN[i].led >> 4, line = BTN[i].led & 15u, ex = 0;
        if (BTN[i].role >= 0)
            id = (int)PANEL_DEFAULT.btn[BTN[i].role];    /* plat_leds: role -> panel id -> the header's LED */
        else {
            id = hal_id(BTN[i].code, &ex);               /* (no id: an extra, 14 + its bit) */
            for (r = 0; r < 4; r++)
                if (ex == 1u << r)
                    id = 14 + r;
        }
        smk_leds(1u << id, 0, 0);
        for (t = 0; t < 64; t++) {                       /* 4 frames */
            uint32_t lit = ~latched & 0xFFC7u, com = fake_com, grp;
            ticks(1);
            lit = ~latched & 0xFFC7u;
            com = fake_com;
            CHECK(com == SMK_COM_ON(0) || com == SMK_COM_ON(1), "%s: commons %X", BTN[i].name, (unsigned)com);
            grp = com == SMK_COM_ON(1);
            if (col_now == 0)
                frames_seen[grp]++;
            if (lit == (1u << line) && grp == g)
                on[grp]++;
            else if (lit)
                other++;
        }
        CHECK(on[g] == 32 && other == 0 && frames_seen[0] == 2 && frames_seen[1] == 2,
              "%s (id %d): lit %d ticks of its group's 32, %d ticks wrong, frames %d/%d", BTN[i].name, id, on[g], other,
              frames_seen[0], frames_seen[1]);
    }
    /* dim: a quarter of its group's ticks */
    smk_leds(0, 1u << 0, 4);
    {
        int t, on = 0;
        for (t = 0; t < 64; t++) {
            ticks(1);
            if ((~latched & 0xFFC7u) == 1u << 13 && fake_com == SMK_COM_ON(0))
                on++;
        }
        CHECK(on == 8, "OCT- dim: %d ticks of 32", on);
    }
    smk_leds(0, 0, 0);
}

static void test_pins(void)
{
    const uint32_t rows_a = 1u << 2 | 1u << 5 | 1u << 6 | 1u << 7 | 1u << 9, rows_h = 1u << 6 | 1u << 8 | 1u << 9;
    const uint32_t sr = 1u << 0 | 1u << 1 | 1u << 3 | 1u << 4;
    uint32_t snap[8][8], p, r;
    for (p = 0; p < 8u; p++)
        for (r = 0; r < 8u; r++)
            fake[p][r] = 0x5A5A5A5Au ^ (p * 0x01010101u) ^ (r * 0x10203040u);
    memcpy(snap, fake, sizeof snap);
    oe_high = bad_words = two_cols = 0;
    boot();
    press(0x66);
    frames(20);
    release(0x66);
    frames(20);
    for (r = 0; r < 8u; r++) {
        CHECK(memcmp(fake[FM1_PB], snap[FM1_PB], sizeof snap[0]) == 0, "port B touched (PB1: the charger)");
        CHECK(memcmp(fake[FM1_PC], snap[FM1_PC], sizeof snap[0]) == 0, "port C touched");
        if (r == (FM1_IN >> 2))
            continue;
        CHECK(((fake[FM1_PA][r] ^ snap[FM1_PA][r]) & ~(rows_a | sr)) == 0u, "port A reg %u: other pins touched",
              (unsigned)(r * 4u));
        CHECK(((fake[FM1_PH][r] ^ snap[FM1_PH][r]) & ~rows_h) == 0u, "port H reg %u: other pins touched",
              (unsigned)(r * 4u));
    }
    CHECK(fake[FM1_PH][0] == snap[FM1_PH][0], "port H's output written");
    CHECK(((fake[FM1_PA][0] ^ snap[FM1_PA][0]) & rows_a) == 0u, "the rows' output bits written");
    CHECK((fake[FM1_PA][FM1_DIR >> 2] & rows_a) == rows_a && (fake[FM1_PH][FM1_DIR >> 2] & rows_h) == rows_h,
          "a row is not an input");
    CHECK((fake[FM1_PA][FM1_PU >> 2] & rows_a) == rows_a && (fake[FM1_PH][FM1_PU >> 2] & rows_h) == rows_h,
          "a row not pulled up");
    CHECK(!(fake[FM1_PA][FM1_PD >> 2] & rows_a) && !(fake[FM1_PH][FM1_PD >> 2] & rows_h), "a row pulled down");
    CHECK(!(fake[FM1_PA][FM1_DIR >> 2] & sr), "a 595 pin is not an output");
    CHECK(oe_high == 0u, "OE driven high %u times", (unsigned)oe_high);
    CHECK(bad_words == 0u, "%u words not 32 bits", (unsigned)bad_words);
    CHECK(two_cols == 0u, "two columns driven at once, %u times", (unsigned)two_cols);
}

static void test_master(void)
{
#ifndef AISLING_QUIET_TEST
    uint32_t k = master_enc_start(), n;
    CHECK(k == 812u && master_curve(k) <= MASTER_START && master_curve(k + 1u) > MASTER_START, "start %u",
          (unsigned)k);
    CHECK(master_enc_turn(812, 1) == 836u && master_enc_turn(812, -1) == 788u, "a detent is 24");
    CHECK(master_enc_turn(10, -1) == 0u && master_enc_turn(1020, 1) == 1023u, "not clamped");
    CHECK(master_enc_turn(812, -1000) == 0u && master_enc_turn(812, 1000) == 1023u, "a big turn, not clamped");
    for (k = 0, n = 0; k < 1023u; n++)
        k = master_enc_turn(k, 1);
    CHECK(n == 43u, "end to end: %u detents", (unsigned)n);
#endif
}

int main(void)
{
    parse_keys();
    test_tables();
    test_word();
    test_buttons();
    test_keys();
    test_knobs();
    test_leds();
    test_pins();
    test_master();
    printf("smk-input: %d failed\n", fails);
    return fails != 0;
}
