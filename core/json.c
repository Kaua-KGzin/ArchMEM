#include "json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct JVal {
    JType type;
    double num;
    int boolean;
    char *str;        /* J_STR */
    char **keys;      /* J_OBJ */
    struct JVal **items; /* J_ARR, J_OBJ */
    size_t len;
};

typedef struct {
    const char *p;
    const char *start;
    char *err;
    size_t errlen;
    int failed;
} Parser;

static void fail(Parser *ps, const char *msg) {
    if (!ps->failed) {
        snprintf(ps->err, ps->errlen, "JSON invalido na posicao %ld: %s", (long)(ps->p - ps->start) + 1, msg);
        ps->failed = 1;
    }
}

static void skip_ws(Parser *ps) {
    while (*ps->p == ' ' || *ps->p == '\t' || *ps->p == '\n' || *ps->p == '\r') ps->p++;
}

static JVal *new_val(JType t) {
    JVal *v = calloc(1, sizeof *v);
    if (v) v->type = t;
    return v;
}

static int push_item(JVal *v, JVal *item, char *key) {
    JVal **items = realloc(v->items, (v->len + 1) * sizeof *items);
    if (!items) return 0;
    v->items = items;
    if (v->type == J_OBJ) {
        char **keys = realloc(v->keys, (v->len + 1) * sizeof *keys);
        if (!keys) return 0;
        v->keys = keys;
        v->keys[v->len] = key;
    }
    v->items[v->len++] = item;
    return 1;
}

static void put_utf8(char **out, unsigned cp) {
    char *o = *out;
    if (cp < 0x80) {
        *o++ = (char)cp;
    } else if (cp < 0x800) {
        *o++ = (char)(0xC0 | (cp >> 6));
        *o++ = (char)(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        *o++ = (char)(0xE0 | (cp >> 12));
        *o++ = (char)(0x80 | ((cp >> 6) & 0x3F));
        *o++ = (char)(0x80 | (cp & 0x3F));
    } else {
        *o++ = (char)(0xF0 | (cp >> 18));
        *o++ = (char)(0x80 | ((cp >> 12) & 0x3F));
        *o++ = (char)(0x80 | ((cp >> 6) & 0x3F));
        *o++ = (char)(0x80 | (cp & 0x3F));
    }
    *out = o;
}

static int hex4(const char *p, unsigned *out) {
    unsigned v = 0;
    for (int i = 0; i < 4; i++) {
        char c = p[i];
        v <<= 4;
        if (c >= '0' && c <= '9') v |= (unsigned)(c - '0');
        else if (c >= 'a' && c <= 'f') v |= (unsigned)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') v |= (unsigned)(c - 'A' + 10);
        else return 0;
    }
    *out = v;
    return 1;
}

static char *parse_string(Parser *ps) {
    /* ps->p is at the opening quote. Output is never longer than input. */
    const char *q = ps->p + 1;
    size_t max = 0;
    while (q[max] && q[max] != '"') {
        if (q[max] == '\\' && q[max + 1]) max++;
        max++;
    }
    if (!q[max]) {
        fail(ps, "string sem fim");
        return NULL;
    }
    char *out = malloc(max + 4);
    if (!out) {
        fail(ps, "sem memoria");
        return NULL;
    }
    char *o = out;
    ps->p++;
    while (*ps->p != '"') {
        unsigned char c = (unsigned char)*ps->p;
        if (c < 0x20) {
            fail(ps, "caractere de controle em string");
            free(out);
            return NULL;
        }
        if (c != '\\') {
            *o++ = (char)c;
            ps->p++;
            continue;
        }
        ps->p++;
        switch (*ps->p) {
        case '"': *o++ = '"'; break;
        case '\\': *o++ = '\\'; break;
        case '/': *o++ = '/'; break;
        case 'b': *o++ = '\b'; break;
        case 'f': *o++ = '\f'; break;
        case 'n': *o++ = '\n'; break;
        case 'r': *o++ = '\r'; break;
        case 't': *o++ = '\t'; break;
        case 'u': {
            unsigned cp;
            if (!hex4(ps->p + 1, &cp)) {
                fail(ps, "escape \\u invalido");
                free(out);
                return NULL;
            }
            ps->p += 4;
            if (cp >= 0xD800 && cp <= 0xDBFF && ps->p[1] == '\\' && ps->p[2] == 'u') {
                unsigned lo;
                if (hex4(ps->p + 3, &lo) && lo >= 0xDC00 && lo <= 0xDFFF) {
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    ps->p += 6;
                }
            }
            put_utf8(&o, cp);
            break;
        }
        default:
            fail(ps, "escape invalido");
            free(out);
            return NULL;
        }
        ps->p++;
    }
    ps->p++;
    *o = '\0';
    return out;
}

static JVal *parse_value(Parser *ps, int depth);

static JVal *parse_container(Parser *ps, int depth, int is_obj) {
    JVal *v = new_val(is_obj ? J_OBJ : J_ARR);
    if (!v) {
        fail(ps, "sem memoria");
        return NULL;
    }
    char close = is_obj ? '}' : ']';
    ps->p++;
    skip_ws(ps);
    if (*ps->p == close) {
        ps->p++;
        return v;
    }
    for (;;) {
        char *key = NULL;
        if (is_obj) {
            skip_ws(ps);
            if (*ps->p != '"') {
                fail(ps, "esperava chave entre aspas");
                break;
            }
            key = parse_string(ps);
            if (!key) break;
            skip_ws(ps);
            if (*ps->p != ':') {
                fail(ps, "esperava ':'");
                free(key);
                break;
            }
            ps->p++;
        }
        JVal *item = parse_value(ps, depth + 1);
        if (!item) {
            free(key);
            break;
        }
        if (!push_item(v, item, key)) {
            json_free(item);
            free(key);
            fail(ps, "sem memoria");
            break;
        }
        skip_ws(ps);
        if (*ps->p == ',') {
            ps->p++;
            skip_ws(ps);
            if (*ps->p == close) {
                fail(ps, "virgula sobrando");
                break;
            }
            continue;
        }
        if (*ps->p == close) {
            ps->p++;
            return v;
        }
        fail(ps, is_obj ? "esperava ',' ou '}'" : "esperava ',' ou ']'");
        break;
    }
    json_free(v);
    return NULL;
}

static JVal *parse_number(Parser *ps) {
    const char *s = ps->p;
    if (*s == '-') s++;
    if (!(*s >= '0' && *s <= '9')) {
        fail(ps, "valor invalido");
        return NULL;
    }
    char *end = NULL;
    double d = strtod(ps->p, &end);
    if (end == ps->p) {
        fail(ps, "numero invalido");
        return NULL;
    }
    JVal *v = new_val(J_NUM);
    if (!v) {
        fail(ps, "sem memoria");
        return NULL;
    }
    v->num = d;
    ps->p = end;
    return v;
}

static JVal *parse_value(Parser *ps, int depth) {
    if (depth >= JSON_MAX_DEPTH) { /* depth counts from 0: at most 64 levels */
        fail(ps, "aninhamento profundo demais");
        return NULL;
    }
    skip_ws(ps);
    switch (*ps->p) {
    case '{': return parse_container(ps, depth, 1);
    case '[': return parse_container(ps, depth, 0);
    case '"': {
        char *s = parse_string(ps);
        if (!s) return NULL;
        JVal *v = new_val(J_STR);
        if (!v) {
            free(s);
            fail(ps, "sem memoria");
            return NULL;
        }
        v->str = s;
        return v;
    }
    case 't':
    case 'f':
    case 'n': {
        const char *words[] = {"true", "false", "null"};
        for (int i = 0; i < 3; i++) {
            size_t n = strlen(words[i]);
            if (strncmp(ps->p, words[i], n) == 0) {
                ps->p += n;
                JVal *v = new_val(i == 2 ? J_NULL : J_BOOL);
                if (!v) {
                    fail(ps, "sem memoria");
                    return NULL;
                }
                v->boolean = (i == 0);
                return v;
            }
        }
        fail(ps, "valor invalido");
        return NULL;
    }
    case '\0':
        fail(ps, "fim inesperado");
        return NULL;
    default:
        return parse_number(ps);
    }
}

JVal *json_parse(const char *text, char *err, size_t errlen) {
    if (errlen) err[0] = '\0';
    if (!text) {
        snprintf(err, errlen, "JSON vazio");
        return NULL;
    }
    if (strlen(text) > JSON_MAX_INPUT) {
        snprintf(err, errlen, "JSON grande demais (limite 4 MiB)");
        return NULL;
    }
    Parser ps = {text, text, err, errlen, 0};
    JVal *v = parse_value(&ps, 0);
    if (!v) return NULL;
    skip_ws(&ps);
    if (*ps.p != '\0') {
        fail(&ps, "conteudo depois do fim");
        json_free(v);
        return NULL;
    }
    return v;
}

void json_free(JVal *v) {
    if (!v) return;
    for (size_t i = 0; i < v->len; i++) {
        json_free(v->items[i]);
        if (v->keys) free(v->keys[i]);
    }
    free(v->items);
    free(v->keys);
    free(v->str);
    free(v);
}

JType json_type(const JVal *v) { return v ? v->type : J_NULL; }

const JVal *json_get(const JVal *obj, const char *key) {
    if (!obj || obj->type != J_OBJ) return NULL;
    for (size_t i = 0; i < obj->len; i++)
        if (strcmp(obj->keys[i], key) == 0) return obj->items[i];
    return NULL;
}

size_t json_len(const JVal *v) {
    return (v && (v->type == J_ARR || v->type == J_OBJ)) ? v->len : 0;
}

const JVal *json_at(const JVal *arr, size_t i) {
    return (arr && arr->type == J_ARR && i < arr->len) ? arr->items[i] : NULL;
}

double json_num(const JVal *v, double def) { return (v && v->type == J_NUM) ? v->num : def; }

const char *json_str(const JVal *v, const char *def) { return (v && v->type == J_STR) ? v->str : def; }

int json_bool(const JVal *v, int def) { return (v && v->type == J_BOOL) ? v->boolean : def; }
