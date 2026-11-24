/* Self-check exercises generated from a seed.
 *   difficulty 1  kind "faults":         FIFO or LRU, 3 frames, 10-12 refs over 5 pages
 *   difficulty 2  kind "victim":         any algorithm, 3-4 frames, 15 refs; which page leaves at step k
 *   difficulty 3  kind "best_algorithm": which of FIFO/LRU/Clock/LFU has the fewest faults (unique);
 *                                        falls back to "faults" if no unique winner in 50 draws
 * The answer is computed by the engine, so it is always consistent with the
 * step-by-step view. */
#ifndef ARCHMEM_CHALLENGE_H
#define ARCHMEM_CHALLENGE_H

#include <stddef.h>
#include <stdint.h>

#include "buf.h"

/* Appends the challenge JSON object. 0 = ok. */
int challenge_make(uint32_t seed, int difficulty, Buf *json, char *err, size_t errlen);

#endif
