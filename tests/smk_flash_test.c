/* SPDX-License-Identifier: GPL-3.0-only */
/* The SMK-37 Elite's flash map (hal/smk37/smk_flash_map.h, through hal/fl_map.h), checked through the
 * guards and code the firmware runs:
 *   - the app's driver guard (FL_RANGE_OK), the store's (FL_STORE_OK) and the update loader's
 *     (FL_LDR_RANGE_OK) accept exactly the planned windows and refuse every byte of the ranges AISLING
 *     must never write, wrapped ranges included;
 *   - storage.c's saves touch only the store, and the rest of the part keeps every byte;
 *   - ota.c's staging and record, and the loader's record scan, stay in the VM.
 * The ranges and windows are typed out below from the plan, not taken from the header.
 *   cc -O2 -DAISLING_BOARD_SMK37=1 -o smk_flash_test tests/smk_flash_test.c && ./smk_flash_test
 * Built without the flag (the FM-1's map) it must FAIL; run_tests.sh runs that as the control. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../firmware/hal/fl_map.h"      /* the board's flash map: OTA_AREA, LDR_*, the objects */

#define FELUCCA_ID "ota-SMK-37 Elite_900"
#define FELUCCA_OTA_DRYRUN 0

typedef struct { uint32_t lo, hi; } win_t;            /* [lo, hi) */
static const win_t PROT[] = {{0x00000u, 0x04000u},    /* the head: SPL, isd_config, the chip key */
                             {0xC0000u, 0xC2000u},    /* BTIF, USRTRIM (the velocity calibration) */
                             {0xF4000u, 0x100000u}};  /* USR, key_mac */
static const win_t STORE[] = {{0xC2000u, 0xC6000u}};
static const win_t APP[] = {{0xC2000u, 0xC6000u}, {0xBB000u, 0xC0000u}};    /* the store, the staging */
static const win_t LOADER[] = {{0x04000u, 0x9D000u}, {0x9D000u, 0xC0000u}}; /* the app area, the VM */
#define NW(a) (sizeof a / sizeof a[0])

static int failed;
static void ok(int cond, const char *what)
{
    printf("%-72s %s\n", what, cond ? "ok" : "FAIL");
    failed += !cond;
}

static int inside(const win_t *w, unsigned nw, uint32_t off, uint32_t n)   /* [off, off + n) in one window */
{
    unsigned i;
    for (i = 0; i < nw; i++)
        if (off >= w[i].lo && (uint64_t)off + n <= w[i].hi)
            return 1;
    return 0;
}
static int touches_prot(uint32_t off, uint32_t n)   /* any byte protected, or past the 1 MiB part (it wraps low) */
{
    unsigned i;
    if (!n)
        return 0;
    if ((uint64_t)off + n > 0x100000u)
        return 1;
    for (i = 0; i < NW(PROT); i++)
        if (off < PROT[i].hi && off + n > PROT[i].lo)
            return 1;
    return 0;
}

/* --- storage.c, on a simulated NOR part, behind the firmware's guard (main.c st_erase / st_prog) --- */
static uint8_t nor[0x100000], before[0x100000];
static uint32_t st_bad;
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + (off & 0xFFFFFu), n); return 0; }
static int st_erase(uint32_t off)
{
    if ((off & 0xFFFu) || !FL_STORE_OK(off, 0x1000u) || !inside(STORE, NW(STORE), off, 0x1000u) || touches_prot(off, 0x1000u)) {
        printf("  storage erase at %#x\n", off);
        st_bad++;
        return -8;
    }
    memset(nor + off, 0xFF, 0x1000);
    return 0;
}
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i;
    if (!FL_STORE_OK(off, n) || !inside(STORE, NW(STORE), off, n) || touches_prot(off, n) || (off & 0xFFu) + n > 256u) {
        printf("  storage program at %#x, %u B\n", off, n);
        st_bad++;
        return -8;
    }
    for (i = 0; i < n; i++)
        nor[off + i] &= s[i];
    return 0;
}

/* --- ota.c: the frames never come; only the staging's own flash calls are driven --- */
static uint32_t ota_bad, ota_erased, ota_progd, now_ms;
static int ota_wire_send(const uint8_t *p, uint32_t n) { (void)p; (void)n; return 0; }
static int ota_frame_get(const uint8_t **p, uint32_t *n) { (void)p; (void)n; return 0; }
static void ota_frame_done(void) {}
static uint32_t ota_now_ms(void) { return now_ms; }
static void ota_idle(void) { now_ms++; }
static int ota_flash_ok(uint32_t off, uint32_t n)
{
    return FL_RANGE_OK(off, n) && inside(APP, NW(APP), off, n) && !touches_prot(off, n) &&
           off >= 0xBB000u && (uint64_t)off + n <= 0xC0000u;
}
static int ota_erase(uint32_t off)
{
    if ((off & 0xFFFu) || !ota_flash_ok(off, 0x1000u)) { printf("  ota erase at %#x\n", off); ota_bad++; return -8; }
    memset(nor + off, 0xFF, 0x1000);
    ota_erased++;
    return 0;
}
static int ota_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i;
    if (!ota_flash_ok(off, n)) { printf("  ota program at %#x, %u B\n", off, n); ota_bad++; return -8; }
    for (i = 0; i < n; i++)
        nor[off + i] &= s[i];
    ota_progd += n;
    return 0;
}
static int ota_fread(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + (off & 0xFFFFFu), n); return 0; }
static void ota_show(uint32_t step, int32_t code) { (void)step; (void)code; }
static void ota_commit(const uint8_t *parm) { (void)parm; }

#include "../firmware/src/app/plat.h"
#include "../firmware/src/storage.c"
#include "../firmware/src/ota.c"

#define G_STORE 0
#define G_APP 1
#define G_LDR 2
static int guard(int g, uint32_t off, uint32_t n)
{
    return g == G_STORE ? FL_STORE_OK(off, n) : g == G_APP ? FL_RANGE_OK(off, n) : FL_LDR_RANGE_OK(off, n);
}

/* every offset of the part and past it, a spread of lengths, and the wrapping ones */
static void sweep(int g, const win_t *w, unsigned nw, const char *name)
{
    static const uint32_t LEN[] = {0, 1, 2, 0x80, 0x100, 0xFFF, 0x1000, 0x1001, 0x2000, 0x5000, 0x23000,
                                   0x40000, 0x9D000, 0x100000, 0x7FFFFFFFu, 0xFFFFFFFFu};
    static const uint32_t WRAP[] = {0, 1, 0x1000, 0x4000, 0xC1000, 0xF4000};
    uint64_t o;
    uint32_t off, mism = 0, prot = 0, acc = 0;
    unsigned i;
    char line[128];
    for (o = 0; o < 0x110000u + 0x2000u; o++) {
        off = o < 0x110000u ? (uint32_t)o : 0xFFFFFFFFu - (uint32_t)(o - 0x110000u);   /* and the top 8 KiB */
        for (i = 0; i < NW(LEN) + NW(WRAP); i++) {
            uint32_t n = i < NW(LEN) ? LEN[i] : (uint32_t)(0u - off) + WRAP[i - NW(LEN)];   /* off + n wraps */
            int a = guard(g, off, n);
            acc += a;
            mism += a != inside(w, nw, off, n);
            prot += a && touches_prot(off, n);
        }
    }
    snprintf(line, sizeof line, "%s: refuses every protected byte and the wrap (%u accepted)", name, acc);
    ok(!prot && acc, line);
    snprintf(line, sizeof line, "%s: accepts exactly its planned windows", name);
    ok(!mism, line);
}

int main(void)
{
    uint32_t i, s, k;
    char line[128];

    sweep(G_STORE, STORE, NW(STORE), "store FL_STORE_OK");
    sweep(G_APP, APP, NW(APP), "app FL_RANGE_OK");
    sweep(G_LDR, LOADER, NW(LOADER), "loader FL_LDR_RANGE_OK");

#ifdef FL_JEDEC_OK
    ok(FL_JEDEC_OK(0xEB6014u) && !FL_JEDEC_OK(0x856014u) && !FL_JEDEC_OK(0xB36014u) && !FL_JEDEC_OK(0u) &&
       !FL_JEDEC_OK(0xFFFFFFu), "flash part: 0xEB6014 only (the FM-1's parts are not this board's)");
#else
    ok(0, "flash part: the map brings no JEDEC list");
#endif

    /* storage.c: both objects, many saves (A/B alternate), every length up to the maximum */
    for (i = 0; i < sizeof nor; i++)
        nor[i] = (uint8_t)(i * 37u + (i >> 12));
    memcpy(before, nor, sizeof nor);
    {
        static uint8_t src[PLAT_STORE_MAX], dst[PLAT_STORE_MAX];
        int bad_load = 0;
        for (k = 0; k < 12; k++)
            for (i = 0; i < OBJ_NOBJ; i++) {
                uint32_t len = k == 11 ? PLAT_STORE_MAX : 1u + (k * 997u + i * 131u) % PLAT_STORE_MAX;
                uint32_t j;
                for (j = 0; j < len; j++)
                    src[j] = (uint8_t)(j ^ k ^ (i << 4));
                if (st_save(i, src, len) || st_load(i, dst, sizeof dst) != (int)len || memcmp(src, dst, len))
                    bad_load++;
            }
        ok(!st_bad, "storage: every erase / program in the store, none refused");
        ok(!bad_load, "storage: each save loads back");
    }
    {
        uint32_t changed_out = 0, changed_in = 0;
        for (i = 0; i < sizeof nor; i++) {
            int in = inside(STORE, NW(STORE), i, 1);
            if (nor[i] != before[i])
                in ? changed_in++ : changed_out++;
        }
        snprintf(line, sizeof line, "storage: no byte outside 0xC2000..0xC5FFF changed (%u inside did)", changed_in);
        ok(!changed_out && changed_in, line);
    }

    /* ota.c: the staging area, its head, the record, the boot cleanup */
    ok(OTA_AREA == 0xBB000u && OTA_AREA_LEN == 0x5000u && OTA_RES == 0xBFF00u,
       "ota: staging 0xBB000, 5 x 4 KiB, record at 0xBFF00 (4K boundary - 256)");
    ok(ota_flash_ok(OTA_RES, 112u) && ota_flash_ok(OTA_AREA, OTA_AREA_LEN),
       "ota: the record and the whole staging area pass the app's guard");
    memcpy(before, nor, sizeof nor);
    ota_unstage();
    ok(!ota_bad && ota_erased == 5, "ota: unstage erases the five staging sectors, and only them");
    ok(!ota_stage_head(0x4DE1u) && !ota_bad && ota_progd == 32, "ota: the staged loader's head goes to OTA_AREA");
    ok(ota_boot_cleanup() == 1 && !ota_bad && ota_erased == 10, "ota: the boot cleanup finds it and erases the area");
    {
        uint32_t changed_out = 0;
        for (i = 0; i < sizeof nor; i++)
            changed_out += nor[i] != before[i] && !(i >= 0xBB000u && i < 0xC0000u);
        ok(!changed_out, "ota: no byte outside 0xBB000..0xBFFFF changed");
    }

    /* the loader's record scan (ldr_core.c step 4: read s - 256, erase s - 4 KiB, over FL_REC_*) */
    {
        uint32_t bad = 0, found = 0;
        for (s = FL_REC_HI; s > FL_REC_LO; s -= 0x1000u) {
            bad += !FL_LDR_RANGE_OK(s - 0x100u, 80u) || touches_prot(s - 0x100u, 80u);
            bad += !FL_LDR_RANGE_OK(s - 0x1000u, 0x1000u) || !inside(LOADER, NW(LOADER), s - 0x1000u, 0x1000u) ||
                   touches_prot(s - 0x1000u, 0x1000u);
            found += s - 0x100u == OTA_RES;
        }
        ok(!bad, "loader: the record scan reads and erases only the VM");
        ok(found == 1, "loader: the scan reaches the app's record (OTA_RES) and clears it");
        ok(FL_APP_LO == 0x4000u && FL_APP_HI == 0x9D000u, "loader: writes the app area 0x4000..0x9CFFF");
    }

    printf("smk-flash: %s\n", failed ? "FAILED" : "all ok");
    return failed ? 1 : 0;
}
