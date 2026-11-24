#include "challenge.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "json.h"
#include "paging.h"
#include "rng.h"

#define CH_MAX_REFS 16
#define CH_PAGE_SIZE 64
#define CH_VPAGES 8

static void draw_refs(Rng *r, Ref *refs, int n, int pages) {
    for (int i = 0; i < n; i++) refs[i] = (Ref){'R', 0, rng_range(r, 0, pages - 1)};
}

static int faults_of(const Ref *refs, int n, int frames, Algorithm alg) {
    PagingConfig cfg = {CH_PAGE_SIZE, frames, CH_VPAGES, 0, alg};
    Totals t;
    char err[128];
    return paging_run(&cfg, refs, (size_t)n, NULL, &t, err, sizeof err) ? -1 : t.faults;
}

static void emit(Buf *b, const char *kind, int difficulty, uint32_t seed, const char *params, int frames, Algorithm alg,
                 const Ref *refs, int n) {
    buf_printf(b, "{\"kind\":\"%s\",\"difficulty\":%d,\"seed\":%u,\"params\":%s,\"scenario\":{\"config\":"
                  "{\"page_size\":%d,\"frames\":%d,\"virtual_pages\":%d,\"tlb_entries\":0,\"algorithm\":\"%s\"},\"refs\":\"",
               kind, difficulty, (unsigned)seed, params, CH_PAGE_SIZE, frames, CH_VPAGES, alg_name(alg));
    refs_format(refs, (size_t)n, b);
    buf_puts(b, "\"},\"answer\":");
}

static void faults_challenge(Rng *r, Buf *b, int difficulty, uint32_t seed) {
    Ref refs[CH_MAX_REFS];
    Algorithm alg = rng_range(r, 0, 1) ? ALG_LRU : ALG_FIFO;
    int n = rng_range(r, 10, 12);
    draw_refs(r, refs, n, 5);
    char params[96];
    snprintf(params, sizeof params, "{\"algorithm\":\"%s\",\"frames\":3}", alg_name(alg));
    emit(b, "faults", difficulty, seed, params, 3, alg, refs, n);
    buf_printf(b, "%d}", faults_of(refs, n, 3, alg));
}

static int victim_challenge(Rng *r, Buf *b, uint32_t seed) {
    for (int attempt = 0; attempt < 50; attempt++) {
        Ref refs[CH_MAX_REFS];
        Algorithm alg = (Algorithm)rng_range(r, 0, ALG_COUNT - 1);
        int frames = rng_range(r, 3, 4);
        draw_refs(r, refs, 15, 7);
        PagingConfig cfg = {CH_PAGE_SIZE, frames, CH_VPAGES, 0, alg};
        Totals t;
        char err[128];
        Buf steps;
        buf_init(&steps);
        if (paging_run(&cfg, refs, 15, &steps, &t, err, sizeof err)) {
            buf_free(&steps);
            continue;
        }
        char *s = buf_take(&steps);
        JVal *v = json_parse(s, err, sizeof err);
        free(s);
        int evictions[15], ne = 0;
        for (size_t i = 0; i < json_len(v); i++)
            if (json_type(json_get(json_at(v, i), "victim")) == J_OBJ) evictions[ne++] = (int)i;
        if (ne == 0) {
            json_free(v);
            continue;
        }
        int pick = evictions[rng_range(r, 0, ne - 1)];
        int answer = (int)json_num(json_get(json_get(json_at(v, (size_t)pick), "victim"), "page"), -1);
        json_free(v);
        char params[128];
        snprintf(params, sizeof params, "{\"algorithm\":\"%s\",\"frames\":%d,\"step\":%d}", alg_name(alg), frames,
                 pick + 1);
        emit(b, "victim", 2, seed, params, frames, alg, refs, 15);
        buf_printf(b, "%d}", answer);
        return 1;
    }
    return 0;
}

static int best_challenge(Rng *r, Buf *b, uint32_t seed) {
    static const Algorithm cands[] = {ALG_FIFO, ALG_LRU, ALG_CLOCK, ALG_LFU};
    for (int attempt = 0; attempt < 50; attempt++) {
        Ref refs[CH_MAX_REFS];
        draw_refs(r, refs, 15, 6);
        int best = 0, ties = 0, f[4];
        for (int i = 0; i < 4; i++) f[i] = faults_of(refs, 15, 3, cands[i]);
        for (int i = 1; i < 4; i++)
            if (f[i] < f[best]) best = i;
        for (int i = 0; i < 4; i++)
            if (f[i] == f[best]) ties++;
        if (ties != 1) continue;
        emit(b, "best_algorithm", 3, seed, "{\"frames\":3,\"candidates\":[\"fifo\",\"lru\",\"clock\",\"lfu\"]}", 3,
             ALG_FIFO, refs, 15);
        buf_printf(b, "\"%s\"}", alg_name(cands[best]));
        return 1;
    }
    return 0;
}

int challenge_make(uint32_t seed, int difficulty, Buf *b, char *err, size_t errlen) {
    if (difficulty < 1 || difficulty > 3) return snprintf(err, errlen, "difficulty deve ser 1, 2 ou 3"), 1;
    Rng r;
    rng_seed(&r, seed);
    if (difficulty == 2 && victim_challenge(&r, b, seed)) return 0;
    if (difficulty == 3 && best_challenge(&r, b, seed)) return 0;
    faults_challenge(&r, b, difficulty, seed);
    return 0;
}
