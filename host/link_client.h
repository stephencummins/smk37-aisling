/* SPDX-License-Identifier: GPL-3.0-only */
/* The other end of the USB link (firmware/src/app/link.c), for scenarios: what the editor page and tools/fm1_link.py do.
 * Replies are taken as link_wire_send gets them. Host only. */
#define LC_ROOM 4096u

static struct {
    uint32_t bytes, frames, hellos, states, acks, chunks, vals_n;
    uint8_t seq, ack_seq, ack_status, ack_x[6], ack_n;
    uint8_t hello[64], hello_n;
    uint8_t state[96], state_n;
    int val[256];
    uint8_t seen[256];
    char text[256][24];
    uint8_t rd[LC_ROOM], wr[LC_ROOM];
    uint8_t rd_got[LC_ROOM];
    uint32_t rd_len, rd_sum, wr_len;
    uint8_t last[640];
    uint32_t last_n;
    int beat;
} lc;

static void lc_take(const uint8_t *p, uint32_t n)
{
    uint32_t i;
    const uint8_t *a = p + 6;
    uint32_t na;
    lc.bytes += n;
    lc.frames++;
    for (i = 1; i + 1u < n; i++)                 /* (nothing above 0x7F between F0 and F7, or MIDI ends it) */
        if (p[i] & 0x80u) {
            printf("FAIL link frame cmd %u: byte %u is 0x%02x\n", p[4], i, p[i]);
            lc.frames = 1000000u;
        }
    if (n < 7u || p[0] != 0xF0u || p[n - 1u] != 0xF7u || p[1] != 0x7Du || p[2] != 0x41u || p[3] != 0x4Cu) {
        printf("FAIL link frame: bad shape, %u bytes\n", n);
        lc.frames = 1000000u;
        return;
    }
    na = n - 7u;
    switch (p[4]) {
    case 65:                                     /* HELLO */
        lc.hellos++;
        lc.hello_n = (uint8_t)(na < sizeof lc.hello ? na : sizeof lc.hello);
        memcpy(lc.hello, a, lc.hello_n);
        break;
    case 66:                                     /* ACK */
        lc.acks++;
        lc.ack_seq = p[5];
        lc.ack_status = a[0];
        lc.ack_n = (uint8_t)(na - 1u < sizeof lc.ack_x ? na - 1u : sizeof lc.ack_x);
        memcpy(lc.ack_x, a + 1, lc.ack_n);
        break;
    case 67:                                     /* STATE */
        lc.states++;
        lc.state_n = (uint8_t)(na < sizeof lc.state ? na : sizeof lc.state);
        memcpy(lc.state, a, lc.state_n);
        break;
    case 68: {                                   /* VALS */
        uint32_t at = 0;
        while (at + 5u <= na) {
            const int k = a[at] | a[at + 1] << 7, u = a[at + 2] | a[at + 3] << 7, len = a[at + 4];
            at += 5u;
            if (k < 256 && len < 24) {
                if (!lc.seen[k])
                    lc.seen[k] = 1, lc.vals_n++;
                lc.val[k] = u >= 8192 ? u - 16384 : u;
                memcpy(lc.text[k], a + at, (size_t)len);
                lc.text[k][len] = 0;
            }
            at += (uint32_t)len;
        }
        break;
    }
    case 69: {                                   /* CHUNK: off.2, then each seven's top bits and them */
        uint32_t off = (uint32_t)(a[0] | a[1] << 7), at = 2, w = off, j;
        lc.chunks++;
        while (at < na) {
            const uint8_t hi = a[at++];
            for (j = 0; j < 7u && at < na; j++, at++) {
                if (w < LC_ROOM) {
                    lc.rd[w] = (uint8_t)(a[at] | (hi >> j & 1u) << 7);
                    lc.rd_got[w] = 1;
                    w++;
                }
            }
        }
        break;
    }
    default:
        printf("FAIL link frame: unknown reply %u\n", p[4]);
        lc.frames = 1000000u;
    }
}

static int link_wire_send(const uint8_t *p, uint32_t n)
{
#ifdef EN_WEB
    web_said(p, n);
#else
    lc_take(p, n);
#endif
    return 0;
}
