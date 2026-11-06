/* SPDX-License-Identifier: GPL-3.0-only */
/* The M-VAVE SMK-37 Elite's flash (1 MiB) and where AISLING may write in it. Included by fl_map.h, which
 * says what each macro is for; macros only.
 *
 * The regions are M-VAVE's own, from the app area's descriptors in SMK-37 Elite_016.fwsc (the package's
 * BOARDS["smk37"], tools/fm1pkg_make.py; tests/pkg_test.py compares them):
 *
 *   0x00000-0x03FFF  the head: SPL, isd_config, the chip key           NEVER WRITTEN
 *   0x04000-0x9CFFF  the app area                                      the update loader
 *   0x9D000-0xBFFFF  VM: the stock firmware's settings                 the app's update staging, its top
 *                                                                      five sectors 0xBB000-0xBFFFF
 *   0xC0000-0xC0FFF  BTIF: Bluetooth pairings                          NEVER WRITTEN
 *   0xC1000-0xC1FFF  USRTRIM: the per-key velocity calibration,        NEVER WRITTEN: without a backup it
 *                    marker "trimdat" at 0xC1224                       cannot be recovered
 *   0xC2000-0xEAFFF  USRFLASH: the stock presets                       AISLING's store, 0xC2000-0xC5FFF
 *                                                                      (the stock backup restores them)
 *   0xEB000-0xF3FFF  (not in the package)                              not written
 *   0xF4000-0xFDFFF  USR: the stock resources                          NEVER WRITTEN
 *   0xFE000-0xFEFFF  (not in the package)                              NEVER WRITTEN
 *   0xFF000-0xFFFFF  key_mac                                           NEVER WRITTEN
 *
 * The FM-1's map would write three of the protected ranges (its store crosses BTIF and USRTRIM, its project
 * sits in USR, its loader's window crosses BTIF and USRTRIM), hence this one. The _Static_asserts below keep
 * every window off them; tests/smk_flash_test.c checks the guards byte by byte. */
#pragma once

/* the protected ranges, [lo, hi) */
#define SMK_HEAD_LO     0x00000000u                /* SPL, isd_config */
#define SMK_HEAD_HI     0x00004000u
#define SMK_CAL_LO      0x000C0000u                /* BTIF, USRTRIM */
#define SMK_CAL_HI      0x000C2000u
#define SMK_TOP_LO      0x000F4000u                /* USR, key_mac: to the end of the part */
#define SMK_TOP_HI      0x00100000u

#define FL_APP_LO       0x00004000u                /* = the package's flash.bin, less the head */
#define FL_APP_HI       0x0009D000u
#define FL_REC_LO       0x0009D000u                /* update records: anywhere in the VM, where the stock */
#define FL_REC_HI       0x000C0000u                /* firmware's updater and ours stage */
#define FL_PLAIN_LO     0x0009D000u
#define FL_OTA_LO       0x000BB000u                /* the VM's top five sectors */
#define FL_OTA_HI       0x000C0000u
#define FL_DATA_LO      0x000C2000u                /* AISLING's store: four sectors of the stock presets */
#define FL_DATA_HI      0x000C6000u
#define FL_PROJ_SECTOR  0x000C2000u                /* the project (its pieces in it) */
#define FL_PANEL_SECTOR 0x000C4000u                /* the panel calibration, should the SMK ever need one */

#define FL_STORE_OK(off, n) FL_IN(off, n, FL_DATA_LO, FL_DATA_HI)
#define FL_LDR_RANGE_OK(off, n) (FL_IN(off, n, FL_APP_LO, FL_APP_HI) || FL_IN(off, n, FL_REC_LO, FL_REC_HI))

/* The flash part: 0xEB6014 (1 MiB), as genkigenki's fm1-to-smk37 guide read it on an SMK-37 Pro. Any other
 * part: RAM only, nothing saved, until it is read from a unit and added here. (The FM-1's list is in
 * fm1_flash.h.) */
#define FL_JEDEC_OK(id) ((id) == 0xEB6014u)

/* [lo, hi) clear of every protected range */
#define SMK_CLEAR(lo, hi) ((hi) <= (lo) || (((hi) <= SMK_HEAD_LO || (lo) >= SMK_HEAD_HI) && \
                           ((hi) <= SMK_CAL_LO || (lo) >= SMK_CAL_HI) && ((hi) <= SMK_TOP_LO || (lo) >= SMK_TOP_HI)))
_Static_assert(SMK_CLEAR(FL_APP_LO, FL_APP_HI), "smk_flash_map.h: the app area is clear of the protected ranges");
_Static_assert(SMK_CLEAR(FL_REC_LO, FL_REC_HI), "smk_flash_map.h: the loader's records are clear of them");
_Static_assert(SMK_CLEAR(FL_OTA_LO, FL_OTA_HI), "smk_flash_map.h: the update staging is clear of them");
_Static_assert(SMK_CLEAR(FL_DATA_LO, FL_DATA_HI), "smk_flash_map.h: the store is clear of them");
_Static_assert(FL_OTA_LO >= FL_PLAIN_LO && FL_OTA_HI <= SMK_CAL_LO && FL_DATA_LO >= SMK_CAL_HI &&
               FL_DATA_HI <= 0x000EB000u, "smk_flash_map.h: the staging is in the VM, the store in USRFLASH");
