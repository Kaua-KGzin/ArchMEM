#include "refs.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int is_sep(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == ','; }

/* Parses one token [start, end) into r. Returns 0 on success. */
static int parse_token(const char *start, const char *end, Ref *r) {
    const char *p = start;
    r->op = 'R';
    r->is_address = 0;
    if (p < end && *p == '@') {
        r->is_address = 1;
        p++;
    }
    if (end > p && (end[-1] == 'w' || end[-1] == 'W')) {
        r->op = 'W';
        end--;
    }
    if (p >= end) return 1;
    int base = 10;
    if (r->is_address && end - p > 2 && p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        base = 16;
        p += 2;
    }
    long v = 0;
    for (; p < end; p++) {
        int d;
        if (isdigit((unsigned char)*p)) d = *p - '0';
        else if (base == 16 && isxdigit((unsigned char)*p)) d = tolower((unsigned char)*p) - 'a' + 10;
        else return 1;
        v = v * base + d;
        if (v > REFS_MAX_VALUE) return 2;
    }
    r->value = v;
    return 0;
}

int refs_parse(const char *s, Ref **out, size_t *count, char *err, size_t errlen) {
    *out = NULL;
    *count = 0;
    if (errlen) err[0] = '\0';
    if (!s) s = "";
    size_t cap = 16, n = 0;
    Ref *refs = malloc(cap * sizeof *refs);
    if (!refs) {
        snprintf(err, errlen, "sem memoria");
        return 1;
    }
    const char *p = s;
    for (;;) {
        while (*p && is_sep(*p)) p++;
        if (!*p) break;
        const char *start = p;
        while (*p && !is_sep(*p)) p++;
        if (n >= REFS_MAX) {
            snprintf(err, errlen, "sequencia longa demais (limite %d acessos)", REFS_MAX);
            free(refs);
            return 1;
        }
        Ref r;
        int rc = parse_token(start, p, &r);
        if (rc) {
            int len = (int)(p - start);
            snprintf(err, errlen, "acesso %zu invalido: \"%.*s\"%s", n + 1, len > 32 ? 32 : len, start,
                     rc == 2 ? " (numero grande demais)" : " (use N, Nw, @endereco ou @enderecow)");
            free(refs);
            return 1;
        }
        if (n == cap) {
            cap *= 2;
            Ref *grown = realloc(refs, cap * sizeof *refs);
            if (!grown) {
                snprintf(err, errlen, "sem memoria");
                free(refs);
                return 1;
            }
            refs = grown;
        }
        refs[n++] = r;
    }
    if (n == 0) {
        snprintf(err, errlen, "sequencia de acessos vazia");
        free(refs);
        return 1;
    }
    *out = refs;
    *count = n;
    return 0;
}

void refs_format(const Ref *refs, size_t count, Buf *b) {
    for (size_t i = 0; i < count; i++) {
        if (i) buf_putc(b, ' ');
        buf_printf(b, "%s%ld%s", refs[i].is_address ? "@" : "", refs[i].value, refs[i].op == 'W' ? "w" : "");
    }
}
