/* SPDX-License-Identifier: GPL-3.0-only */
/* A sound pack: eight pieces as data (tools/make_pack.py writes one from packs/NAME.json; the Packs page sends it
 * over the link, area 10; the FM-1 keeps one, OBJ_PACK, beside its own eight). Bytes, little-endian:
 *
 *    0  "APK1"
 *    4  format (1)      5  level: the engine it needs (EN_LEVEL or lower)      6  pieces (8)      7  0
 *    8  length.2: all of it      10  version.2      12  name[20], NUL-padded
 *   32  the pieces, one after another:
 *         name[16], NUL-padded      flags (bit 0: a Saint, the white screen)      air notes (0..16)      pairs
 *         0
 *         the air: its notes' semitones over the root (int8), then their sixteenths (1..255)
 *         the recipe: pairs of parameter (a byte), value (int16), over the defaults (en_recipe_apply)
 *
 * Nothing here is trusted: pk_check walks the lot before a pack is kept or played. A parameter the engine has not
 * is refused (the level says so first); a value out of range is clamped as it is applied. */
#ifndef PACK_H
#define PACK_H
#include <stdint.h>
#if defined(__has_include) && __has_include(<string.h>)
#include <string.h>
#else                                            /* (the device: freestanding, firmware/src/libc.c's own) */
int memcmp(const void *a, const void *b, unsigned n);
#endif
#include "../dsp/amb.h"

#define PK_FORMAT 1
#define PK_NPIECES 8
#define PK_HEAD 32
#define PK_NAME 20                               /* (the pack's name, its NUL in them) */
#define PK_PNAME 16                              /* (a piece's) */
#define PK_MAX 3840                              /* (a storage object's payload) */
#define PK_LIGHT 1u

typedef struct {
    const uint8_t *name;                         /* (PK_PNAME bytes, NUL-terminated: pk_check) */
    const int8_t *air_s;
    const uint8_t *air_d, *pairs;
    uint8_t flags, air_n, npairs;
} pk_piece_t;

static inline int pk_len(const uint8_t *b) { return b[8] | b[9] << 8; }
static inline int pk_version(const uint8_t *b) { return b[10] | b[11] << 8; }
static inline int pk_level(const uint8_t *b) { return b[5]; }
static inline const char *pk_name(const uint8_t *b) { return (const char *)b + 12; }

/* piece i of a pack len bytes long: 0, or -1 if the pack ends before it does */
static inline int pk_piece(const uint8_t *b, int len, int i, pk_piece_t *p)
{
    int o = PK_HEAD, k;
    for (k = 0;; k++) {
        if (o + PK_PNAME + 4 > len)
            return -1;
        p->name = b + o;
        p->flags = b[o + PK_PNAME], p->air_n = b[o + PK_PNAME + 1], p->npairs = b[o + PK_PNAME + 2];
        o += PK_PNAME + 4;
        if (p->air_n > 16 || o + 2 * p->air_n + 3 * p->npairs > len)
            return -1;
        p->air_s = (const int8_t *)b + o, p->air_d = b + o + p->air_n;
        p->pairs = b + o + 2 * p->air_n;
        o += 2 * p->air_n + 3 * p->npairs;
        if (k == i)
            return 0;
    }
}

/* 0: a pack this firmware can play; else why not (1 not a pack, 2 a newer engine's, 3 malformed) */
static inline int pk_check(const uint8_t *b, int len)
{
    pk_piece_t p;
    int i, k, n;
    if (len < PK_HEAD || len > PK_MAX || memcmp(b, "APK1", 4) || b[4] != PK_FORMAT || pk_len(b) != len)
        return 1;
    if (b[5] > EN_LEVEL)
        return 2;
    if (b[6] != PK_NPIECES || b[12 + PK_NAME - 1] || !b[12])
        return 3;
    for (i = 0; i < PK_NPIECES; i++) {
        if (pk_piece(b, len, i, &p) || p.name[PK_PNAME - 1] || !p.name[0])
            return 3;
        for (k = 0; k < PK_PNAME && p.name[k]; k++)
            if (p.name[k] < 32 || p.name[k] > 126)
                return 3;
        for (k = 0; k < p.air_n; k++)
            if (!p.air_d[k] || p.air_s[k] < -24 || p.air_s[k] > 36)
                return 3;
        for (k = 0; k < p.npairs; k++)
            if (p.pairs[3 * k] >= P_NPARAMS || p.pairs[3 * k] == P_AIR)
                return 3;
    }
    n = (int)(p.pairs + 3 * p.npairs - b);       /* (the eighth's end: the pack's) */
    return n == len ? 0 : 3;
}

/* piece p's recipe into par and pool, its air into the engine (P_AIR the pack's, or none) */
static inline void pk_recipe(const pk_piece_t *p, int16_t *par, uint16_t *pool)
{
    en_recipe_apply(p->pairs, p->npairs, par, pool);
    par[P_AIR] = (int16_t)(p->air_n ? EN_NAIRS + 1 : 0);
}
static inline void pk_air(const pk_piece_t *p) { en_air_set(p->air_s, p->air_d, p->air_n); }
#endif
