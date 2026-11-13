/* SPDX-License-Identifier: GPL-3.0-only */
/* hal/smk37/smk_pins.h, the SMK-37 Elite's one pin table, and the three drivers that set their pins up from it
 * (smk_input.h, smk_lcd_hw.h, smk_adc.h) and the pads' lights (smk_rgb.h), on fake GPIO registers:
 *   - the table is the guide's pins (genkigenki's fm1-to-smk37 guide, 01-hardware.md), typed again here, not taken
 *     from the header: each pin once, with its owner and mode;
 *   - each driver's init, run alone from all-zero and from all-one registers, writes its own pins in the table and
 *     no other pin: every write's changed bits are logged, so a bit written shows from one start or the other.
 *     Two more starts turn each direction round (the table's outputs inputs, its inputs outputs, the rest zeros or
 *     ones), so a pin's drive and a row's turn to an input are always seen, whatever order they come in;
 *   - afterwards each pin is in its table mode (an input pulled up; analog; an output high or low, no pulls);
 *   - an output's level and pulls are set before it drives (so CS never drives low), and the rows are inputs before
 *     any 595 pin drives;
 *   - the pins nothing may touch (PB1 the charger's status, PC7 MIDI OUT) and the codec's
 *     (PC0 PC1 PC2 PC6, fm1_audio.h's) are never written, are not in the table, and are the header's lists.
 * -DCONTROL rotates the fake's ports (A's registers answer for B, B's for C, C's for H, H's for A), as if the pins
 * were wired elsewhere: it must fail. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t *fake_gpio(uint32_t p, uint32_t r);
static uint32_t *fake_reg(uint32_t a);
static uint32_t fake_com;
#define SMK_GPIO(p, r) (*fake_gpio((p), (r)))
#define SMK_REG(a) (*fake_reg(a))
#define SMK_LED_COM fake_com
#define SMK_IRQ_OFF() ((void)0)
#define SMK_IRQ_ON() ((void)0)
#include "smk37/smk_input.h"
#include "smk37/smk_lcd_hw.h"
#include "smk37/smk_adc.h"
#include "smk37/smk_rgb.h"

enum { NPORT = 8, NREG = 8, NLOG = 1024 };
enum { PA = 0, PB = 1, PC = 2, PH = 7 };     /* the port numbers, typed (fm1_gpio.h: 0x50000 + port * 0x40) */
enum { OUT = 0, DIR = 2, DIE = 3, PU = 4, PD = 5 };   /* register / 4 (DIR set = input) */

/* the guide's pins, typed */
static const struct { const char *name; uint8_t owner, port, bit, mode; } WANT[] = {
    {"PA2 row 0", SMK_OWN_INPUT, PA, 2, SMK_IN_PU},      {"PA5 row 1", SMK_OWN_INPUT, PA, 5, SMK_IN_PU},
    {"PA6 row 2", SMK_OWN_INPUT, PA, 6, SMK_IN_PU},      {"PA7 row 3", SMK_OWN_INPUT, PA, 7, SMK_IN_PU},
    {"PA9 row 4", SMK_OWN_INPUT, PA, 9, SMK_IN_PU},      {"PH6 row 5", SMK_OWN_INPUT, PH, 6, SMK_IN_PU},
    {"PH8 row 6", SMK_OWN_INPUT, PH, 8, SMK_IN_PU},      {"PH9 row 7", SMK_OWN_INPUT, PH, 9, SMK_IN_PU},
    {"PA0 595 OE", SMK_OWN_INPUT, PA, 0, SMK_OUT_LO},    {"PA1 595 latch", SMK_OWN_INPUT, PA, 1, SMK_OUT_LO},
    {"PA3 595 clock", SMK_OWN_INPUT, PA, 3, SMK_OUT_LO}, {"PA4 595 data", SMK_OWN_INPUT, PA, 4, SMK_OUT_LO},
    {"PB5 screen D/C", SMK_OWN_LCD, PB, 5, SMK_OUT_LO},  {"PB6 screen CLK", SMK_OWN_LCD, PB, 6, SMK_OUT_LO},
    {"PB7 screen DO", SMK_OWN_LCD, PB, 7, SMK_OUT_LO},   {"PC8 screen CS", SMK_OWN_LCD, PC, 8, SMK_OUT_HI},
    {"PA10 mux C", SMK_OWN_ADC, PA, 10, SMK_ANALOG},     {"PA8 mux A", SMK_OWN_ADC, PA, 8, SMK_ANALOG},
    {"PC9 mux B", SMK_OWN_ADC, PC, 9, SMK_ANALOG},       {"PC10 pad LEDs", SMK_OWN_RGB, PC, 10, SMK_OUT_LO},
};
#define NWANT (sizeof WANT / sizeof WANT[0])
static const struct { const char *name; uint8_t port, bit, shared; } NEVER[] = {
    {"PB1 charger status", PB, 1, 0}, {"PC7 MIDI OUT", PC, 7, 0},
    {"PC0 codec", PC, 0, 1},          {"PC1 codec", PC, 1, 1}, {"PC2 codec", PC, 2, 1}, {"PC6 codec", PC, 6, 1},
};
#define NNEVER (sizeof NEVER / sizeof NEVER[0])
static const uint8_t PORTS[] = {PA, PB, PC, PH};

static int fails;
static void check(const char *what, int ok)
{
    printf("  %s %s\n", ok ? "ok  " : "FAIL", what);
    fails += !ok;
}

/* the fake: each access first logs what the previous one changed (a statement writes one register) */
static uint32_t gpio[NPORT][NREG];
static struct { uint8_t port, reg; uint32_t before, after; } wlog[NLOG];
static uint32_t nlog, overflow, pend = 0xFFu, pend_v;
static void commit(void)
{
    if (pend != 0xFFu && gpio[pend / NREG][pend % NREG] != pend_v) {
        if (nlog < NLOG) {
            wlog[nlog].port = (uint8_t)(pend / NREG);
            wlog[nlog].reg = (uint8_t)(pend % NREG);
            wlog[nlog].before = pend_v;
            wlog[nlog].after = gpio[pend / NREG][pend % NREG];
            nlog++;
        } else
            overflow = 1;
    }
    pend = 0xFFu;
}
static uint32_t *fake_gpio(uint32_t p, uint32_t r)
{
    commit();
    if (p >= NPORT || (r & 3u) || r / 4u >= NREG) {
        fprintf(stderr, "stray GPIO access: port %u reg %#x\n", (unsigned)p, (unsigned)r);
        exit(2);
    }
#ifdef CONTROL
    p = p == PA ? PB : p == PB ? PC : p == PC ? PH : p == PH ? PA : p;
#endif
    pend = p * NREG + r / 4u;
    pend_v = gpio[p][r / 4u];
    return &gpio[p][r / 4u];
}
static struct { uint32_t a, v; } regs[16];
static uint32_t *fake_reg(uint32_t a)        /* the screen's and ADC's other registers: not pins, not checked here */
{
    uint32_t i;
    commit();
    for (i = 0; i < 16u && regs[i].a && regs[i].a != a; i++)
        ;
    if (i == 16u) {
        fprintf(stderr, "too many registers\n");
        exit(2);
    }
    regs[i].a = a;
    return &regs[i].v;
}

static int want_of(uint32_t port, uint32_t bit)
{
    uint32_t i;
    for (i = 0; i < NWANT; i++)
        if (WANT[i].port == port && WANT[i].bit == bit)
            return (int)i;
    return -1;
}

static uint32_t owner_mask(uint32_t owner, uint32_t port)
{
    uint32_t i, m = 0;
    for (i = 0; i < NWANT; i++)
        if (WANT[i].owner == owner && WANT[i].port == port)
            m |= 1u << WANT[i].bit;
    return m;
}

static uint32_t dir_mask(uint32_t port, int outputs)    /* every owner's outputs, or its inputs, on a port */
{
    uint32_t i, m = 0;
    for (i = 0; i < NWANT; i++)
        if (WANT[i].port == port && (WANT[i].mode == SMK_OUT_HI || WANT[i].mode == SMK_OUT_LO) == outputs)
            m |= 1u << WANT[i].bit;
    return m;
}

/* one driver's init from one start (0 zeros, 1 ones; 2, 3 the same with every table pin's direction turned round):
 * what it wrote (into touched), the pins' modes after, the order it wrote them in */
static void run(uint32_t owner, uint32_t start, uint32_t *touched, int *touch_ok, int *mode_ok, int *order_ok)
{
    static const char *START[] = {"zeros", "ones", "zeros, directions turned", "ones, directions turned"};
    static uint32_t st[NPORT][NREG];
    uint32_t i, k;
    memset(gpio, start & 1u ? 0xFF : 0, sizeof gpio);
    if (start >= 2u)
        for (k = 0; k < sizeof PORTS; k++)
            gpio[PORTS[k]][DIR] = (gpio[PORTS[k]][DIR] | dir_mask(PORTS[k], 1)) & ~dir_mask(PORTS[k], 0);
    memset(regs, 0, sizeof regs);
    memcpy(st, gpio, sizeof st);
    nlog = overflow = 0;
    pend = 0xFFu;
    if (owner == SMK_OWN_INPUT)
        fm1_input_init();
    else if (owner == SMK_OWN_LCD)
        fm1_lcd_hw_init();
    else if (owner == SMK_OWN_ADC)
        smk_adc_init();
    else
        smk_rgb_init();
    commit();
    if (overflow)
        *touch_ok = 0;
    for (i = 0; i < nlog; i++) {
        const uint32_t p = wlog[i].port, r = wlog[i].reg, ch = wlog[i].before ^ wlog[i].after;
        const uint32_t driven = r == DIR ? wlog[i].before & ~wlog[i].after : 0;   /* bits made outputs */
        touched[p] |= ch;
        for (k = 0; k < 32u; k++) {
            int w;
            if (!(driven >> k & 1u))
                continue;
            w = want_of(p, k);
            if (w < 0 || (WANT[w].mode != SMK_OUT_HI && WANT[w].mode != SMK_OUT_LO)) {
                *order_ok = 0;      /* not an output (touch_ok sees it too) */
                continue;
            }
            if ((st[p][OUT] >> k & 1u) != (WANT[w].mode == SMK_OUT_HI) || ((st[p][PU] | st[p][PD]) >> k & 1u))
                *order_ok = 0;      /* drove before its level, or still pulled */
            if (WANT[w].owner == SMK_OWN_INPUT &&
                ((st[PA][DIR] & 0x2E4u) != 0x2E4u ||        /* PA2 PA5 PA6 PA7 PA9 */
                 (st[PH][DIR] & 0x340u) != 0x340u))         /* PH6 PH8 PH9 */
                *order_ok = 0;      /* a 595 pin drove while a row was not yet an input */
        }
        st[p][r] = wlog[i].after;
    }
    for (i = 0; i < NWANT; i++) {
        const uint32_t p = WANT[i].port, b = 1u << WANT[i].bit;
        const uint32_t *g = gpio[p];
        int ok;
        if (WANT[i].owner != owner)
            continue;
        switch (WANT[i].mode) {
        case SMK_IN_PU: ok = (g[DIE] & b) && (g[DIR] & b) && (g[PU] & b) && !(g[PD] & b); break;
        case SMK_ANALOG: ok = !(g[DIE] & b) && (g[DIR] & b) && !(g[PU] & b) && !(g[PD] & b); break;
        default:
            ok = (g[DIE] & b) && !(g[DIR] & b) && !(g[PU] & b) && !(g[PD] & b) &&
                 !(g[OUT] & b) == (WANT[i].mode == SMK_OUT_LO);
            break;
        }
        if (!ok) {
            printf("       %s: not in its mode after init (start: %s)\n", WANT[i].name, START[start]);
            *mode_ok = 0;
        }
    }
}

int main(void)
{
    static const char *OWNER[] = {"input (rows, 595s)", "screen", "ADC", "pad lights"};
    uint32_t i, j, o;
    int same = SMK_NPIN == NWANT, once = 1, lists = 1, disjoint = 1;

    /* the table is the guide's pins, each once */
    for (i = 0; i < SMK_NPIN; i++) {
        const int w = want_of(SMK_PIN_TABLE[i].port, SMK_PIN_TABLE[i].bit);
        if (w < 0 || WANT[w].owner != SMK_PIN_TABLE[i].owner || WANT[w].mode != SMK_PIN_TABLE[i].mode) {
            printf("       table row %u (port %u bit %u) is not the guide's\n", (unsigned)i,
                   (unsigned)SMK_PIN_TABLE[i].port, (unsigned)SMK_PIN_TABLE[i].bit);
            same = 0;
        }
        for (j = i + 1; j < SMK_NPIN; j++)
            if (SMK_PIN_TABLE[j].port == SMK_PIN_TABLE[i].port && SMK_PIN_TABLE[j].bit == SMK_PIN_TABLE[i].bit)
                once = 0;
    }
    check("the table is the guide's 20 pins, owners and modes", same);
    check("no pin in the table twice", once);
    for (i = 0; i < NNEVER; i++) {
        const uint8_t(*l)[2] = NEVER[i].shared ? SMK_PINS_SHARED : SMK_PINS_UNUSED;
        const uint32_t n = NEVER[i].shared ? sizeof SMK_PINS_SHARED / 2 : sizeof SMK_PINS_UNUSED / 2;
        int found = 0;
        for (j = 0; j < n; j++)
            found |= l[j][0] == NEVER[i].port && l[j][1] == NEVER[i].bit;
        lists &= found;
        for (j = 0; j < SMK_NPIN; j++)
            if (SMK_PIN_TABLE[j].port == NEVER[i].port && SMK_PIN_TABLE[j].bit == NEVER[i].bit)
                disjoint = 0;
    }
    lists &= sizeof SMK_PINS_UNUSED / 2 + sizeof SMK_PINS_SHARED / 2 == NNEVER;
    check("the header lists the 2 unused pins and the codec's 4", lists);
    check("none of those 6 is in the table", disjoint);

    for (o = 0; o < SMK_NOWN; o++) {
        uint32_t touched[NPORT] = {0}, s, k;
        int touch_ok = 1, mode_ok = 1, order_ok = 1, never_ok = 1;
        char what[160];
        for (s = 0; s < 4u; s++)
            run(o, s, touched, &touch_ok, &mode_ok, &order_ok);
        for (k = 0; k < NPORT; k++)         /* every pin it owns written from some start, and no other */
            if (touched[k] != owner_mask(o, k))
                touch_ok = 0;
        for (k = 0; k < NNEVER; k++)
            if (touched[NEVER[k].port] >> NEVER[k].bit & 1u)
                never_ok = 0;
        snprintf(what, sizeof what, "%s init: writes its table pins and no other pin", OWNER[o]);
        check(what, touch_ok);
        snprintf(what, sizeof what, "%s init: each pin left in its table mode", OWNER[o]);
        check(what, mode_ok);
        snprintf(what, sizeof what, "%s init: levels before drive%s", OWNER[o],
                 o == SMK_OWN_INPUT ? ", the rows inputs before a 595 pin drives" : "");
        check(what, order_ok);
        snprintf(what, sizeof what, "%s init: never PB1 PC7, nor the codec's PC0 PC1 PC2 PC6", OWNER[o]);
        check(what, never_ok);
    }
    printf(fails ? "smk_pins: %d FAILED\n" : "smk_pins: ok\n", fails);
    return fails != 0;
}
