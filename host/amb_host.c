/* SPDX-License-Identifier: GPL-3.0-only */
/* AMB-1 on the host: the whole app (UI, project, engine) against a simulated FM-1 driven by a script.
 * Audio goes to a WAV, the screen to PNGs, the lights to text, MIDI out to a log. The device-only
 * parts (HAL, USB, OTA, flash driver) are replaced by plat.h implemented here. (From X0X's x0x_host.)
 *
 *   amb_host SCRIPT [OUTDIR]
 *
 * Script, one command per line (# comments):
 *   wait MS                      run the device for MS milliseconds
 *   press BTN | release BTN | tap BTN     (BTN: FX SEL ENV LFO EDIT GLO HOME SAVE ARP SEQ PLAY REC OCT- OCT+)
 *   key K down|up | tapkey K     (K: 0..26 = F3..G5, or w0..w15 white keys, b0..b10 black keys)
 *   turn ENC N [MS]              (ENC: SELECT ALGO PRESET K1 K2 K3 K4; N detents, MS apart, default 80)
 *   spin ENC N                   N detents at once, exactly N (no acceleration)
 *   master N                     MASTER pot 0..4096
 *   battery N                    the battery's raw reading (plat_battery; else AMB_BATT, else -1: none)
 *   midi B0 B1 B2                incoming USB MIDI message (hex bytes)
 *   wav FILE | wavstop           start / stop recording the output
 *   peakreset                    restart the output peak (expect peak_db_min / peak_db_max)
 *   shot FILE.png                save the screen
 *   leds                         print the lit buttons and keys
 *   expect WHAT VALUE            check state: exit 1 on mismatch; VALUE "<N" / ">N" is a bound
 *   reboot                       re-run boot from the simulated flash (persistence test)
 *   splash N                     draw the launch screen's frame N (0..UI_SPLASH_N-1) on the screen
 *   at S [LABEL]                 run until S seconds after the film started (or the boot, without a film)
 *   bar N [LABEL]                run until bar N's downbeat (N x 16 sixteenths played): a move on the beat.
 *                                Both print the film's time there, "@ mm:ss.s  bar N  LABEL" (the edit's cue)
 *   storefile PATH               a saved project from a file (tests/host/proj-*.bin)
 *   pingmark                     remember the ping loops (expect pingsame 1: they are as they were)
 *   splashrun                    the launch screen played out as the device does at power-on (55 ms a frame,
 *                                the music running, the UI not), then the UI started again
 *   film FILE | filmstop         record the device, 30 frames a second of simulated time, to FILE: each frame
 *                                a header (FILM_HDR words: ms, buttons held, keys held, buttons lit, keys lit,
 *                                master, the detents each encoder turned since the frame before) and the
 *                                screen (240 x 240 RGB565, big-endian). tools/film.py makes it a video.
 *   listen                       a screen reader starts listening (HELLO, again every 2 s): each SAY is printed
 *                                ("said: ..."; "said!:" cuts off what was being said)
 *   where                        the reader asks for everything on the screen
 *   said TEXT                    some SAY since the last "said" (or "listen") has TEXT in it: else a failure
 *   said_none TEXT               no SAY since the last check has TEXT in it: else a failure
 *   speakdump                    every page, knob value, help card, chord and message as the reader gets them
 * AMB_SAID_LOG=FILE: a reader listens from boot, quietly, and every SAY is appended to FILE (the audit).
 *
 * amb_host_smk37 (host/build_host.sh, -DAISLING_BOARD_SMK37=1) is the same app on the SMK-37 Elite, its controls
 * by their printed names (underscores for spaces; tests/scenarios_smk37/):
 *   buttons  FX SCALE CHORD NOTE_REPEAT PARA GLOBE PATCH BT ARP SEQ SEQ_PLAY SEQ_REC OCT- OCT+ (the FM-1's FX SEL
 *            ENV LFO EDIT GLO HOME SAVE ARP SEQ PLAY REC OCT- OCT+, in that order), and KNOB_BANK FADER_BANK < >,
 *            which reach nothing
 *   encoders K2 K3 K4 K5 K6 K7 K8 (SELECT ALGO PRESET K1 K2 K3 K4); K1 is MASTER, an encoder (turn, spin), so
 *            `master N` is refused
 *   keys     0..36 = C3..C6, w0..w21 white, b0..b14 black; AISLING plays 5..31 (F3..G5), the rest nothing
 */
#define EN_HOST 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#define __attribute__(x)

/* ------------------------------------------------------------ screen --- */
static uint16_t fb[240 * 240];          /* RGB565, byte-swapped as the panel takes it (gfx.c) */
static void lcd_sync(void) {}
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    uint32_t i, j;
    uint16_t sc = (uint16_t)(((c >> 8) & 0xFFu) | ((c & 0xFFu) << 8));
    for (j = y; j < y + h && j < 240u; j++)
        for (i = x; i < x + w && i < 240u; i++)
            fb[j * 240u + i] = sc;
}
static uint32_t blits;
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px)
{
    uint32_t i, j;
    blits++;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            if (x + i < 240u && y + j < 240u)
                fb[(y + j) * 240u + x + i] = px[j * w + i];
}
#include "../firmware/src/gfx.c"


/* ---------------------------------------------------------- platform --- */
#include "../firmware/src/app/plat.h"

#include "../firmware/src/board_caps.h"
#include "../firmware/src/app/master.h"

static uint32_t now_ms, held_btn, held_keys, master = 2048;
#if AISLING_BOARD_SMK37
static uint32_t master_k10 = 0xFFFFu, held_extra;   /* K1's position (set at power-on); the buttons that reach nothing */
static uint64_t held_outer;                         /* the keys outside the 27: held, and nothing reads them */
#endif
static int32_t enc_acc[NE];
static uint32_t lit_btn, lit_keys;

uint32_t plat_ms(void) { return now_ms; }
uint32_t plat_buttons(void) { return held_btn; }
uint32_t plat_keys(void) { return held_keys; }
int32_t plat_enc(int role)
{
    int32_t v = enc_acc[role];
    enc_acc[role] = 0;
    return v;
}
uint32_t plat_master(void) { return master; }
#if AISLING_BOARD_SMK37
uint32_t plat_master_raw(void) { return master_k10; }
/* MASTER is K1, an encoder, as main_fm1.c has it: at power-on the knob position nearest MASTER_START from below,
 * and the level MASTER_START itself until the first turn; a turn moves the position and the level follows it */
static void master_power_on(void)
{
    master_k10 = master_enc_start();
    master = MASTER_START;
}
static void master_turn(int32_t d)
{
    if (d) {
        master_k10 = master_enc_turn(master_k10, d);
        master = master_curve(master_k10);
    }
}
#else
uint32_t plat_master_raw(void) { return master >= 4092u ? 1023u : master / 4u; }
#endif
void plat_leds(uint32_t b, uint32_t k)
{
    lit_btn = b;
    lit_keys = k;
}
static int glow_on = 1;                 /* the device's button glow (plat_glow); the lights printed are the bright ones */
void plat_glow(int on) { glow_on = on; }
static int screen_on = 1;               /* (1.1) the screen (plat_screen): off while asleep */
void plat_screen(int on) { screen_on = on != 0; }
void plat_play_red(int red) { (void)red; }      /* the device's PLAY colour: green, red while recording */

#define MQ 256
static uint32_t min_q[MQ], mi_w, mi_r;
static FILE *midi_log;
static uint32_t midi_out_count, clock_out_count;
static uint32_t fxcc_count, fxcc_last;
int plat_midi_in(uint32_t *pkt)
{
    if (mi_r == mi_w)
        return 0;
    *pkt = min_q[mi_r++ % MQ];
    return 1;
}
#ifdef EN_WEB
static void web_midi(uint32_t pkt);
#endif
void plat_midi_out(uint32_t pkt)
{
#ifdef EN_WEB
    web_midi(pkt);
#endif
    midi_out_count++;
    if (((pkt >> 8) & 0xFF) == 0xF8) {
        clock_out_count++;
        return;
    }
    if (((pkt >> 8) & 0xFF) == 0xBF)             /* (1.3) the effects' CCs out: how many, the last (cc << 8 | value) */
        fxcc_count++, fxcc_last = ((pkt >> 16) & 0xFF) << 8 | ((pkt >> 24) & 0xFF);
    if (midi_log)
        fprintf(midi_log, "%u ms: %02X %02X %02X\n", now_ms, (pkt >> 8) & 0xFF, (pkt >> 16) & 0xFF, (pkt >> 24) & 0xFF);
}
/* the engine's MIDI out, as the device's glue sends it (USB-MIDI packet, cable 0) */
static uint32_t notes_ch[16];                    /* (note-ons on each channel since the last peakreset) */
static uint32_t seq_apart;                       /* (BERLIN: blocks the two sequences were at different steps, since the
                                                  * last peakreset) */
static int last_note[16];                        /* (the last note-on on each channel) */
static uint32_t orph_midi, orph_out;             /* (orphans: the MIDI out and the samples, as hashes) */
void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2)
{
    orph_midi = (orph_midi ^ (st | d1 << 8 | d2 << 16)) * 16777619u;
    if ((st & 0xF0u) == 0x90u && d2)
        notes_ch[st & 15u]++, last_note[st & 15u] = (int)d1;
    plat_midi_out(st >> 4 | st << 8 | d1 << 16 | d2 << 24);
}

/* flash: one buffer per object, kept across a simulated reboot */
static uint8_t store[OBJ_NOBJ][PLAT_STORE_MAX];
static int store_len[OBJ_NOBJ] = {-1, -1};
static uint32_t store_writes;
int plat_store_load(uint32_t obj, void *dst, uint32_t max)
{
    int n;
    if (obj >= OBJ_NOBJ || (n = store_len[obj]) < 0)
        return -1;
    if ((uint32_t)n > max)
        n = (int)max;
    memcpy(dst, store[obj], (size_t)n);
    return n;
}
int plat_store_save(uint32_t obj, const void *src, uint32_t len)
{
    if (obj >= OBJ_NOBJ || len > PLAT_STORE_MAX)
        return -1;
    memcpy(store[obj], src, len);
    store_len[obj] = (int)len;
    store_writes++;
    return 0;
}

static uint32_t cpu_pct;
uint32_t plat_cpu_pct(void) { return cpu_pct; }
uint32_t plat_xruns(void) { return 0; }
uint32_t plat_cpu_peak(void) { return cpu_pct; }
static int sim_batt_set;                        /* (`battery N`) */
static int32_t sim_batt;
int32_t plat_battery(void)                      /* (to see it drawn) */
{
    return sim_batt_set ? sim_batt : getenv("AMB_BATT") ? atoi(getenv("AMB_BATT")) : -1;
}
int plat_usb_power(void) { return getenv("AMB_USB") != 0; }

/* -------------------------------------------------------------- app --- */
#include "../firmware/src/app/app.h"
#define ANN_NAME "DUB-1 " EN_VERSION
#include "ann_words.h"                                  /* the words file's link (tools/gen_words.py) */
#include "../firmware/src/announce.c"
/* what the reader is told: every SAY to stdout with "said:" (listen), and kept for the "said" check */
static char said_last[ANN_MAX + 1], said_log[16384];   /* said_log: every SAY since the last "said" check */
static uint32_t said_len;
static int said_print = 1, sim_listen;
static FILE *said_log_f;                                /* AMB_SAID_LOG=FILE: every SAY, for fm1-reader's audit */
#ifdef EN_WEB
static void web_said(const uint8_t *p, uint32_t n);
#endif
static int ann_wire_send(const uint8_t *p, uint32_t n)
{
#ifdef EN_WEB
    web_said(p, n);
#endif
    if (n >= 7u && p[4] == ANN_SAY) {
        uint32_t i, k = 0;
        for (i = 6; i + 1u < n && k < ANN_MAX; i++)
            said_last[k++] = (char)p[i];
        said_last[k] = 0;
        if (said_len + k + 2u < sizeof said_log) {
            memcpy(said_log + said_len, said_last, k);
            said_len += k;
            said_log[said_len++] = '\n';
            said_log[said_len] = 0;
        }
#ifndef EN_WEB
        if (said_print)
            printf("said%s: %s\n", (p[5] & ANN_NOW) ? "!" : "", said_last);
        if (said_log_f)
            fprintf(said_log_f, "%s\t%s\n", (p[5] & ANN_KEEP) ? "WARN" : "MSG", said_last);
#endif
    }
    return 0;
}
#include "link_client.h"
#include "../firmware/src/app/project.c"
#include "../firmware/src/app/ui.c"

/* (BERLIN) orphans: the engine alone from a reset, 40 s with one knob at v and everything heard and moving (the
 * chord every 2 bars, Sequence 2 on): the energy and the high energy (first differences: brighter, higher) in eight
 * stretches, and the MIDI out as a hash */
#ifndef EN_WEB
#include <unistd.h>
#include <sys/wait.h>
#define ORPH_B (40 * 44100 / 256)
static void orph_run(int p, int v, float *e, uint32_t *mh)
{
    static int32_t ob[2 * 256];
    float last = 0.0f;
    int i, b, j;
    en_init();
    for (i = 0; i < P_NPARAMS; i++)
        en_set(i, en_param_info(i)->def);
    en_pool(EN_MODE_POOL[MODE_AEOLIAN]);
    en_set(P_DRIFT, 0);
    en_set(P_SEQ2, 60);
    en_set(P_TAPB, 60);                          /* (Repeat B heard: its time can be) */
    en_set(P_NCHAR, 1);                          /* (an Atmosphere heard: Wind; the default is None) */
    en_set(P_LDIN, 0);                           /* (the Lead heard from the start) */
    if (p == P_BRIDGE)                           /* (the arc's Bridge within the 40 s: Motion 100, 16 bars of Flow at
                                                  * 160, then its first section, a Bridge from en_init's dice) */
        en_set(P_TEMPO, 160), en_set(P_MOTION, 100);
    if (p == P_LMEM)                             /* (the Lead's Memory: a block's length and what it keeps, so a second
                                                  * block within the 40 s: 160, the Lead's phrases at their most often) */
        en_set(P_TEMPO, 160), en_set(P_LDENS, 100);
    en_set(p, v);
    en_regrow();
    en_play(1);
    orph_midi = orph_out = 2166136261u;
    for (i = 0; i < 16; i++)
        e[i] = 0.0f;
    for (b = 0; b < ORPH_B; b++) {
        en_render(ob, 256, 2048);
        for (j = 0; j < 512; j++) {
            const float x = (float)ob[j] * (1.0f / 8388608.0f);
            e[b * 8 / ORPH_B] += x * x;
            e[8 + b * 8 / ORPH_B] += (x - last) * (x - last);
            last = x;
            orph_out = (orph_out ^ (uint32_t)ob[j]) * 16777619u;   /* (every sample: any change at all) */
        }
    }
    *mh = orph_midi ^ orph_out * 2654435761u;
}
static void orph_render(int p, int v, float *e, uint32_t *mh)   /* (in a copy of this process: each from the same state,
                                                                  * whatever an earlier render left) */
{
    struct {
        float e[16];
        uint32_t m;
    } r;
    int fd[2];
    pid_t pid;
    memset(&r, 0, sizeof r);
    if (pipe(fd) == 0 && (pid = fork()) >= 0) {
        if (!pid) {
            close(fd[0]);
            orph_run(p, v, r.e, &r.m);
            if (write(fd[1], &r, sizeof r) != (ssize_t)sizeof r)
                _exit(1);
            _exit(0);
        }
        close(fd[1]);
        if (read(fd[0], &r, sizeof r) != (ssize_t)sizeof r)
            memset(&r, 0, sizeof r);
        close(fd[0]);
        waitpid(pid, 0, 0);
    }
    memcpy(e, r.e, sizeof r.e);
    *mh = r.m;
}
static int orph_same(const float *a, const float *b, uint32_t ma, uint32_t mb)
{
    int i;
    (void)a, (void)b, (void)i;                   /* (the same: not a sample, not a message changed) */
    return ma == mb;
}
#endif

/* ---------------------------------------------------------- outputs --- */
static void png_write(const char *path)
{
    /* RGB PNG with stored (uncompressed) deflate blocks: no zlib needed */
    static uint8_t raw[240 * (1 + 240 * 3)];
    FILE *f = fopen(path, "wb");
    uint32_t crc_t[256], i, j, k, n = 0, a = 1, b = 0, pos, len = sizeof raw;
    uint8_t hdr[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (!f) {
        fprintf(stderr, "cannot write %s\n", path);
        return;
    }
    for (i = 0; i < 256; i++) {
        uint32_t c = i;
        for (k = 0; k < 8; k++)
            c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        crc_t[i] = c;
    }
    for (j = 0; j < 240; j++) {
        raw[n++] = 0;
        for (i = 0; i < 240; i++) {
            uint16_t p = fb[j * 240 + i];
            p = (uint16_t)((p >> 8) | (p << 8));
            raw[n++] = (uint8_t)(((p >> 11) & 31) * 255 / 31);
            raw[n++] = (uint8_t)(((p >> 5) & 63) * 255 / 63);
            raw[n++] = (uint8_t)((p & 31) * 255 / 31);
        }
    }
    for (i = 0; i < len; i++) {
        a = (a + raw[i]) % 65521;
        b = (b + a) % 65521;
    }
#define PUT32(v) do { uint8_t q_[4] = {(uint8_t)((v) >> 24), (uint8_t)((v) >> 16), (uint8_t)((v) >> 8), (uint8_t)(v)}; fwrite(q_, 1, 4, f); } while (0)
    fwrite(hdr, 1, 8, f);
    {
        uint8_t ih[17] = {'I', 'H', 'D', 'R', 0, 0, 0, 240, 0, 0, 0, 240, 8, 2, 0, 0, 0};
        uint32_t c = 0xFFFFFFFFu;
        PUT32(13u);
        fwrite(ih, 1, 17, f);
        for (i = 0; i < 17; i++)
            c = crc_t[(c ^ ih[i]) & 255] ^ (c >> 8);
        PUT32(~c);
    }
    {
        uint32_t nblk = (len + 65534) / 65535, zlen = 2 + len + nblk * 5 + 4, c = 0xFFFFFFFFu;
        uint8_t *z = malloc(zlen + 4), *q = z;
        memcpy(q, "IDAT", 4);
        q += 4;
        *q++ = 0x78;
        *q++ = 0x01;
        for (pos = 0; pos < len;) {
            uint32_t m = len - pos > 65535 ? 65535 : len - pos;
            *q++ = pos + m >= len ? 1 : 0;
            *q++ = (uint8_t)m;
            *q++ = (uint8_t)(m >> 8);
            *q++ = (uint8_t)~m;
            *q++ = (uint8_t)(~m >> 8);
            memcpy(q, raw + pos, m);
            q += m;
            pos += m;
        }
        *q++ = (uint8_t)(b >> 8);
        *q++ = (uint8_t)b;
        *q++ = (uint8_t)(a >> 8);
        *q++ = (uint8_t)a;
        PUT32(zlen);
        fwrite(z, 1, zlen + 4, f);
        for (i = 0; i < zlen + 4; i++)
            c = crc_t[(c ^ z[i]) & 255] ^ (c >> 8);
        PUT32(~c);
        free(z);
    }
    {
        uint8_t ie[4] = {'I', 'E', 'N', 'D'};
        uint32_t c = 0xFFFFFFFFu;
        PUT32(0u);
        fwrite(ie, 1, 4, f);
        for (i = 0; i < 4; i++)
            c = crc_t[(c ^ ie[i]) & 255] ^ (c >> 8);
        PUT32(~c);
    }
    fclose(f);
}

static FILE *wav;
static uint32_t wav_frames;
static float peak_out;
static void wav_open(const char *path)
{
    static const uint8_t h[44] = {0};
    wav = fopen(path, "wb");
    if (wav)
        fwrite(h, 1, 44, wav);
    wav_frames = 0;
}
static void wav_close(void)
{
    uint32_t bytes = wav_frames * 4, v;
    if (!wav)
        return;
    fseek(wav, 0, SEEK_SET);
    fwrite("RIFF", 1, 4, wav);
    v = 36 + bytes;
    fwrite(&v, 4, 1, wav);
    fwrite("WAVEfmt ", 1, 8, wav);
    v = 16;
    fwrite(&v, 4, 1, wav);
    {
        uint16_t fmt[2] = {1, 2};
        uint32_t r[2] = {44100, 44100 * 4};
        uint16_t al[2] = {4, 16};
        fwrite(fmt, 2, 2, wav);
        fwrite(r, 4, 2, wav);
        fwrite(al, 2, 2, wav);
    }
    fwrite("data", 1, 4, wav);
    fwrite(&bytes, 4, 1, wav);
    fclose(wav);
    wav = 0;
}

/* the ping loops, as a hash (pingmark, expect pingsame) */
static uint32_t ping_mark;
static uint32_t ping_hash(void)
{
    uint32_t h = 2166136261u;
    int l, i;
    for (l = 0; l < EN_NLOOP; l++)
        for (i = 0; i < EN_LOOP_MAX; i++)
            h = (h ^ (uint32_t)(uint8_t)en_loop_note[l][i]) * 16777619u;
    return h;
}

/* ------------------------------------------------------------ film --- */
#define FILM_HDR (6 + NE)
static FILE *film;
static double film_due;
static uint32_t film_t0;
static int32_t film_enc[NE];
static int ui_off;                       /* (splashrun: the launch screen owns the screen) */
static void film_frame(void)
{
    uint32_t h[FILM_HDR], i;
    h[0] = now_ms, h[1] = held_btn, h[2] = held_keys, h[3] = lit_btn, h[4] = lit_keys, h[5] = master;
    for (i = 0; i < NE; i++) {
        h[6 + i] = (uint32_t)film_enc[i];
        film_enc[i] = 0;
    }
    fwrite(h, 4, FILM_HDR, film);
    fwrite(fb, 2, 240 * 240, film);
}

/* --------------------------------------------------------- the device --- */
static double audio_due_ms, render_ns_total, render_budget_ns_total;
static uint32_t ui_due;
static double now_ns(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec * 1e9 + (double)t.tv_nsec;
}

static void boot(void)
{
    en_init();
    project_load();
    project_apply();
    lcd_fill(0, 0, 240, 240, 0);
    ui_init();
}

static void run_ms(uint32_t ms)
{
    static int32_t blk[512];
    uint32_t i, k;
    for (i = 0; i < ms; i++) {
        now_ms++;
        while (audio_due_ms <= (double)now_ms) {      /* 256-frame blocks, as the device renders them */
            double t0 = now_ns(), dt;
            en_render(blk, 256, master * ui_gain_q12 >> 12);   /* (1.1) the sleep timer's fade, as the device */
            seq_apart += en_loop_pos[0] != en_loop_pos[1];
#ifdef EN_WEB
            web_audio(blk, 256);
#endif
            dt = now_ns() - t0;
            render_ns_total += dt;
            render_budget_ns_total += 256.0 * 1e9 / 44100.0;
            cpu_pct = (uint32_t)(100.0 * dt / (256.0 * 1e9 / 44100.0));
            for (k = 0; k < 256; k++) {
                float l = (float)blk[2 * k] / 8388608.0f, r = (float)blk[2 * k + 1] / 8388608.0f;
                int16_t s[2];
                if (fabsf(l) > peak_out)
                    peak_out = fabsf(l);
                if (fabsf(r) > peak_out)
                    peak_out = fabsf(r);
                s[0] = (int16_t)(l * 32767.0f);
                s[1] = (int16_t)(r * 32767.0f);
                if (wav) {
                    fwrite(s, 2, 2, wav);
                    wav_frames++;
                }
            }
            audio_due_ms += 256.0 * 1000.0 / 44100.0;
        }
        if (sim_listen && now_ms % 2000u == 0) {        /* the reader's HELLO, as it sends it */
            static const uint8_t HELLO[] = {ANN_H0, ANN_H1, ANN_H2, ANN_HELLO, ANN_VER};
            ann_frame(HELLO, sizeof HELLO, now_ms);
        }
        if (lc.beat && now_ms % 2000u == 0) {           /* the link's heartbeat, as the other end sends it */
            static const uint8_t BEAT[] = {LK_H0, LK_H1, LK_H2, LK_BEAT};
            link_frame(BEAT, sizeof BEAT, now_ms);
        }
        if (ui_off) {
        } else if (now_ms >= ui_due) {
            ui_due = now_ms + 16;
            ui_frame();
        } else {
            ui_input_only();
        }
        if (film && (double)now_ms >= film_due) {
            film_frame();
            film_due += 1000.0 / 30.0;
        }
    }
}

/* ---------------------------------------------------------- script --- */
#if AISLING_BOARD_SMK37
/* the SMK-37 Elite's printed names by the job each does, the FM-1's buttons in order (hal/smk37/smk_input.h gives each
 * button the FM-1 id of its job, so PANEL_DEFAULT is the map; tests/host/smk_input_test.c checks the HAL's side) */
static const char *const BTN_N[NB] = {"FX", "SCALE", "CHORD", "NOTE_REPEAT", "PARA", "GLOBE", "PATCH", "BT", "ARP",
                                      "SEQ", "SEQ_PLAY", "SEQ_REC", "OCT-", "OCT+"};
static const char *const EXTRA_N[4] = {"KNOB_BANK", "FADER_BANK", "<", ">"};
static const char *const ENC_N[NE] = {"K2", "K3", "K4", "K5", "K6", "K7", "K8"};   /* (K1: MASTER) */
#else
static const char *const BTN_N[NB] = {"FX", "SEL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ",
                                      "PLAY", "REC", "OCT-", "OCT+"};
static const char *const ENC_N[NE] = {"SELECT", "ALGO", "PRESET", "K1", "K2", "K3", "K4"};
#endif

static int find_name(const char *const *t, int n, const char *s)
{
    int i;
    for (i = 0; i < n; i++)
        if (!strcmp(t[i], s))
            return i;
    return -1;
}

/* the app's 27 keys, F3..G5 (the FM-1's; `leds` prints them so) */
static const uint8_t WHITE_K[16] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};
static const uint8_t BLACK_K[11] = {1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25};
#if AISLING_BOARD_SMK37
/* the SMK-37 Elite's keys by its own numbers, 0..36 = C3..C6: AISLING plays the middle 27 (keys SIM_KEY_LO.., the
 * app's 0..26, F3..G5); the 5 below and the 5 above are read (fm1_in.outer) and play nothing. SIM_KEY_LO is the HAL's
 * SMK_NOTE_LO (hal/smk37/smk_input.h, not included here): tests/host/smk_input_test.c and tests/scenarios_smk37/keys.amb
 * each hold it at 5 */
#define SIM_KEY_LO (CAP_OUTER_KEYS / 2)
static const uint8_t SMK_WHITE[22] = {0, 2, 4, 5, 7, 9, 11, 12, 14, 16, 17, 19, 21, 23, 24, 26, 28, 29, 31, 33, 35, 36};
static const uint8_t SMK_BLACK[15] = {1, 3, 6, 8, 10, 13, 15, 18, 20, 22, 25, 27, 30, 32, 34};
static int key_of(const char *s)                /* -1: no such key */
{
    int i = atoi(s + (s[0] == 'w' || s[0] == 'b'));
    if (s[0] == 'w')
        return i >= 0 && i < 22 ? SMK_WHITE[i] : -1;
    if (s[0] == 'b')
        return i >= 0 && i < 15 ? SMK_BLACK[i] : -1;
    return i >= 0 && i < CAP_KEYS ? i : -1;
}
static void key_set(int k, int down)
{
    int p = k - SIM_KEY_LO;
    if (p >= 0 && p < 27)
        held_keys = down ? held_keys | 1u << p : held_keys & ~(1u << p);
    else
        held_outer = down ? held_outer | 1ull << k : held_outer & ~(1ull << k);
}
#define STRUM_KEY(x) SMK_WHITE[((x) % 22 + 22) % 22]
#else
static int key_of(const char *s)
{
    if (s[0] == 'w')
        return WHITE_K[atoi(s + 1) & 15];
    if (s[0] == 'b')
        return BLACK_K[atoi(s + 1) % 11];
    return atoi(s);
}
static void key_set(int k, int down) { held_keys = down ? held_keys | 1u << k : held_keys & ~(1u << k); }
#define STRUM_KEY(x) WHITE_K[(x) & 15]
#endif

static int16_t mark_par[P_NPARAMS];              /* (`mark`, `expect marked`: what an undo must bring back) */
static uint16_t mark_pool;
static int expect(const char *what, const char *val)
{
    int got;
    if (!strcmp(what, "playing"))
        got = en_playing;
    else if (!strcmp(what, "frozen"))
        got = en_frozen;
    else if (!strcmp(what, "sleep"))              /* (1.1) the sleep timer: its minutes (0: off) */
        got = SLEEP_MIN[sl.set];
    else if (!strcmp(what, "asleep"))
        got = sl.asleep;
    else if (!strcmp(what, "screen"))
        got = screen_on;
    else if (!strcmp(what, "gain"))               /* the gain after MASTER, Q12 (4096: whole) */
        got = (int)ui_gain_q12;
    else if (!strcmp(what, "view"))
        got = ui.view;
    else if (!strcmp(what, "master"))             /* the level the engine gets (Q12, plat_master) */
        got = (int)plat_master();
    else if (!strcmp(what, "masterraw"))          /* MASTER's knob position, 0..1023 (plat_master_raw) */
        got = (int)plat_master_raw();
    else if (!strcmp(what, "battbars"))           /* the battery's bars and rough charge, by board_caps.h */
        got = batt_bars();
    else if (!strcmp(what, "battpct"))
        got = batt_pct();
    else if (!strcmp(what, "piece"))
        got = proj.piece;
    else if (!strcmp(what, "pool"))               /* the scale, relative to the root (hex ok) */
        got = proj.pool;
    else if (!strcmp(what, "evbars"))             /* (1.2) Evolve's timing: 0 now and then, 1..4 every 4, 8, 16, 32 bars */
        got = proj.evbars;
    else if (!strcmp(what, "evolves"))            /* (1.2) Evolve's nudges so far */
        got = (int)ui_evolve_n;
    else if (!strcmp(what, "evolvebar"))          /* (1.2) the bar of the last (from 0 at the top) */
        got = (int)ui_evolve_bar;
    else if (!strcmp(what, "format"))
        got = (int)proj.format;
    else if (!strncmp(what, "lastnote_ch", 11))   /* lastnote_chN: the last note-on on MIDI channel N */
        got = last_note[(atoi(what + 11) + 15) % 16];
    else if (!strncmp(what, "act", 3) && what[3] >= '0' && what[3] <= '4')   /* (BERLIN) actN: layer N's activity x100 */
        got = (int)(en_layer_act[what[3] - '0'] * 100.0f);
    else if (!strcmp(what, "pflash"))             /* (1.3) PERFORM's flash on the screen */
        got = pflash_showing();
    else if (!strcmp(what, "pfcontrast")) {       /* (1.3) the flash's colours whose black lettering reads under 4.5:1 */
        int k, c;
        got = 0;
        for (k = 0; k < PERF_N; k++) {
            uint16_t q[2] = {pkey_col(k), K_PANEL};
            double l[2];
            for (c = 0; c < 2; c++) {
                double ch[3] = {(q[c] >> 11) / 31.0, ((q[c] >> 5) & 63) / 63.0, (q[c] & 31) / 31.0};
                int i;
                for (i = 0; i < 3; i++)
                    ch[i] = ch[i] <= 0.03928 ? ch[i] / 12.92 : pow((ch[i] + 0.055) / 1.055, 2.4);
                l[c] = 0.2126 * ch[0] + 0.7152 * ch[1] + 0.0722 * ch[2];
            }
            got += (l[0] + 0.05) / (l[1] + 0.05) < 4.5;
        }
    } else if (!strcmp(what, "pffit")) {          /* (1.3) the names and lines wider than the flash allows */
        int k;
        got = 0;
        for (k = 0; k < PERF_N; k++) {
            char t[16];
            caps(t, PKEY[k].name, sizeof t);
            got += text_w(&FONT_UL, t) > 232 || text_w(&FONT_UM, t) > 232 || text_w(&FONT_XS, PLINE[k]) > 232;
        }
    }
    else if (!strcmp(what, "pheld"))              /* (1.3) PERFORM's keys held, a bit a key (0..26) */
        got = (int)ui.pheld;
    else if (!strcmp(what, "pcc"))                /* (1.3) the effects a CC holds */
        got = (int)ui.pcc;
    else if (!strcmp(what, "ptrisk"))             /* (1.3) the map's triskele: its angle within 2 sixteenths of the music's */
        got = (int)((en_bars + 48u - ptr_at) % 48u) <= 2;
    else if (!strcmp(what, "pnow"))               /* (1.3) every effect sounding (the link's STATE) */
        got = (int)ui.pnow;
    else if (!strcmp(what, "fxcc"))               /* (1.3) the effects' CCs sent out */
        got = (int)fxcc_count;
    else if (!strcmp(what, "fxcclast"))
        got = (int)fxcc_last;
    else if (!strcmp(what, "platch"))             /* (1.3) and latched */
        got = (int)ui.platch;
    else if (!strcmp(what, "bfx"))                /* (BERLIN) the black keys' effects and moves held (0: none) */
        got = (int)(ui.bfx | ui.mv << 16);
    else if (!strncmp(what, "fo", 2) && what[2] >= '0' && what[2] <= '4' && !what[3])   /* foN: layer N's offset x100 */
        got = (int)(en_fo[what[2] - '0'] * 100.0f);
    else if (!strcmp(what, "fodot"))              /* (BERLIN) the page's brightness knob: where its filter is now, in the
                                                  * knob's own units (-999: none on this page) */
        got = knob_dot(ui.view);
    else if (!strcmp(what, "seqapart"))           /* blocks the two sequences were at different steps (peakreset) */
        got = (int)seq_apart;
    else if (!strncmp(what, "notes_ch", 8))       /* notes_chN: note-ons on MIDI channel N since the last peakreset */
        got = (int)notes_ch[(atoi(what + 8) + 15) % 16];
    else if (!strcmp(what, "pingsame"))           /* 1: the ping loops as at the last pingmark */
        got = ping_hash() == ping_mark;
    else if (!strcmp(what, "screen"))
        got = proj.screen;
    else if (!strncmp(what, "litkey", 6))         /* litkeyN: key N (0..26) lit */
        got = (int)(lit_keys >> (atoi(what + 6) % 27) & 1u);
    else if (!strcmp(what, "ksstruck"))           /* (Pluck) its strings struck so far */
        got = (int)en_ks_struck;
    else if (!strcmp(what, "chwork"))             /* (Choir) its samples rendered so far */
        got = (int)en_ch_work;
    else if (!strcmp(what, "s2shift"))            /* Sequence 2's octave away from home, Motion's (-1, 0, 1) */
        got = en_s2oct_shift;
    else if (!strcmp(what, "s2knob"))             /* Sequence 2's Octave knob as the screen shows it */
        got = knob_get(P_SEQ2OCT);
    else if (!strncmp(what, "litbtn", 6))         /* litbtnN: button N (B_*, the order of "press": OCT- 12, OCT+ 13) lit */
        got = (int)(lit_btn >> (atoi(what + 6) % NB) & 1u);
    else if (!strncmp(what, "mac", 3) && what[3] >= '0' && what[3] <= '3' && !what[4])   /* macN: master action N
                                                  * as the knob shows it (0 Filter, 1 Wet, 2 Energy), -50..50 */
        got = ui.mac[what[3] - '0'];
    else if (!strncmp(what, "emac", 4) && what[4] >= '0' && what[4] <= '3' && !what[5])   /* emacN: as the engine hears it */
        got = en_mac[what[4] - '0'];
    else if (!strcmp(what, "ringdots")) {        /* (the user, 2026-10-28) the chord's ring: its dots lit, 0..32 (-1:
                                                  * none, Drift Hold) */
        int l;
        got = ring_dots(&l);
    }
    else if (!strcmp(what, "ringlast")) {        /* 1: the ring brighter, the chord's last bar */
        int l;
        (void)ring_dots(&l);
        got = l;
    }
    else if (!strcmp(what, "layoutclash")) {      /* (the user, 2026-10-28) the orbits round the chord (and its ring)
                                                  * against what is round them, in the main canvas: the page's name,
                                                  * its dots and the user's name (top right), the section,
                                                  * hold and lock (top left), the meters, the ECHO meter and its word,
                                                  * the lanes; the places a dot would touch (0: none) */
        int v, k;
        int32_t pw = 0;
        char pn[16];
        static const float RX[] = {ORB_RX0, ORB_RX1, RING_RX}, RY[] = {ORB_RX0 * ORB_RY, ORB_RX1 * ORB_RY, RING_RY};
        for (v = 0; v < NVIEWS; v++) {
            caps(pn, VIEW_NAME[v], sizeof pn);
            pw = text_w(&FONT_B, pn) > pw ? text_w(&FONT_B, pn) : pw;
        }
        got = 0;
        for (v = 0; v < (int)(sizeof RX / sizeof RX[0]); v++)
            for (k = 0; k < 720; k++) {
                const double a = 6.283185307 * k / 720.0;
                const int32_t x = CX + (int32_t)lrint(RX[v] * cos(a)), y = CY + (int32_t)lrint(RY[v] * sin(a));
                const int32_t x0 = x - 2, x1 = x + 2, y0 = y - 2, y1 = y + 2;   /* (a dot, 3 px, and a pixel clear) */
                int bad = (x1 >= 232 - pw - 6 - text_w(&FONT_XS, "SOLO") && y0 <= 12 + FONT_B.h)   /* the page's name,
                                                                       * SOLO, the user's name */
                    || (x1 >= 212 - NROUND * 6 && y0 <= 36)            /* the page's dots */
                    || (x0 <= 8 + text_w(&FONT_XS, "EMERGE") + 6 + text_w(&FONT_XS, "hold") + 6 + text_w(&FONT_XS, "lock")
                        && y0 <= 2 + FONT_XS.h)                        /* the section, hold, lock */
                    || (x0 <= 52 && y0 <= 90)                          /* the meters, the bars under the section */
                    || (x1 >= 216 - text_w(&FONT_XS, "ECHO") - 2 && y0 <= 90 && y1 >= 30)   /* ECHO */
                    || y1 >= 104;                                      /* the lanes */
                if (bad && !got)
                    printf("  layoutclash: ellipse %d at (%d, %d)\n", v, (int)x, (int)y);
                got += bad;
            }
    }
    else if (!strcmp(what, "mutes"))              /* the layers muted (EN_M_*: 1 chord .. 16 hats, 32 noise) */
        got = (int)ui.mutes;
    else if (!strcmp(what, "scopepk")) {          /* the scope buffer (the ripples' outlines), its peak */
        int i;
        got = 0;
        for (i = 0; i < EN_SCOPE_N; i++)
            got = abs(en_scope[i]) > got ? abs(en_scope[i]) : got;
    }
    else if (!strcmp(what, "helpfit")) {          /* the knobs' help lines too wide for the rule (0: all fit) */
        int v, k;
        char hp[48];
        got = 0;
        for (v = 0; v < NVIEWS; v++)
            for (k = 0; k < 4; k++) {
                if (!KNOB_HELP[v][k]) {           /* (none: a page added without its help) */
                    printf("  no help: view %d knob %d\n", v, k + 1), got++;
                    continue;
                }
                caps(hp, KNOB_HELP[v][k], sizeof hp);
                if (text_w(&FONT_XS, hp) > 222 || strlen(KNOB_HELP[v][k]) >= sizeof hp)
                    printf("  too wide: %s\n", hp), got++;
            }
    }
    else if (!strcmp(what, "gridfit")) {          /* (the voices batch) the effects grid's cells whose name is wider
                                                  * than the cell (55 px; TWITTER fills it) (0: all fit) */
        int e;
        char t[12];
        got = 0;
        for (e = 0; e < EN_NFX; e++) {
            caps(t, GRID_CELL[e], sizeof t);
            if (text_w(&FONT_S, t) > 55)
                printf("  too wide: %s (%d px)\n", t, (int)text_w(&FONT_S, t)), got++;
        }
    }
    else if (!strcmp(what, "nev"))                /* the noise's events that show, so far */
        got = (int)en_nev_w;
    else if (!strcmp(what, "blits")) {           /* the screen's draws since the last ask */
        static uint32_t was;
        got = (int)(blits - was);
        was = blits;
    }
    else if (!strcmp(what, "section"))            /* the phrase's section (0 Full, 1 Break, 2 Groove, 3 Thin) */
        got = en_section;
    else if (!strcmp(what, "card"))               /* a nudge's card in the middle: 1 a nudge's, 2 a shake's (0: none) */
        got = card_on(plat_ms()) ? 1 + ui.card_shake : 0;
    else if (!strcmp(what, "undo"))               /* (0.5) how many steps back, and forward */
        got = ud.un;
    else if (!strcmp(what, "redo"))
        got = ud.rn;
    else if (!strcmp(what, "marked")) {           /* settings and scale that differ from the last `mark` */
        int p;
        got = proj.pool != mark_pool;
        for (p = 0; p < P_NPARAMS; p++)
            got += proj.par[p] != mark_par[p];
    }
    else if (!strcmp(what, "moves"))
        got = (int)ui.moves;
    else if (!strcmp(what, "linkon"))             /* (0.5) the USB link: the firmware's side */
        got = lk.on;
    else if (!strcmp(what, "linkheld"))
        got = (int)(link_v_btn | link_v_keys | lk.perf);
    else if (!strcmp(what, "linkbytes"))          /* what came back, in all */
        got = (int)lc.bytes;
    else if (!strcmp(what, "linkhellos"))
        got = (int)lc.hellos;
    else if (!strcmp(what, "linkstates"))
        got = (int)lc.states;
    else if (!strcmp(what, "linkacks"))
        got = (int)lc.acks;
    else if (!strcmp(what, "linkack"))            /* the last ACK's status (0 ok, 1 bad, 2 refused, 3 sum, 4 busy, 5 none) */
        got = lc.ack_status;
    else if (!strcmp(what, "linkackseq"))
        got = lc.ack_seq;
    else if (!strcmp(what, "linkgood"))           /* every frame well formed */
        got = lc.frames < 1000000u;
    else if (!strcmp(what, "linkvals"))           /* how many knobs have been told */
        got = (int)lc.vals_n;
    else if (!strncmp(what, "lhello", 6))         /* HELLO's bytes: 0 ver, 1 format, 2-3 project, 4-5 slot, 6-7 pars, 8 pieces */
        got = lc.hello[atoi(what + 6)];
    else if (!strcmp(what, "lproj"))
        got = lc.hello[2] | lc.hello[3] << 7;
    else if (!strcmp(what, "lslot"))
        got = lc.hello[4] | lc.hello[5] << 7;
    else if (!strcmp(what, "lpars"))
        got = lc.hello[6] | lc.hello[7] << 7;
    else if (!strcmp(what, "lview"))              /* STATE, as the other end reads it */
        got = lc.state[0];
    else if (!strcmp(what, "lflags"))             /* 1 playing, 2 frozen, 4 dirty, 8 PERFORM, 16 a save waiting */
        got = lc.state[1];
    else if (!strcmp(what, "lpiece"))
        got = lc.state[2];
    else if (!strcmp(what, "lmoves"))
        got = lc.state[3] | lc.state[4] << 7;
    else if (!strcmp(what, "lmuted"))
        got = lc.state[10];
    else if (!strcmp(what, "lsolo"))
        got = lc.state[11];
    else if (!strcmp(what, "lsounds"))
        got = lc.state[12];
    else if (!strcmp(what, "lundo"))
        got = lc.state[13];
    else if (!strcmp(what, "lredo"))
        got = lc.state[14];
    else if (!strcmp(what, "lmaster"))
        got = lc.state[19] | lc.state[20] << 7;
    else if (!strcmp(what, "lknob"))
        got = lc.state[21] | lc.state[22] << 7;
    else if (!strcmp(what, "llit"))
        got = lc.state[23] | lc.state[24] << 7;
    else if (!strcmp(what, "lgen"))
        got = lc.state[29];
    else if (!strcmp(what, "chorus"))             /* (1.3) PERFORM's Chorus knob */
        got = ui.chorus;
    else if (!strcmp(what, "lfx"))                /* (1.3) the effects sounding, as STATE tells them */
        got = lc.state_n >= 34 ? lc.state[30] | lc.state[31] << 7 | lc.state[32] << 14 | lc.state[33] << 21 : -1;
    else if (!strcmp(what, "lstatelen"))
        got = lc.state_n;
    else if (!strncmp(what, "lval", 4))           /* a knob as VALS told it */
        got = lc.val[atoi(what + 4)];
    else if (!strcmp(what, "lread"))              /* bytes of the last read, all there, and their sum right (`linkread`) */
        got = (int)lc.rd_len;
    else if (!strcmp(what, "lsame"))              /* the last read against `linkkeep`'s */
        got = lc.wr_len && lc.rd_len == lc.wr_len && !memcmp(lc.rd, lc.wr, lc.wr_len);
    else if (!strcmp(what, "flash"))              /* a button's new page: its name, large (0.5) */
        got = flash_on(plat_ms());
    else if (!strcmp(what, "pagedots")) {         /* how many pages this page's button has (0.5) */
        int at = 0;
        got = page_dots(ui.view, &at);
    }
    else if (!strcmp(what, "pagedot")) {          /* and which of them this is */
        int at = -1;
        page_dots(ui.view, &at);
        got = at;
    }
    else if (!strcmp(what, "lock"))               /* PERFORM's Lock (SEL held) */
        got = ui.lock;
    else if (!strcmp(what, "octring"))            /* the level ring showing: its layer + 1 (0: none) */
        got = oct_ring_on(plat_ms());
    else if (!strcmp(what, "dirty"))
        got = ui.dirty;
    else if (!strcmp(what, "store_writes"))
        got = (int)store_writes;
    else if (!strcmp(what, "clocks_out"))
        got = (int)clock_out_count;
    else if (!strcmp(what, "sixteenths"))
        got = (int)en_bars;
    else if (!strcmp(what, "ext"))
        got = en_ext;
    else if (!strcmp(what, "midi_out"))
        got = (int)midi_out_count;
    else if (!strcmp(what, "pad_level"))
        got = (int)(en_pad_level * 100.0f);
    else if (!strcmp(what, "bass_level"))
        got = (int)(en_bass_level * 100.0f);
    else if (!strcmp(what, "chordroot"))
        got = en_chord_root;
    else if (!strcmp(what, "progpos"))            /* the slot the progression is on (Endless: 0; -1: none) */
        got = en_prog_pos;
    else if (!strncmp(what, "chord", 5))          /* chordN: pad voice N's note */
        got = en_chord[atoi(what + 5) & 3];
    else if (!strcmp(what, "loopnotes")) {        /* the notes in all the loops */
        int l, i;
        got = 0;
        for (l = 0; l < EN_NLOOP; l++)
            for (i = 0; i < EN_LOOP_MAX; i++)
                got += en_loop_note[l][i] >= 0;
    } else if (!strncmp(what, "par", 3)) {        /* parN: parameter N (amb.h P_*) */
        int k = atoi(what + 3);
        if (k < 0 || k >= P_NPARAMS) {
            printf("  FAIL expect %s: no such parameter (0..%d)\n", what, P_NPARAMS - 1);
            return 1;
        }
        got = proj.par[k];
    }
    else if (!strcmp(what, "peak_db_max") || !strcmp(what, "peak_db_min")) {
        double db = 20.0 * log10(peak_out > 1e-9f ? peak_out : 1e-9f);
        int bad = what[8] == 'm' && what[9] == 'a' ? db > atof(val) : db < atof(val);
        printf("%s peak %.1f dBFS (%s %s)\n", bad ? "FAIL" : "  ok", db, what + 8, val);
        return bad;
    } else {
        printf("FAIL unknown expect %s\n", what);
        return 1;
    }
    if (val[0] == '!' && val[1] == '=') {        /* (!=N: anything but N) */
        int no = (int)strtol(val + 2, 0, 0);
        if (got == no) {
            printf("FAIL expect %s %s, got %d\n", what, val, got);
            return 1;
        }
        printf("  ok %s == %d (%s)\n", what, got, val);
        return 0;
    }
    if (val[0] == '<' || val[0] == '>') {
        int lim = (int)strtol(val + 1, 0, 0);
        if (val[0] == '<' ? got >= lim : got <= lim) {
            printf("FAIL expect %s %s, got %d\n", what, val, got);
            return 1;
        }
        printf("  ok %s == %d (%s)\n", what, got, val);
        return 0;
    }
    if (got != (int)strtol(val, 0, 0)) {
        printf("FAIL expect %s == %s, got %d\n", what, val, got);
        return 1;
    }
    printf("  ok %s == %d\n", what, got);
    return 0;
}

#ifndef EN_WEB
/* (0.5) scenarios' `link NAME args`: a frame as the page and tools/fm1_link.py send it */
static uint8_t lc_out[640];
static uint32_t lc_out_n;
static int link_command(const char *name, const char *line)
{
    static const struct { const char *name; int cmd; const char *args; } OPS[] = {
        {"set", LK_SET, "22"}, {"view", LK_VIEW, "11"}, {"piece", LK_PIECE, "1"}, {"play", LK_PLAY, ""}, {"hold", LK_HOLD, ""},
        {"perf", LK_PERF, "11"}, {"forget", LK_FORGET, ""}, {"keys", LK_KEYS, "4"}, {"buttons", LK_BUTTONS, "2"},
        {"undo", LK_UNDO, ""}, {"redo", LK_REDO, ""}, {"save", LK_SAVE, ""}, {"voice", LK_VOICE, "22"}, {"copy", LK_COPY, "11"},
        {"swap", LK_SWAP, "11"}, {"reset", LK_RESET, "1"}, {"open", LK_OPEN, "1"}, {"read", LK_READ, "2"}, {"begin", LK_BEGIN, "1"},
        {"write", LK_WRITE, "2"}, {"commit", LK_COMMIT, "3"}, {"nonsense", 60, ""},
    };
    uint32_t i, n = 0;
    if (!strcmp(name, "again"))
        return link_frame(lc_out, lc_out_n, now_ms) != 1;
    lc_out[n++] = LK_H0, lc_out[n++] = LK_H1, lc_out[n++] = LK_H2;
    if (!strcmp(name, "hello")) {
        lc_out[n++] = LK_HELLO, lc_out[n++] = LK_VER;
    } else if (!strcmp(name, "beat")) {
        lc_out[n++] = LK_BEAT;
    } else if (!strcmp(name, "bye")) {
        lc_out[n++] = LK_BYE;
    } else {
        const char *t = line;
        const char *f = 0;
        int cmd = 0, field;
        for (i = 0; i < sizeof OPS / sizeof OPS[0]; i++)
            if (!strcmp(OPS[i].name, name))
                cmd = OPS[i].cmd, f = OPS[i].args;
        if (!f)
            return 1;
        lc_out[n++] = (uint8_t)cmd;
        lc.seq = (uint8_t)((lc.seq + 1u) & 0x7Fu);
        lc_out[n++] = lc.seq;
        while (*t == ' ') t++;                   /* (past "link" and the name) */
        while (*t && *t != ' ') t++;
        while (*t == ' ') t++;
        while (*t && *t != ' ') t++;
        for (field = 0; f[field]; field++) {
            char *e;
            const long v = strtol(t, &e, 0);
            int b;
            if (e == t)
                return 1;
            t = e;
            for (b = 0; b < f[field] - '0'; b++)
                lc_out[n++] = (uint8_t)((unsigned long)v >> (7 * b) & 0x7Fu);
        }
    }
    lc_out_n = n;
    return link_frame(lc_out, n, now_ms) != 1;
}
static int link_send_op(int cmd, const uint8_t *args, uint32_t na)
{
    uint32_t n = 0;
    lc_out[n++] = LK_H0, lc_out[n++] = LK_H1, lc_out[n++] = LK_H2, lc_out[n++] = (uint8_t)cmd;
    lc.seq = (uint8_t)((lc.seq + 1u) & 0x7Fu);
    lc_out[n++] = lc.seq;
    if (na)
        memcpy(lc_out + n, args, na);
    n += na;
    lc_out_n = n;
    return link_frame(lc_out, n, now_ms) != 1;
}
static int link_read(int area)
{
    uint8_t a[2];
    uint32_t len, sum, off;
    a[0] = (uint8_t)area;
    link_send_op(LK_OPEN, a, 1);
    if (lc.ack_status != LK_OK || lc.ack_seq != lc.seq || lc.ack_n != 5u) {
        lc.rd_len = 0;
        return 0;                                /* (`expect linkack` says why) */
    }
    len = (uint32_t)(lc.ack_x[0] | lc.ack_x[1] << 7);
    sum = (uint32_t)(lc.ack_x[2] | lc.ack_x[3] << 7 | lc.ack_x[4] << 14);
    memset(lc.rd_got, 0, sizeof lc.rd_got);
    for (off = 0; off < len; off += LK_CHUNK) {
        a[0] = (uint8_t)(off & 0x7Fu), a[1] = (uint8_t)(off >> 7);
        link_send_op(LK_READ, a, 2);
    }
    for (off = 0; off < len; off++)
        if (!lc.rd_got[off]) {
            printf("link read: byte %u never came\n", off);
            return 1;
        }
    if (lk_sum(lc.rd, len) != sum) {
        printf("link read: wrong sum\n");
        return 1;
    }
    lc.rd_len = len;
    return 0;
}
static void link_write(int area, int sumbad)
{
    uint8_t a[LK_FRAME];
    uint32_t off, sum;
    a[0] = (uint8_t)area;
    link_send_op(LK_BEGIN, a, 1);
    for (off = 0; off < lc.wr_len; off += LK_CHUNK) {
        const uint32_t m = lc.wr_len - off < LK_CHUNK ? lc.wr_len - off : LK_CHUNK;
        a[0] = (uint8_t)(off & 0x7Fu), a[1] = (uint8_t)(off >> 7);
        link_send_op(LK_WRITE, a, 2u + lk_pack(a + 2, lc.wr + off, m));
    }
    sum = lk_sum(lc.wr, lc.wr_len) ^ (sumbad ? 1u : 0u);
    a[0] = (uint8_t)(sum & 0x7Fu), a[1] = (uint8_t)(sum >> 7 & 0x7Fu), a[2] = (uint8_t)(sum >> 14 & 0x7Fu);
    link_send_op(LK_COMMIT, a, 3);
}

int main(int argc, char **argv)
{
    FILE *sc;
    char line[512], out[400];
    const char *dir = argc > 2 ? argv[2] : ".";
    int lineno = 0, fails = 0;
    if (argc < 2) {
        fprintf(stderr, "usage: amb_host SCRIPT [OUTDIR]\n");
        return 2;
    }
    sc = fopen(argv[1], "r");
    if (!sc) {
        perror(argv[1]);
        return 2;
    }
    snprintf(out, sizeof out, "%s/midi_out.txt", dir);
    midi_log = fopen(out, "w");
    if (getenv("AMB_SAID_LOG")) {                        /* a reader listens from the start, quietly: the log only */
        said_log_f = fopen(getenv("AMB_SAID_LOG"), "a");
        sim_listen = 1;
        said_print = 0;
    }
#if AISLING_BOARD_SMK37
    master_power_on();
#endif
    boot();
    if (sim_listen) {
        static const uint8_t HELLO[] = {ANN_H0, ANN_H1, ANN_H2, ANN_HELLO, ANN_VER};
        ann_frame(HELLO, sizeof HELLO, now_ms);
    }
    while (fgets(line, sizeof line, sc)) {
        char cmd[32] = {0}, a[256] = {0}, b[64] = {0}, c[64] = {0};
        lineno++;
        if (line[0] == '#' || sscanf(line, "%31s %255s %63s %63s", cmd, a, b, c) < 1)
            continue;
        if (!strcmp(cmd, "wait"))
            run_ms((uint32_t)atoi(a));
        else if (!strcmp(cmd, "press") || !strcmp(cmd, "release") || !strcmp(cmd, "tap")) {
            int i = find_name(BTN_N, NB, a);
            uint32_t *held = &held_btn;
#if AISLING_BOARD_SMK37
            if (i < 0 && (i = find_name(EXTRA_N, 4, a)) >= 0)
                held = &held_extra;
#endif
            if (i < 0) {
                printf("line %d: no button %s\n", lineno, a);
                return 2;
            }
            if (cmd[0] != 'r')
                *held |= 1u << i;
            if (cmd[0] == 't')
                run_ms(40);
            if (cmd[0] != 'p')
                *held &= ~(1u << i);
            run_ms(cmd[0] == 't' ? 40 : 2);
        } else if (!strcmp(cmd, "key") || !strcmp(cmd, "tapkey")) {
            int k = key_of(a);
            if (k < 0) {
                printf("line %d: no key %s\n", lineno, a);
                return 2;
            }
            if (cmd[0] == 't') {
                key_set(k, 1);
                run_ms(60);
                key_set(k, 0);
                run_ms(30);
            } else {
                key_set(k, !strcmp(b, "down"));
                run_ms(2);
            }
        } else if (!strcmp(cmd, "strum")) {          /* strum A B [MS]: a finger across the white keys */
            int x = atoi(a), y = atoi(b), gap = c[0] ? atoi(c) : 25, s = y >= x ? 1 : -1;
            for (;; x += s) {
                key_set(STRUM_KEY(x), 1);
                run_ms((uint32_t)gap);
                key_set(STRUM_KEY(x), 0);
                if (x == y)
                    break;
            }
            run_ms(2);
        } else if (!strcmp(cmd, "turn")) {
            int e = find_name(ENC_N, NE, a), n = atoi(b), s = n > 0 ? 1 : -1;
            uint32_t gap = c[0] ? (uint32_t)atoi(c) : 80u;
#if AISLING_BOARD_SMK37
            if (e < 0 && !strcmp(a, "K1")) {
                for (; n; n -= s) {
                    master_turn(s);
                    run_ms(gap);
                }
            } else
#endif
            if (e < 0) {
                printf("line %d: no encoder %s\n", lineno, a);
                return 2;
            }
            for (; e >= 0 && n; n -= s) {
                enc_acc[e] += s;
                film_enc[e] += s;
                run_ms(gap);
            }
        } else if (!strcmp(cmd, "spin")) {
            int e = find_name(ENC_N, NE, a);
#if AISLING_BOARD_SMK37
            if (e < 0 && !strcmp(a, "K1")) {
                master_turn(atoi(b));
                run_ms(1);
                continue;
            }
#endif
            if (e < 0) {
                printf("line %d: no encoder %s\n", lineno, a);
                return 2;
            }
            enc_acc[e] += atoi(b);
            film_enc[e] += atoi(b);
            accel_off = 1;
            run_ms(1);
            accel_off = 0;
        } else if (!strcmp(cmd, "master")) {
#if AISLING_BOARD_SMK37
            printf("line %d: no MASTER pot on the SMK-37 Elite (turn K1)\n", lineno);
            return 2;
#else
            master = (uint32_t)atoi(a);
#endif
        } else if (!strcmp(cmd, "battery"))
            sim_batt_set = 1, sim_batt = atoi(a);
        else if (!strcmp(cmd, "midi")) {
            uint32_t s = (uint32_t)strtoul(a, 0, 16), d1 = (uint32_t)strtoul(b, 0, 16), d2 = (uint32_t)strtoul(c, 0, 16);
            uint32_t cin = s >= 0xF0 ? 0x0F : s >> 4;
            min_q[mi_w++ % MQ] = cin | s << 8 | d1 << 16 | d2 << 24;
        } else if (!strcmp(cmd, "wav")) {
            snprintf(out, sizeof out, "%s/%s", dir, a);
            wav_open(out);
            if (midi_log)                        /* (where the WAV starts and stops, for tools/midi_export.py) */
                fprintf(midi_log, "%u ms: wav %s %d bpm\n", now_ms, a, (int)proj.par[P_TEMPO]);
        } else if (!strcmp(cmd, "wavstop")) {
            wav_close();
            if (midi_log)
                fprintf(midi_log, "%u ms: wavstop\n", now_ms);
        } else if (!strcmp(cmd, "link") || !strcmp(cmd, "linkagain")) {   /* (0.5) a frame, as the other end sends it */
            int bad = link_command(cmd[4] ? "again" : a, line);
            if (bad)
                printf("FAIL line %d: %s\n", lineno, line), fails++;
            run_ms(30);
        } else if (!strcmp(cmd, "linkzero")) {     /* the counts start again */
            lc.bytes = lc.frames = lc.hellos = lc.states = lc.acks = lc.chunks = 0;
        } else if (!strcmp(cmd, "linkbeat")) {
            lc.beat = atoi(a);
        } else if (!strcmp(cmd, "linkread")) {     /* OPEN area, READ it all, sum checked */
            if (link_read(atoi(a)))
                printf("FAIL line %d: %s\n", lineno, line), fails++;
        } else if (!strcmp(cmd, "linkkeep")) {     /* the last read, kept: what `linkwrite` sends, `lsame` compares */
            memcpy(lc.wr, lc.rd, sizeof lc.wr);
            lc.wr_len = lc.rd_len;
        } else if (!strcmp(cmd, "linkpoke")) {     /* a byte of what's kept: to test the checks */
            int at = 0, v = 0;
            sscanf(line, "%*s %d %d", &at, &v);
            lc.wr[at] = (uint8_t)v;
        } else if (!strcmp(cmd, "linkpar")) {      /* a setting of what's kept (a project's): to test the clamps */
            int p = 0, v = 0;
            sscanf(line, "%*s %d %d", &p, &v);
            ((project_t *)lc.wr)->par[p] = (int16_t)v;
        } else if (!strcmp(cmd, "linkwrite")) {    /* BEGIN area, WRITE it all, COMMIT; `linkwrite A sumbad` lies about the sum */
            link_write(atoi(a), strstr(line, "sumbad") != 0);
            run_ms(30);
        } else if (!strcmp(cmd, "mark")) {
            memcpy(mark_par, proj.par, sizeof mark_par);
            mark_pool = proj.pool;
        } else if (!strcmp(cmd, "peakreset"))
            peak_out = 0.0f, memset(notes_ch, 0, sizeof notes_ch), seq_apart = 0;
        else if (!strcmp(cmd, "shot")) {
            snprintf(out, sizeof out, "%s/%s", dir, a);
            png_write(out);
        } else if (!strcmp(cmd, "leds")) {
            int i;
            printf("  leds: buttons");
            for (i = 0; i < NB; i++)
                if (lit_btn >> i & 1u)
                    printf(" %s", BTN_N[i]);
            printf(" | white ");
            for (i = 0; i < 16; i++)
                printf("%c", (lit_keys >> WHITE_K[i] & 1u) ? '#' : '.');
            printf(" black ");
            for (i = 0; i < 11; i++)
                printf("%c", (lit_keys >> BLACK_K[i] & 1u) ? '#' : '.');
            printf("\n");
        } else if (!strcmp(cmd, "expect"))
            fails += expect(a, b);
        else if (!strcmp(cmd, "film")) {
            snprintf(out, sizeof out, "%s/%s", dir, a);
            film = fopen(out, "wb");
            film_due = (double)now_ms;
            film_t0 = now_ms;
        } else if (!strcmp(cmd, "filmstop")) {
            if (film)
                fclose(film);
            film = 0;
        } else if (!strcmp(cmd, "at") || !strcmp(cmd, "bar")) {
            const char *lab = strstr(line, a) + strlen(a);
            uint32_t t;
            while (*lab == ' ' || *lab == '\t')
                lab++;
            if (cmd[0] == 'a') {
                uint32_t due = film_t0 + (uint32_t)(atof(a) * 1000.0 + 0.5);
                if (now_ms > due)
                    printf("line %d: at %s is already past (%.1f s)\n", lineno, a, (now_ms - film_t0) / 1000.0);
                while (now_ms < due)
                    run_ms(1);
            } else {
                uint32_t want = 16u * (uint32_t)atoi(a), guard = 0;
                while (en_bars < want && guard++ < 600000u)
                    run_ms(1);
            }
            t = now_ms - film_t0;
            t = (t + 50u) / 100u;                /* (in tenths, rounded first: 59.96 s is 01:00.0, not 00:60.0) */
            printf("@ %02u:%02u.%u  bar %u  %s", t / 600u, t % 600u / 10u, t % 10u, en_bars / 16u, lab[0] ? lab : "\n");
        } else if (!strcmp(cmd, "storefile")) {  /* a saved project from a file, as it would be in the flash */
            static uint8_t fb[PLAT_STORE_MAX];
            FILE *f = fopen(a, "rb");
            size_t got = f ? fread(fb, 1, sizeof fb, f) : 0;
            if (f)
                fclose(f);
            if (!got) {
                printf("line %d: no file %s\n", lineno, a);
                return 2;
            }
            plat_store_save(OBJ_PROJ, fb, (uint32_t)got);
        } else if (!strcmp(cmd, "orphans")) {   /* (BERLIN) every knob on every page the buttons reach changes the
                                                  * sound or the MIDI, its lowest against its highest (alone in a
                                                  * scenario: it leaves the engine reset) */
            static const uint8_t REACH[] = {V_DUB, V_KEY, V_CHORD, V_CSND, V_TEXTURE, V_PSND, V_PFM, V_BASS, V_BSND,
                                            V_DRUMS, V_DSND, V_PAT, V_SPACE, V_ESND, V_SETUP, V_MIX, V_PERF, V_NSND,
                                            V_NSND2, V_ARC, V_CFM, V_BFM, V_SMOVE, V_S2MOVE, V_AMOVE,
                                            V_LSTYLE};
            float e0[16], e1[16];
            uint32_t m0, m1;
            unsigned k;
            int i, n = 0;
            orph_render(P_TUNE, 0, e0, &m0);     /* (the same twice: repeatable, or nothing can be told) */
            orph_render(P_TUNE, 0, e1, &m1);
            if (!orph_same(e0, e1, m0, m1)) {
                printf("FAIL orphans: the engine is not repeatable from a reset (MIDI %s; energy by stretch:",
                       m0 == m1 ? "the same" : "differs");
                for (i = 0; i < 16; i++)
                    printf(" %.3g/%.3g", (double)e0[i], (double)e1[i]);
                printf(")\n");
                fails++;
            } else
                for (k = 0; k < sizeof REACH; k++)
                    for (i = 0; i < 4; i++) {
                        const int p = VIEW_KNOB[REACH[k]][i];
                        if (p >= P_NPARAMS)
                            continue;
                        orph_render(p, en_param_info(p)->lo, e0, &m0);
                        orph_render(p, en_param_info(p)->hi, e1, &m1);
                        n++;
                        if (orph_same(e0, e1, m0, m1)) {
                            printf("FAIL orphan: %s page, knob %d (%s) changes nothing\n", VIEW_NAME[REACH[k]], i + 1,
                                   en_param_info(p)->name);
                            fails++;
                        }
                    }
            printf("  orphans: %d knobs checked\n", n);
        } else if (!strcmp(cmd, "pingmark")) {
            ping_mark = ping_hash();
        } else if (!strcmp(cmd, "splashrun")) {
            int f;
            ui_off = 1;
            for (f = 1; f < UI_SPLASH_N; f++) {
                ui_splash(f);
                run_ms(55);
            }
            ui_off = 0;
            ui_init();
        } else if (!strcmp(cmd, "splash"))           /* splash N: the launch screen's frame N (shot it after) */
            ui_splash(atoi(a));
        else if (!strcmp(cmd, "reboot")) {
            memset(&proj, 0, sizeof proj);
#if AISLING_BOARD_SMK37
            master_power_on();                       /* (K1's position is not kept) */
#endif
            boot();
        } else if (!strcmp(cmd, "listen")) {
            static const uint8_t HELLO[] = {ANN_H0, ANN_H1, ANN_H2, ANN_HELLO, ANN_VER};
            sim_listen = 1;
            said_len = 0;
            said_log[0] = 0;
            ann_frame(HELLO, sizeof HELLO, now_ms);
            run_ms(40);
        } else if (!strcmp(cmd, "speakdump")) {        /* every screen's text for a screen reader, to stdout */
            speak_dump();
        } else if (!strcmp(cmd, "where")) {
            static const uint8_t WHERE[] = {ANN_H0, ANN_H1, ANN_H2, ANN_WHERE};
            ann_frame(WHERE, sizeof WHERE, now_ms);
            run_ms(40);
        } else if (!strcmp(cmd, "said") || !strcmp(cmd, "said_none")) {
            int none = cmd[4] != 0;
            char *t = line + strlen(cmd), *e;
            while (*t == ' ')
                t++;
            e = t + strlen(t);
            while (e > t && (e[-1] == '\n' || e[-1] == '\r' || e[-1] == ' '))
                *--e = 0;
            if ((strstr(said_log, t) != 0) == none) {
                printf("FAIL line %d: %s \"%s\": %s (since the last check: %s)\n", lineno, cmd, t,
                       none ? "said" : "not said", said_len ? said_log : "nothing\n");
                fails++;
            } else {
                printf("  ok %s \"%s\"\n", cmd, t);
            }
            said_len = 0;
            said_log[0] = 0;
        } else if (!strcmp(cmd, "echo"))
            printf("%s", line + 5);
        else {
            printf("line %d: unknown command %s\n", lineno, cmd);
            return 2;
        }
    }
    wav_close();
    if (midi_log)
        fclose(midi_log);
    printf("host: %u ms simulated, %u blits, render %.2f%% of real time (host CPU)%s\n", now_ms, blits,
           render_budget_ns_total > 0 ? 100.0 * render_ns_total / render_budget_ns_total : 0.0,
           fails ? ", EXPECTATIONS FAILED" : "");
    return fails ? 1 : 0;
}
#endif /* EN_WEB */
