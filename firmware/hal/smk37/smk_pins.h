/* SPDX-License-Identifier: GPL-3.0-only */
/* Every GPIO pin the SMK-37 Elite build sets up, in one table (SMK_PIN_TABLE). Each hal/smk37 driver sets its own
 * rows up with smk_pins_setup(owner) and names its pins from here, so a pin is written down once.
 *
 * The pins are from genkigenki's fm1-to-smk37 guide, 01-hardware.md (CC BY-NC 4.0; LICENSING.md).
 *
 * Never touched by the SMK build, so not in the table (SMK_PINS_UNUSED lists them for the tests):
 *   PB1    the charger's status (held low 8 s it resets the chip)
 *   PC7    the TRS MIDI OUT (UART1 TX): AISLING sends no TRS MIDI
 * Set up elsewhere, as on the FM-1 (SMK_PINS_SHARED): PC0 PC1 PC2 PC6, the codec's control lines (hal/fm1_audio.h;
 * the guide: the audio path is the FM-1's). The flash's and USB's pins are IOMAP functions (fm1_flash.h, fm1_usb.h),
 * not GPIO.
 *
 * smk_pins_setup(owner) writes by mode, then port, a whole mask at a time, in this order: the inputs (pulled up,
 * then analog) before any output, and an output's level before its direction, high outputs first. So the rows are
 * inputs before the 595s drive, and the screen's CS is high before it drives.
 *
 * Host tests replace the hardware access: SMK_GPIO(port, reg) (smk_reg.h). */
#pragma once
#include <stdint.h>
#include "smk_reg.h"

enum { SMK_OWN_INPUT, SMK_OWN_LCD, SMK_OWN_ADC, SMK_OWN_RGB, SMK_NOWN };
enum { SMK_IN_PU, SMK_ANALOG, SMK_OUT_HI, SMK_OUT_LO, SMK_NMODE };    /* smk_pins_setup's order */

/* the pins' bit numbers, as the drivers name them */
#define SMK_SR_OE 0u            /* PA0  the 595s' OE, held low */
#define SMK_SR_LATCH 1u         /* PA1  latch, on its rising edge */
#define SMK_SR_CLK 3u           /* PA3  shift, on its rising edge */
#define SMK_SR_DATA 4u          /* PA4 */
#define SMK_ROW0 2u             /* PA2  rows 0..4: the keys */
#define SMK_ROW1 5u             /* PA5 */
#define SMK_ROW2 6u             /* PA6 */
#define SMK_ROW3 7u             /* PA7 */
#define SMK_ROW4 9u             /* PA9 */
#define SMK_ROW5 6u             /* PH6  rows 5, 6: the buttons */
#define SMK_ROW6 8u             /* PH8  (the FM-1's UART RX: no TRS MIDI IN here) */
#define SMK_ROW7 9u             /* PH9  row 7: the knobs */
#define SMK_LCD_DC_BIT 5u       /* PB5  D/C (SPI1 group A's DI pin, a GPIO here) */
#define SMK_LCD_CLK_BIT 6u      /* PB6  SPI1 group A's clock */
#define SMK_LCD_DO_BIT 7u       /* PB7  SPI1 group A's data out */
#define SMK_LCD_CS_BIT 8u       /* PC8  CS, active low */
#define SMK_ADC_MUXC_BIT 10u    /* PA10 mux C: faders, wheels, pedal, battery (ADC ch2) */
#define SMK_ADC_MUXA_BIT 8u     /* PA8  mux A: the pads' top row (ADC ch1) */
#define SMK_ADC_MUXB_BIT 9u     /* PC9  mux B: the pads' bottom row (ADC ch8) */
#define SMK_RGB_DO_BIT 10u      /* PC10 the pads' RGB LEDs: SPI1 group B's data out while a frame goes (smk_rgb.h) */

typedef struct { uint8_t owner, port, bit, mode; } smk_pin_t;
static const smk_pin_t SMK_PIN_TABLE[] = {
    {SMK_OWN_INPUT, FM1_PA, SMK_ROW0, SMK_IN_PU},
    {SMK_OWN_INPUT, FM1_PA, SMK_ROW1, SMK_IN_PU},
    {SMK_OWN_INPUT, FM1_PA, SMK_ROW2, SMK_IN_PU},
    {SMK_OWN_INPUT, FM1_PA, SMK_ROW3, SMK_IN_PU},
    {SMK_OWN_INPUT, FM1_PA, SMK_ROW4, SMK_IN_PU},
    {SMK_OWN_INPUT, FM1_PH, SMK_ROW5, SMK_IN_PU},
    {SMK_OWN_INPUT, FM1_PH, SMK_ROW6, SMK_IN_PU},
    {SMK_OWN_INPUT, FM1_PH, SMK_ROW7, SMK_IN_PU},
    {SMK_OWN_INPUT, FM1_PA, SMK_SR_OE, SMK_OUT_LO},
    {SMK_OWN_INPUT, FM1_PA, SMK_SR_LATCH, SMK_OUT_LO},
    {SMK_OWN_INPUT, FM1_PA, SMK_SR_CLK, SMK_OUT_LO},
    {SMK_OWN_INPUT, FM1_PA, SMK_SR_DATA, SMK_OUT_LO},
    {SMK_OWN_LCD, FM1_PC, SMK_LCD_CS_BIT, SMK_OUT_HI},
    {SMK_OWN_LCD, FM1_PB, SMK_LCD_DC_BIT, SMK_OUT_LO},
    {SMK_OWN_LCD, FM1_PB, SMK_LCD_CLK_BIT, SMK_OUT_LO},
    {SMK_OWN_LCD, FM1_PB, SMK_LCD_DO_BIT, SMK_OUT_LO},
    {SMK_OWN_ADC, FM1_PA, SMK_ADC_MUXC_BIT, SMK_ANALOG},
    {SMK_OWN_ADC, FM1_PA, SMK_ADC_MUXA_BIT, SMK_ANALOG},
    {SMK_OWN_ADC, FM1_PC, SMK_ADC_MUXB_BIT, SMK_ANALOG},
    {SMK_OWN_RGB, FM1_PC, SMK_RGB_DO_BIT, SMK_OUT_LO},
};
#define SMK_NPIN (sizeof SMK_PIN_TABLE / sizeof SMK_PIN_TABLE[0])

/* the pins nothing here touches, and the codec's (fm1_audio.h's), as {port, bit} */
static const uint8_t SMK_PINS_UNUSED[][2] = {{FM1_PB, 1}, {FM1_PC, 7}};
static const uint8_t SMK_PINS_SHARED[][2] = {{FM1_PC, 0}, {FM1_PC, 1}, {FM1_PC, 2}, {FM1_PC, 6}};

/* one owner's pins of one mode on one port, as a mask */
static uint32_t smk_pins_mask(uint32_t owner, uint32_t port, uint32_t mode)
{
    uint32_t i, m = 0;
    for (i = 0; i < SMK_NPIN; i++)
        if (SMK_PIN_TABLE[i].owner == owner && SMK_PIN_TABLE[i].port == port && SMK_PIN_TABLE[i].mode == mode)
            m |= 1u << SMK_PIN_TABLE[i].bit;
    return m;
}

static void smk_pins_setup(uint32_t owner)
{
    static const uint8_t ports[] = {FM1_PA, FM1_PB, FM1_PC, FM1_PH};
    uint32_t mode, k;
    for (mode = 0; mode < SMK_NMODE; mode++)
        for (k = 0; k < sizeof ports; k++) {
            const uint32_t p = ports[k], m = smk_pins_mask(owner, p, mode);
            if (!m)
                continue;
            switch (mode) {
            case SMK_IN_PU:                     /* a digital input, pulled up */
                SMK_GPIO(p, FM1_DIE) |= m;
                SMK_GPIO(p, FM1_DIR) |= m;
                SMK_GPIO(p, FM1_PD) &= ~m;
                SMK_GPIO(p, FM1_PU) |= m;
                break;
            case SMK_ANALOG:                    /* an ADC input: no digital input, no pulls */
                SMK_GPIO(p, FM1_DIE) &= ~m;
                SMK_GPIO(p, FM1_PU) &= ~m;
                SMK_GPIO(p, FM1_PD) &= ~m;
                SMK_GPIO(p, FM1_DIR) |= m;
                break;
            default:                            /* an output, no pulls: its level, then drive it */
                SMK_GPIO(p, FM1_DIE) |= m;
                SMK_GPIO(p, FM1_PU) &= ~m;
                SMK_GPIO(p, FM1_PD) &= ~m;
                if (mode == SMK_OUT_HI)
                    SMK_GPIO(p, FM1_OUT) |= m;
                else
                    SMK_GPIO(p, FM1_OUT) &= ~m;
                SMK_GPIO(p, FM1_DIR) &= ~m;
                break;
            }
        }
}
