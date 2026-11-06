/* SPDX-License-Identifier: GPL-3.0-only */
/* The M-VAVE SMK-37 Elite's SARADC: the three analog muxes' outputs, polled, 10 bit.
 *
 * From genkigenki's fm1-to-smk37 guide, 01-hardware.md (CC BY-NC 4.0; LICENSING.md): mux A on PA8 (ch1), mux B on
 * PC9 (ch8), mux C on PA10 (ch2), all three addressed by bits 5..3 of the 595 word (smk_input.h's smk_mux). Mux C:
 * 0..3 the faders, 4 the pitch wheel, 5 the mod wheel, 6 the pedal, 7 the battery. And the timing: CON 0x400D, where
 * fm1_adc.h's 0xF04E takes ~200 us a conversion, too slow for the pads at one a 100 us tick. The read sequence is
 * fm1_adc.h's (Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments) with that CON.
 *
 * Stage 1 reads the battery alone (mux C, address 7), from the main loop, so smk_adc_init() sets up PA10 alone. The
 * pads (muxes A and B; PC9 is also SPI1 group B's clock, which the pads' LEDs use), the faders and the wheels are
 * stage 4's. PB1 (the charger's status) is no ADC input here and is never touched.
 *
 *   smk_adc_init()      PA10 an analog input, no pulls
 *   smk_adc_read(ch)    0..1023, -1 on timeout
 *
 * Host tests replace the hardware access: SMK_GPIO(port, reg), SMK_REG(address) (smk_reg.h). */
#pragma once
#include <stdint.h>
#include "smk_reg.h"

#define SMK_ADC_CON SMK_REG(0x13100u)
#define SMK_ADC_RES SMK_REG(0x13104u)
#define SMK_WLA_CON0 SMK_REG(0x11900u)
enum { SMK_ADC_MUXA = 1, SMK_ADC_MUXC = 2, SMK_ADC_MUXB = 8 };            /* channels */
enum { SMK_MUX_PITCH = 4, SMK_MUX_MOD = 5, SMK_MUX_PEDAL = 6, SMK_MUX_BATT = 7 };   /* mux C's addresses */

static void smk_adc_init(void)
{
    const uint32_t m = 1u << 10;
    SMK_GPIO(FM1_PA, FM1_DIE) &= ~m;
    SMK_GPIO(FM1_PA, FM1_PU) &= ~m;
    SMK_GPIO(FM1_PA, FM1_PD) &= ~m;
    SMK_GPIO(FM1_PA, FM1_DIR) |= m;
}

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
