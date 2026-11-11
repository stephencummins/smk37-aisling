/* SPDX-License-Identifier: GPL-3.0-only */
/* AISLING's editor (a design draft): the emulator's whole firmware, with a way in for a page. Everything the page
 * knows about a piece, its pages, its knobs, their names, their help and their ranges, comes out of the firmware's own
 * tables here, so the page can't drift from the FM-1. Everything the page changes goes through the same function a
 * knob or a key calls on the device. No firmware source changes for this: only additions, on this side of the include.
 *
 *   web/editor/build.sh  ->  build/editor/{index.html, engine.js, worklet.js, amb.wasm} */
#include "../emu/amb_web.c"

#define ED __attribute__((used, visibility("default")))

/* ------------------------------------------------------------- the tables --- */
ED int ed_nviews(void) { return NVIEWS; }
ED int ed_npieces(void) { return NPIECES; }
ED int ed_nparams(void) { return P_NPARAMS; }
ED int ed_nlayers(void) { return NLAYER; }
ED int ed_format(void) { return PROJ_FORMAT; }
ED const char *ed_view_name(int v) { return VIEW_NAME[v]; }
ED int ed_view_knob(int v, int i) { return VIEW_KNOB[v][i]; }
ED const char *ed_knob_label(int v, int i) { return KNOB_LABEL[v][i]; }
ED const char *ed_knob_help(int v, int i) { return KNOB_HELP[v][i]; }
ED int ed_layer_level(int i) { return LAYER_LEVEL[i]; }
ED int ed_layer_voice(int i)
{
    static const int V[NLAYER] = {P_CVOICE, P_SVOICE, P_LVOICE, P_S2VOICE, P_NCHAR};
    return V[i];
}
ED int ed_k_none(void) { return K_NONE; }
ED int ed_k_cpu(void) { return K_CPU; }
ED int ed_k_save(void) { return K_SAVE; }
ED int ed_k_mode(void) { return K_MODE; }
ED int ed_k_leds(void) { return K_LEDS; }
ED int ed_k_play(void) { return K_PLAY; }
ED int ed_p_tempo(void) { return P_TEMPO; }
ED int ed_p_root(void) { return P_ROOT; }
ED int ed_p_mode(void) { return P_MODE; }
ED int ed_k_slot(void) { return K_SLOT; }
ED int ed_k_lane(void) { return K_LANE; }
ED int ed_is_global(int p) { return global_par(p); }
ED const char *ed_version(void) { return EN_VERSION; }
/* the screen's colours, by their own names, so the Look page starts from the firmware's and can't drift from them.
 * 0-5 the page, 6-10 the layers, 11-14 the layers' dark tints, 15- the rest (unchanged by a Look) */
ED int ed_ncolours(void) { return 23; }
ED int ed_colour(int i)
{
    static const uint16_t C[23] = {K_BG, K_PANEL, K_LINE, K_DIM, K_TEXT, K_ACC,
                                   K_PAD, K_BELL, K_BASS, K_PULSE, K_NOISE,
                                   K_PAD_T, K_BELL_T, K_BASS_T, K_PULSE_T,
                                   K_FROST, K_FROST_T, K_RIM, K_RIM_T, K_CLAP, K_CLAP_T, K_EIRE, K_EIRE_O};
    return C[i];
}
/* what the keys do, for their labels */
ED int ed_nblack(void) { return NBLACK; }
ED int ed_black_key(int b) { return BLACK_KEY[b]; }
ED const char *ed_black_name(int b) { return BLACK_NAME[b]; }
ED int ed_perf_key(int i) { return i; }           /* (1.3) PERFORM: every key an effect, in the keys' order */
ED int ed_perf_n(void) { return PERF_N; }
ED int ed_perf_shift(void) { return -1; }         /* (1.3) no Mix key, and Forget is SAVE tapped */
ED int ed_perf_forget(void) { return -1; }

/* ------------------------------------------------------------- the knobs --- */
static char text_buf[32];
ED int ed_get(int k) { return knob_get(k); }
ED int ed_lo(int k)
{
    int lo, hi;
    knob_range(k, &lo, &hi);
    return lo;
}
ED int ed_hi(int k)
{
    int lo, hi;
    knob_range(k, &lo, &hi);
    return hi;
}
ED void ed_set(int k, int v) { knob_set(k, v); }
ED const char *ed_text(int k)
{
    knob_text(k, text_buf);
    return text_buf;
}
/* what a value of k reads as, without setting it: a voice's name for its menu, a value as it's dragged */
ED const char *ed_value_text(int k, int v)
{
    text_buf[0] = 0;
    if (k < P_NPARAMS)
        en_param_text(k, v, text_buf);
    else if (k == K_MODE)
        return EN_MODE_NAME[MODE_ORDER[v < 0 ? 0 : v >= EN_NMODES ? EN_NMODES - 1 : v]];
    else if (k == K_PLAY)
        return v ? "Loop" : "Endless";
    else if (k == K_LEDS)
        return v == 2 ? "Keys" : v ? "On" : "Off";
    else if (k == K_LANE)
        return LANE_NAME[(unsigned)v < EN_NDRUM ? v : 0];
    else if (k == K_SLOT)
        snprintf(text_buf, sizeof text_buf, "%d", v + 1);
    return text_buf;
}

/* ---------------------------------------------------- where the FM-1 is looking --- */
ED int ed_view_now(void) { return ui.view; }
ED void ed_view_go(int v) { link_view_go(v); }      /* a page: as its button would (firmware/src/app/link.c) */
ED void ed_touch(int i) { link_touch(i); }           /* the page's knob i lit on the screen, as if just turned */

/* ---------------------------------------------------------- playing --- */
ED int ed_playing(void) { return en_playing; }
ED int ed_frozen(void) { return en_frozen; }
ED int ed_dirty(void) { return ui.dirty; }
ED int ed_piece_now(void) { return proj.piece; }
ED void ed_play_toggle(void) { button(B_PLAY); }
ED void ed_hold_toggle(void) { link_hold_toggle(); }
ED void ed_rest(void)                            /* (the page opened by touching a knob: silence until Play) */
{
    en_play(0);                                  /* (always: ui_init's en_play(1) is queued, en_playing still 0) */
    sp_tp.playing = 0;
    sp_tp.play_t = plat_ms();
}
ED void ed_piece_go(int i) { set_piece(i); }
ED void ed_save(void) { do_save(); }
ED void ed_voice(int p, int v) { link_voice(p, v); }

/* 1.2's fifteen, by the link's own function (1.3: no longer on PERFORM's keys): 0-4 a layer's mute, 5-9 a move
 * (held), 10-14 a layer's solo */
ED void ed_perf(int i, int down)
{
    link_perf(i, down);
}
ED void ed_forget(void) { lead_forget(); }
ED const char *ed_perf_name(int i) { return i >= 0 && i < PERF_N ? PKEY[i].name : ""; }
ED int ed_layer_muted(int i) { return link_layer_muted(i); }
ED int ed_layer_solo(int i) { return link_layer_solo(i); }
ED int ed_layer_sounds(int i) { return perf_sounds(i); }
ED int ed_moves(void) { return ui.moves; }
ED float ed_act(int i) { return en_layer_act[i]; }

/* ---------------------------------------------------------- the pieces --- */
ED const char *ed_piece_name(int i) { return piece_name(i); }
static slot_t recipes[NPIECES];
static uint8_t recipes_ok;
static void recipes_take(void)
{
    int i;
    if (!recipes_ok)
        for (i = 0; i < NPIECES; i++)
            recipe(i, &recipes[i]);
    recipes_ok = 1;
}
ED int ed_recipe(int i, int p)                   /* a parameter in a piece's recipe: what double-click goes back to */
{
    recipes_take();
    return recipes[i].par[p];
}
ED int ed_recipe_pool(int i) { recipes_take(); return recipes[i].pool; }
ED int ed_slot_par(int i, int p) { return i == proj.piece ? proj.par[p] : proj.slot[i].par[p]; }
ED int ed_edited(int i)                          /* changed from its recipe? (Tune and MIDI out are the FM-1's) */
{
    int p;
    recipes_take();
    if ((i == proj.piece ? proj.pool : proj.slot[i].pool) != recipes[i].pool)
        return 1;
    for (p = 0; p < P_NPARAMS; p++)
        if (!global_par(p) && ed_slot_par(i, p) != recipes[i].par[p])
            return 1;
    return 0;
}

/* -------------------------------------------------- the project, as bytes --- */
/* Undo, backups and sharing: the bytes of project_t and slot_t, the same bytes the FM-1 keeps. A snapshot's copied out
 * of ed_proj; ed_commit takes one back from ed_incoming, checked as project_load checks the flash */
static project_t incoming;
static slot_t slot_in;
ED uint8_t *ed_proj(void) { return (uint8_t *)&proj; }
ED int ed_proj_size(void) { return (int)sizeof proj; }
ED int ed_layout(int i)                          /* where things are in those bytes: for a project read from an FM-1 */
{
    switch (i) {
    case 0: return (int)__builtin_offsetof(project_t, par);
    case 1: return (int)__builtin_offsetof(project_t, pool);
    case 2: return (int)__builtin_offsetof(project_t, piece);
    case 3: return (int)__builtin_offsetof(project_t, slot);
    default: return (int)__builtin_offsetof(slot_t, pool);
    }
}
ED uint8_t *ed_incoming(void) { return (uint8_t *)&incoming; }
ED int ed_slot_size(void) { return (int)sizeof(slot_t); }
ED uint8_t *ed_slot_ptr(int i) { return (uint8_t *)&proj.slot[i]; }
ED uint8_t *ed_slot_in(void) { return (uint8_t *)&slot_in; }
ED int ed_in_par(int p) { return slot_in.par[p]; }   /* (what's in a file, read without installing it) */
ED void ed_keep(void) { keep(); }                /* the piece playing, into its slot, before reading the slots */

ED int ed_commit(void) { return link_project_install(&incoming, 0); }

ED int ed_slot_install(int i) { return link_slot_install(i, &slot_in); }   /* slot_in, from a file or a share line, into piece i */
ED void ed_slot_copy(int from, int to) { link_slot_copy(from, to); }
ED void ed_slot_swap(int a, int b) { link_slot_swap(a, b); }
ED void ed_slot_reset(int i) { link_slot_reset(i); }
