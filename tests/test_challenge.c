#include "../core/challenge.h"
#include "../core/json.h"
#include "../core/paging.h"
#include "test.h"

#include <stdlib.h>

static char *make(unsigned seed, int difficulty) {
    Buf b;
    buf_init(&b);
    char err[160];
    if (challenge_make(seed, difficulty, &b, err, sizeof err)) {
        fprintf(stderr, "challenge_make: %s\n", err);
        buf_free(&b);
        return NULL;
    }
    return buf_take(&b);
}

/* Re-runs a scenario; returns totals and, for step k, the victim page. */
static int replay(const JVal *scenario, Algorithm alg, int step_k, int *victim) {
    const JVal *cfgj = json_get(scenario, "config");
    PagingConfig cfg = {(int)json_num(json_get(cfgj, "page_size"), 64), (int)json_num(json_get(cfgj, "frames"), 0),
                        (int)json_num(json_get(cfgj, "virtual_pages"), 0), (int)json_num(json_get(cfgj, "tlb_entries"), 0),
                        alg};
    Ref *refs = NULL;
    size_t n = 0;
    char err[160];
    if (refs_parse(json_str(json_get(scenario, "refs"), ""), &refs, &n, err, sizeof err)) return -1;
    Buf b;
    buf_init(&b);
    Totals t;
    int rc = paging_run(&cfg, refs, n, victim ? &b : NULL, &t, err, sizeof err);
    free(refs);
    if (rc) return -1;
    if (victim) {
        char *s = buf_take(&b);
        JVal *steps = json_parse(s, err, sizeof err);
        free(s);
        *victim = (int)json_num(json_get(json_get(json_at(steps, (size_t)(step_k - 1)), "victim"), "page"), -1);
        json_free(steps);
    }
    return t.faults;
}

static void test_challenge_answers_match_engine(void) {
    int mismatches = 0;
    for (unsigned seed = 1; seed <= 200; seed++) {
        for (int d = 1; d <= 3; d++) {
            char *s = make(seed, d);
            char *again = make(seed, d);
            CHECK(s && again && strcmp(s, again) == 0);
            char err[128];
            JVal *c = json_parse(s, err, sizeof err);
            const char *kind = json_str(json_get(c, "kind"), "");
            const JVal *params = json_get(c, "params");
            const JVal *scenario = json_get(c, "scenario");
            Algorithm alg = ALG_FIFO;
            alg_parse(json_str(json_get(json_get(scenario, "config"), "algorithm"), ""), &alg);
            if (strcmp(kind, "faults") == 0) {
                if (replay(scenario, alg, 0, NULL) != (int)json_num(json_get(c, "answer"), -1)) mismatches++;
            } else if (strcmp(kind, "victim") == 0) {
                int victim = -2;
                replay(scenario, alg, (int)json_num(json_get(params, "step"), 0), &victim);
                if (victim < 0 || victim != (int)json_num(json_get(c, "answer"), -1)) mismatches++;
            } else if (strcmp(kind, "best_algorithm") == 0) {
                const char *answer = json_str(json_get(c, "answer"), "");
                Algorithm best;
                if (alg_parse(answer, &best)) mismatches++;
                int bf = replay(scenario, best, 0, NULL);
                const Algorithm cands[] = {ALG_FIFO, ALG_LRU, ALG_CLOCK, ALG_LFU};
                for (int i = 0; i < 4; i++)
                    if (cands[i] != best && replay(scenario, cands[i], 0, NULL) <= bf) mismatches++;
            } else {
                mismatches++;
            }
            if (d == 1) CHECK_EQ_STR(kind, "faults");
            json_free(c);
            free(s);
            free(again);
        }
    }
    CHECK_EQ_INT(mismatches, 0);
    Buf b;
    buf_init(&b);
    char err[160];
    CHECK(challenge_make(1, 0, &b, err, sizeof err) != 0);
    CHECK(challenge_make(1, 4, &b, err, sizeof err) != 0);
    buf_free(&b);
}

int main(void) {
    RUN(test_challenge_answers_match_engine);
    return test_summary();
}
