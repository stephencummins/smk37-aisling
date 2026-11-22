/* SPDX-License-Identifier: GPL-3.0-only */
/* The engine's parameter table, for tools/make_pack.py: a line a parameter,
 *
 *   INDEX LO HI DEF "Name" ["text of LO" "text of LO+1" ...]
 *
 * the texts only where the values are a list of names (a voice, a scale, a character). First line: level N.
 *   cc -DEN_HOST -Ifirmware/src/dsp -o build/host/en_params host/en_params.c firmware/src/dsp/amb.c -lm */
#define EN_HOST 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "amb.h"

void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2) { (void)st, (void)d1, (void)d2; }

int main(int argc, char **argv)
{
    int i, v;
    if (argc > 1 && !strcmp(argv[1], "patch")) { /* the four layers' plain Patch, a line each (tools/make_pack.py) */
        for (i = 0; i < 4; i++) {
            uint8_t pb[EN_PATCH_N];
            en_patch(i, 0);
            en_patch_get(i, pb);
            for (v = 0; v < EN_PATCH_N; v++)
                printf("%d%c", v == 3 || v == 4 || v == 10 || v == 15 ? (int8_t)pb[v] : pb[v], v == EN_PATCH_N - 1 ? '\n' : ' ');
        }
        return 0;
    }
    printf("level %d airs %d\n", EN_LEVEL, EN_NAIRS);
    for (i = 0; i < P_NPARAMS; i++) {
        const en_param_t *p = en_param_info(i);
        char b[32];
        int names = p->hi - p->lo <= 16 && p->hi > p->lo;
        printf("%d %d %d %d \"%s\"", i, p->lo, p->hi, p->def, p->name);
        if (names) {                             /* (names: not every text a number; "2600" is a voice) */
            int nums = 0;
            for (v = p->lo; v <= p->hi; v++) {
                char *e;
                en_param_text(i, v, b);
                strtol(b, &e, 10);
                nums += !b[0] || !*e;
            }
            names = nums < 2;
        }
        for (v = p->lo; names && v <= p->hi; v++) {
            en_param_text(i, v, b);
            printf(" \"%s\"", b);
        }
        printf("\n");
    }
    return 0;
}
