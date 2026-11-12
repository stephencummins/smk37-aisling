/* SPDX-License-Identifier: GPL-3.0-only */
/* AMB-1 platform interface (from X0X's): everything the app needs from the hardware.
 *
 * The device implementation (plat_fm1.c) wraps Felucca's HAL and panel map; the
 * host implementation (host/amb_host.c) is a simulator fed by a script, which is
 * what lets the whole app — UI, sequencer, engines — run and be tested on a Mac.
 * Drawing goes through Felucca's lcd_fill / lcd_blit (lcd.c on the device, a
 * framebuffer on the host) and gfx.c on top. */
#pragma once
#include <stdint.h>

/* big zero-initialised buffers go in the .pool RAM region on the device */
#ifdef EN_HOST
#define EN_POOL
#else
#define EN_POOL __attribute__((section(".pool")))
#endif

/* panel controls, by printed label (Felucca's panel.c map turns these into matrix ids; Felucca
 * calls the second button SCL, the panel prints SEL) */
enum { B_FX, B_SEL, B_ENV, B_LFO, B_EDIT, B_GLO, B_HOME, B_SAVE, B_ARP, B_SEQ, B_PLAY, B_REC,
       B_OCTDN, B_OCTUP, NB };
enum { EN_SELECT, EN_ALGO, EN_PRESET, EN_K1, EN_K2, EN_K3, EN_K4, NE };

#define NKEYS 27                       /* F3 .. G5 */

uint32_t plat_ms(void);
uint32_t plat_buttons(void);           /* held: bit B_* */
uint32_t plat_keys(void);              /* held: bit k = key k (0 = F3) */
int32_t plat_enc(int role);            /* detents since the last call, + = clockwise; EN_* */
uint32_t plat_master(void);            /* MASTER pot, 0..4096 gain Q12 */
uint32_t plat_master_raw(void);        /* the knob itself, 0..1023, before the curve (the USB link reports both) */
void plat_leds(uint32_t buttons, uint32_t keys);   /* lit: bit B_*, bit key */
void plat_glow(int on);                /* the unlit buttons glow dimly, readable in the dark (call before plat_leds) */
void plat_play_red(int red);           /* PLAY lights green; red (its other LED) while recording (before plat_leds) */
void plat_screen(int on);              /* (1.1) the screen: off (black) while asleep (the sleep timer, ui.c) */

/* the SMK-37 Elite's own controls (board_caps.h CAP_PADS, CAP_FADERS; only called where those are not 0) */
uint32_t plat_pads(void);              /* down: bit p = pad p, 0..7 the top row left to right, 8..15 the bottom */
void plat_pads_rest(void);             /* every pad let go, its resting level learned again (one stuck: the way out) */
int32_t plat_fader(int i);             /* fader i (0..3, left to right), 0..1023; -1: not read yet */
int32_t plat_wheel(int i);             /* 0 the pitch wheel, 1 the mod wheel, 0..1023 as read; -1: not read yet */
enum { X_KNOB_BANK = 1, X_FADER_BANK = 2, X_LEFT = 4, X_RIGHT = 8 };
uint32_t plat_extra(void);             /* KNOB BANK, FADER BANK, < and > held: X_* */
void plat_extra_leds(uint32_t lit);    /* and their lights: X_* (before plat_leds) */

/* USB MIDI, 4-byte USB-MIDI event packets (cable 0) */
int plat_midi_in(uint32_t *pkt);       /* 1 = got one (called from the audio ISR) */
void plat_midi_out(uint32_t pkt);      /* queue one (audio ISR or main loop) */

/* persistent storage (Felucca storage.c objects: A/B sectors, CRC, torn-write safe) */
int plat_store_load(uint32_t obj, void *dst, uint32_t max);   /* bytes loaded, < 0 = none / bad */
int plat_store_save(uint32_t obj, const void *src, uint32_t len);   /* 0 = ok */
#define PLAT_STORE_MAX 3840u           /* bytes per object */
enum { OBJ_PROJ, OBJ_PANEL, OBJ_NOBJ };
/* flash: OBJ_PROJ FL_PROJ_SECTOR, OBJ_PANEL FL_PANEL_SECTOR (hal/fl_map.h; FM-1 0xFC000 and 0xDC000), 2 sectors
 * each, inside the board's store */

/* audio load in % of the render budget, and xruns since boot (device: from the ISR) */
uint32_t plat_cpu_pct(void);
uint32_t plat_xruns(void);
uint32_t plat_cpu_peak(void);            /* the heaviest block of the last ~2 s, % of its time */
int32_t plat_battery(void);              /* the battery divider's reading, raw (0..1023, smoothed); -1: none */
int plat_usb_power(void);                /* a USB host is there (it powers, and charges, the FM-1) */
