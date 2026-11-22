/* SPDX-License-Identifier: GPL-3.0-only */
/* The engine for the plugin: the calls it needs, in C (amb.h and pack.h stay out of the C++). The engine's state is
 * one for the process: one AISLING plays at a time. All but ag_peek, ag_save and the names: the audio thread's. */
#ifndef AISLING_GLUE_H
#define AISLING_GLUE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define AG_PATCH_N 16
#define AG_PACK_MAX 3840
void ag_init(void);
void ag_defaults(uint8_t out[4][AG_PATCH_N]);          /* the four layers' plain Patch (ag_init first; kept) */
int ag_check(const uint8_t *pk, int len);              /* 0: a pack this engine plays */
void ag_pack_name(const uint8_t *pk, char *out, int n);
void ag_piece_name(const uint8_t *pk, int len, int piece, char *out, int n);   /* (pk 0: AISLING's own) */
/* a piece's patches as the pack has them (the plain one where it has none); returns the layers it has, as bits */
int ag_peek(const uint8_t *pk, int len, int piece, uint8_t out[4][AG_PATCH_N]);
void ag_load(const uint8_t *pk, int len, int piece, int skip_intros);   /* the piece into the engine, from its top */
void ag_voice(int layer, int patch_on);                /* the layer plays its Patch, or the piece's own voice */
void ag_patch(int layer, const uint8_t *p);
void ag_tempo(int bpm);
int ag_piece_tempo(void);
void ag_mute(uint32_t mask);
void ag_hand(uint32_t mask);
void ag_note(int layer, int note, int vel);
void ag_play(int on);
void ag_clock(int msg);                                /* 0 a tick (24 a quarter), 1 start, 2 continue, 3 stop */
void ag_render(float *l, float *r, int n);
/* the pack with this piece's patches put in (mask: whose) and those layers' voices set to Patch; the new length,
 * or -1 too big, -2 not a pack that reads through, -3 the order can't be stored (make_pack.py has why) */
int ag_save(const uint8_t *pk, int len, int piece, int mask, const uint8_t patches[4][AG_PATCH_N], uint8_t *out);
#ifdef __cplusplus
}
#endif
#endif
