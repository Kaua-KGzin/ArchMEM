#include "seg.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int fit_parse(const char *name, Fit *out) {
    static const char *names[] = {"first", "best", "worst"};
    if (!name) return 1;
    for (int i = 0; i < 3; i++)
        if (strcmp(name, names[i]) == 0) {
            *out = (Fit)i;
            return 0;
        }
    return 1;
}

const char *fit_name(Fit f) { return f == FIT_BEST ? "best" : f == FIT_WORST ? "worst" : "first"; }

typedef struct {
    int base, size;
    char name[17]; /* "" = hole */
} Region;

typedef struct {
    Region *r;
    int n, cap;
} Memory;

static int mem_insert(Memory *m, int at, Region reg) {
    if (m->n == m->cap) {
        int cap = m->cap ? m->cap * 2 : 16;
        Region *p = realloc(m->r, (size_t)cap * sizeof *p);
        if (!p) return 0;
        m->r = p;
        m->cap = cap;
    }
    memmove(&m->r[at + 1], &m->r[at], (size_t)(m->n - at) * sizeof *m->r);
    m->r[at] = reg;
    m->n++;
    return 1;
}

static void mem_remove(Memory *m, int at) {
    memmove(&m->r[at], &m->r[at + 1], (size_t)(m->n - at - 1) * sizeof *m->r);
    m->n--;
}

static void emit_map(const Memory *m, Buf *b) {
    buf_putc(b, '[');
    for (int i = 0; i < m->n; i++) {
        buf_printf(b, "%s{\"base\":%d,\"size\":%d,\"name\":", i ? "," : "", m->r[i].base, m->r[i].size);
        buf_json_str(b, m->r[i].name[0] ? m->r[i].name : NULL);
        buf_putc(b, '}');
    }
    buf_putc(b, ']');
}

static void free_stats(const Memory *m, int *free_total, int *largest, int *holes) {
    *free_total = *largest = *holes = 0;
    for (int i = 0; i < m->n; i++)
        if (!m->r[i].name[0]) {
            *free_total += m->r[i].size;
            if (m->r[i].size > *largest) *largest = m->r[i].size;
            (*holes)++;
        }
}

/* Splits ops into trimmed statements; returns count or -1 (too many). */
static int split_ops(const char *ops, char ***out) {
    int cap = 16, n = 0;
    char **list = malloc((size_t)cap * sizeof *list);
    if (!list) return -1;
    const char *p = ops;
    while (*p) {
        const char *start = p;
        while (*p && *p != ';' && *p != '\n') p++;
        const char *end = p;
        if (*p) p++;
        while (start < end && isspace((unsigned char)*start)) start++;
        while (end > start && isspace((unsigned char)end[-1])) end--;
        if (end == start) continue;
        if (n == SEG_MAX_OPS) {
            for (int i = 0; i < n; i++) free(list[i]);
            free(list);
            return -2;
        }
        if (n == cap) {
            cap *= 2;
            char **grown = realloc(list, (size_t)cap * sizeof *grown);
            if (!grown) {
                for (int i = 0; i < n; i++) free(list[i]);
                free(list);
                return -1;
            }
            list = grown;
        }
        size_t len = (size_t)(end - start);
        list[n] = malloc(len + 1);
        if (!list[n]) {
            for (int i = 0; i < n; i++) free(list[i]);
            free(list);
            return -1;
        }
        memcpy(list[n], start, len);
        list[n][len] = '\0';
        n++;
    }
    *out = list;
    return n;
}

static int valid_name(const char *s) {
    size_t n = strlen(s);
    if (n < 1 || n > 16) return 0;
    for (size_t i = 0; i < n; i++)
        if (!isalnum((unsigned char)s[i]) && s[i] != '_') return 0;
    return 1;
}

int seg_run(int memory_size, Fit fit, const char *ops, Buf *b, char *err, size_t errlen) {
    if (memory_size < 1 || memory_size > SEG_MAX_MEMORY)
        return snprintf(err, errlen, "memory_size deve estar entre 1 e %d", SEG_MAX_MEMORY), 1;
    char **list = NULL;
    int n = split_ops(ops ? ops : "", &list);
    if (n == -2) return snprintf(err, errlen, "operacoes demais (limite %d)", SEG_MAX_OPS), 1;
    if (n < 0) return snprintf(err, errlen, "sem memoria"), 1;
    if (n == 0) {
        free(list);
        return snprintf(err, errlen, "nenhuma operacao (use \"alloc NOME TAMANHO\" ou \"free NOME\")"), 1;
    }

    Memory m = {0};
    Buf steps;
    buf_init(&steps);
    int rc = 0, failures = 0;
    if (!mem_insert(&m, 0, (Region){0, memory_size, ""})) { snprintf(err, errlen, "sem memoria"); rc = 1; }

    for (int k = 0; k < n && !rc; k++) {
        char verb[16] = "", name[64] = "", extra[8] = "";
        long size = 0;
        int got = sscanf(list[k], "%15s %63s %ld %7s", verb, name, &size, extra);
        int is_alloc = strcmp(verb, "alloc") == 0, is_free = strcmp(verb, "free") == 0;
        if ((!is_alloc && !is_free) || (is_alloc && got != 3) || (is_free && got != 2)) {
            rc = 1;
            snprintf(err, errlen, "operacao %d invalida: \"%.40s\" (use \"alloc NOME TAMANHO\" ou \"free NOME\")", k + 1,
                     list[k]);
            break;
        }
        if (!valid_name(name)) {
            rc = 1;
            snprintf(err, errlen, "operacao %d: nome \"%.20s\" invalido (letras, numeros e _, ate 16)", k + 1, name);
            break;
        }
        int owner = -1;
        for (int i = 0; i < m.n; i++)
            if (strcmp(m.r[i].name, name) == 0) owner = i;

        if (k) buf_putc(&steps, ',');
        if (is_free) {
            if (owner < 0) {
                rc = 1;
                snprintf(err, errlen, "operacao %d: \"%s\" nao esta alocado", k + 1, name);
                break;
            }
            int base = m.r[owner].base, sz = m.r[owner].size;
            m.r[owner].name[0] = '\0';
            if (owner + 1 < m.n && !m.r[owner + 1].name[0]) {
                m.r[owner].size += m.r[owner + 1].size;
                mem_remove(&m, owner + 1);
            }
            if (owner > 0 && !m.r[owner - 1].name[0]) {
                m.r[owner - 1].size += m.r[owner].size;
                mem_remove(&m, owner);
            }
            buf_printf(&steps, "{\"step\":%d,\"op\":\"free\",\"name\":", k + 1);
            buf_json_str(&steps, name);
            buf_printf(&steps, ",\"size\":%d,\"result\":\"ok\",\"base\":%d,\"chosen_hole\":null,\"candidates\":[],"
                               "\"why\":\"seg_free\",\"map\":", sz, base);
        } else {
            if (size <= 0 || size > SEG_MAX_MEMORY) {
                rc = 1;
                snprintf(err, errlen, "operacao %d: tamanho deve estar entre 1 e %d", k + 1, SEG_MAX_MEMORY);
                break;
            }
            if (owner >= 0) {
                rc = 1;
                snprintf(err, errlen, "operacao %d: \"%s\" ja esta alocado", k + 1, name);
                break;
            }
            int chosen = -1;
            Buf cand;
            buf_init(&cand);
            int nc = 0;
            for (int i = 0; i < m.n; i++) {
                if (m.r[i].name[0] || m.r[i].size < size) continue;
                buf_printf(&cand, "%s{\"base\":%d,\"size\":%d}", nc++ ? "," : "", m.r[i].base, m.r[i].size);
                if (chosen < 0 || (fit == FIT_BEST && m.r[i].size < m.r[chosen].size) ||
                    (fit == FIT_WORST && m.r[i].size > m.r[chosen].size))
                    chosen = i;
            }
            char *cands = buf_take(&cand);
            buf_printf(&steps, "{\"step\":%d,\"op\":\"alloc\",\"name\":", k + 1);
            buf_json_str(&steps, name);
            if (chosen < 0) {
                int free_total, largest, holes;
                free_stats(&m, &free_total, &largest, &holes);
                failures++;
                buf_printf(&steps, ",\"size\":%ld,\"result\":\"fail\",\"base\":-1,\"chosen_hole\":null,\"candidates\":[],"
                                   "\"why\":\"%s\",\"map\":",
                           size, free_total >= size ? "seg_fail_fragmentation" : "seg_fail_no_space");
            } else {
                Region hole = m.r[chosen];
                buf_printf(&steps, ",\"size\":%ld,\"result\":\"ok\",\"base\":%d,\"chosen_hole\":{\"base\":%d,\"size\":%d},"
                                   "\"candidates\":[%s],\"why\":\"seg_%s_fit\",\"map\":",
                           size, hole.base, hole.base, hole.size, cands ? cands : "", fit_name(fit));
                Region used = {hole.base, (int)size, ""};
                strcpy(used.name, name);
                if (hole.size == size) {
                    m.r[chosen] = used;
                } else {
                    m.r[chosen].base += (int)size;
                    m.r[chosen].size -= (int)size;
                    if (!mem_insert(&m, chosen, used)) {
                        free(cands);
                        { snprintf(err, errlen, "sem memoria"); rc = 1; }
                        break;
                    }
                }
            }
            free(cands);
        }
        emit_map(&m, &steps);
        buf_putc(&steps, '}');
    }

    if (!rc) {
        int free_total, largest, holes;
        free_stats(&m, &free_total, &largest, &holes);
        buf_puts(b, "\"steps\":[");
        char *s = buf_take(&steps);
        buf_puts(b, s ? s : "");
        free(s);
        buf_printf(b, "],\"totals\":{\"memory_size\":%d,\"free\":%d,\"largest_hole\":%d,\"holes\":%d,\"failures\":%d,"
                      "\"external_fragmentation\":",
                   memory_size, free_total, largest, holes, failures);
        buf_json_num(b, free_total ? 1.0 - (double)largest / free_total : 0.0);
        buf_putc(b, '}');
    }
    buf_free(&steps);
    for (int i = 0; i < n; i++) free(list[i]);
    free(list);
    free(m.r);
    return rc;
}
