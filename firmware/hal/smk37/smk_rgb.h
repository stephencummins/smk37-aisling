/* SPDX-License-Identifier: GPL-3.0-only */
/* The M-VAVE SMK-37 Elite's pad lights: sixteen WS2812-class RGB LEDs on one data line, PC10.
 *
 * The wiring and the method are from genkigenki's fm1-to-smk37 guide, 01-hardware.md and 08-pitfalls.md (CC BY-NC
 * 4.0; LICENSING.md), typed here, not copied: PC10 is SPI1's data out on pin group B (IOMAP_CON1 bit 4 set), the
 * screen's SPI1 on group A (smk_lcd_hw.h). A frame is sent as SPI bytes, one byte an LED's bit, the high bit first:
 * 0xF0 a one, 0xC0 a nought, at BAUD 8 (lsb / 9, 6.67 MHz: 150 ns a SPI bit, so a one is high 600 ns of 1200, a
 * nought 300). An LED takes 24 bits, green, red, blue. Before them a long reset, the line low (SMK_RGB_RESET
 * noughts of bytes, 300 us: the guide's pitfall, a short one and the first LED takes the wrong colour).
 *
 * Group B's clock would come out on PC9, mux B's output (the bottom row of pads, smk_adc.h). PC9 stays what the ADC
 * made it, an analog input, so nothing drives it: this header never writes PC9.
 *
 * Pins: PC10, nothing else, from smk_pins.h's table: an output, low, between frames (the line's rest).
 *
 *   smk_rgb_init()             PC10 low; every LED's colour black (nothing sent)
 *   smk_rgb_set(pad, rgb)      pad 0..15 (0..7 the top row left to right, 8..15 the bottom), 0xRRGGBB: into the frame
 *   smk_rgb_send(lcd_baud)     the frame out, then SPI1 back on group A at the screen's baud. THE CALLER: no screen
 *                              transfer running and CS high (lcd.c's lcd_sync), from the main loop only
 *
 * Checked on an Elite (2026-11-13): the lights' order along the chain (SMK_RGB_SLOT, the guide's) and the colours'
 * order; the bottom row's pads read while frames went out.
 *
 * Host tests replace the hardware access: SMK_GPIO(port, reg), SMK_REG(address) (smk_reg.h). */
#pragma once
#include <stdint.h>
#include "../fm1_cc.h"
#include "smk_reg.h"
#include "smk_pins.h"
#include "smk_lcd_hw.h"

#define SMK_RGB_N 16u
#define SMK_RGB_RESET 250u                  /* bytes of nought before the LEDs' */
#define SMK_RGB_BAUD 8u
#define SMK_RGB_ONE 0xF0u
#define SMK_RGB_NOUGHT 0xC0u
#define SMK_RGB_FRAME (SMK_RGB_RESET + SMK_RGB_N * 24u)

/* where each pad's light is along the chain (the guide's): the top row's first, the bottom row left to right,
 * then the top row back from its right end */
static const uint8_t SMK_RGB_SLOT[SMK_RGB_N] = {0, 15, 14, 13, 12, 11, 10, 9, 1, 2, 3, 4, 5, 6, 7, 8};

static uint8_t smk_rgb_frame[SMK_RGB_FRAME];        /* in RAM: SPI1's DMA reads it */

static void smk_rgb_set(uint32_t pad, uint32_t rgb)
{
    uint8_t *p = smk_rgb_frame + SMK_RGB_RESET + SMK_RGB_SLOT[pad & 15u] * 24u;
    const uint32_t grb = (rgb & 0xFF00u) << 8 | (rgb & 0xFF0000u) >> 8 | (rgb & 0xFFu);
    uint32_t i;
    for (i = 0; i < 24u; i++)
        p[i] = (uint8_t)(grb >> (23u - i) & 1u ? SMK_RGB_ONE : SMK_RGB_NOUGHT);
}

static void smk_rgb_init(void)
{
    uint32_t i;
    smk_pins_setup(SMK_OWN_RGB);                    /* PC10 an output, low */
    for (i = 0; i < SMK_RGB_RESET; i++)
        smk_rgb_frame[i] = 0;
    for (i = 0; i < SMK_RGB_N; i++)
        smk_rgb_set(i, 0);
}

static void smk_rgb_send(uint32_t lcd_baud)
{
    SMK_LCD_SPI_BAUD = SMK_RGB_BAUD;
    SMK_LCD_IOMAP_CON1 |= 0x10u;                    /* SPI1 on group B: its data on PC10 */
    SMK_LCD_SPI_CON |= 0x4000u;
    SMK_LCD_SPI_ADR = (uint32_t)(uintptr_t)smk_rgb_frame;
    SMK_LCD_SPI_CNT = SMK_RGB_FRAME;
    fm1_lcd_wait();                                 /* (the group never changes while it runs) */
    SMK_LCD_IOMAP_CON1 &= ~0x10u;                   /* back on group A, the screen's: PC10 is the GPIO again, low */
    SMK_LCD_SPI_BAUD = lcd_baud;
}
