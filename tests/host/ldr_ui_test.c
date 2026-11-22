/* SPDX-License-Identifier: GPL-3.0-only */
/* The update loader's screen (firmware/loader/ldr_ui.c) through the real panel code (src/lcd.c), against a
 * panel modelled at the wire: column/row window, memory write, 16-bit colour. Checks where things land, that
 * the bar and the number follow the progress, and that a dead SPI turns the screen off instead of hanging.
 *   ldr_ui_test [OUT.ppm ...]   (frames: 0 %, 42 %, 100 %, done, failed) */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint16_t fb[240][240];
static uint32_t wx0, wx1, wy0, wy1, cx, cy, cmd, argn, outside, sends, dead;
static uint8_t arg[8], half, hi;
static uint32_t fm1_lcd_timeouts;
static void fm1_lcd_hw_init(void) {}
static void fm1_lcd_baud(uint32_t b) { (void)b; }
static void fm1_lcd_wait(void) { if (dead) fm1_lcd_timeouts++; }
static void fm1_lcd_deselect(void) {}
static void fm1_lcd_send_cmd(uint8_t c)
{
    cmd = c, argn = 0, half = 0;
    if (c == 0x2C) cx = wx0, cy = wy0;
}
static void fm1_lcd_send_data(const void *p, uint32_t n)
{
    const uint8_t *s = p;
    uint32_t i;
    sends++;
    for (i = 0; i < n; i++) {
        if (cmd == 0x2A || cmd == 0x2B) {
            if (argn < 4) arg[argn++] = s[i];
            if (argn == 4) {
                uint32_t a = (uint32_t)arg[0] << 8 | arg[1], b = (uint32_t)arg[2] << 8 | arg[3];
                if (cmd == 0x2A) wx0 = a, wx1 = b; else wy0 = a, wy1 = b;
            }
        } else if (cmd == 0x2C) {
            if (!half) { hi = s[i], half = 1; continue; }
            half = 0;
            if (cx > 239 || cy > 239 || cy > wy1) outside++;
            else fb[cy][cx] = (uint16_t)(hi << 8 | s[i]);
            if (++cx > wx1) cx = wx0, cy++;
        }
    }
}
#include "../../firmware/src/lcd.c"
#include "../../firmware/loader/ldr_ui.c"

static uint32_t count(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    uint32_t n = 0, i, j;
    for (j = y; j < y + h; j++)
        for (i = x; i < x + w; i++)
            n += fb[j][i] == c;
    return n;
}
static void dump(const char *path)
{
    FILE *f;
    uint32_t i, j;
    if (!path) return;
    f = fopen(path, "wb");
    fprintf(f, "P6 240 240 255\n");
    for (j = 0; j < 240; j++)
        for (i = 0; i < 240; i++) {
            uint16_t c = fb[j][i];
            fputc((c >> 11) * 255 / 31, f), fputc((c >> 5 & 63) * 255 / 63, f), fputc((c & 31) * 255 / 31, f);
        }
    fclose(f);
}
static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

int main(int argc, char **argv)
{
    const uint32_t total = 0x8F000u;
    uint32_t s, lit0, lit42, before;
    memset(fb, 0x55, sizeof fb);
    ui_init();
    lcd_sync();
    CHECK(!outside && !ui_off);
    CHECK(count(0, 0, 240, UI_TITLE_Y, 0) == 240 * UI_TITLE_Y);            /* black above the title */
    CHECK(count(0, UI_TITLE_Y, 240, 28, 0xFFFF) > 600);                    /* UPDATING */
    CHECK(count(0, UI_TITLE_Y, 26, 28, 0xFFFF) == 0 && count(214, UI_TITLE_Y, 26, 28, 0xFFFF) == 0);
    CHECK(count(UI_BAR_X, UI_BAR_Y, UI_BAR_W, UI_BAR_H, UI_TRACK) == UI_BAR_W * UI_BAR_H);
    CHECK(count(0, UI_FOOT_Y, 240, 14, UI_DIM) > 300);                     /* the bottom line */
    CHECK(count(0, 0, 240, 240, 0x5555) == 0);                             /* every dot was written */
    lit0 = count(0, UI_NUM_Y, 240, 28, 0xFFFF);
    CHECK(lit0 > 100);
    dump(argc > 1 ? argv[1] : 0);
    for (s = 0x1000u; s <= total; s += 0x1000u) {                          /* as ldr_core.c calls it */
        before = sends;
        ui_progress(s, total);
        CHECK(!lcd_busy);
        CHECK(sends - before < 40);
        CHECK(ui_pct == (s == total ? 100u : s * 100u / total));
        CHECK(count(UI_BAR_X, UI_BAR_Y, UI_BAR_W, UI_BAR_H, 0xFFFF) == ui_fill * UI_BAR_H);
        CHECK(ui_fill == (uint32_t)((uint64_t)s * UI_BAR_W / total) || ui_fill + 1 == (uint32_t)((uint64_t)s * UI_BAR_W / total)
              || ui_fill == (uint32_t)((uint64_t)s * UI_BAR_W / total) + 1);
        if (ui_pct == 42 && argc > 2) dump(argv[2]), argv[2] = 0;
    }
    lit42 = count(0, UI_NUM_Y, 240, 28, 0xFFFF);
    CHECK(ui_fill == UI_BAR_W && ui_pct == 100 && lit42 > lit0);
    dump(argc > 3 ? argv[3] : 0);
    ui_end(0);
    CHECK(count(0, UI_NUM_Y, 240, 28, 0xFFFF) > 200);
    dump(argc > 4 ? argv[4] : 0);
    ui_begin();                                                           /* a second session starts clean */
    CHECK(ui_pct == 0 && ui_fill == 0 && count(UI_BAR_X, UI_BAR_Y, UI_BAR_W, UI_BAR_H, 0xFFFF) == 0);
    CHECK(count(0, UI_NUM_Y, 240, 28, 0xFFFF) == lit0);
    ui_end(-11);
    CHECK(count(0, UI_NUM_Y, 240, 28, 0xFFFF) > 300 && count(0, UI_NUM_Y, 20, 28, 0xFFFF) == 0);
    dump(argc > 5 ? argv[5] : 0);
    CHECK(!outside && !ui_off);
    dead = 1;                                                             /* the SPI stops answering */
    ui_progress(0x1000, total);
    before = sends;
    ui_begin(), ui_progress(total, total), ui_end(0);
    CHECK(ui_off && sends == before);
    printf("%s\n", fails ? "FAILED" : "ldr_ui: ok");
    return fails != 0;
}
