/* SPDX-License-Identifier: GPL-3.0-only */
/* The SMK-37 Elite's screen and ADC hal (hal/smk37/smk_lcd_hw.h, smk_adc.h) against fake registers:
 *   - the pins each one sets up, and nothing else: PB5 PB6 PB7 PC8 for the screen, PA10 for the ADC. Never the rest
 *     of port A (the 595s and the matrix rows, and PA2, the FM-1's backlight), never PB1 (the charger's status), never
 *     the FM-1's screen pins on port C (PC7 is the SMK's MIDI OUT, PC9 mux B, PC10 the pads' LEDs);
 *   - CS high before it drives; SPI1 on pin group A, otherwise the FM-1's SPI settings;
 *   - a command, a data transfer, deselect, the wait and its timeout;
 *   - the ADC's sequence (CON 0x400D | ch << 8, enable, start, poll, stop), the WLA bit, the timeout.
 * The pin checks run from all-zero and from all-one registers, so every bit written shows in one of the two.
 * -DCONTROL rotates the fake's ports (port A's registers answer for B, B's for C, C's for A), as if the pins were
 * wired elsewhere: it must fail. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t *fake_gpio(uint32_t p, uint32_t r);
static uint32_t *fake_reg(uint32_t a);
#define SMK_GPIO(p, r) (*fake_gpio((p), (r)))
#define SMK_REG(a) (*fake_reg(a))
#include "smk37/smk_lcd_hw.h"
#include "smk37/smk_adc.h"

/* the pins as the guide gives them, typed here, not taken from the header under test */
#define T_DC (1u << 5)      /* PB5 */
#define T_CLK (1u << 6)     /* PB6 */
#define T_DO (1u << 7)      /* PB7 */
#define T_CS (1u << 8)      /* PC8 */
enum { NPORT = 8, NREG = 8 };
static uint32_t gpio[NPORT][NREG];       /* [port][reg / 4] */
static struct { uint32_t a, v; } regs[] = {
    {0x51020u, 0}, {0x11D00u, 0}, {0x11D04u, 0}, {0x11D08u, 0}, {0x11D0Cu, 0}, {0x11D10u, 0},   /* IOMAP_CON1, SPI1 */
    {0x13100u, 0}, {0x13104u, 0}, {0x11900u, 0},                                                /* ADC CON RES, WLA */
};
enum { R_IOMAP, R_SPI_CON, R_SPI_BAUD, R_SPI_BUF, R_SPI_ADR, R_SPI_CNT, R_ADC_CON, R_ADC_RES, R_WLA, NREGS };

static int fails;
static void check(const char *what, int ok)
{
    printf("  %s %s\n", ok ? "ok  " : "FAIL", what);
    fails += !ok;
}

/* CS order: watched while cs_watch; a glitch = PC8 an output while low */
static int cs_watch, cs_glitch;
static uint32_t *fake_gpio(uint32_t p, uint32_t r)
{
    if (p >= NPORT || (r & 3u) || r / 4u >= NREG) {
        fprintf(stderr, "stray GPIO access: port %u reg %#x\n", (unsigned)p, (unsigned)r);
        exit(2);
    }
#ifdef CONTROL
    p = p == FM1_PA ? FM1_PB : p == FM1_PB ? FM1_PC : p == FM1_PC ? FM1_PA : p;
#endif
    if (cs_watch && !(gpio[FM1_PC][FM1_DIR / 4] & T_CS) && !(gpio[FM1_PC][FM1_OUT / 4] & T_CS))
        cs_glitch = 1;
    return &gpio[p][r / 4u];
}

/* the ADC: each access to CON first sees what the last write left (logged when it changed); configured, enabled
 * (0x10) and started (0x40), the conversion is done at once (0x80), unless adc_dead, with that channel's value */
static uint32_t con_log[16], con_n, con_seen, adc_dead, adc_ch = 0xFF;
static uint32_t adc_value(uint32_t ch) { return 0x100u + ch * 37u; }
static uint32_t *fake_reg(uint32_t a)
{
    uint32_t i;
    for (i = 0; i < NREGS && regs[i].a != a; i++)
        ;
    if (i == NREGS) {
        fprintf(stderr, "stray register access: %#x\n", (unsigned)a);
        exit(2);
    }
    if (i == R_ADC_CON) {
        uint32_t *c = &regs[R_ADC_CON].v;
        if (*c != con_seen) {
            if (con_n < 16u)
                con_log[con_n] = *c;
            con_n++;
        }
        if (!adc_dead && (*c & 0x50u) == 0x50u && !(*c & 0x80u)) {
            adc_ch = (*c >> 8) & 0xFu;
            regs[R_ADC_RES].v = 0xFC00u | adc_value(adc_ch);     /* the bits above the 10 are not the reading */
            *c |= 0x80u;
        }
        con_seen = *c;
    }
    return &regs[i].v;
}

static uint32_t snap[NPORT][NREG];
static void fill(uint32_t z)
{
    uint32_t p, r;
    for (p = 0; p < NPORT; p++)
        for (r = 0; r < NREG; r++)
            gpio[p][r] = z;
    for (r = 0; r < NREGS; r++)
        regs[r].v = z;
    con_seen = z;
    memcpy(snap, gpio, sizeof snap);
}
/* the bits of gpio changed since fill(), added into w */
static void changed(uint32_t w[NPORT][NREG])
{
    uint32_t p, r;
    for (p = 0; p < NPORT; p++)
        for (r = 0; r < NREG; r++)
            w[p][r] |= gpio[p][r] ^ snap[p][r];
}
/* only the given bits written: (port, reg, bits) triples, the rest nothing */
static int only(uint32_t w[NPORT][NREG], const uint32_t (*want)[3], uint32_t n, int exact)
{
    uint32_t p, r, k, m;
    for (p = 0; p < NPORT; p++)
        for (r = 0; r < NREG; r++) {
            for (k = 0, m = 0; k < n; k++)
                if (want[k][0] == p && want[k][1] == r * 4u)
                    m |= want[k][2];
            if (w[p][r] & ~m)
                return 0;
            if (exact && w[p][r] != m)
                return 0;
        }
    return 1;
}

static void test_lcd(void)
{
    static const uint32_t lcd_pins[][3] = {          /* outputs: level, direction, no pulls, digital */
        {FM1_PB, FM1_OUT, T_DC | T_CLK | T_DO}, {FM1_PB, FM1_DIR, T_DC | T_CLK | T_DO},
        {FM1_PB, FM1_PU, T_DC | T_CLK | T_DO}, {FM1_PB, FM1_PD, T_DC | T_CLK | T_DO},
        {FM1_PB, FM1_DIE, T_DC | T_CLK | T_DO},
        {FM1_PC, FM1_OUT, T_CS}, {FM1_PC, FM1_DIR, T_CS}, {FM1_PC, FM1_PU, T_CS}, {FM1_PC, FM1_PD, T_CS},
        {FM1_PC, FM1_DIE, T_CS},
    };
    static uint8_t px[480];
    uint32_t w[NPORT][NREG], z, ok_vals = 1, iomap_ok = 1, spi_ok = 1;
    memset(w, 0, sizeof w);
    for (z = 0; z < 2u; z++) {
        uint32_t init = z ? 0xFFFFFFFFu : 0u;
        fill(init);
        fm1_lcd_hw_init();
        changed(w);
        ok_vals &= !(gpio[FM1_PB][FM1_OUT / 4] & (T_DC | T_CLK | T_DO)) &&
                   !(gpio[FM1_PB][FM1_DIR / 4] & (T_DC | T_CLK | T_DO)) &&
                   !((gpio[FM1_PB][FM1_PU / 4] | gpio[FM1_PB][FM1_PD / 4]) & (T_DC | T_CLK | T_DO)) &&
                   (gpio[FM1_PC][FM1_OUT / 4] & T_CS) && !(gpio[FM1_PC][FM1_DIR / 4] & T_CS) &&
                   !((gpio[FM1_PC][FM1_PU / 4] | gpio[FM1_PC][FM1_PD / 4]) & T_CS);
        iomap_ok &= regs[R_IOMAP].v == (init & ~0x10u);
        spi_ok &= regs[R_SPI_CON].v == 0x4021u && regs[R_SPI_BAUD].v == 4u;
    }
    check("screen init writes PB5 PB6 PB7 and PC8 (OUT DIR PU PD DIE) and nothing else", only(w, lcd_pins, 10, 1));
    check("  so never port A, never PB1, never PC7 PC9 PC10", !(w[FM1_PA][0] | w[FM1_PA][1] | w[FM1_PA][2] |
          w[FM1_PA][3] | w[FM1_PA][4] | w[FM1_PA][5]) && !((w[FM1_PB][0] | w[FM1_PB][2]) & 2u) &&
          !((w[FM1_PC][0] | w[FM1_PC][2]) & ((1u << 7) | (1u << 9) | (1u << 10))));
    check("D/C, CLK, DO outputs low; CS an output, high; no pulls", ok_vals);
    check("SPI1 on pin group A (IOMAP_CON1 bit 4 clear, the rest kept)", iomap_ok);
    check("SPI1: CON 0x4021, BAUD 4 (as the FM-1's)", spi_ok);

    fill(0);
    gpio[FM1_PC][FM1_DIR / 4] = 0xFFFFFFFFu;            /* all inputs; CS's OUT low: driving first would glitch */
    cs_watch = 1;
    fm1_lcd_hw_init();
    (void)fake_gpio(FM1_PC, FM1_OUT);                   /* see the last write */
    cs_watch = 0;
    check("CS goes high before it drives", !cs_glitch);

    {   /* from all-zero and all-one registers again, with SPI1's pending bit clear so setting it shows */
        uint32_t wc[NPORT][NREG], cmd_ok = 1, data_ok = 1, desel_ok = 1;
        static const uint32_t cmd_pins[][3] = {{FM1_PB, FM1_OUT, T_DC}, {FM1_PC, FM1_OUT, T_CS}};
        memset(wc, 0, sizeof wc);
        for (z = 0; z < 2u; z++) {
            fill(z ? 0xFFFFFFFFu : 0u);
            regs[R_SPI_CON].v = 0x0021u;
            fm1_lcd_send_cmd(0x2A);
            changed(wc);
            cmd_ok &= !(gpio[FM1_PB][FM1_OUT / 4] & T_DC) && !(gpio[FM1_PC][FM1_OUT / 4] & T_CS) &&
                      regs[R_SPI_BUF].v == 0x2Au && regs[R_SPI_CON].v == 0x4021u;
            memcpy(snap, gpio, sizeof snap);
            regs[R_SPI_CON].v = 0x0021u;
            fm1_lcd_send_data(px, sizeof px);
            changed(wc);
            data_ok &= (gpio[FM1_PB][FM1_OUT / 4] & T_DC) && !(gpio[FM1_PC][FM1_OUT / 4] & T_CS) &&
                       regs[R_SPI_ADR].v == (uint32_t)(uintptr_t)px && regs[R_SPI_CNT].v == sizeof px &&
                       regs[R_SPI_CON].v == 0x4021u;
            memcpy(snap, gpio, sizeof snap);
            fm1_lcd_deselect();
            changed(wc);
            desel_ok &= (gpio[FM1_PC][FM1_OUT / 4] & T_CS) != 0;
        }
        check("command: D/C low, CS low, pending cleared, the byte in BUF", cmd_ok);
        check("data: D/C high, CS low, pending cleared, ADR the buffer, CNT its length", data_ok);
        check("deselect: CS high", desel_ok);
        check("command, data, deselect touch D/C and CS alone", only(wc, cmd_pins, 2, 0));
    }
    regs[R_SPI_CON].v = 0x8021u;
    fm1_lcd_timeouts = 0;
    fm1_lcd_wait();
    check("wait: done at once, pending cleared, no timeout", fm1_lcd_timeouts == 0 && (regs[R_SPI_CON].v & 0x4000u));
    regs[R_SPI_CON].v = 0x0021u;
    fm1_lcd_wait();
    check("wait: never done = one timeout counted", fm1_lcd_timeouts == 1);
    fm1_lcd_baud(0);
    check("baud", regs[R_SPI_BAUD].v == 0);
}

static void test_adc(void)
{
    static const uint32_t adc_pins[][3] = {
        {FM1_PA, FM1_DIE, 1u << 10}, {FM1_PA, FM1_PU, 1u << 10}, {FM1_PA, FM1_PD, 1u << 10}, {FM1_PA, FM1_DIR, 1u << 10},
    };
    uint32_t w[NPORT][NREG], z, ok_vals = 1, ch, seq_ok = 1, val_ok = 1;
    memset(w, 0, sizeof w);
    for (z = 0; z < 2u; z++) {
        fill(z ? 0xFFFFFFFFu : 0u);
        smk_adc_init();
        changed(w);
        ok_vals &= !(gpio[FM1_PA][FM1_DIE / 4] & (1u << 10)) && !(gpio[FM1_PA][FM1_PU / 4] & (1u << 10)) &&
                   !(gpio[FM1_PA][FM1_PD / 4] & (1u << 10)) && (gpio[FM1_PA][FM1_DIR / 4] & (1u << 10));
    }
    check("ADC init writes PA10 (DIE PU PD DIR) and nothing else", only(w, adc_pins, 4, 1));
    check("PA10 an analog input, no pulls", ok_vals);

    for (ch = 0; ch < 16u; ch++) {
        const uint32_t want[6] = {0, 0x400Du | ch << 8, 0x401Du | ch << 8, 0x405Du | ch << 8, 0x40u, 0};
        int32_t v;
        fill(0);
        regs[R_ADC_CON].v = con_seen = 0xDEAD0000u;
        con_n = 0;
        v = smk_adc_read(ch);
        (void)fake_reg(0x13100u);
        seq_ok &= con_n == 6u && !memcmp(con_log, want, sizeof want);
        val_ok &= adc_ch == ch && v == (int32_t)adc_value(ch);
    }
    check("read: CON 0, 0x400D | ch << 8, | 0x10, | 0x40, poll, 0x40, 0 (every channel)", seq_ok);
    check("read: that channel's conversion, 10 bits", val_ok);
    check("mux C is channel 2, the battery its address 7", SMK_ADC_MUXC == 2 && SMK_MUX_BATT == 7);
    fill(0);
    regs[R_WLA].v = 0x0000C001u;
    (void)smk_adc_read(SMK_ADC_MUXC);
    check("WLA_CON0 bit 14 cleared, its other bits kept", regs[R_WLA].v == 0x00008001u);
    fill(0);
    adc_dead = 1;
    con_n = 0;
    check("no conversion: -1, and CON left 0", smk_adc_read(SMK_ADC_MUXC) == -1 && regs[R_ADC_CON].v == 0);
    adc_dead = 0;
}

int main(void)
{
    test_lcd();
    test_adc();
    printf(fails ? "smk_hw: %d FAILED\n" : "smk_hw: ok\n", fails);
    return fails != 0;
}
