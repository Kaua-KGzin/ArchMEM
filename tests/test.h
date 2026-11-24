/* Tiny test harness: CHECK* record failures, RUN runs a test, test_summary
 * prints the result and returns the process exit code. */
#ifndef ARCHMEM_TEST_H
#define ARCHMEM_TEST_H

#include <stdio.h>
#include <string.h>

static int test_failures = 0;
static int test_checks = 0;
static const char *test_current = "";

#define CHECK(cond)                                                              \
    do {                                                                         \
        test_checks++;                                                           \
        if (!(cond)) {                                                           \
            test_failures++;                                                     \
            fprintf(stderr, "  FAIL %s:%d [%s] %s\n", __FILE__, __LINE__,        \
                    test_current, #cond);                                        \
        }                                                                        \
    } while (0)

#define CHECK_EQ_INT(a, b)                                                       \
    do {                                                                         \
        long long va_ = (long long)(a), vb_ = (long long)(b);                    \
        test_checks++;                                                           \
        if (va_ != vb_) {                                                        \
            test_failures++;                                                     \
            fprintf(stderr, "  FAIL %s:%d [%s] %s == %s (%lld != %lld)\n",       \
                    __FILE__, __LINE__, test_current, #a, #b, va_, vb_);         \
        }                                                                        \
    } while (0)

#define CHECK_EQ_STR(a, b)                                                       \
    do {                                                                         \
        const char *sa_ = (a), *sb_ = (b);                                       \
        test_checks++;                                                           \
        if (!sa_ || !sb_ || strcmp(sa_, sb_) != 0) {                             \
            test_failures++;                                                     \
            fprintf(stderr, "  FAIL %s:%d [%s] %s == %s\n    got:  %s\n    want: %s\n", \
                    __FILE__, __LINE__, test_current, #a, #b,                    \
                    sa_ ? sa_ : "(null)", sb_ ? sb_ : "(null)");                 \
        }                                                                        \
    } while (0)

/* CHECK_CONTAINS: haystack contains needle. */
#define CHECK_CONTAINS(hay, needle)                                              \
    do {                                                                         \
        const char *h_ = (hay), *n_ = (needle);                                  \
        test_checks++;                                                           \
        if (!h_ || !strstr(h_, n_)) {                                            \
            test_failures++;                                                     \
            fprintf(stderr, "  FAIL %s:%d [%s] expected to contain %s\n    in: %.400s\n", \
                    __FILE__, __LINE__, test_current, n_, h_ ? h_ : "(null)");   \
        }                                                                        \
    } while (0)

#define RUN(fn)                                                                  \
    do {                                                                         \
        test_current = #fn;                                                      \
        fn();                                                                    \
    } while (0)

static int test_summary(void) {
    if (test_failures) {
        fprintf(stderr, "%s: %d of %d checks FAILED\n", __FILE__, test_failures, test_checks);
        return 1;
    }
    printf("ok %d checks\n", test_checks);
    return 0;
}

#endif
