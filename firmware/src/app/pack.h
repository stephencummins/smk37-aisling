/* SPDX-License-Identifier: GPL-3.0-only */
/* A sound pack: eight pieces as data (tools/make_pack.py writes one from packs/NAME.json; the Packs page sends it
 * over the link, area 10; the FM-1 keeps one, OBJ_PACK, beside its own eight). Bytes, little-endian:
 *
 *    0  "APK1"
 *    4  format (1)      5  level: the engine it needs (EN_LEVEL or lower)      6  pieces (8)      7  0
 *    8  length.2: all of it      10  version.2      12  name[20], NUL-padded
 *   32  the pieces, one after another:
 *         name[16], NUL-padded      flags (bit 0: a Saint, the white screen)      air notes (0..16)      pairs
 *         patches (level 3; else 0): a bit a layer, 1 the Drone's, 2 the Sequence's, 4 the Lead's, 8 Sequence 2's
 *         the air: its notes' semitones over the root (int8), then their sixteenths (1..255)
 *         the recipe: pairs of parameter (a byte), value (int16), over the defaults (en_recipe_apply)
 *         the patches (level 3): EN_PATCH_N bytes for each layer that has one, in that order (en_patch: what the
 *         layer's voice "Patch" plays; a layer without one plays the plain Patch)
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
    const uint8_t *air_d, *pairs, *patches;
    uint8_t flags, air_n, npairs, pmask;
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
        int np = 0, l;
        p->flags = b[o + PK_PNAME], p->air_n = b[o + PK_PNAME + 1], p->npairs = b[o + PK_PNAME + 2];
        p->pmask = b[o + PK_PNAME + 3];
        for (l = 0; l < 8; l++)                  /* (a bit past the four: counted, so it never reads through) */
            np += p->pmask >> l & 1;
        o += PK_PNAME + 4;
        if (p->air_n > 16 || o + 2 * p->air_n + 3 * p->npairs + EN_PATCH_N * np > len)
            return -1;
        p->air_s = (const int8_t *)b + o, p->air_d = b + o + p->air_n;
        p->pairs = b + o + 2 * p->air_n;
        p->patches = p->pairs + 3 * p->npairs;
        o += 2 * p->air_n + 3 * p->npairs + EN_PATCH_N * np;
        if (k == i)
            return 0;
    }
}

/* 0: a pack this firmware can play; else why not (1 not a pack, 2 a newer engine's, 3 malformed) */
static inline int pk_check(const uint8_t *b, int len)
{
    pk_piece_t p;
    const uint8_t *end = b;
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
        if (p.pmask > 15 || (p.pmask && b[5] < 3))   /* (patches: four layers', a level 3 pack's) */
            return 3;
        end = p.patches;
        for (k = 0; k < 4; k++)
            end += p.pmask >> k & 1 ? EN_PATCH_N : 0;
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
    n = (int)(end - b);                          /* (the eighth's end: the pack's) */
    return n == len ? 0 : 3;
}

/* piece p's recipe into par and pool, its air into the engine (P_AIR the pack's, or none) */
static inline void pk_recipe(const pk_piece_t *p, int16_t *par, uint16_t *pool)
{
    en_recipe_apply(p->pairs, p->npairs, par, pool);
    par[P_AIR] = (int16_t)(p->air_n ? EN_NAIRS + 1 : 0);
}
/* (1.6) a place with nothing in it: the Packs page's own filler, byte for byte (web/packs/packfile.js: empty), since
 * a pack is always eight. PRESET and the Journey pass over it. */
static inline int pk_empty(const pk_piece_t *p)
{
    static const uint8_t E[PK_PNAME + 4 + 18] = {'E', 'm', 'p', 't', 'y', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0,
                                                 0, 0, 0, 1, 0, 0, 2, 0, 0, 3, 0, 0, 29, 0, 0, 32, 0, 0};
    int i;
    if (p->flags || p->air_n || p->npairs != 6 || p->pmask)
        return 0;
    for (i = 0; i < (int)sizeof E; i++)          /* (the name, the four, the six pairs: they lie together) */
        if (p->name[i] != E[i])
            return 0;
    return 1;
}
static inline void pk_air(const pk_piece_t *p) { en_air_set(p->air_s, p->air_d, p->air_n); }
/* its patches into the engine; a layer it has none for, and every layer of p 0 (AISLING's own): the plain one */
static inline void pk_patch(const pk_piece_t *p)
{
    const uint8_t *q = p ? p->patches : 0;
    int l;
    for (l = 0; l < 4; l++)
        if (p && p->pmask >> l & 1)
            en_patch(l, q), q += EN_PATCH_N;
        else
            en_patch(l, 0);
}
#endif
