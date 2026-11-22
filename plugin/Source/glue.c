/* SPDX-License-Identifier: GPL-3.0-only */
#include <string.h>
#include "glue.h"
#include "amb.h"
#include "pack.h"

void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2) { (void)st, (void)d1, (void)d2; }

static const int VOICE[4] = {P_CVOICE, P_SVOICE, P_LVOICE, P_S2VOICE};
static const int PATCHV[4] = {EN_NCV - 1, EN_NSV - 1, EN_NBV - 1, EN_NS2V - 1};
static int16_t par[P_NPARAMS];
static uint8_t def[4][EN_PATCH_N];
static int inited;

void ag_init(void)
{
    int l;
    if (inited)
        return;
    inited = 1;
    en_init();
    for (l = 0; l < 4; l++) {
        en_patch(l, 0);
        en_patch_get(l, def[l]);
    }
}
void ag_defaults(uint8_t out[4][AG_PATCH_N]) { memcpy(out, def, sizeof def); }
int ag_check(const uint8_t *pk, int len) { return pk_check(pk, len); }
void ag_pack_name(const uint8_t *pk, char *out, int n)
{
    strncpy(out, pk_name(pk), (size_t)n - 1);
    out[n - 1] = 0;
}
void ag_piece_name(const uint8_t *pk, int len, int piece, char *out, int n)
{
    pk_piece_t p;
    const char *s = pk ? (pk_piece(pk, len, piece, &p) ? "?" : (const char *)p.name) : en_piece_name(piece);
    strncpy(out, s, (size_t)n - 1);
    out[n - 1] = 0;
}
int ag_peek(const uint8_t *pk, int len, int piece, uint8_t out[4][AG_PATCH_N])
{
    pk_piece_t p;
    const uint8_t *q;
    int l;
    memcpy(out, def, sizeof def);
    if (!pk || pk_piece(pk, len, piece, &p))
        return 0;
    for (l = 0, q = p.patches; l < 4; l++)
        if (p.pmask >> l & 1)
            memcpy(out[l], q, EN_PATCH_N), q += EN_PATCH_N;
    return p.pmask & 15;
}
void ag_load(const uint8_t *pk, int len, int piece, int skip_intros)
{
    static const int INTRO[] = {55, 59, 96, 97, 128, 126};   /* (the layers' intros, the Atmosphere's, the Arc) */
    uint16_t pool;
    pk_piece_t p;
    int i;
    en_play(0);
    en_panic();
    en_piece(pk ? 0 : piece, par, &pool);
    if (pk && !pk_piece(pk, len, piece, &p)) {
        pk_air(&p);
        pk_recipe(&p, par, &pool);
    }
    if (skip_intros)
        for (i = 0; i < (int)(sizeof INTRO / sizeof INTRO[0]); i++)
            par[INTRO[i]] = 0;
    for (i = 0; i < P_NPARAMS; i++)
        if (i != P_TUNE && i != P_MIDI)
            en_set(i, par[i]);
    en_pool(pool);
    en_regrow();
}
void ag_voice(int layer, int patch_on) { en_set(VOICE[layer & 3], patch_on ? PATCHV[layer & 3] : par[VOICE[layer & 3]]); }
void ag_patch(int layer, const uint8_t *p) { en_patch(layer, p); }
void ag_tempo(int bpm)
{
    const en_param_t *t = en_param_info(P_TEMPO);
    en_set(P_TEMPO, bpm < t->lo ? t->lo : bpm > t->hi ? t->hi : bpm);
}
int ag_piece_tempo(void) { return par[P_TEMPO]; }
void ag_mute(uint32_t mask) { en_mute(mask); }
void ag_hand(uint32_t mask) { en_hand(mask); }
void ag_note(int layer, int note, int vel) { en_note(layer, note, vel); }
void ag_play(int on) { en_play(on); }
void ag_clock(int msg) { en_clock(msg); }
void ag_render(float *l, float *r, int n)
{
    static int32_t buf[2 * 256];
    int i;
    while (n > 0) {
        const int k = n > 256 ? 256 : n;
        en_render(buf, (uint32_t)k, 2048);
        for (i = 0; i < k; i++) {
            *l++ = (float)buf[2 * i] * (1.0f / 8388608.0f);
            *r++ = (float)buf[2 * i + 1] * (1.0f / 8388608.0f);
        }
        n -= k;
    }
}
int ag_save(const uint8_t *pk, int len, int piece, int mask, const uint8_t patches[4][AG_PATCH_N], uint8_t *out)
{
    static uint8_t b[2 * PK_MAX];
    pk_piece_t p;
    int i, l, k, o = PK_HEAD, level = 2;
    if (pk_check(pk, len))
        return -2;
    memcpy(b, pk, PK_HEAD);
    for (i = 0; i < PK_NPIECES; i++) {
        uint8_t pairs[3 * 255 + 12];
        int np, m;
        if (pk_piece(pk, len, i, &p))
            return -2;
        np = p.npairs, m = i == piece ? mask & 15 : p.pmask;
        memcpy(pairs, p.pairs, (size_t)(3 * np));
        if (i == piece)
            for (l = 0; l < 4; l++)
                if (m >> l & 1) {                /* (the layer's voice: Patch; the pair it has, or one more) */
                    for (k = 0; k < np && pairs[3 * k] != VOICE[l]; k++)
                        ;
                    if (k == np && np++ == 255)
                        return -1;
                    pairs[3 * k] = (uint8_t)VOICE[l], pairs[3 * k + 1] = (uint8_t)PATCHV[l], pairs[3 * k + 2] = 0;
                }
        if (o + PK_PNAME + 4 + 2 * p.air_n + 3 * np + EN_PATCH_N * 4 > (int)sizeof b)
            return -1;
        memcpy(b + o, p.name, PK_PNAME);
        b[o + PK_PNAME] = p.flags, b[o + PK_PNAME + 1] = p.air_n, b[o + PK_PNAME + 2] = (uint8_t)np;
        b[o + PK_PNAME + 3] = (uint8_t)m;
        o += PK_PNAME + 4;
        memcpy(b + o, p.air_s, (size_t)(2 * p.air_n)), o += 2 * p.air_n;
        memcpy(b + o, pairs, (size_t)(3 * np)), o += 3 * np;
        for (l = 0, k = 0; l < 4; l++)
            if (m >> l & 1) {
                memcpy(b + o, i == piece ? patches[l] : p.patches + EN_PATCH_N * k, EN_PATCH_N);
                o += EN_PATCH_N, k++;
            }
        if (m)
            level = 3;
    }
    if (o > PK_MAX)
        return -1;
    if (b[5] < level)
        b[5] = (uint8_t)level;
    b[8] = (uint8_t)(o & 255), b[9] = (uint8_t)(o >> 8);
    if (o > 0xE08 && b[0xE06] == 0x41 && b[0xE07] == 0x54)
        return -3;
    if (pk_check(b, o))
        return -2;
    memcpy(out, b, (size_t)o);
    return o;
}
