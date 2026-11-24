#include "../core/json.h"
#include "test.h"

#include <stdlib.h>

static void test_parse_nested(void) {
    char err[128] = "";
    JVal *v = json_parse("{\"a\":[{\"b\":1.5},{\"b\":-2}],\"s\":\"caf\\u00e9\",\"t\":true,\"n\":null}", err, sizeof err);
    CHECK(v != NULL);
    CHECK_EQ_INT(json_type(v), J_OBJ);
    const JVal *a = json_get(v, "a");
    CHECK_EQ_INT(json_len(a), 2);
    CHECK(json_num(json_get(json_at(a, 0), "b"), 0) == 1.5);
    CHECK(json_num(json_get(json_at(a, 1), "b"), 0) == -2);
    CHECK_EQ_STR(json_str(json_get(v, "s"), NULL), "caf\xc3\xa9");
    CHECK_EQ_INT(json_bool(json_get(v, "t"), 0), 1);
    CHECK_EQ_INT(json_type(json_get(v, "n")), J_NULL);
    CHECK(json_get(v, "missing") == NULL);
    CHECK_EQ_STR(json_str(json_get(v, "missing"), "def"), "def");
    json_free(v);
}

static void test_parse_errors(void) {
    const char *bad[] = {"{\"a\":", "[1,2,]", "{\"a\" 1}", "tru", "\"unterminated", "{} x", ""};
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        char err[128] = "";
        JVal *v = json_parse(bad[i], err, sizeof err);
        CHECK(v == NULL);
        CHECK(err[0] != '\0');
    }
    char deep[200];
    int i = 0;
    for (; i < 65; i++) deep[i] = '[';
    for (int j = 0; j < 65; j++) deep[i++] = ']';
    deep[i] = '\0';
    char err[128] = "";
    CHECK(json_parse(deep, err, sizeof err) == NULL);
    CHECK_CONTAINS(err, "profund");
}

int main(void) {
    RUN(test_parse_nested);
    RUN(test_parse_errors);
    return test_summary();
}
