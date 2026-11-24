/* archmem_run: the single JSON boundary shared by the CLI and WebAssembly. */
#include "archmem.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "buf.h"
#include "challenge.h"
#include "json.h"
#include "paging.h"
#include "refs.h"
#include "rng.h"
#include "seg.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#define API EMSCRIPTEN_KEEPALIVE
#else
#define API
#endif

#define ERRLEN 256

/* Reads an optional integer field. Returns 0 if absent (keeps *out) or valid. */
static int get_int(const JVal *obj, const char *key, int *out, char *err) {
    const JVal *v = json_get(obj, key);
    if (!v) return 0;
    double d = json_num(v, NAN);
    if (json_type(v) != J_NUM || d != floor(d) || fabs(d) > 2e9) {
        snprintf(err, ERRLEN, "campo \"%s\" deve ser um numero inteiro", key);
        return 1;
    }
    *out = (int)d;
    return 0;
}

static int get_str(const JVal *obj, const char *key, const char **out, char *err) {
    const JVal *v = json_get(obj, key);
    if (!v) return 0;
    if (json_type(v) != J_STR) {
        snprintf(err, ERRLEN, "campo \"%s\" deve ser texto", key);
        return 1;
    }
    *out = json_str(v, *out);
    return 0;
}

static int read_config(const JVal *req, PagingConfig *cfg, char *err) {
    *cfg = (PagingConfig){64, 3, 16, 0, ALG_LRU};
    const JVal *c = json_get(req, "config");
    if (c && json_type(c) != J_OBJ) return snprintf(err, ERRLEN, "campo \"config\" deve ser um objeto"), 1;
    const char *alg = NULL;
    if (get_int(c, "page_size", &cfg->page_size, err) || get_int(c, "frames", &cfg->frames, err) ||
        get_int(c, "virtual_pages", &cfg->virtual_pages, err) || get_int(c, "tlb_entries", &cfg->tlb_entries, err) ||
        get_str(c, "algorithm", &alg, err))
        return 1;
    if (alg && alg_parse(alg, &cfg->algorithm))
        return snprintf(err, ERRLEN, "algoritmo \"%.20s\" desconhecido (fifo, lru, clock, lfu, opt)", alg), 1;
    return paging_config_check(cfg, err, ERRLEN);
}

static int read_refs(const JVal *req, Ref **refs, size_t *n, char *err) {
    const JVal *r = json_get(req, "refs");
    if (!r) return snprintf(err, ERRLEN, "campo \"refs\" (sequencia de acessos) e obrigatorio"), 1;
    if (json_type(r) != J_STR) return snprintf(err, ERRLEN, "campo \"refs\" deve ser texto, ex.: \"7 0 1 2w @130\""), 1;
    return refs_parse(json_str(r, ""), refs, n, err, ERRLEN);
}

/* Reads "algorithms" (default: all five). */
static int read_algorithms(const JVal *req, Algorithm *out, int *count, char *err) {
    const JVal *a = json_get(req, "algorithms");
    if (!a) {
        for (int i = 0; i < ALG_COUNT; i++) out[i] = (Algorithm)i;
        *count = ALG_COUNT;
        return 0;
    }
    if (json_type(a) != J_ARR || json_len(a) == 0 || json_len(a) > ALG_COUNT)
        return snprintf(err, ERRLEN, "campo \"algorithms\" deve ser uma lista de 1 a %d algoritmos", ALG_COUNT), 1;
    *count = (int)json_len(a);
    for (int i = 0; i < *count; i++) {
        const char *name = json_str(json_at(a, (size_t)i), NULL);
        if (!name || alg_parse(name, &out[i]))
            return snprintf(err, ERRLEN, "algorithms[%d] invalido (fifo, lru, clock, lfu, opt)", i), 1;
    }
    return 0;
}

static void emit_totals(Buf *b, const Totals *t) {
    buf_printf(b,
               "{\"accesses\":%d,\"faults\":%d,\"hits\":%d,\"hit_rate\":", t->accesses, t->faults, t->hits);
    buf_json_num(b, t->accesses ? (double)t->hits / t->accesses : 0.0);
    buf_printf(b, ",\"replacements\":%d,\"writebacks\":%d,\"tlb_hits\":%d,\"tlb_misses\":%d}", t->replacements,
               t->writebacks, t->tlb_hits, t->tlb_misses);
}

static void emit_config(Buf *b, const PagingConfig *c) {
    buf_printf(b, "{\"page_size\":%d,\"frames\":%d,\"virtual_pages\":%d,\"tlb_entries\":%d,\"algorithm\":\"%s\"}",
               c->page_size, c->frames, c->virtual_pages, c->tlb_entries, alg_name(c->algorithm));
}

static int do_run(const JVal *req, Buf *b, char *err) {
    PagingConfig cfg;
    Ref *refs = NULL;
    size_t n = 0;
    if (read_config(req, &cfg, err) || read_refs(req, &refs, &n, err)) return 1;
    Buf steps;
    buf_init(&steps);
    Totals t;
    int rc = paging_run(&cfg, refs, n, &steps, &t, err, ERRLEN);
    free(refs);
    if (!rc) {
        buf_printf(b, "{\"ok\":true,\"algorithm\":\"%s\",\"config\":", alg_name(cfg.algorithm));
        emit_config(b, &cfg);
        buf_puts(b, ",\"totals\":");
        emit_totals(b, &t);
        buf_puts(b, ",\"steps\":");
        char *s = buf_take(&steps);
        buf_puts(b, s ? s : "[]");
        free(s);
        buf_putc(b, '}');
    }
    buf_free(&steps);
    return rc;
}

static int do_compare(const JVal *req, Buf *b, char *err) {
    PagingConfig cfg;
    Algorithm algs[ALG_COUNT];
    int na;
    Ref *refs = NULL;
    size_t n = 0;
    if (read_config(req, &cfg, err) || read_algorithms(req, algs, &na, err) || read_refs(req, &refs, &n, err)) return 1;
    buf_puts(b, "{\"ok\":true,\"config\":");
    emit_config(b, &cfg);
    buf_puts(b, ",\"results\":[");
    for (int i = 0; i < na; i++) {
        cfg.algorithm = algs[i];
        Totals t;
        if (paging_run(&cfg, refs, n, NULL, &t, err, ERRLEN)) {
            free(refs);
            return 1;
        }
        buf_printf(b, "%s{\"algorithm\":\"%s\",\"totals\":", i ? "," : "", alg_name(algs[i]));
        emit_totals(b, &t);
        buf_putc(b, '}');
    }
    buf_puts(b, "]}");
    free(refs);
    return 0;
}

static int do_curve(const JVal *req, Buf *b, char *err) {
    PagingConfig cfg;
    Algorithm algs[ALG_COUNT];
    int na, max_frames = 8;
    Ref *refs = NULL;
    size_t n = 0;
    if (read_config(req, &cfg, err) || read_algorithms(req, algs, &na, err) ||
        get_int(req, "max_frames", &max_frames, err))
        return 1;
    if (max_frames < 1 || max_frames > PAGING_MAX_FRAMES)
        return snprintf(err, ERRLEN, "max_frames deve estar entre 1 e %d", PAGING_MAX_FRAMES), 1;
    if (read_refs(req, &refs, &n, err)) return 1;
    buf_puts(b, "{\"ok\":true,\"frames\":[");
    for (int f = 1; f <= max_frames; f++) buf_printf(b, f > 1 ? ",%d" : "%d", f);
    buf_puts(b, "],\"series\":[");
    Buf belady;
    buf_init(&belady);
    int nb = 0;
    for (int i = 0; i < na; i++) {
        buf_printf(b, "%s{\"algorithm\":\"%s\",\"faults\":[", i ? "," : "", alg_name(algs[i]));
        int prev = -1;
        for (int f = 1; f <= max_frames; f++) {
            cfg.algorithm = algs[i];
            cfg.frames = f;
            Totals t;
            if (paging_run(&cfg, refs, n, NULL, &t, err, ERRLEN)) {
                free(refs);
                buf_free(&belady);
                return 1;
            }
            buf_printf(b, f > 1 ? ",%d" : "%d", t.faults);
            if (prev >= 0 && t.faults > prev)
                buf_printf(&belady, "%s{\"algorithm\":\"%s\",\"at_frames\":%d}", nb++ ? "," : "", alg_name(algs[i]), f);
            prev = t.faults;
        }
        buf_puts(b, "]}");
    }
    char *bl = buf_take(&belady);
    buf_printf(b, "],\"belady\":[%s]}", bl ? bl : "");
    free(bl);
    free(refs);
    return 0;
}

static int do_gen(const JVal *req, Buf *b, char *err) {
    const char *kind_s = "uniform";
    int length = 20, pages = 8, seed = 1;
    if (get_str(req, "kind", &kind_s, err) || get_int(req, "length", &length, err) ||
        get_int(req, "pages", &pages, err) || get_int(req, "seed", &seed, err))
        return 1;
    const JVal *wr = json_get(req, "write_ratio");
    if (wr && json_type(wr) != J_NUM) return snprintf(err, ERRLEN, "campo \"write_ratio\" deve ser numero"), 1;
    GenKind kind;
    if (gen_parse(kind_s, &kind))
        return snprintf(err, ERRLEN, "kind \"%.20s\" desconhecido (uniform, locality, loop, sequential)", kind_s), 1;
    Ref *refs = NULL;
    size_t n = 0;
    if (gen_refs(kind, length, pages, (uint32_t)seed, json_num(wr, 0.0), &refs, &n, err, ERRLEN)) return 1;
    buf_puts(b, "{\"ok\":true,\"refs\":\"");
    refs_format(refs, n, b);
    buf_puts(b, "\"}");
    free(refs);
    return 0;
}

static int do_seg(const JVal *req, Buf *b, char *err) {
    int memory = 1024;
    const char *fit_s = "first", *ops = NULL;
    if (get_int(req, "memory_size", &memory, err) || get_str(req, "fit", &fit_s, err) || get_str(req, "ops", &ops, err))
        return 1;
    if (!ops) return snprintf(err, ERRLEN, "campo \"ops\" e obrigatorio, ex.: \"alloc A 300; free A\""), 1;
    Fit fit;
    if (fit_parse(fit_s, &fit)) return snprintf(err, ERRLEN, "fit \"%.20s\" desconhecido (first, best, worst)", fit_s), 1;
    Buf body;
    buf_init(&body);
    if (seg_run(memory, fit, ops, &body, err, ERRLEN)) {
        buf_free(&body);
        return 1;
    }
    char *s = buf_take(&body);
    buf_printf(b, "{\"ok\":true,\"fit\":\"%s\",%s}", fit_name(fit), s ? s : "");
    free(s);
    return 0;
}

static int do_challenge(const JVal *req, Buf *b, char *err) {
    int seed = 1, difficulty = 1;
    if (get_int(req, "seed", &seed, err) || get_int(req, "difficulty", &difficulty, err)) return 1;
    Buf body;
    buf_init(&body);
    if (challenge_make((uint32_t)seed, difficulty, &body, err, ERRLEN)) {
        buf_free(&body);
        return 1;
    }
    char *s = buf_take(&body);
    buf_printf(b, "{\"ok\":true,\"challenge\":%s}", s ? s : "null");
    free(s);
    return 0;
}

static char *error_response(const char *msg) {
    Buf b;
    buf_init(&b);
    buf_puts(&b, "{\"ok\":false,\"error\":");
    buf_json_str(&b, msg);
    buf_putc(&b, '}');
    char *s = buf_take(&b);
    return s ? s : NULL;
}

API char *archmem_run(const char *request_json) {
    char err[ERRLEN] = "";
    JVal *req = json_parse(request_json, err, sizeof err);
    if (!req) return error_response(err);
    if (json_type(req) != J_OBJ) {
        json_free(req);
        return error_response("a requisicao deve ser um objeto JSON");
    }
    const char *action = json_str(json_get(req, "action"), NULL);
    Buf b;
    buf_init(&b);
    int rc;
    if (!action) rc = (snprintf(err, sizeof err, "campo \"action\" e obrigatorio"), 1);
    else if (strcmp(action, "run") == 0) rc = do_run(req, &b, err);
    else if (strcmp(action, "compare") == 0) rc = do_compare(req, &b, err);
    else if (strcmp(action, "curve") == 0) rc = do_curve(req, &b, err);
    else if (strcmp(action, "gen") == 0) rc = do_gen(req, &b, err);
    else if (strcmp(action, "seg") == 0) rc = do_seg(req, &b, err);
    else if (strcmp(action, "challenge") == 0) rc = do_challenge(req, &b, err);
    else if (strcmp(action, "version") == 0) rc = (buf_puts(&b, "{\"ok\":true,\"version\":\"" ARCHMEM_VERSION "\"}"), 0);
    else rc = (snprintf(err, sizeof err, "action \"%.20s\" desconhecida (run, compare, curve, gen, seg, challenge)", action), 1);
    json_free(req);
    if (rc) {
        buf_free(&b);
        return error_response(err[0] ? err : "erro desconhecido");
    }
    char *out = buf_take(&b);
    return out ? out : error_response("sem memoria");
}

API void archmem_free(char *response) { free(response); }
