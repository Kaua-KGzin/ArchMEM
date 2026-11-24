/* Growable string buffer with JSON writing helpers. */
#ifndef ARCHMEM_BUF_H
#define ARCHMEM_BUF_H

#include <stddef.h>

typedef struct {
    char *data;
    size_t len;
    size_t cap;
    int oom; /* set when an allocation failed; further writes are ignored */
} Buf;

void buf_init(Buf *b);
void buf_free(Buf *b);
void buf_putc(Buf *b, char c);
void buf_puts(Buf *b, const char *s);
void buf_append(Buf *b, const char *s, size_t n);
#if defined(__GNUC__)
__attribute__((format(printf, 2, 3)))
#endif
void buf_printf(Buf *b, const char *fmt, ...);
/* Writes s as a JSON string literal (with quotes). NULL writes null. */
void buf_json_str(Buf *b, const char *s);
/* Writes a JSON number: integers without decimals, otherwise up to 4
 * decimals with trailing zeros removed. */
void buf_json_num(Buf *b, double v);
/* Returns the NUL-terminated content and leaves b empty (caller frees).
 * Never returns NULL unless memory ran out. */
char *buf_take(Buf *b);

#endif
