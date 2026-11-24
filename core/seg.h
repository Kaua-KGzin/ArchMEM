/* Contiguous allocation (segments) with first-, best- and worst-fit.
 *
 * Ops: "alloc NAME SIZE" or "free NAME", separated by ';' or newlines.
 * Names are [A-Za-z0-9_]{1,16} and unique while allocated. Freed space is
 * merged with neighbouring holes. A failed alloc is "seg_fail_fragmentation"
 * when the total free space would fit it, else "seg_fail_no_space". */
#ifndef ARCHMEM_SEG_H
#define ARCHMEM_SEG_H

#include <stddef.h>

#include "buf.h"

#define SEG_MAX_MEMORY 1048576
#define SEG_MAX_OPS 1000

typedef enum { FIT_FIRST, FIT_BEST, FIT_WORST } Fit;

int fit_parse(const char *name, Fit *out); /* "first" | "best" | "worst" */
const char *fit_name(Fit f);

/* Appends "steps":[...],"totals":{...} (no enclosing braces). 0 = ok. */
int seg_run(int memory_size, Fit fit, const char *ops, Buf *json, char *err, size_t errlen);

#endif
