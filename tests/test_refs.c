#include "../core/refs.h"
#include "test.h"

#include <stdlib.h>

static void test_refs_roundtrip(void) {
    Ref *r = NULL;
    size_t n = 0;
    char err[128] = "";
    CHECK_EQ_INT(refs_parse("7 0 1 2w @130 @0x82w", &r, &n, err, sizeof err), 0);
    CHECK_EQ_INT(n, 6);
    CHECK_EQ_INT(r[0].value, 7);
    CHECK_EQ_INT(r[0].op, 'R');
    CHECK_EQ_INT(r[3].op, 'W');
    CHECK_EQ_INT(r[3].is_address, 0);
    CHECK_EQ_INT(r[4].is_address, 1);
    CHECK_EQ_INT(r[4].value, 130);
    CHECK_EQ_INT(r[4].op, 'R');
    CHECK_EQ_INT(r[5].is_address, 1);
    CHECK_EQ_INT(r[5].value, 130);
    CHECK_EQ_INT(r[5].op, 'W');
    Buf b;
    buf_init(&b);
    refs_format(r, n, &b);
    char *s = buf_take(&b);
    CHECK_EQ_STR(s, "7 0 1 2w @130 @130w");
    free(s);
    free(r);

    CHECK_EQ_INT(refs_parse("1,2\n3\t4W", &r, &n, err, sizeof err), 0);
    CHECK_EQ_INT(n, 4);
    CHECK_EQ_INT(r[3].op, 'W');
    free(r);
}

static void test_refs_errors(void) {
    Ref *r = NULL;
    size_t n = 0;
    char err[128] = "";
    CHECK(refs_parse("7 x 1", &r, &n, err, sizeof err) != 0);
    CHECK_CONTAINS(err, "\"x\"");
    CHECK_CONTAINS(err, "2");
    const char *bad[] = {"-3", "@0xZZ", "", "   ", "@", "5ww", "99999999999"};
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        err[0] = '\0';
        r = NULL;
        CHECK(refs_parse(bad[i], &r, &n, err, sizeof err) != 0);
        CHECK(err[0] != '\0');
        CHECK(r == NULL);
    }
}

int main(void) {
    RUN(test_refs_roundtrip);
    RUN(test_refs_errors);
    return test_summary();
}
