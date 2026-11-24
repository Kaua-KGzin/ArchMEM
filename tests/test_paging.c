#include "../core/json.h"
#include "../core/paging.h"
#include "test.h"

#include <stdlib.h>

#define S "7 0 1 2 0 3 0 4 2 3 0 3 2 1 2 0 1 7 0 1"
#define B "1 2 3 4 1 2 5 1 2 3 4 5"

/* Runs refs and returns parsed {"steps":[...]} plus totals; NULL + err on failure. */
static JVal *sim(Algorithm alg, int frames, int pages, int page_size, int tlb, const char *refs_s,
                 Totals *t, char *err) {
    Ref *refs = NULL;
    size_t n = 0;
    if (refs_parse(refs_s, &refs, &n, err, 160)) return NULL;
    PagingConfig cfg = {page_size, frames, pages, tlb, alg};
    Buf b;
    buf_init(&b);
    buf_puts(&b, "{\"steps\":");
    int rc = paging_run(&cfg, refs, n, &b, t, err, 160);
    free(refs);
    if (rc) {
        buf_free(&b);
        return NULL;
    }
    buf_putc(&b, '}');
    char *s = buf_take(&b);
    char jerr[128];
    JVal *v = json_parse(s, jerr, sizeof jerr);
    if (!v) fprintf(stderr, "bad json: %s\n%s\n", jerr, s);
    free(s);
    return v;
}

static int faults(Algorithm alg, int frames, const char *refs_s) {
    Ref *refs = NULL;
    size_t n = 0;
    char err[160];
    if (refs_parse(refs_s, &refs, &n, err, sizeof err)) return -1;
    PagingConfig cfg = {64, frames, 16, 0, alg};
    Totals t;
    int rc = paging_run(&cfg, refs, n, NULL, &t, err, sizeof err);
    free(refs);
    return rc ? -1 : t.faults;
}

static const JVal *step(const JVal *v, int k) { return json_at(json_get(v, "steps"), (size_t)(k - 1)); }
static int geti(const JVal *o, const char *key) { return (int)json_num(json_get(o, key), -999); }

static void test_silberschatz_3_frames(void) {
    CHECK_EQ_INT(faults(ALG_FIFO, 3, S), 15);
    CHECK_EQ_INT(faults(ALG_LRU, 3, S), 12);
    CHECK_EQ_INT(faults(ALG_OPT, 3, S), 9);
}

static void test_belady_fifo(void) {
    CHECK_EQ_INT(faults(ALG_FIFO, 3, B), 9);
    CHECK_EQ_INT(faults(ALG_FIFO, 4, B), 10);
}

static void test_first_steps_events(void) {
    char err[160];
    Totals t;
    JVal *v = sim(ALG_LRU, 3, 16, 64, 0, S, &t, err);
    CHECK(v != NULL);
    CHECK_EQ_INT(json_len(json_get(v, "steps")), 20);
    const JVal *s1 = step(v, 1);
    CHECK_EQ_STR(json_str(json_get(s1, "result"), ""), "fault");
    CHECK_EQ_STR(json_str(json_get(s1, "why"), ""), "fault_free_frame");
    CHECK_EQ_INT(geti(s1, "frame"), 0);
    CHECK_EQ_STR(json_str(json_get(s1, "tlb"), ""), "off");
    CHECK(json_type(json_get(s1, "victim")) == J_NULL);
    CHECK_EQ_INT(json_len(json_get(s1, "frames")), 3);
    const JVal *s4 = step(v, 4);
    CHECK_EQ_INT(geti(json_get(s4, "victim"), "page"), 7);
    CHECK_EQ_STR(json_str(json_get(s4, "why"), ""), "fault_evict_lru");
    CHECK_EQ_INT(geti(json_get(s4, "why_args"), "last"), 1);
    const JVal *s5 = step(v, 5);
    CHECK_EQ_STR(json_str(json_get(s5, "result"), ""), "hit");
    CHECK_EQ_STR(json_str(json_get(s5, "why"), ""), "hit");
    CHECK_EQ_INT(t.hits, 8);
    CHECK_EQ_INT(t.accesses, 20);
    json_free(v);
}

static void test_opt_tie_lowest_page(void) {
    char err[160];
    Totals t;
    JVal *v = sim(ALG_OPT, 3, 16, 64, 0, "1 2 3 4", &t, err);
    CHECK_EQ_INT(geti(json_get(step(v, 4), "victim"), "page"), 1);
    CHECK_EQ_INT(geti(json_get(step(v, 4), "why_args"), "next_use"), -1);
    json_free(v);
}

static void test_no_replacement_when_enough_frames(void) {
    char err[160];
    Totals t;
    JVal *v = sim(ALG_LRU, 6, 16, 64, 0, S, &t, err);
    CHECK_EQ_INT(t.replacements, 0);
    for (int k = 1; k <= 20; k++) CHECK(json_type(json_get(step(v, k), "victim")) == J_NULL);
    json_free(v);
}

static void test_dirty_writeback(void) {
    char err[160];
    Totals t;
    JVal *v = sim(ALG_FIFO, 3, 16, 64, 0, "1w 2 3 1 4", &t, err);
    const JVal *victim = json_get(step(v, 5), "victim");
    CHECK_EQ_INT(geti(victim, "page"), 1);
    CHECK_EQ_INT(json_bool(json_get(victim, "dirty"), 0), 1);
    CHECK_EQ_INT(t.writebacks, 1);
    json_free(v);
    v = sim(ALG_FIFO, 3, 16, 64, 0, "1 1w 2 3 4", &t, err);
    CHECK_EQ_INT(t.writebacks, 1);
    json_free(v);
}

static void test_address_decomposition(void) {
    char err[160];
    Totals t;
    JVal *v = sim(ALG_FIFO, 3, 16, 64, 0, "@130 5", &t, err);
    CHECK_EQ_INT(geti(step(v, 1), "page"), 2);
    CHECK_EQ_INT(geti(step(v, 1), "offset"), 2);
    CHECK_EQ_INT(geti(step(v, 1), "address"), 130);
    CHECK_EQ_INT(geti(step(v, 2), "address"), 320); /* page 5 * 64, offset 0 */
    json_free(v);
}

static void test_tlb_hits_and_invalidation(void) {
    char err[160];
    Totals t;
    JVal *v = sim(ALG_LRU, 2, 16, 64, 2, "1 1 2 3 1", &t, err);
    CHECK_EQ_STR(json_str(json_get(step(v, 1), "tlb"), ""), "miss");
    CHECK_EQ_STR(json_str(json_get(step(v, 2), "tlb"), ""), "hit");
    CHECK_EQ_STR(json_str(json_get(step(v, 2), "why"), ""), "hit_tlb");
    /* step 4 (page 3) evicts page 1, which must leave the TLB */
    const JVal *tlb4 = json_get(step(v, 4), "tlb_pages");
    for (size_t i = 0; i < json_len(tlb4); i++) CHECK(json_num(json_at(tlb4, i), -1) != 1);
    CHECK_EQ_INT(t.tlb_hits, 1);
    json_free(v);
}

static void test_tlb_invalidated_on_evict(void) {
    char err[160];
    Totals t;
    JVal *v = sim(ALG_LRU, 2, 16, 64, 2, "1 1 2 3 1", &t, err);
    CHECK_EQ_STR(json_str(json_get(step(v, 5), "tlb"), ""), "miss");
    CHECK_EQ_STR(json_str(json_get(step(v, 5), "result"), ""), "fault");
    json_free(v);
}

static void test_out_of_range(void) {
    char err[160] = "";
    Totals t;
    CHECK(sim(ALG_FIFO, 3, 8, 64, 0, "9", &t, err) == NULL);
    CHECK_CONTAINS(err, "1");
    CHECK_CONTAINS(err, "9");
    err[0] = '\0';
    CHECK(sim(ALG_FIFO, 3, 8, 64, 0, "1 2 @600", &t, err) == NULL);
    CHECK_CONTAINS(err, "3");
    CHECK_CONTAINS(err, "600");
}

static void test_config_limits(void) {
    char err[160];
    PagingConfig bad[] = {
        {48, 3, 16, 0, ALG_FIFO},   {64, 0, 16, 0, ALG_FIFO},  {64, 65, 128, 0, ALG_FIFO},
        {64, 3, 16, 65, ALG_FIFO},  {64, 3, 4097, 0, ALG_FIFO}, {8, 3, 16, 0, ALG_FIFO},
        {131072, 3, 16, 0, ALG_FIFO}, {64, 3, 0, 0, ALG_FIFO},
    };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) CHECK(paging_config_check(&bad[i], err, sizeof err) != 0);
    PagingConfig ok = {64, 3, 16, 4, ALG_LRU};
    CHECK_EQ_INT(paging_config_check(&ok, err, sizeof err), 0);
    Algorithm a;
    CHECK_EQ_INT(alg_parse("LRU", &a), 0);
    CHECK_EQ_INT(a, ALG_LRU);
    CHECK(alg_parse("random", &a) != 0);
    CHECK_EQ_STR(alg_name(ALG_CLOCK), "clock");
}

int main(void) {
    RUN(test_silberschatz_3_frames);
    RUN(test_belady_fifo);
    RUN(test_first_steps_events);
    RUN(test_opt_tie_lowest_page);
    RUN(test_no_replacement_when_enough_frames);
    RUN(test_dirty_writeback);
    RUN(test_address_decomposition);
    RUN(test_tlb_hits_and_invalidation);
    RUN(test_tlb_invalidated_on_evict);
    RUN(test_out_of_range);
    RUN(test_config_limits);
    return test_summary();
}
