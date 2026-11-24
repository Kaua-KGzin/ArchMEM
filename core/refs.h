/* Reference strings: "7 0 1 2w @130 @0x82w".
 *   N    read of page N        Nw   write to page N
 *   @A   read of address A     @Aw  write to address A (decimal or 0x hex)
 * Separators: spaces, tabs, commas, newlines. */
#ifndef ARCHMEM_REFS_H
#define ARCHMEM_REFS_H

#include <stddef.h>

#include "buf.h"

#define REFS_MAX 10000
#define REFS_MAX_VALUE 100000000L

typedef struct {
    char op;        /* 'R' or 'W' */
    int is_address; /* value is an address (1) or a page number (0) */
    long value;
} Ref;

/* Returns 0 and a malloc'd array in *out (caller frees). On error returns
 * non-zero, leaves *out NULL and writes a message naming the token and its
 * 1-based position. */
int refs_parse(const char *s, Ref **out, size_t *count, char *err, size_t errlen);
void refs_format(const Ref *refs, size_t count, Buf *b);

#endif
