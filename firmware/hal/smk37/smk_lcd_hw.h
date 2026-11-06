/* SPDX-License-Identifier: GPL-3.0-only */
/* The M-VAVE SMK-37 Elite's LCD wiring: hal/fm1_lcd_hw.h's interface on the SMK's pins, so src/lcd.c drives the
 * screen unchanged.
 *
 * The wiring is from genkigenki's fm1-to-smk37 guide, 01-hardware.md (CC BY-NC 4.0; LICENSING.md): an ST7789V,
 * 240 x 240, no window offset (src/lcd.c has none), on SPI1's pin group A, CLK PB6 and DO PB7 (the SDK's
 * wl82/asm/spi.h: group A = IOMAP_CON1 bit 4 clear; the FM-1 sets it, group B), CS PC8 (active low) and D/C PB5,
 * group A's DI pin, a GPIO here as the FM-1's D/C is group B's (PC8). No reset pin; the backlight is always on.
 * The SPI1 sequence is fm1_lcd_hw.h's (Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments),
 * typed again with these pins.
 *
 * Pins: PB5 PB6 PB7 PC8, nothing else. Never port A (the 595s and the matrix rows: smk_input.h), never PB1 (the
 * charger's status: held low 8 s it resets the chip).
 *
 *   fm1_lcd_hw_init()          pins, SPI1 master on group A at BAUD 4
 *   fm1_lcd_baud(b)            SPI1 clock = lsb / (b + 1)
 *   fm1_lcd_send_cmd(c)        D/C low, CS low, one byte (then fm1_lcd_wait)
 *   fm1_lcd_send_data(p, n)    D/C high, CS low, DMA n bytes from RAM p
 *   fm1_lcd_wait()             SPI done (or timeout, counted), pending cleared
 *   fm1_lcd_deselect()         CS high
 *
 * NOT YET CHECKED ON THE DEVICE (stage 3): the clock (main.c's LCD_BAUD 4, 12 MHz, the stock's and the guide's; the
 * FM-1 runs BAUD 0). Stage 4: the pads' RGB LEDs share SPI1 on group B (PC10), so SPI1 will change groups between
 * the screen and the LEDs.
 *
 * Host tests replace the hardware access: SMK_GPIO(port, reg), SMK_REG(address) (smk_reg.h). */
#pragma once
#include <stdint.h>
#include "../fm1_cc.h"
#include "smk_reg.h"

#define SMK_LCD_IOMAP_CON1 SMK_REG(0x51020u)
#define SMK_LCD_SPI_CON SMK_REG(0x11D00u)
#define SMK_LCD_SPI_BAUD SMK_REG(0x11D04u)
#define SMK_LCD_SPI_BUF SMK_REG(0x11D08u)
#define SMK_LCD_SPI_ADR SMK_REG(0x11D0Cu)
#define SMK_LCD_SPI_CNT SMK_REG(0x11D10u)
#define SMK_LCD_DC (1u << 5)                /* port B */
#define SMK_LCD_CLK (1u << 6)               /* port B */
#define SMK_LCD_DO (1u << 7)                /* port B */
#define SMK_LCD_CS (1u << 8)                /* port C */

static uint32_t fm1_lcd_timeouts;

FM1_INLINE void fm1_lcd_hw_init(void)
{
    const uint32_t pb = SMK_LCD_DC | SMK_LCD_CLK | SMK_LCD_DO;
    SMK_LCD_IOMAP_CON1 &= ~0x10u;                   /* SPI1 on group A */
    SMK_GPIO(FM1_PC, FM1_OUT) |= SMK_LCD_CS;        /* high before it drives */
    SMK_GPIO(FM1_PC, FM1_DIR) &= ~SMK_LCD_CS;
    SMK_GPIO(FM1_PB, FM1_OUT) &= ~pb;
    SMK_GPIO(FM1_PB, FM1_DIR) &= ~pb;
    SMK_LCD_SPI_CON = 0x4021u;
    SMK_LCD_SPI_BAUD = 4u;
}

FM1_INLINE void fm1_lcd_baud(uint32_t b) { SMK_LCD_SPI_BAUD = b; }

static void fm1_lcd_wait(void)
{
    uint32_t n;
    for (n = 0; n < 4000000u && !(SMK_LCD_SPI_CON & 0x8000u); n++)
        ;
    if (n == 4000000u)
        fm1_lcd_timeouts++;
    SMK_LCD_SPI_CON |= 0x4000u;
}

FM1_INLINE void fm1_lcd_deselect(void) { SMK_GPIO(FM1_PC, FM1_OUT) |= SMK_LCD_CS; }

FM1_INLINE void fm1_lcd_send_cmd(uint8_t c)
{
    SMK_GPIO(FM1_PB, FM1_OUT) &= ~SMK_LCD_DC;
    SMK_GPIO(FM1_PC, FM1_OUT) &= ~SMK_LCD_CS;
    SMK_LCD_SPI_CON |= 0x4000u;
    SMK_LCD_SPI_BUF = c;
}

FM1_INLINE void fm1_lcd_send_data(const void *p, uint32_t n)
{
    SMK_GPIO(FM1_PB, FM1_OUT) |= SMK_LCD_DC;
    SMK_GPIO(FM1_PC, FM1_OUT) &= ~SMK_LCD_CS;
    SMK_LCD_SPI_CON |= 0x4000u;
    SMK_LCD_SPI_ADR = (uint32_t)(uintptr_t)p;
    SMK_LCD_SPI_CNT = n;
}
