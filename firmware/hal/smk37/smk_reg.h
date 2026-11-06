/* SPDX-License-Identifier: GPL-3.0-only */
/* The SMK-37 Elite's hal headers reach the hardware through these two, so a host test can put fake registers
 * behind them (tests/host/smk_input_test.c, smk_hw_test.c):
 *   SMK_GPIO(port, reg)   a GPIO port register (hal/fm1_gpio.h's ports and offsets)
 *   SMK_REG(address)      any other special-function register */
#pragma once
#include <stdint.h>
#include "../fm1_gpio.h"

#ifndef SMK_GPIO
#define SMK_GPIO(p, r) FM1_PR(p, r)
#endif
#ifndef SMK_REG
#define SMK_REG(a) (*(volatile uint32_t *)(a))
#endif
