/* Minimal JSON DOM parser for ArchMEM requests. */
#ifndef ARCHMEM_JSON_H
#define ARCHMEM_JSON_H

#include <stddef.h>

typedef enum { J_NULL, J_BOOL, J_NUM, J_STR, J_ARR, J_OBJ } JType;
typedef struct JVal JVal;

#define JSON_MAX_DEPTH 64
#define JSON_MAX_INPUT (4u * 1024u * 1024u)

/* Returns NULL and a pt-BR message in err on failure. */
JVal *json_parse(const char *text, char *err, size_t errlen);
void json_free(JVal *v);

JType json_type(const JVal *v);             /* J_NULL for NULL */
const JVal *json_get(const JVal *obj, const char *key);
size_t json_len(const JVal *v);             /* array/object length, else 0 */
const JVal *json_at(const JVal *arr, size_t i);
double json_num(const JVal *v, double def); /* def unless v is a number */
const char *json_str(const JVal *v, const char *def);
int json_bool(const JVal *v, int def);

#endif
