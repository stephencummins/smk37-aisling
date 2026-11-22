/* SPDX-License-Identifier: GPL-3.0-only */
/* The update loader's screen: UPDATING, how much of the app area is done (a
 * number and a bar), and a line that says to leave the cable in. The loader
 * has no fonts and no canvas, so this brings its own: 5x7 capitals, drawn one
 * character at a time through src/lcd.c.
 *
 * The screen is only a courtesy. If the panel's SPI ever times out, the screen
 * is given up (ui_off) and the update goes on without it.
 *
 *   ui_init()              panel on, the whole screen drawn at 0%
 *   ui_begin()             a session starts (again): back to 0%
 *   ui_progress(done, total)
 *   ui_end(rc)             0: DONE; else FAILED and the code
 * Needs lcd.c (lcd_init, lcd_fill, lcd_blit, lcd_sync) and fm1_lcd_timeouts. */
#define UI_WHITE 0xFFFFu
#define UI_DIM 0x8410u                          /* mid grey: the bottom line, the bar's frame */
#define UI_TRACK 0x2104u                        /* dark grey: the bar not yet filled */
#define UI_TITLE_Y 62u
#define UI_NUM_Y 112u
#define UI_BAR_X 30u
#define UI_BAR_Y 160u
#define UI_BAR_W 180u
#define UI_BAR_H 12u
#define UI_FOOT_Y 204u

static const struct {
    char ch;
    uint8_t row[7];                             /* bit 4 = the left column */
} UI_FONT[] = {
    {'A', {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}}, {'B', {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}},
    {'C', {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}}, {'D', {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E}},
    {'E', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}}, {'F', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10}},
    {'G', {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F}}, {'I', {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}},
    {'K', {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}}, {'L', {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}},
    {'N', {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11}}, {'O', {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}},
    {'P', {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}}, {'S', {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E}},
    {'T', {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}}, {'U', {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}},
    {'0', {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}}, {'1', {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}},
    {'2', {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}}, {'3', {0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E}},
    {'4', {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}}, {'5', {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E}},
    {'6', {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}}, {'7', {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}},
    {'8', {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}}, {'9', {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C}},
    {'%', {0x18, 0x19, 0x02, 0x04, 0x08, 0x13, 0x03}}, {'-', {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00}},
};

static uint16_t ui_px[24 * 28];                 /* one character cell at the largest scale (4) */
static uint8_t ui_off;                          /* the panel did not answer: draw nothing more */
static uint32_t ui_pct, ui_fill;                /* what the screen shows now (ui_pct 255: nothing) */

static int ui_ok(void)
{
    if (fm1_lcd_timeouts)
        ui_off = 1;
    return !ui_off;
}

static const uint8_t *ui_glyph(char ch)
{
    uint32_t i;
    for (i = 0; i < sizeof UI_FONT / sizeof UI_FONT[0]; i++)
        if (UI_FONT[i].ch == ch)
            return UI_FONT[i].row;
    return 0;                                   /* (a space, or a character the font lacks) */
}

/* s centred on the screen at row y, cells of 6 x 7 dots at scale sc (1..4), on black */
static void ui_text(uint32_t y, uint32_t sc, const char *s, uint16_t c)
{
    const uint32_t cw = 6u * sc, chh = 7u * sc, sw = (uint16_t)((c >> 8) | (c << 8));
    uint32_t n = 0, x, i, r, k;
    while (s[n])
        n++;
    x = (240u - n * cw + sc) / 2u;              /* (the last cell's empty column does not count) */
    for (i = 0; i < n && ui_ok(); i++, x += cw) {
        const uint8_t *g = ui_glyph(s[i]);
        lcd_sync();                             /* the last cell may still be going out of ui_px */
        for (r = 0; r < chh; r++)
            for (k = 0; k < cw; k++)
                ui_px[r * cw + k] = g && k < 5u * sc && ((g[r / sc] >> (4u - k / sc)) & 1u) ? (uint16_t)sw : 0;
        lcd_blit(x, y, cw, chh, ui_px);
    }
}

static void ui_num_clear(void)
{
    if (ui_ok())
        lcd_fill(0, UI_NUM_Y, 240, 28, 0);
}

static void ui_show_pct(uint32_t pct)
{
    char t[5];
    uint32_t n = 0;
    if (pct >= 100u)
        t[n++] = '1';
    if (pct >= 10u)
        t[n++] = (char)('0' + pct / 10u % 10u);
    t[n++] = (char)('0' + pct % 10u);
    t[n++] = '%';
    t[n] = 0;
    ui_text(UI_NUM_Y, 4, t, UI_WHITE);          /* (only ever grows wider: it covers the one before) */
    ui_pct = pct;
}

static void ui_begin(void)
{
    if (!ui_ok())
        return;
    ui_num_clear();
    lcd_fill(UI_BAR_X, UI_BAR_Y, UI_BAR_W, UI_BAR_H, UI_TRACK);
    ui_fill = 0;
    ui_show_pct(0);
    lcd_sync();
}

static void ui_init(void)
{
    lcd_init();
    if (!ui_ok())
        return;
    ui_text(UI_TITLE_Y, 4, "UPDATING", UI_WHITE);
    lcd_fill(UI_BAR_X - 2u, UI_BAR_Y - 2u, UI_BAR_W + 4u, UI_BAR_H + 4u, UI_DIM);
    lcd_fill(UI_BAR_X - 1u, UI_BAR_Y - 1u, UI_BAR_W + 2u, UI_BAR_H + 2u, 0);
    ui_text(UI_FOOT_Y, 2, "KEEP USB CONNECTED", UI_DIM);
    ui_begin();
}

static void ui_progress(uint32_t done, uint32_t total)
{
    uint32_t pct, w;
    if (!total || !ui_ok())
        return;
    if (done > total)
        done = total;
    pct = done / (total / 100u ? total / 100u : 1u);
    if (pct > 99u && done < total)
        pct = 99u;                              /* 100 only when it is all there */
    if (pct > 100u)
        pct = 100u;
    w = done / (total / UI_BAR_W ? total / UI_BAR_W : 1u);
    if (w > UI_BAR_W)
        w = UI_BAR_W;
    if (w > ui_fill) {
        lcd_fill(UI_BAR_X + ui_fill, UI_BAR_Y, w - ui_fill, UI_BAR_H, UI_WHITE);
        ui_fill = w;
    }
    if (pct != ui_pct)
        ui_show_pct(pct);
    lcd_sync();                                 /* nothing left running while the flash is written */
}

static void ui_end(int32_t rc)
{
    char t[12] = "FAILED ";
    uint32_t n = 7, v = (uint32_t)(rc < 0 ? -rc : rc);
    if (!ui_ok())
        return;
    ui_num_clear();
    ui_pct = 255;
    if (rc == 0) {
        ui_text(UI_NUM_Y, 4, "DONE", UI_WHITE);
    } else {
        if (rc < 0)
            t[n++] = '-';
        if (v >= 100u)
            t[n++] = (char)('0' + v / 100u % 10u);
        if (v >= 10u)
            t[n++] = (char)('0' + v / 10u % 10u);
        t[n++] = (char)('0' + v % 10u);
        t[n] = 0;
        ui_text(UI_NUM_Y, 3, t, UI_WHITE);
    }
    lcd_sync();
}
