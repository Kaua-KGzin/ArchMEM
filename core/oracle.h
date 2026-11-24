/* Deliberately naive second implementation of the replacement algorithms,
 * written straight from the conventions in paging.h and sharing no code with
 * paging.c. Used only by tests, as an oracle where textbooks give no answer
 * key (Clock, LFU). Not linked into the CLI or the WebAssembly build. */
#ifndef ARCHMEM_ORACLE_H
#define ARCHMEM_ORACLE_H

#include <stddef.h>

#include "paging.h"

int oracle_faults(Algorithm alg, int frames, const int *pages, size_t n);

#endif
