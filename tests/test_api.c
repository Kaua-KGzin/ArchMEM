#include "../core/archmem.h"
#include "../core/buf.h"
#include "../core/json.h"
#include "test.h"

#include <stdlib.h>

#define S "7 0 1 2 0 3 0 4 2 3 0 3 2 1 2 0 1 7 0 1"
#define B "1 2 3 4 1 2 5 1 2 3 4 5"

static JVal *call(const char *req) {
    char *res = archmem_run(req);
    CHECK(res != NULL);
    char err[128];
    JVal *v = json_parse(res, err, sizeof err);
    if (!v) fprintf(stderr, "response is not JSON: %s\n%.300s\n", err, res);
    archmem_free(res);
    return v;
}

static int ok(const JVal *v) { return json_bool(json_get(v, "ok"), 0); }

static void test_api_run_contract(void) {
    JVal *v = call("{\"action\":\"run\",\"config\":{\"page_size\":64,\"frames\":3,\"virtual_pages\":16,"
                   "\"tlb_entries\":0,\"algorithm\":\"lru\"},\"refs\":\"" S "\"}");
    CHECK(ok(v));
    CHECK_EQ_STR(json_str(json_get(v, "algorithm"), ""), "lru");
    const JVal *t = json_get(v, "totals");
    CHECK_EQ_INT(json_num(json_get(t, "faults"), -1), 12);
    CHECK_EQ_INT(json_num(json_get(t, "hits"), -1), 8);
    CHECK(json_num(json_get(t, "hit_rate"), -1) == 0.4);
    CHECK_EQ_INT(json_len(json_get(v, "steps")), 20);
    CHECK_EQ_INT(json_num(json_get(json_get(v, "config"), "frames"), -1), 3);
    json_free(v);
}

static void test_api_compare(void) {
    JVal *v = call("{\"action\":\"compare\",\"config\":{\"frames\":3},\"algorithms\":[\"fifo\",\"lru\",\"opt\"],"
                   "\"refs\":\"" S "\"}");
    CHECK(ok(v));
    const JVal *r = json_get(v, "results");
    CHECK_EQ_INT(json_len(r), 3);
    CHECK_EQ_STR(json_str(json_get(json_at(r, 0), "algorithm"), ""), "fifo");
    CHECK_EQ_INT(json_num(json_get(json_get(json_at(r, 0), "totals"), "faults"), -1), 15);
    CHECK_EQ_INT(json_num(json_get(json_get(json_at(r, 1), "totals"), "faults"), -1), 12);
    CHECK_EQ_INT(json_num(json_get(json_get(json_at(r, 2), "totals"), "faults"), -1), 9);
    json_free(v);
    v = call("{\"action\":\"compare\",\"config\":{\"frames\":3},\"refs\":\"" S "\"}");
    CHECK_EQ_INT(json_len(json_get(v, "results")), 5);
    json_free(v);
}

static void test_api_curve_belady(void) {
    JVal *v = call("{\"action\":\"curve\",\"config\":{},\"algorithms\":[\"fifo\"],\"max_frames\":5,\"refs\":\"" B "\"}");
    CHECK(ok(v));
    CHECK_EQ_INT(json_len(json_get(v, "frames")), 5);
    const JVal *faults = json_get(json_at(json_get(v, "series"), 0), "faults");
    int want[] = {12, 12, 9, 10, 5};
    for (int i = 0; i < 5; i++) CHECK_EQ_INT(json_num(json_at(faults, (size_t)i), -1), want[i]);
    const JVal *belady = json_get(v, "belady");
    CHECK_EQ_INT(json_len(belady), 1);
    CHECK_EQ_STR(json_str(json_get(json_at(belady, 0), "algorithm"), ""), "fifo");
    CHECK_EQ_INT(json_num(json_get(json_at(belady, 0), "at_frames"), -1), 4);
    json_free(v);
}

static void test_api_errors(void) {
    const char *bad[] = {
        "not json",
        "{}",
        "{\"action\":\"fly\"}",
        "{\"action\":\"run\",\"config\":{\"frames\":\"3\"},\"refs\":\"1 2\"}",
        "{\"action\":\"run\",\"config\":{\"frames\":2.5},\"refs\":\"1 2\"}",
        "{\"action\":\"run\",\"config\":{\"frames\":3}}",
        "{\"action\":\"run\",\"config\":{\"algorithm\":\"random\"},\"refs\":\"1\"}",
        "{\"action\":\"curve\",\"max_frames\":65,\"refs\":\"1\"}",
        "{\"action\":\"compare\",\"algorithms\":[\"lru\",7],\"refs\":\"1\"}",
        "[1,2]",
    };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        JVal *v = call(bad[i]);
        CHECK(v != NULL);
        CHECK(!ok(v));
        CHECK(strlen(json_str(json_get(v, "error"), "")) > 0);
        json_free(v);
    }
    Buf b;
    buf_init(&b);
    buf_puts(&b, "{\"action\":\"run\",\"refs\":\"");
    for (int i = 0; i < 10001; i++) buf_puts(&b, "1 ");
    buf_puts(&b, "\"}");
    char *req = buf_take(&b);
    JVal *v = call(req);
    CHECK(!ok(v));
    CHECK_CONTAINS(json_str(json_get(v, "error"), ""), "10000");
    json_free(v);
    free(req);
    char *res = archmem_run(NULL);
    CHECK_CONTAINS(res, "\"ok\":false");
    archmem_free(res);
}

static void test_api_gen_and_seg_and_challenge(void) {
    JVal *v = call("{\"action\":\"gen\",\"kind\":\"locality\",\"length\":30,\"pages\":10,\"seed\":42}");
    CHECK(ok(v));
    CHECK(strlen(json_str(json_get(v, "refs"), "")) > 30);
    json_free(v);
    v = call("{\"action\":\"seg\",\"memory_size\":1000,\"fit\":\"best\",\"ops\":\"alloc A 300; free A\"}");
    CHECK(ok(v));
    CHECK_EQ_INT(json_len(json_get(v, "steps")), 2);
    CHECK_EQ_INT(json_num(json_get(json_get(v, "totals"), "free"), -1), 1000);
    json_free(v);
    v = call("{\"action\":\"challenge\",\"seed\":7,\"difficulty\":2}");
    CHECK(ok(v));
    CHECK(json_type(json_get(json_get(v, "challenge"), "answer")) != J_NULL);
    json_free(v);
    v = call("{\"action\":\"version\"}");
    CHECK_EQ_STR(json_str(json_get(v, "version"), ""), ARCHMEM_VERSION);
    json_free(v);
}

int main(void) {
    RUN(test_api_run_contract);
    RUN(test_api_compare);
    RUN(test_api_curve_belady);
    RUN(test_api_errors);
    RUN(test_api_gen_and_seg_and_challenge);
    return test_summary();
}
