/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * AISLING: the map taken out of fm1_flash.h, one per board. */
/* Where each board's flash may be written. Macros only, so the host tests check the same guards the
 * firmware runs: the RAM flash driver (fm1_flash.h), the update loader (firmware/loader), the update entry
 * (ota.c) and the store (storage.c) all take their windows from here.
 *
 *   FL_APP_LO..HI      the app area; only the update loader writes it
 *   FL_REC_LO..HI      where the loader looks for update records to clear (each at a 4 KiB boundary - 256)
 *   FL_OTA_LO..HI      the app's update staging (ota.c): the package's loader and its record
 *   FL_PLAIN_LO        XIP reads from here up are plain, not decrypted (fl_plain_window_init)
 *   FL_PROJ_SECTOR     storage.c's objects: copy A, and copy B in the next sector
 *   FL_PANEL_SECTOR
 *   FL_STORE_OK(off, n)       the store's window
 *   FL_LDR_RANGE_OK(off, n)   the loader's: the app area and the update records
 *   FL_RANGE_OK(off, n)       the app's: the store and the staging (the loader sets its own)
 *
 * The FM-1's map is below. The SMK-37 Elite's, with the ranges it must never write, is
 * hal/smk37/smk_flash_map.h. */
#pragma once
#include <stdint.h>

/* [off, off + n) inside [lo, hi), without wrapping: off + n can overflow, and
 * the 1 MiB part ignores the high address bits, so a wrapped range lands low. */
#define FL_IN(off, n, lo, hi) ((uint32_t)(off) >= (lo) && (uint32_t)(off) <= (hi) && \
                               (uint32_t)(n) <= (hi) - (uint32_t)(off))

#if defined(AISLING_BOARD_SMK37) && AISLING_BOARD_SMK37
#include "smk37/smk_flash_map.h"
#else
#define FL_APP_LO       0x00004000u                /* app area (the head below it is never written) */
#define FL_APP_HI       0x00093000u
#define FL_REC_LO       0x00093000u                /* update records: above the app ... */
#define FL_REC_HI       0x000FC000u                /* ... and below Felucca's globals */
#define FL_PLAIN_LO     0x00093000u
#define FL_DATA_LO      0x00097000u                /* Felucca main store */
#define FL_DATA_HI      0x000E0000u
#define FL_GLOB_LO      0x000FC000u                /* Felucca superblock / globals */
#define FL_GLOB_HI      0x000FF000u
#define FL_OTA_LO       0x000E0000u                /* M-UPGRADE loader staging, ota.c */
#define FL_OTA_HI       0x000E5000u
#define FL_PROJ_SECTOR  0x000FC000u                /* AMB-1: the project in Felucca's FL_GLOB ... */
#define FL_PANEL_SECTOR 0x000DC000u                /* ... the panel calibration in its user-preset area */
/* Felucca's own store (projects, user samples; settings) */
#define FL_STORE_OK(off, n) (FL_IN(off, n, FL_DATA_LO, FL_DATA_HI) || FL_IN(off, n, FL_GLOB_LO, FL_GLOB_HI))
/* the loader may write the app area and erase update records; never the head */
#define FL_LDR_RANGE_OK(off, n) (FL_IN(off, n, FL_APP_LO, FL_APP_HI) || FL_IN(off, n, FL_REC_LO, FL_REC_HI))
#endif

/* Where the RAM driver may erase / program. The app build allows only its own
 * data regions; the update loader (firmware/loader) defines its own window. */
#ifndef FL_RANGE_OK
#define FL_RANGE_OK(off, n) (FL_STORE_OK(off, n) || FL_IN(off, n, FL_OTA_LO, FL_OTA_HI))
#endif

_Static_assert(FL_STORE_OK(FL_PROJ_SECTOR, 0x2000u) && FL_STORE_OK(FL_PANEL_SECTOR, 0x2000u),
               "fl_map.h: storage.c's objects (A and B) lie in the store");
_Static_assert(FL_IN(FL_OTA_HI - 0x100u, 0x100u, FL_REC_LO, FL_REC_HI) && !(FL_OTA_HI & 0xFFFu),
               "fl_map.h: the loader finds the app's update record (ota.c OTA_RES) and clears it");
_Static_assert(FL_APP_LO == 0x4000u && FL_APP_HI <= FL_PLAIN_LO,
               "fl_map.h: the head is below the app area, and the app area reads decrypted");
