#include "../core/json.h"
#include "../core/seg.h"
#include "test.h"

#include <stdlib.h>

static JVal *seg(int memory, Fit fit, const char *ops, char *err) {
    Buf b;
    buf_init(&b);
    buf_putc(&b, '{');
    if (seg_run(memory, fit, ops, &b, err, 160)) {
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

static const JVal *last_step(const JVal *v) {
    const JVal *steps = json_get(v, "steps");
    return json_at(steps, json_len(steps) - 1);
}

#define OPS "alloc A 100; alloc B 200; alloc C 100; alloc D 400; alloc F 100; free B; free D; alloc E 50"

static void test_seg_fits(void) {
    char err[160];
    struct {
        Fit fit;
        int base;
        const char *why;
    } cases[] = {{FIT_FIRST, 100, "seg_first_fit"}, {FIT_BEST, 900, "seg_best_fit"}, {FIT_WORST, 400, "seg_worst_fit"}};
    for (int i = 0; i < 3; i++) {
        JVal *v = seg(1000, cases[i].fit, OPS, err);
        CHECK(v != NULL);
        const JVal *s = last_step(v);
        CHECK_EQ_STR(json_str(json_get(s, "result"), ""), "ok");
        CHECK_EQ_INT(json_num(json_get(s, "base"), -1), cases[i].base);
        CHECK_EQ_STR(json_str(json_get(s, "why"), ""), cases[i].why);
        CHECK_EQ_INT(json_len(json_get(s, "candidates")), 3);
        json_free(v);
    }
}

static void test_seg_free_coalesces(void) {
    char err[160];
    JVal *v = seg(1000, FIT_FIRST, "alloc A 100; alloc B 100; alloc C 100; free A; free C; free B", err);
    const JVal *map = json_get(last_step(v), "map");
    CHECK_EQ_INT(json_len(map), 1);
    CHECK_EQ_INT(json_num(json_get(json_at(map, 0), "size"), 0), 1000);
    CHECK_EQ_STR(json_str(json_get(last_step(v), "why"), ""), "seg_free");
    CHECK_EQ_INT(json_num(json_get(json_get(v, "totals"), "holes"), 0), 1);
    json_free(v);
}

static void test_seg_external_fragmentation(void) {
    char err[160];
    JVal *v = seg(400, FIT_FIRST, "alloc A 100; alloc B 100; alloc C 100; alloc D 100; free A; free C; alloc E 150", err);
    const JVal *s = last_step(v);
    CHECK_EQ_STR(json_str(json_get(s, "result"), ""), "fail");
    CHECK_EQ_STR(json_str(json_get(s, "why"), ""), "seg_fail_fragmentation");
    const JVal *t = json_get(v, "totals");
    CHECK(json_num(json_get(t, "external_fragmentation"), -1) == 0.5);
    CHECK_EQ_INT(json_num(json_get(t, "free"), 0), 200);
    CHECK_EQ_INT(json_num(json_get(t, "largest_hole"), 0), 100);
    CHECK_EQ_INT(json_num(json_get(t, "failures"), 0), 1);
    json_free(v);
    v = seg(100, FIT_FIRST, "alloc A 60; alloc B 60", err);
    CHECK_EQ_STR(json_str(json_get(last_step(v), "why"), ""), "seg_fail_no_space");
    json_free(v);
}

static void test_seg_errors(void) {
    const char *bad[] = {"alloc A 10; alloc A 20", "free Z", "alloc A 0", "alloc A", "grow A 10", "alloc nome-com-traco 5",
                         ""};
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        char err[160] = "";
        CHECK(seg(1000, FIT_FIRST, bad[i], err) == NULL);
        CHECK(err[0] != '\0');
    }
    char err[160] = "";
    seg(1000, FIT_FIRST, "alloc A 10; free Z", err);
    CHECK_CONTAINS(err, "2"); /* op index */
    Fit f;
    CHECK_EQ_INT(fit_parse("best", &f), 0);
    CHECK_EQ_INT(f, FIT_BEST);
    CHECK(fit_parse("next", &f) != 0);
    CHECK(seg(0, FIT_FIRST, "alloc A 1", err) == NULL);
}

int main(void) {
    RUN(test_seg_fits);
    RUN(test_seg_free_coalesces);
    RUN(test_seg_external_fragmentation);
    RUN(test_seg_errors);
    return test_summary();
}
