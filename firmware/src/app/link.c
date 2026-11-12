/* SPDX-License-Identifier: GPL-3.0-only */
/* The USB link (0.5): an editor page, or tools/fm1_link.py, reads and changes the FM-1 over its USB MIDI port. Included
 * by ui.c, so the firmware, the host and the page's own copy all run this same code.
 *
 *   F0 7D 41 4C cmd seq args.. F7        (7D: non-commercial. The screen reader's is 53 52, the debug tool's 46 4C)
 *
 * Silent until a HELLO: a DAW on the same port never sees a byte. Gone after 5 s without one of its frames: whatever it
 * was holding down lets go. USB only: the TRS jack ignores SysEx.
 *
 * usb.c keeps one SysEx frame at a time and drops any that arrives before the last was handled, so the other end sends
 * one request, waits for its ACK, and after a timeout sends it again with the same seq. Here, a repeated seq only sends
 * the ACK again: a button isn't to be pressed twice. (READ only reads: it runs each time.)
 *
 * Every byte after F0 is below 0x80: numbers in 7-bit pieces, the project's bytes in eights packed into sevens.
 *
 * host -> FM-1 (seq on all but HELLO, BYE and BEAT)
 *   HELLO ver            start: everything is sent afresh                     reply: HELLO
 *   BYE, BEAT            finished; still here (no reply)
 *   SET k.2 v.2          a knob, as turning it would                          all others reply: ACK seq status ..
 *   VIEW v knob          a page, and the knob to light on it (127: none)
 *   PIECE i, PLAY, HOLD, UNDO, REDO, FORGET
 *   PERF i down          PERFORM's key i
 *   KEYS m.4, BUTTONS m.2  held down, as if by hand
 *   SAVE                 saved as soon as it's at rest (STATE's flag shows it waiting)
 *   VOICE p.2 v.2        a layer's voice
 *   COPY from to, SWAP a b, RESET i
 *   OPEN area            to read: ACK adds len.2 sum.3                        area: LK_A_*
 *   READ off.2                                                                reply: CHUNK seq off.2 data
 *   BEGIN area           to write
 *   WRITE off.2 data
 *   COMMIT sum.3         checked as flash is checked at boot, then taken
 *
 * FM-1 -> host, seq 0 unless it answers one
 *   HELLO ver format proj.2 slot.2 npars.2 npieces len version-text
 *   STATE (about 10 times a second; link_state)
 *   VALS  (k.2 v.2 len text)..  a knob's value and its words, when they move
 */

#define LK_H0 0x7Du
#define LK_H1 0x41u
#define LK_H2 0x4Cu
#define LK_VER 1u
#define LK_GONE_MS 5000u
#define LK_TICK_MS 100u
#define LK_CHUNK 128u
#define LK_FRAME 200u                            /* the longest frame sent: about a third of usb.c's queue */
#define LK_VALS_MAX 150u
#define LK_VALS_N 30u

enum { LK_HELLO = 1, LK_BYE, LK_BEAT, LK_SET, LK_VIEW, LK_PIECE, LK_PLAY, LK_HOLD, LK_PERF, LK_FORGET, LK_KEYS, LK_BUTTONS,
       LK_UNDO, LK_REDO, LK_SAVE, LK_VOICE, LK_COPY, LK_SWAP, LK_RESET, LK_OPEN, LK_READ, LK_BEGIN, LK_WRITE, LK_COMMIT };
enum { LKR_HELLO = 65, LKR_ACK, LKR_STATE, LKR_VALS, LKR_CHUNK };
enum { LK_DIAG = 40, LK_DIAG_AN = 41, LKR_DIAG = 70, LKR_DIAG_AN = 71 };            /* (AISLING_SMK_DIAG builds only) */
enum { LK_OK, LK_BAD, LK_REFUSED, LK_SUM, LK_BUSY, LK_NONE };
/* areas: the project as it plays; pieces 1..8's slots (LK_A_SLOT + i); the bytes saved in flash */
enum { LK_A_PROJ, LK_A_SLOT, LK_A_FLASH = LK_A_SLOT + NPIECES };
enum { LKF_PLAYING = 1, LKF_FROZEN = 2, LKF_DIRTY = 4, LKF_PERF = 8, LKF_SAVING = 16 };

/* the knobs that aren't settings, sent as settings are (K_CPU is in STATE; K_NONE is nothing) */
static const uint8_t LK_EXTRA[] = {K_LEDS, K_SLOT, K_CQ, K_CX, K_LANE, K_LCH, K_FILTER, K_DUB, K_ENERGY, K_SPACE, K_MODE,
                                   K_PLAY, K_SAVE, K_CHORUS};
#define LK_NEXTRA ((int)(sizeof LK_EXTRA / sizeof LK_EXTRA[0]))
#define LK_NVALS (P_NPARAMS + LK_NEXTRA)
#define LK_UNSENT ((int16_t)0x7FFF)

static struct {
    uint8_t on, seen_seq, seq, status;
    uint8_t ack[6], ack_n;                       /* (an ACK's extras: what a repeated seq sends again) */
    uint8_t save_wait, mode, area;
    uint16_t len, scan;
    uint32_t heard, tick_t, perf;
    int16_t val[LK_NVALS];
} lk;
/* the bytes of a transfer, one at a time (in the pool: the link costs the firmware's RAM little) */
static EN_POOL union {
    project_t p;
    slot_t s;
    uint8_t b[sizeof(project_t)];
} lk_x;
static EN_POOL project_t lk_saved;               /* (a backup going back to flash, byte for byte) */

static void put14(uint8_t *o, int v)             /* two pieces, low first; a negative's two's complement */
{
    o[0] = (uint8_t)(v & 0x7F);
    o[1] = (uint8_t)(v >> 7 & 0x7F);
}
static int get14u(const uint8_t *a) { return a[0] | a[1] << 7; }
static int get14s(const uint8_t *a)
{
    const int v = get14u(a);
    return v >= 8192 ? v - 16384 : v;
}

static uint32_t lk_sum(const uint8_t *p, uint32_t n)   /* FNV-1a, in 21 bits: three pieces */
{
    uint32_t h = 2166136261u, i;
    for (i = 0; i < n; i++)
        h = (h ^ p[i]) * 16777619u;
    return (h ^ h >> 21) & 0x1FFFFFu;
}

static uint32_t lk_pack(uint8_t *o, const uint8_t *s, uint32_t n)   /* each seven: a byte of their top bits, then them */
{
    uint32_t i, j, w = 0;
    for (i = 0; i < n; i += 7u) {
        const uint32_t m = n - i < 7u ? n - i : 7u, h = w++;
        uint8_t hi = 0;
        for (j = 0; j < m; j++) {
            hi = (uint8_t)(hi | (s[i + j] >> 7) << j);
            o[w++] = s[i + j] & 0x7Fu;
        }
        o[h] = hi;
    }
    return w;
}
static uint32_t lk_unpack(uint8_t *o, const uint8_t *s, uint32_t n, uint32_t room)   /* bytes out; more than room: 0 */
{
    uint32_t i = 0, w = 0, j;
    while (i < n) {
        const uint8_t hi = s[i++];
        for (j = 0; j < 7u && i < n; j++, i++) {
            if (w >= room)
                return 0;
            o[w++] = (uint8_t)(s[i] | ((hi >> j & 1u) << 7));
        }
    }
    return w;
}

static uint32_t lk_head(uint8_t *f, int cmd, int seq)
{
    f[0] = 0xF0, f[1] = LK_H0, f[2] = LK_H1, f[3] = LK_H2, f[4] = (uint8_t)cmd, f[5] = (uint8_t)seq;
    return 6u;
}
static uint32_t lk_len(const char *t)            /* (the firmware has no libc) */
{
    uint32_t n = 0;
    while (t[n])
        n++;
    return n;
}

static int lk_send(uint8_t *f, uint32_t n)       /* 0: sent; else not at all (no room: nobody reading, or too much) */
{
    f[n++] = 0xF7;
    return link_wire_send(f, n);
}

static void lk_ack(void)
{
    uint8_t f[16];
    uint32_t n = lk_head(f, LKR_ACK, lk.seq), i;
    f[n++] = lk.status;
    for (i = 0; i < lk.ack_n; i++)
        f[n++] = lk.ack[i];
    lk_send(f, n);                               /* (no room: the other end asks again) */
}

/* ------------------------------------------------ the operations (the page's own copy uses these too) --- */
static void link_view_go(int v)                  /* a page: as its button would, PERFORM entered or left as needed */
{
    if (v < 0 || v >= NVIEWS)
        return;
    if (v == V_PERF || v == V_MIX) {
        if (!ui.perf)
            perf_set(1);
    } else if (ui.perf) {
        perf_set(0);
    }
    if (v == V_CHORDS && ui.view != V_CHORDS)
        ui.chords_from = (uint8_t)ui.view;
    if (ui.view != v)
        set_view(v);
}
static void link_touch(int i)
{
    ui.touched = (int8_t)i;
    ui.touch_until = plat_ms() + 1200u;
}
static void link_hold_toggle(void)
{
    ui.down_t[B_REC] = plat_ms();                /* (a tap: dub hold, not the grid) */
    button(B_REC);
}
static int link_voice(int p, int v)
{
    const en_param_t *q;
    if (p != P_CVOICE && p != P_SVOICE && p != P_LVOICE && p != P_S2VOICE && p != P_NCHAR)
        return -1;
    q = en_param_info(p);
    if (v < q->lo || v > q->hi)
        return -1;
    if (p == P_NCHAR) {
        knob_set(P_NCHAR, v);
        say("NOISE", EN_NCHAR_NAME[v]);
    } else {
        set_voice(p, v);
    }
    return 0;
}
static int link_layer_muted(int i) { return (ui.mutes & LAYER_MUTE[i]) == LAYER_MUTE[i]; }
static int link_layer_solo(int i) { return (ui.solo & LAYER_MUTE[i]) != 0; }

static void link_reload(void)                    /* the piece playing's slot changed under it */
{
    take(&proj.slot[proj.piece]);
    project_apply();
    oct_home_take();
    mac_home();
    en_regrow();
}

/* a whole project, in place of this one, checked as flash is at boot. pats: its patterns for the piece playing too
 * (a backup coming back). Without it, the patterns aren't regrown by undoing a level. in is changed (clamped) */
static int link_project_install(project_t *in, int pats)
{
    int p, i, oct = 0;
    int other;
    pars_clamp(in->par);
    for (i = 0; i < NPIECES; i++) {
        pars_clamp(in->slot[i].par);
        in->slot[i].pings_ok = in->slot[i].pings_ok ? 1 : 0;
        in->slot[i].screen = (uint8_t)(in->slot[i].screen % 3u);
    }
    if (in->leds > 2u)
        in->leds = 2u;
    in->screen = (uint8_t)(in->screen % 3u);
    if (!project_check(in))
        return -1;
    other = in->piece != proj.piece;
    if (!other) {                                /* the same piece: only what differs */
        for (p = 0; p < P_NPARAMS; p++)
            if (in->par[p] != proj.par[p]) {
                en_set(p, in->par[p]);
                oct |= p == P_SEQOCT || p == P_LOCT || p == P_SEQ2OCT;
            }
        if (in->pool != proj.pool)
            en_pool(in->pool);
    }
    proj = *in;
    if (other) {                                 /* another piece: the same as turning PRESET to it */
        mac_home();
        project_apply();
        oct_home_take();
        en_seed(plat_ms());
        en_regrow();
        sp_piece();
    } else {
        if (oct)
            oct_home_take();
        if (pats && proj.slot[proj.piece].pings_ok)
            en_seq(proj.slot[proj.piece].pings);
    }
    link_gen++;
    mark_dirty();
    return 0;
}

static int link_slot_install(int i, slot_t *in)  /* a piece's slot, from a file or a share line */
{
    if (i < 0 || i >= NPIECES)
        return -1;
    pars_clamp(in->par);
    if (!pool_ok(in->pool))
        return -1;
    keep();
    in->pings_ok = in->pings_ok ? 1 : 0;
    in->screen = (uint8_t)(in->screen % 3u);
    proj.slot[i] = *in;
    if (i == proj.piece)
        link_reload();
    link_gen++;
    mark_dirty();
    return 0;
}
static void link_slot_copy(int from, int to)
{
    if (from == to)
        return;
    keep();
    proj.slot[to] = proj.slot[from];
    if (to == proj.piece)
        link_reload();
    link_gen++;
    mark_dirty();
}
static void link_slot_swap(int a, int b)
{
    slot_t t;
    if (a == b)
        return;
    keep();
    t = proj.slot[a];
    proj.slot[a] = proj.slot[b];
    proj.slot[b] = t;
    if (a == proj.piece || b == proj.piece)
        link_reload();
    link_gen++;
    mark_dirty();
}
static void link_slot_reset(int i)               /* a piece back to its recipe */
{
    if (i == proj.piece) {
        project_piece_reset();
        project_apply();
        oct_home_take();
        mac_home();
        en_regrow();
        say("RESET", piece_name(i));
    } else {
        recipe(i, &proj.slot[i]);
    }
    link_gen++;
    mark_dirty();
}

/* ------------------------------------------------ the link --- */
static int lk_at_rest(void) { return !en_playing && !ui.btn && !ui.keys && en_pad_level <= 0.01f; }

static void lk_release(void)                     /* whatever it held down lets go */
{
    int i;
    link_v_btn = link_v_keys = 0;
    for (i = 0; i < LPERF_N; i++)
        if (lk.perf >> i & 1u)
            link_perf(i, 0);
    lk.perf = 0;
}
static void link_off(void)
{
    lk_release();
    lk.on = lk.save_wait = lk.mode = 0;
}

static void lk_hello(void)
{
    uint8_t f[LK_FRAME];
    uint32_t n = lk_head(f, LKR_HELLO, 0), i;
    const char *v = EN_VERSION;
    f[n++] = LK_VER;
    f[n++] = (uint8_t)PROJ_FORMAT;
    put14(f + n, (int)sizeof(project_t)), n += 2u;
    put14(f + n, (int)sizeof(slot_t)), n += 2u;
    put14(f + n, P_NPARAMS), n += 2u;
    f[n++] = NPIECES;
    f[n++] = (uint8_t)lk_len(v);
    for (i = 0; v[i]; i++)
        f[n++] = (uint8_t)v[i] & 0x7Fu;
    lk_send(f, n);
}

static uint32_t lk_open(int area)                /* the bytes into lk_x; how many, or 0 */
{
    if (area == LK_A_FLASH) {
        const int got = plat_store_load(OBJ_PROJ, &lk_x, sizeof lk_x);
        return got == (int)sizeof(project_t) ? (uint32_t)got : 0u;
    }
    keep();                                      /* the piece playing, into its slot, as saving does */
    if (area == LK_A_PROJ) {
        lk_x.p = proj;
        return sizeof(project_t);
    }
    if (area >= LK_A_SLOT && area < LK_A_FLASH) {
        lk_x.s = proj.slot[area - LK_A_SLOT];
        return sizeof(slot_t);
    }
    return 0u;
}

static int lk_commit(const uint8_t *a)
{
    const int area = lk.area;
    if (lk.mode != 'w')
        return LK_BAD;
    if (lk_sum(lk_x.b, lk.len) != (uint32_t)(a[0] | a[1] << 7 | a[2] << 14))
        return LK_SUM;
    if (area == LK_A_FLASH && !lk_at_rest())
        return LK_BUSY;                          /* (still open: ask again at rest) */
    lk.mode = 0;
    if (area == LK_A_PROJ)
        return link_project_install(&lk_x.p, 1) ? LK_REFUSED : LK_OK;
    if (area == LK_A_FLASH) {                    /* a backup back, exactly: not the piece as it plays since */
        lk_saved = lk_x.p;
        if (link_project_install(&lk_x.p, 1))
            return LK_REFUSED;
        undo_clear();
        if (plat_store_save(OBJ_PROJ, &lk_saved, sizeof lk_saved))
            return LK_REFUSED;
        ui.dirty = 0;
        return LK_OK;
    }
    return link_slot_install(area - LK_A_SLOT, &lk_x.s) ? LK_REFUSED : LK_OK;
}

static int lk_do(int cmd, const uint8_t *a, uint32_t n)
{
    static const uint8_t ARGS[] = {[LK_SET] = 4, [LK_VIEW] = 2, [LK_PIECE] = 1, [LK_PERF] = 2, [LK_KEYS] = 4, [LK_BUTTONS] = 2,
                                   [LK_VOICE] = 4, [LK_COPY] = 2, [LK_SWAP] = 2, [LK_RESET] = 1, [LK_OPEN] = 1, [LK_BEGIN] = 1,
                                   [LK_WRITE] = 2, [LK_COMMIT] = 3};
    int k, v;
    if (cmd >= (int)sizeof ARGS || n < ARGS[cmd])
        return LK_BAD;
    switch (cmd) {
    case LK_SET:
        k = get14u(a), v = get14s(a + 2);
        if (k >= P_NPARAMS && k != K_LEDS && !(k >= K_SLOT && k < K_SAVE))   /* (K_SAVE: a turn saves; SAVE is the way) */
            return LK_BAD;
        knob_set(k, v);
        break;
    case LK_VIEW:
        if (a[0] >= NVIEWS || (a[1] > 3u && a[1] != 127u))
            return LK_BAD;
        link_view_go(a[0]);
        if (a[1] != 127u)
            link_touch(a[1]);
        break;
    case LK_PIECE:
        if (a[0] >= NPIECES)
            return LK_BAD;
        set_piece(a[0]);
        break;
    case LK_PLAY:
        button(B_PLAY);
        break;
    case LK_HOLD:
        link_hold_toggle();
        break;
    case LK_PERF:
        if (a[0] >= LPERF_N)
            return LK_BAD;
        if (a[1])
            lk.perf |= 1u << a[0];
        else
            lk.perf &= ~(1u << a[0]);
        link_perf(a[0], a[1] != 0);
        break;
    case LK_FORGET:
        lead_forget();
        break;
    case LK_KEYS:
        link_v_keys = (uint32_t)(a[0] | a[1] << 7 | a[2] << 14 | a[3] << 21) & ((1u << NKEYS) - 1u);
        break;
    case LK_BUTTONS:
        link_v_btn = (uint32_t)get14u(a) & ((1u << NB) - 1u);
        break;
    case LK_UNDO:
    case LK_REDO:
        undo_step(cmd == LK_REDO);
        break;
    case LK_SAVE:
        lk.save_wait = 1;                        /* (link_tick: at rest, as autosave saves) */
        break;
    case LK_VOICE:
        if (link_voice(get14u(a), get14s(a + 2)))
            return LK_BAD;
        break;
    case LK_COPY:
    case LK_SWAP:
        if (a[0] >= NPIECES || a[1] >= NPIECES)
            return LK_BAD;
        if (cmd == LK_COPY)
            link_slot_copy(a[0], a[1]);
        else
            link_slot_swap(a[0], a[1]);
        break;
    case LK_RESET:
        if (a[0] >= NPIECES)
            return LK_BAD;
        link_slot_reset(a[0]);
        break;
    case LK_OPEN: {
        const uint32_t len = lk_open(a[0]), s = len ? lk_sum(lk_x.b, len) : 0u;
        if (!len)
            return LK_NONE;
        lk.mode = 'r', lk.area = a[0], lk.len = (uint16_t)len;
        put14(lk.ack, (int)len);
        lk.ack[2] = (uint8_t)(s & 0x7Fu), lk.ack[3] = (uint8_t)(s >> 7 & 0x7Fu), lk.ack[4] = (uint8_t)(s >> 14 & 0x7Fu);
        lk.ack_n = 5;
        break;
    }
    case LK_BEGIN:
        if (a[0] > LK_A_FLASH)
            return LK_BAD;
        lk.mode = 'w', lk.area = a[0];
        lk.len = (uint16_t)(a[0] >= LK_A_SLOT && a[0] < LK_A_FLASH ? sizeof(slot_t) : sizeof(project_t));
        memset(lk_x.b, 0, sizeof lk_x.b);
        break;
    case LK_WRITE: {
        const uint32_t off = (uint32_t)get14u(a);
        if (lk.mode != 'w' || off >= lk.len)
            return LK_BAD;
        if (!lk_unpack(lk_x.b + off, a + 2, n - 2u, lk.len - off))
            return LK_BAD;
        break;
    }
    case LK_COMMIT:
        return lk_commit(a);
    default:
        return LK_BAD;
    }
    return LK_OK;
}

static int link_frame(const uint8_t *p, uint32_t n, uint32_t now)   /* the bytes between F0 and F7; 1: it was ours */
{
    int cmd;
    if (n < 4u || p[0] != LK_H0 || p[1] != LK_H1 || p[2] != LK_H2)
        return 0;
    cmd = p[3];
    if (cmd == LK_HELLO) {
        int i;
        if (!lk.on) {
            lk.on = 1;
            lk.tick_t = now - LK_TICK_MS;
            say("EDITOR", "CONNECTED");
            card_show("EDITOR", "connected", K_ACC, 0);
        }
        lk_release();                            /* (the other end started again: it holds nothing now) */
        lk.seen_seq = 0, lk.mode = 0, lk.scan = 0;
        for (i = 0; i < LK_NVALS; i++)           /* everything, afresh */
            lk.val[i] = LK_UNSENT;
        lk.heard = now;
        lk_hello();
        return 1;
    }
    if (!lk.on)                                  /* not listening: nothing is sent, nothing changes */
        return 1;
    lk.heard = now;
    if (cmd == LK_BEAT)
        return 1;
    if (cmd == LK_BYE) {
        link_off();
        return 1;
    }
    if (n < 5u)
        return 1;
#if AISLING_SMK_DIAG                             /* the SMK-37's stage 3: the matrix and the pins as they read */
    if (cmd == LK_DIAG) {
        uint8_t f[LK_FRAME], raw[SMK_NCOL];
        uint32_t reg[SMK_DIAG_NREG], i, k;
        smk_diag(raw, reg);
        n = lk_head(f, LKR_DIAG, p[4]);
        for (i = 0; i < SMK_NCOL; i++)
            put14(f + n, raw[i]), n += 2u;
        for (i = 0; i < SMK_DIAG_NREG; i++)
            for (k = 0; k < 5u; k++)
                f[n++] = (uint8_t)(reg[i] >> (7u * k) & 0x7Fu);
        lk_send(f, n);
        return 1;
    }
    if (cmd == LK_DIAG_AN) {
        uint8_t f[LK_FRAME];
        uint32_t i;
        n = lk_head(f, LKR_DIAG_AN, p[4]);
        for (i = 0; i < 24u; i++)                /* (stage 4) the analog scan: mux A, B, C by address, as read */
            put14(f + n, smk_an.raw[i >> 3][i & 7u]), n += 2u;
        put14(f + n, smk_an.pads & 0xFFu), n += 2u;
        put14(f + n, smk_an.pads >> 8), n += 2u;
        put14(f + n, (int)(smk_an.scans & 0x3FFFu)), n += 2u;
        put14(f + n, (int)(smk_an.misses & 0x3FFFu)), n += 2u;
        lk_send(f, n);
        return 1;
    }
#endif
    if (cmd == LK_READ) {                        /* reads only: each time it's asked */
        uint8_t f[LK_FRAME];
        uint32_t m, off;
        if (n < 7u || lk.mode != 'r' || (off = (uint32_t)get14u(p + 5)) >= lk.len) {
            lk.seq = p[4], lk.seen_seq = 1, lk.status = LK_BAD, lk.ack_n = 0;
            lk_ack();
            return 1;
        }
        m = lk.len - off;
        if (m > LK_CHUNK)
            m = LK_CHUNK;
        n = lk_head(f, LKR_CHUNK, p[4]);
        put14(f + n, (int)off), n += 2u;
        n += lk_pack(f + n, lk_x.b + off, m);
        lk_send(f, n);                           /* (no room: the other end asks again) */
        return 1;
    }
    if (lk.seen_seq && lk.seq == p[4]) {         /* its last again: only the answer */
        lk_ack();
        return 1;
    }
    lk.seq = p[4], lk.seen_seq = 1, lk.ack_n = 0;
    lk.status = (uint8_t)lk_do(cmd, p + 5, n - 5u);
    ui.act_t = now;                              /* (autosave waits for quiet, as after a knob) */
    lk_ack();
    return 1;
}

static void link_state(void)
{
    uint8_t f[LK_FRAME];
    uint32_t n = lk_head(f, LKR_STATE, 0);
    uint32_t i, muted = 0, solo = 0, sounds = 0;
    for (i = 0; i < NLAYER; i++) {
        float a = en_layer_act[i];
        muted |= (uint32_t)link_layer_muted((int)i) << i;
        solo |= (uint32_t)link_layer_solo((int)i) << i;
        sounds |= (uint32_t)(perf_sounds((int)i) != 0) << i;
        f[n + 5u + i] = (uint8_t)(a <= 0.0f ? 0 : a >= 1.0f ? 127 : (int)(a * 127.0f));
    }
    f[n++] = ui.view;
    f[n++] = (uint8_t)((en_playing ? LKF_PLAYING : 0) | (en_frozen ? LKF_FROZEN : 0) | (ui.dirty ? LKF_DIRTY : 0) |
                       (ui.perf ? LKF_PERF : 0) | (lk.save_wait ? LKF_SAVING : 0));
    f[n++] = proj.piece;
    put14(f + n, (int)(ui.moves & 0x3FFFu)), n += 2u;
    n += NLAYER;                                 /* (the five levels, above) */
    f[n++] = (uint8_t)muted, f[n++] = (uint8_t)solo, f[n++] = (uint8_t)sounds;
    f[n++] = ud.un, f[n++] = ud.rn;
    f[n++] = (uint8_t)(plat_cpu_pct() > 127u ? 127u : plat_cpu_pct());
    f[n++] = (uint8_t)(plat_cpu_peak() > 127u ? 127u : plat_cpu_peak());
    put14(f + n, (int)(plat_xruns() > 16383u ? 16383u : plat_xruns())), n += 2u;
    put14(f + n, (int)plat_master()), n += 2u;
    put14(f + n, (int)plat_master_raw()), n += 2u;
    put14(f + n, (int)(link_lit_b & 0x3FFFu)), n += 2u;
    put14(f + n, (int)(link_lit_k & 0x3FFFu)), n += 2u;
    put14(f + n, (int)(link_lit_k >> 14 & 0x3FFFu)), n += 2u;
    f[n++] = link_gen & 0x7Fu;
    put14(f + n, (int)(ui.pnow & 0x3FFFu)), n += 2u;   /* (1.3) the effects sounding, a bit a key (PKEY's) */
    put14(f + n, (int)(ui.pnow >> 14 & 0x3FFFu)), n += 2u;
    lk_send(f, n);
}

static void link_vals(void)                      /* the knobs that moved, and their words */
{
    int frames;
    for (frames = 0; frames < 3; frames++) {
        uint8_t f[LK_FRAME];
        uint16_t at[LK_VALS_N];
        int16_t vs[LK_VALS_N];
        uint32_t n = lk_head(f, LKR_VALS, 0), got = 0, i;
        uint16_t scan = lk.scan, wrap = 0;
        while (wrap < LK_NVALS && got < LK_VALS_N) {
            const int k = scan < P_NPARAMS ? scan : LK_EXTRA[scan - P_NPARAMS], v = knob_get(k);
            if ((int16_t)v != lk.val[scan]) {
                char t[24];
                uint32_t len;
                knob_text(k, t);
                len = lk_len(t);
                if (n + 5u + len + 1u > LK_VALS_MAX)
                    break;
                put14(f + n, k), n += 2u;
                put14(f + n, v), n += 2u;
                f[n++] = (uint8_t)len;
                for (i = 0; i < len; i++)
                    f[n++] = (uint8_t)t[i] & 0x7Fu;
                at[got] = scan, vs[got] = (int16_t)v, got++;
            }
            scan = (uint16_t)((scan + 1u) % (uint32_t)LK_NVALS);
            wrap++;
        }
        if (!got)
            return;
        if (lk_send(f, n))                       /* no room: these again, next time */
            return;
        for (i = 0; i < got; i++)
            lk.val[at[i]] = vs[i];
        lk.scan = scan;
    }
}

static void link_tick(uint32_t now)
{
    if (!lk.on)
        return;
    if (now - lk.heard > LK_GONE_MS) {
        link_off();
        say("EDITOR", "GONE");
        return;
    }
    if (lk.save_wait && lk_at_rest()) {
        lk.save_wait = 0;
        if (ui.dirty)
            do_save();
    }
    if (now - lk.tick_t >= LK_TICK_MS) {
        lk.tick_t = now;
        link_state();
        link_vals();
    }
}
