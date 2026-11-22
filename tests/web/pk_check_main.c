/* SPDX-License-Identifier: GPL-3.0-only */
/* The firmware's own checker on a file, for tests/web/packfile_test.mjs: prints pk_check's answer. With a second
 * argument: which of the eight it takes for empty places (pk_empty), a bit each. */
#include <stdio.h>
#include "pack.h"
int main(int argc, char **argv)
{
    static unsigned char b[PK_MAX + 16];
    FILE *f = argc > 1 ? fopen(argv[1], "rb") : 0;
    int n = f ? (int)fread(b, 1, sizeof b, f) : 0;
    if (argc > 2) {
        int i, m = 0;
        pk_piece_t p;
        for (i = 0; i < PK_NPIECES; i++)
            m |= (!pk_piece(b, n, i, &p) && pk_empty(&p)) << i;
        printf("%d\n", m);
        return 0;
    }
    printf("%d\n", pk_check(b, n));
    return 0;
}
