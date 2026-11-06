/* SPDX-License-Identifier: GPL-3.0-only */
/* The host tests' .fwsc reader (ota_test.c, ldr_test.c), with its own ENC and CRC (not ota.c's: these
 * check it).
 *
 * A .fwsc is its logical image with a marker byte after each of the first n 0x2F-byte blocks; n is the
 * file's length less the logical length in the package header (u32 at 4), so a reader needs no table:
 * M-VAVE's FM-1 and SMK-37 Elite packages have 20, the SMK-37 Pro's and AISLING's SMK-37 Elite packages 36.
 * The header is the logical image's first 0x40 bytes, ENC'd; its first u16 is the CRC of the rest.
 *
 * Also the per-board values the tests check against, written out rather than taken from hal/fl_map.h so a
 * wrong map fails here (tests/smk_flash_test.c checks the map itself). */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#if defined(AISLING_BOARD_SMK37) && AISLING_BOARD_SMK37
#define T_BOARD "SMK-37 Elite"
#define T_BLOCKS 36u                /* AISLING's SMK-37 packages (tools/fm1pkg_make.py BOARDS) */
#define T_APP_HI 0x9D000u           /* the app area ends ... */
#define T_REC_HI 0xC0000u           /* ... update records below the calibration (0xC0000..0xC1FFF) */
#define T_OTA_LO 0xBB000u           /* ota.c's staging: the top of the VM */
#define T_OTA_HI 0xC0000u
#else
#define T_BOARD "FM-1"
#define T_BLOCKS 20u
#define T_APP_HI 0x93000u
#define T_REC_HI 0xFC000u
#define T_OTA_LO 0xE0000u
#define T_OTA_HI 0xE5000u
#endif
#define T_RES (T_OTA_HI - 0x100u)   /* the update record: a 4 KiB boundary less 256 */

/* raw (len bytes) -> out (at least len bytes), *out_len; returns n, or 0 when the header is not valid */
static uint32_t fwsc_logical(const uint8_t *raw, size_t len, uint8_t *out, size_t *out_len)
{
    uint8_t h[0x40];
    uint32_t i, k, n, total, key = 0xFFFFu, crc = 0;
    if (len < 20u * 0x30u)
        return 0;
    memcpy(h, raw, 0x2F);
    memcpy(h + 0x2F, raw + 0x30, 0x40 - 0x2F);
    for (i = 0; i < 0x40u; i++) {                                     /* JieLi's ENC, key 0xFFFF */
        h[i] ^= (uint8_t)key;
        key = ((key << 1) ^ (key & 0x8000u ? 0x1021u : 0u)) & 0xFFFFu;
    }
    for (i = 2; i < 0x40u; i++)                                       /* CRC-16/XMODEM */
        for (crc ^= (uint32_t)h[i] << 8, k = 0; k < 8u; k++)
            crc = (crc & 0x8000u ? (crc << 1) ^ 0x1021u : crc << 1) & 0xFFFFu;
    total = h[4] | (uint32_t)h[5] << 8 | (uint32_t)h[6] << 16 | (uint32_t)h[7] << 24;
    if (total >= len || (h[0] | (uint32_t)h[1] << 8) != crc || memcmp(h + 16, "AC791N", 6))
        return 0;
    n = (uint32_t)(len - total);
    if (n < 20u || n > 64u)
        return 0;
    for (i = 0; i < n; i++)
        memcpy(out + i * 0x2Fu, raw + i * 0x30u, 0x2F);
    memcpy(out + n * 0x2Fu, raw + n * 0x30u, len - n * 0x30u);
    *out_len = len - n;
    return n;
}
