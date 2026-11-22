/* SPDX-License-Identifier: GPL-3.0-only */
/* BERLIN project: one flash object (OBJ_PROJ); (packs) and the installed pack's bank's, OBJ_PROJ_B. Only BERLIN's own magic and format are read; anything else in the
 * flash (DUB-1's, AMB-1's, an older format, none) gives the recipes. Part of the unity build, after app.h.
 *
 * The pieces (their recipes are the engine's, en_piece): each keeps its own settings as you leave it;
 * Tune and MIDI out are global. */
#include "pack.h"
project_t proj;
uint8_t bank;
static project_t proj_off;                       /* (packs) the other bank's project, as it was left */
static uint8_t off_dirty;                        /* (it has changed since it was saved) */
static uint8_t pack_b[PK_MAX];                   /* the pack installed (pack_n 0: none) */
static uint16_t pack_n;

int pack_have(void) { return pack_n != 0; }
const char *pack_title(void) { return pack_n ? pk_name(pack_b) : ""; }
const uint8_t *pack_bytes(int *len)
{
    *len = pack_n;
    return pack_b;
}
uint32_t proj_obj(void) { return bank ? OBJ_PROJ_B : OBJ_PROJ; }
static int pack_piece(int b, int i, pk_piece_t *p) { return b && pack_n && !pk_piece(pack_b, pack_n, i, p); }

const char *piece_name(int i)
{
    pk_piece_t p;
    return pack_piece(bank, i, &p) ? (const char *)p.name : en_piece_name(i);
}
int piece_light(int i)
{
    pk_piece_t p;                                /* (1.6) AISLING's own eight: light */
    return !pack_piece(bank, i, &p) || (p.flags & PK_LIGHT);
}
int piece_empty(int b, int i)                    /* (1.6) piece i of bank b is a place the pack left empty */
{
    pk_piece_t p;
    return pack_piece(b, i, &p) && pk_empty(&p);
}
int screen_light(void) { return proj.light == 2 ? piece_light(proj.piece) : proj.light != 0; }

static int global_par(int p) { return p == P_TUNE || p == P_MIDI; }

static void recipe_of(int b, int i, slot_t *s)
{
    pk_piece_t p;
    if (pack_piece(b, i, &p))
        pk_recipe(&p, s->par, &s->pool);
    else
        en_piece(i, s->par, &s->pool);
    s->pings_ok = 0;
    s->screen = 0;
}
static void recipe(int i, slot_t *s) { recipe_of(bank, i, s); }
void piece_recipe(int i, int16_t *par, uint16_t *pool)
{
    slot_t s;
    recipe(i, &s);
    memcpy(par, s.par, sizeof s.par);
    *pool = s.pool;
}

_Static_assert(sizeof(project_t) <= 3840u, "the project must fit one storage sector's payload");

static void take(const slot_t *s)                /* a piece's settings into play, the globals kept */
{
    int p;
    for (p = 0; p < P_NPARAMS; p++)
        if (!global_par(p))
            proj.par[p] = s->par[p];
    proj.pool = s->pool;
    proj.screen = (uint8_t)(s->screen % 3u);      /* (each piece its own view) */
}

static uint8_t bank_stale;                       /* (packs) the bank just changed: the engine still has the other's
                                                  * piece, so nothing is kept from it till this one's is applied */
static void keep(void)                           /* the piece playing, into its slot (with its ping loops) */
{
    int p;
    if (bank_stale)
        return;
    for (p = 0; p < P_NPARAMS; p++)
        proj.slot[proj.piece].par[p] = proj.par[p];
    proj.slot[proj.piece].pool = proj.pool;
    proj.slot[proj.piece].screen = proj.screen;
    en_seq_get(proj.slot[proj.piece].pings);
    proj.slot[proj.piece].pings_ok = 1;
}

static void defaults_of(int b, project_t *x)     /* bank b's recipes, its first piece playing */
{
    int i;
    memset(x, 0, sizeof *x);
    x->magic = PROJ_MAGIC;
    x->format = PROJ_FORMAT;
    for (i = 0; i < NPIECES; i++)
        recipe_of(b, i, &x->slot[i]);
    for (i = 0; i < P_NPARAMS; i++)
        x->par[i] = global_par(i) ? en_param_info(i)->def : x->slot[0].par[i];
    x->pool = x->slot[0].pool;
    x->leds = 1;
    x->light = 2;                                /* (1.6) the screen as the piece is, and the eight are light */
}
void project_defaults(void) { defaults_of(bank, &proj); }

static int pool_ok(uint32_t p)                   /* the root and at least two more notes */
{
    int n = 0, i;
    for (i = 0; i < 12; i++)
        n += (int)(p >> i & 1u);
    return (p & 1u) && !(p >> 12) && n >= 3;
}

static int pars_ok(const int16_t *par)
{
    int i;
    for (i = 0; i < P_NPARAMS; i++) {
        const en_param_t *p = en_param_info(i);
        if (par[i] < p->lo || par[i] > p->hi)
            return 0;
    }
    return 1;
}

static void pars_clamp(int16_t *par)            /* a value out of its (now narrower) range: to its end, not the whole
                                                  * project thrown away (1.3.0's one-sided knobs) */
{
    int i;
    for (i = 0; i < P_NPARAMS; i++) {
        const en_param_t *p = en_param_info(i);
        if (par[i] < p->lo)
            par[i] = p->lo;
        if (par[i] > p->hi)
            par[i] = p->hi;
    }
}
static int project_check(const project_t *x)    /* (not: the recipes again) */
{
    int i;
    if (x->magic != PROJ_MAGIC || !pool_ok(x->pool) || !pars_ok(x->par))
        return 0;
    if (x->format != PROJ_FORMAT || x->piece >= NPIECES)
        return 0;
    for (i = 0; i < NPIECES; i++)
        if (!pool_ok(x->slot[i].pool) || !pars_ok(x->slot[i].par))
            return 0;
    return 1;
}
static int load_of(uint32_t obj, int b, project_t *x)   /* bank b's project from flash: 0, or its recipes: -1 */
{
    static project_t buf;
    int n, i;
    memset(&buf, 0, sizeof buf);
    n = plat_store_load(obj, &buf, sizeof buf);
    if (n == (int)sizeof buf && buf.magic == PROJ_MAGIC && buf.format == PROJ_FORMAT) {
        *x = buf;
        pars_clamp(x->par);                      /* (the ranges as they are now) */
        for (i = 0; i < NPIECES; i++)
            pars_clamp(x->slot[i].par);
        x->light = (uint8_t)(x->light > 2u ? 1u : x->light);   /* (1.4.1; packs: 2, as the piece is) */
        x->bank = (uint8_t)(x->bank != 0);
        if (x->evbars > 4u)                      /* (1.2) */
            x->evbars = 0;
        if (project_check(x))
            return 0;
    }
    defaults_of(b, x);
    return -1;
}

static void globals_to(project_t *to, const project_t *from)   /* what is the player's, not a piece's nor a bank's */
{
    to->leds = from->leds, to->evbars = from->evbars, to->light = from->light, to->bank = from->bank;
    to->par[P_TUNE] = from->par[P_TUNE], to->par[P_MIDI] = from->par[P_MIDI];
}
static void bank_swap(void)                      /* (a byte at a time: no third project's room) */
{
    uint8_t *a = (uint8_t *)&proj, *b = (uint8_t *)&proj_off;
    uint32_t i;
    for (i = 0; i < sizeof proj; i++) {
        const uint8_t t = a[i];
        a[i] = b[i], b[i] = t;
    }
}

int project_load(void)                           /* (packs) the pack, the eight's project, the pack's bank's */
{
    const int n = plat_store_load(OBJ_PACK, pack_b, sizeof pack_b);
    int r;
    pack_n = (uint16_t)(n > 0 && !pk_check(pack_b, n) ? n : 0);
    bank = 0, off_dirty = 0;
    r = load_of(OBJ_PROJ, 0, &proj);
    if (!pack_n) {
        proj.bank = 0;
        return r;
    }
    load_of(OBJ_PROJ_B, 1, &proj_off);
    globals_to(&proj_off, &proj);
    if (proj.bank) {
        bank_swap();
        bank = 1;
    }
    return r;
}

int project_save(void)
{
    int r;
    keep();
    r = plat_store_save(proj_obj(), &proj, sizeof proj);
    if (off_dirty && (pack_n || bank)) {
        globals_to(&proj_off, &proj);
        if (plat_store_save(bank ? OBJ_PROJ : OBJ_PROJ_B, &proj_off, sizeof proj_off) == 0)
            off_dirty = 0;
        else
            r = -1;
    }
    return r;
}

int project_bank(int b)
{
    b = b && pack_n;
    if (b == bank)
        return 0;
    keep();
    proj.bank = (uint8_t)b;
    globals_to(&proj_off, &proj);
    bank_swap();
    bank = (uint8_t)b;
    off_dirty = 1;
    bank_stale = 1;
    return 1;
}

int pack_install(const uint8_t *b, int len)
{
    static const uint8_t none[4] = {0, 0, 0, 0};
    const int why = len ? pk_check(b, len) : 0;
    if (why)
        return why;
    project_bank(0);                             /* (the eight play: the pack's bank is about to change under it) */
    if (len)
        memmove(pack_b, b, (unsigned)len);
    pack_n = (uint16_t)len;
    if (plat_store_save(OBJ_PACK, len ? pack_b : none, len ? (uint32_t)len : sizeof none))
        return 4;
    if (len) {
        defaults_of(1, &proj_off);
        if (!proj.light)                         /* (dark: now as the pieces are, and the eight are light) */
            proj.light = 2;
        globals_to(&proj_off, &proj);
        off_dirty = 1;
    } else
        off_dirty = 0;
    return 0;
}

void project_piece(int i)
{
    keep();
    proj.piece = (uint8_t)((i % NPIECES + NPIECES) % NPIECES);
    take(&proj.slot[proj.piece]);
}

void project_piece_reset(void)
{
    slot_t s;
    recipe(proj.piece, &s);
    s.screen = proj.screen;                      /* (the view stays as it is) */
    take(&s);
    proj.slot[proj.piece].pings_ok = 0;          /* (its loops grown afresh too) */
}

void pack_patch_own(int l)                       /* (1.6) layer l's Patch as the piece playing has it */
{
    pk_piece_t pk;
    const uint8_t *q = 0;
    int i;
    if (pack_piece(bank, proj.piece, &pk) && (pk.pmask >> l & 1))
        for (q = pk.patches, i = 0; i < l; i++)
            if (pk.pmask >> i & 1)
                q += EN_PATCH_N;
    en_patch(l, q);
}

void project_apply(void)                         /* (then en_regrow keeps the piece's loops, if it has them) */
{
    int i;
    const slot_t *s = &proj.slot[proj.piece];
    pk_piece_t pk;
    bank_stale = 0;
    if (pack_piece(bank, proj.piece, &pk)) {     /* (packs) its air, before the parameters ask for it; its patches */
        pk_air(&pk);
        pk_patch(&pk);
    } else
        pk_patch(0);
    for (i = 0; i < P_NPARAMS; i++)
        en_set(i, proj.par[i]);
    en_pool(proj.pool);
    en_seq(s->pings_ok ? s->pings : 0);
}
