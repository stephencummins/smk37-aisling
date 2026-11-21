/* SPDX-License-Identifier: GPL-3.0-only */
/* BERLIN app: the project (everything that is saved) and the app's entry points. Included by the unity
 * build after gfx.c (drawing helpers in scope) and by the host simulator. */
#pragma once
#include <stdint.h>
#include "plat.h"
#include "../dsp/amb.h"

#ifndef EN_VERSION                       /* a release build passes its own (tools/build.py --release) */
#define EN_VERSION "DEV"
#endif
#define PROJ_MAGIC 0x31534941u           /* "AIS1": NACHTFAHRT's ("BRL1"), DUB-1's ("DUB1") and AMB-1's ("ENO1") projects give the recipes */
#define PROJ_FORMAT 3u                   /* (AISLING's own count, from 1; 2: Ornament appended; 3: Lead air
                                         * appended; while AISLING is built, a slot that changes meaning moves the
                                         * format on, so a save from before gives the recipes) */
#define NPIECES EN_NPIECES

typedef struct {
    int16_t par[P_NPARAMS];
    uint16_t pool;
    uint8_t pings[EN_SEQ_N];             /* its patterns as they were left (en_seq_get) */
    uint8_t pings_ok;                    /* 0: none kept (a recipe, or a project from 1.0.0): grown afresh */
    uint8_t screen;                      /* its screen view, 0..2: ripples, scope, spectrum (before 1.2.0: 0) */
} slot_t;

typedef struct {
    uint32_t magic, format;
    int16_t par[P_NPARAMS];              /* the piece playing, as it is now (amb.h P_*) */
    uint16_t pool;                       /* its scale, relative to the root (bit 0 = the root) */
    uint8_t leds;                        /* lights: 0 off, 1 keys and the buttons' glow, 2 keys only */
    uint8_t piece;                       /* which piece */
    uint8_t screen;                      /* the Dub page's middle: 0 ripples, 1 scope, 2 spectrum (HOME again) */
    uint8_t evbars;                      /* (1.2) Evolve's timing, HOME's ALGORITHM: 0 now and then on a phrase's
                                         * start, as before; 1..4 every 4, 8, 16, 32 bars. Global, as Tune (the
                                         * byte was spare: a save from 1.1 reads 0) */
    slot_t slot[NPIECES];                /* every piece as it was left (Tune and MIDI out are global) */
    uint8_t light;                       /* (1.4.1) the screen: 0 dark (on black, as it was), 1 light (on white);
                                         * (packs) 2 as the piece is: a Saint light, a Sinner and the eight dark.
                                         * Global. (The byte was spare: a save from before reads 0) */
    uint8_t bank;                        /* (packs) the bank playing as this was saved: 0 the eight, 1 the pack's.
                                         * Read from the eight's project only (OBJ_PROJ), at boot. (Spare before) */
    uint8_t rsv[14];                     /* room to grow */
} project_t;

extern project_t proj;

void project_defaults(void);
int project_load(void);                  /* 0 = loaded; else defaults are in place */
int project_save(void);                  /* 0 = saved and verified */
void project_apply(void);                /* every value to the engine (boot, load) */
const char *piece_name(int i);
/* (packs) the bank playing: 0 the eight, 1 the installed pack's (firmware/src/app/pack.h). Each has its project
 * (OBJ_PROJ, OBJ_PROJ_B); proj is the one playing, the other kept beside it and saved with it. */
extern uint8_t bank;
int pack_have(void);                     /* a pack is installed */
const char *pack_title(void);            /* its name ("": none) */
const uint8_t *pack_bytes(int *len);     /* as installed (len 0: none) */
int pack_install(const uint8_t *b, int len);   /* 0 kept (len 0: the pack removed), else pk_check's refusal; the
                                                * pack's bank is its recipes again, the eight play. Then project_save */
int project_bank(int b);                 /* 1: the other bank taken up as it was left (then project_apply) */
int piece_light(int i);                  /* piece i of the bank playing a Saint: the white screen */
int screen_light(void);                  /* the screen now: proj.light, or (2) as the piece playing is */
void piece_recipe(int i, int16_t *par, uint16_t *pool);   /* piece i's recipe, of the bank playing */
uint32_t proj_obj(void);                 /* the playing bank's storage object */
void project_piece(int i);               /* the playing piece kept, piece i taken up as it was left */
void project_piece_reset(void);          /* the playing piece back to its recipe */

void ui_init(void);
void ui_frame(void);
void ui_input_only(void);
int ui_dirty(void);
void ui_say(const char *a, const char *b);
#define UI_SPLASH_N 26                   /* the launch screen's frames (~1.4 s at 55 ms) */
void ui_splash(int f);                   /* frame f of the launch screen (the last: as it stays) */
