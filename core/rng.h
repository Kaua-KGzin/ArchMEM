/* Deterministic PRNG (xorshift32) and reference-string generators. The same
 * seed gives the same sequence on every platform, native or WebAssembly. */
#ifndef ARCHMEM_RNG_H
#define ARCHMEM_RNG_H

#include <stddef.h>
#include <stdint.h>

#include "refs.h"

typedef struct {
    uint32_t s;
} Rng;

void rng_seed(Rng *r, uint32_t seed); /* seed 0 is replaced by 0x9E3779B9 */
uint32_t rng_next(Rng *r);
int rng_range(Rng *r, int lo, int hi_inclusive);

typedef enum { GEN_UNIFORM, GEN_LOCALITY, GEN_LOOP, GEN_SEQUENTIAL } GenKind;

int gen_parse(const char *name, GenKind *out); /* 0 = ok */

/* uniform:    any page with equal chance
 * locality:   a working set of max(2, pages/4) pages that slides by one page
 *             every length/5 accesses; 90% of accesses fall inside it
 * loop:       0, 1, ..., pages-1, 0, 1, ...
 * sequential: walks the pages in order, touching each one length/pages times
 *             in a row (like scanning an array)
 * write_ratio (0..1) is the chance of each access being a write. */
int gen_refs(GenKind kind, int length, int pages, uint32_t seed, double write_ratio, Ref **out, size_t *n, char *err,
             size_t errlen);

#endif
