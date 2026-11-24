#include "../core/buf.h"
#include "test.h"

#include <stdlib.h>

static void test_json_escaping(void) {
    Buf b;
    buf_init(&b);
    buf_json_str(&b, "a\"b\\c\n\x01");
    char *s = buf_take(&b);
    CHECK_EQ_STR(s, "\"a\\\"b\\\\c\\n\\u0001\"");
    free(s);
}

static void test_json_numbers(void) {
    Buf b;
    buf_init(&b);
    buf_json_num(&b, 3);
    buf_puts(&b, ",");
    buf_json_num(&b, 0.4);
    buf_puts(&b, ",");
    buf_json_num(&b, 0.33333);
    buf_puts(&b, ",");
    buf_json_num(&b, -2);
    buf_puts(&b, ",");
    buf_json_num(&b, 0.5);
    char *s = buf_take(&b);
    CHECK_EQ_STR(s, "3,0.4,0.3333,-2,0.5");
    free(s);
}

static void test_printf_grows(void) {
    Buf b;
    buf_init(&b);
    for (int i = 0; i < 2000; i++) buf_printf(&b, "%d,", i);
    char *s = buf_take(&b);
    CHECK(strlen(s) > 8000);
    CHECK(strncmp(s, "0,1,2,", 6) == 0);
    free(s);
}

int main(void) {
    RUN(test_json_escaping);
    RUN(test_json_numbers);
    RUN(test_printf_grows);
    return test_summary();
}
