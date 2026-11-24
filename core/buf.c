#include "buf.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void buf_init(Buf *b) {
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
    b->oom = 0;
}

void buf_free(Buf *b) {
    free(b->data);
    buf_init(b);
}

static int buf_reserve(Buf *b, size_t extra) {
    if (b->oom) return 0;
    size_t need = b->len + extra + 1;
    if (need <= b->cap) return 1;
    size_t cap = b->cap ? b->cap : 256;
    while (cap < need) cap *= 2;
    char *p = realloc(b->data, cap);
    if (!p) {
        b->oom = 1;
        return 0;
    }
    b->data = p;
    b->cap = cap;
    return 1;
}

void buf_append(Buf *b, const char *s, size_t n) {
    if (!buf_reserve(b, n)) return;
    memcpy(b->data + b->len, s, n);
    b->len += n;
    b->data[b->len] = '\0';
}

void buf_putc(Buf *b, char c) { buf_append(b, &c, 1); }

void buf_puts(Buf *b, const char *s) { buf_append(b, s, strlen(s)); }

void buf_printf(Buf *b, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    va_list copy;
    va_copy(copy, ap);
    int n = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (n > 0 && buf_reserve(b, (size_t)n)) {
        vsnprintf(b->data + b->len, (size_t)n + 1, fmt, ap);
        b->len += (size_t)n;
    }
    va_end(ap);
}

void buf_json_str(Buf *b, const char *s) {
    if (!s) {
        buf_puts(b, "null");
        return;
    }
    buf_putc(b, '"');
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
        case '"': buf_puts(b, "\\\""); break;
        case '\\': buf_puts(b, "\\\\"); break;
        case '\n': buf_puts(b, "\\n"); break;
        case '\r': buf_puts(b, "\\r"); break;
        case '\t': buf_puts(b, "\\t"); break;
        default:
            if (*p < 0x20) buf_printf(b, "\\u%04x", *p);
            else buf_putc(b, (char)*p);
        }
    }
    buf_putc(b, '"');
}

void buf_json_num(Buf *b, double v) {
    if (!isfinite(v)) {
        buf_puts(b, "null");
        return;
    }
    if (v == floor(v) && fabs(v) < 1e15) {
        buf_printf(b, "%.0f", v);
        return;
    }
    char tmp[64];
    snprintf(tmp, sizeof tmp, "%.4f", v);
    size_t n = strlen(tmp);
    while (n > 0 && tmp[n - 1] == '0') tmp[--n] = '\0';
    if (n > 0 && tmp[n - 1] == '.') tmp[--n] = '\0';
    if (strcmp(tmp, "-0") == 0) strcpy(tmp, "0");
    buf_puts(b, tmp);
}

char *buf_take(Buf *b) {
    if (b->oom) {
        buf_free(b);
        return NULL;
    }
    if (!b->data && !buf_reserve(b, 0)) return NULL;
    b->data[b->len] = '\0';
    char *out = b->data;
    buf_init(b);
    return out;
}
