#include "rng.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "paging.h"

void rng_seed(Rng *r, uint32_t seed) { r->s = seed ? seed : 0x9E3779B9u; }

uint32_t rng_next(Rng *r) {
    uint32_t x = r->s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    r->s = x;
    return x;
}

int rng_range(Rng *r, int lo, int hi) {
    if (hi <= lo) return lo;
    return lo + (int)(rng_next(r) % (uint32_t)(hi - lo + 1));
}

int gen_parse(const char *name, GenKind *out) {
    static const char *names[] = {"uniform", "locality", "loop", "sequential"};
    if (!name) return 1;
    for (int i = 0; i < 4; i++)
        if (strcmp(name, names[i]) == 0) {
            *out = (GenKind)i;
            return 0;
        }
    return 1;
}

int gen_refs(GenKind kind, int length, int pages, uint32_t seed, double write_ratio, Ref **out, size_t *n, char *err,
             size_t errlen) {
    *out = NULL;
    *n = 0;
    if (length < 1 || length > REFS_MAX) return snprintf(err, errlen, "length deve estar entre 1 e %d", REFS_MAX), 1;
    if (pages < 1 || pages > PAGING_MAX_PAGES)
        return snprintf(err, errlen, "pages deve estar entre 1 e %d", PAGING_MAX_PAGES), 1;
    if (!(write_ratio >= 0.0 && write_ratio <= 1.0)) return snprintf(err, errlen, "write_ratio deve estar entre 0 e 1"), 1;
    Ref *refs = malloc((size_t)length * sizeof *refs);
    if (!refs) return snprintf(err, errlen, "sem memoria"), 1;

    Rng r;
    rng_seed(&r, seed);
    int ws = pages / 4 < 2 ? 2 : pages / 4;
    if (ws > pages) ws = pages;
    int shift_every = length / 5 < 1 ? 1 : length / 5;
    int run = length / pages < 1 ? 1 : length / pages;
    uint32_t write_threshold = (uint32_t)(write_ratio * 10000.0 + 0.5);

    for (int i = 0; i < length; i++) {
        int page;
        switch (kind) {
        case GEN_LOCALITY: {
            int start = (i / shift_every) % pages;
            page = rng_range(&r, 0, 9) < 9 ? (start + rng_range(&r, 0, ws - 1)) % pages : rng_range(&r, 0, pages - 1);
            break;
        }
        case GEN_LOOP: page = i % pages; break;
        case GEN_SEQUENTIAL: page = (i / run) % pages; break;
        default: page = rng_range(&r, 0, pages - 1);
        }
        int write = (rng_next(&r) % 10000u) < write_threshold;
        refs[i] = (Ref){write ? 'W' : 'R', 0, page};
    }
    *out = refs;
    *n = (size_t)length;
    return 0;
}
