#include "../core/json.h"
#include "../core/oracle.h"
#include "../core/paging.h"
#include "test.h"

#include <stdlib.h>

static JVal *sim(Algorithm alg, int frames, const char *refs_s) {
    Ref *refs = NULL;
    size_t n = 0;
    char err[160];
    if (refs_parse(refs_s, &refs, &n, err, sizeof err)) return NULL;
    PagingConfig cfg = {64, frames, 16, 0, alg};
    Totals t;
    Buf b;
    buf_init(&b);
    buf_puts(&b, "{\"steps\":");
    int rc = paging_run(&cfg, refs, n, &b, &t, err, sizeof err);
    free(refs);
    if (rc) {
        fprintf(stderr, "paging_run: %s\n", err);
        buf_free(&b);
        return NULL;
    }
    buf_putc(&b, '}');
    char *s = buf_take(&b);
    JVal *v = json_parse(s, err, sizeof err);
    free(s);
    return v;
}

static const JVal *step(const JVal *v, int k) { return json_at(json_get(v, "steps"), (size_t)(k - 1)); }
static int geti(const JVal *o, const char *key) { return (int)json_num(json_get(o, key), -999); }

/* count of the frame holding page p at step k */
static int count_of(const JVal *v, int k, int p) {
    const JVal *frames = json_get(step(v, k), "frames");
    for (size_t i = 0; i < json_len(frames); i++)
        if (geti(json_at(frames, i), "page") == p) return geti(json_at(frames, i), "count");
    return -1;
}

static void test_clock_hand_by_hand(void) {
    JVal *v = sim(ALG_CLOCK, 3, "1 2 3 1 4 5");
    CHECK(v != NULL);
    const JVal *s5 = step(v, 5);
    CHECK_EQ_STR(json_str(json_get(s5, "why"), ""), "fault_evict_clock");
    const JVal *cleared = json_get(json_get(s5, "why_args"), "cleared");
    CHECK_EQ_INT(json_len(cleared), 3);
    CHECK_EQ_INT(json_num(json_at(cleared, 0), -1), 1);
    CHECK_EQ_INT(json_num(json_at(cleared, 1), -1), 2);
    CHECK_EQ_INT(json_num(json_at(cleared, 2), -1), 3);
    CHECK_EQ_INT(geti(json_get(s5, "victim"), "page"), 1);
    CHECK_EQ_INT(geti(s5, "hand"), 1);
    CHECK_EQ_INT(geti(json_get(step(v, 6), "victim"), "page"), 2);
    CHECK_EQ_INT(geti(step(v, 6), "hand"), 2);
    json_free(v);
}

static void test_clock_hit_does_not_move_hand(void) {
    JVal *v = sim(ALG_CLOCK, 3, "1 2 3 1");
    CHECK_EQ_INT(geti(step(v, 4), "hand"), 0);
    json_free(v);
}

static void test_lfu_counts_and_ties(void) {
    JVal *v = sim(ALG_LFU, 3, "1 1 2 3 4");
    CHECK_EQ_INT(geti(json_get(step(v, 5), "victim"), "page"), 2);
    CHECK_EQ_STR(json_str(json_get(step(v, 5), "why"), ""), "fault_evict_lfu");
    CHECK_EQ_INT(count_of(v, 5, 1), 2);
    json_free(v);
}

static void test_lfu_count_resets_after_eviction(void) {
    JVal *v = sim(ALG_LFU, 3, "1 1 2 2 2 3 3 3 4 1");
    CHECK_EQ_INT(geti(json_get(step(v, 9), "victim"), "page"), 1);
    CHECK_EQ_INT(geti(json_get(step(v, 10), "victim"), "page"), 4);
    CHECK_EQ_INT(count_of(v, 10, 1), 1);
    json_free(v);
}

static void test_oracle_agrees(void) {
    unsigned lcg = 12345u;
    int disagreements = 0;
    for (int trial = 0; trial < 3000; trial++) {
        lcg = lcg * 1103515245u + 12345u;
        int len = 1 + (int)((lcg >> 16) % 60);
        lcg = lcg * 1103515245u + 12345u;
        int distinct = 1 + (int)((lcg >> 16) % 9);
        lcg = lcg * 1103515245u + 12345u;
        int frames = 1 + (int)((lcg >> 16) % 6);
        int pages[60];
        Ref refs[60];
        for (int i = 0; i < len; i++) {
            lcg = lcg * 1103515245u + 12345u;
            pages[i] = (int)((lcg >> 16) % (unsigned)distinct);
            refs[i] = (Ref){'R', 0, pages[i]};
        }
        for (int a = 0; a < ALG_COUNT; a++) {
            PagingConfig cfg = {64, frames, 16, 0, (Algorithm)a};
            Totals t;
            char err[160];
            int rc = paging_run(&cfg, refs, (size_t)len, NULL, &t, err, sizeof err);
            int want = oracle_faults((Algorithm)a, frames, pages, (size_t)len);
            if (rc || t.faults != want) {
                if (disagreements++ < 5)
                    fprintf(stderr, "  disagree alg=%s frames=%d len=%d engine=%d oracle=%d\n", alg_name((Algorithm)a),
                            frames, len, rc ? -1 : t.faults, want);
            }
        }
    }
    CHECK_EQ_INT(disagreements, 0);
}

int main(void) {
    RUN(test_clock_hand_by_hand);
    RUN(test_clock_hit_does_not_move_hand);
    RUN(test_lfu_counts_and_ties);
    RUN(test_lfu_count_resets_after_eviction);
    RUN(test_oracle_agrees);
    return test_summary();
}
