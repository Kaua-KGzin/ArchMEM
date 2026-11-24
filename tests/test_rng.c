#include "../core/rng.h"
#include "test.h"

#include <stdlib.h>

static void test_rng_fixed_sequence(void) {
    Rng r;
    rng_seed(&r, 42);
    CHECK(rng_next(&r) == 11355432u);
    CHECK(rng_next(&r) == 2836018348u);
    CHECK(rng_next(&r) == 476557059u);
    rng_seed(&r, 0);
    CHECK(rng_next(&r) != 0);
    for (int i = 0; i < 1000; i++) {
        int v = rng_range(&r, 3, 7);
        CHECK(v >= 3 && v <= 7);
    }
}

static void test_generators_deterministic_and_bounded(void) {
    const char *kinds[] = {"uniform", "locality", "loop", "sequential"};
    for (int k = 0; k < 4; k++) {
        GenKind kind;
        CHECK_EQ_INT(gen_parse(kinds[k], &kind), 0);
        Ref *a = NULL, *b = NULL;
        size_t na = 0, nb = 0;
        char err[128];
        CHECK_EQ_INT(gen_refs(kind, 50, 10, 7, 0.0, &a, &na, err, sizeof err), 0);
        CHECK_EQ_INT(gen_refs(kind, 50, 10, 7, 0.0, &b, &nb, err, sizeof err), 0);
        CHECK_EQ_INT(na, 50);
        CHECK_EQ_INT(nb, 50);
        int same = 1, in_range = 1;
        for (size_t i = 0; i < na; i++) {
            if (a[i].value != b[i].value) same = 0;
            if (a[i].value < 0 || a[i].value > 9 || a[i].is_address || a[i].op != 'R') in_range = 0;
        }
        CHECK(same);
        CHECK(in_range);
        free(a);
        free(b);
        CHECK_EQ_INT(gen_refs(kind, 30, 10, 7, 1.0, &a, &na, err, sizeof err), 0);
        int all_w = 1;
        for (size_t i = 0; i < na; i++)
            if (a[i].op != 'W') all_w = 0;
        CHECK(all_w);
        free(a);
    }
    GenKind kind;
    CHECK(gen_parse("bogus", &kind) != 0);
    Ref *r = NULL;
    size_t n = 0;
    char err[128];
    CHECK(gen_refs(GEN_UNIFORM, 0, 10, 1, 0, &r, &n, err, sizeof err) != 0);
    CHECK(gen_refs(GEN_UNIFORM, 10, 0, 1, 0, &r, &n, err, sizeof err) != 0);
    CHECK(gen_refs(GEN_UNIFORM, 10001, 10, 1, 0, &r, &n, err, sizeof err) != 0);
    CHECK(gen_refs(GEN_UNIFORM, 10, 10, 1, 1.5, &r, &n, err, sizeof err) != 0);
    /* the loop and sequential patterns are what their names say */
    CHECK_EQ_INT(gen_refs(GEN_LOOP, 7, 3, 1, 0, &r, &n, err, sizeof err), 0);
    CHECK_EQ_INT(r[3].value, 0);
    CHECK_EQ_INT(r[4].value, 1);
    free(r);
}

int main(void) {
    RUN(test_rng_fixed_sequence);
    RUN(test_generators_deterministic_and_bounded);
    return test_summary();
}
