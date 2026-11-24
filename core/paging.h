/* Demand paging simulation with a TLB and five replacement algorithms.
 *
 * Conventions (Silberschatz), shown in the UI because they change answers:
 *   - free frames are used in ascending order before any replacement;
 *   - FIFO evicts the page loaded earliest, LRU the one used least recently;
 *   - Clock: a loaded page gets ref=1; the hand starts at frame 0; looking for
 *     a victim, ref=1 becomes 0 and the hand advances; after a replacement the
 *     hand stops at the frame after the replaced one; a hit only sets ref=1;
 *   - LFU counts uses while resident (reset on eviction, new page = 1);
 *     ties go to the page loaded earliest;
 *   - OPT evicts the page whose next use is farthest (or never); ties go to
 *     the lowest page number.
 * The TLB is fully associative with LRU replacement; an evicted page's entry
 * is invalidated. */
#ifndef ARCHMEM_PAGING_H
#define ARCHMEM_PAGING_H

#include <stddef.h>

#include "buf.h"
#include "refs.h"

typedef enum { ALG_FIFO, ALG_LRU, ALG_CLOCK, ALG_LFU, ALG_OPT, ALG_COUNT } Algorithm;

#define PAGING_MAX_FRAMES 64
#define PAGING_MAX_PAGES 4096
#define PAGING_MAX_TLB 64

int alg_parse(const char *name, Algorithm *out); /* case-insensitive; 0 = ok */
const char *alg_name(Algorithm a);

typedef struct {
    int page_size;     /* power of two, 16..65536 */
    int frames;        /* 1..64 */
    int virtual_pages; /* 1..4096 */
    int tlb_entries;   /* 0 (no TLB)..64 */
    Algorithm algorithm;
} PagingConfig;

typedef struct {
    int accesses, faults, hits, replacements, writebacks, tlb_hits, tlb_misses;
} Totals;

int paging_config_check(const PagingConfig *cfg, char *err, size_t errlen);

/* Simulates refs. When steps_json is not NULL, appends the JSON array of
 * per-step events (see the JSON contract). Returns 0 on success. */
int paging_run(const PagingConfig *cfg, const Ref *refs, size_t n, Buf *steps_json, Totals *out, char *err,
               size_t errlen);

#endif
