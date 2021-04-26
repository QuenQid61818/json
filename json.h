/* -*- encoding: utf-8; -*- */
/* -*- c-mode -*- */
/* File-name:    <json.h> */
/* Author:       <Xsoda> */
/* Create:       <Wednesday November 20 23:12:51 2013> */
/* Time-stamp:   <Wednesday November 20, 14:5:31 2013> */
/* Mail:         <Xsoda@Live.com> */

#ifndef __JSON_H__
#define __JSON_H__

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
   JSON_NONE = -1,
   JSON_OBJECT,
   JSON_ARRAY,
   JSON_STRING,
   JSON_INTEGER,
   JSON_REAL,
   JSON_TRUE,
   JSON_FALSE,
   JSON_NULL,
} json_type;

typedef struct _json_t json_t;
typedef struct _json_iter_t json_iter_t;
json_type json_typeof(json_t *json);
#define json_is_object(json)   (json_typeof(json) == JSON_OBJECT)
#define json_is_array(json)    (json_typeof(json) == JSON_ARRAY)
#define json_is_string(json)   (json_typeof(json) == JSON_STRING)
#define json_is_integer(json)  (json_typeof(json) == JSON_INTEGER)
#define json_is_real(json)     (json_typeof(json) == JSON_REAL)
#define json_is_number(json)   (json_is_integer(json) || json_is_real(json))
#define json_is_true(json)     (json_typeof(json) == JSON_TRUE)
#define json_is_false(json)    (json_typeof(json) == JSON_FALSE)
#define json_is_boolean(json)  (json_is_true(json) || json_is_false(json))
#define json_is_null(json)     (json_typeof(json) == JSON_NULL)

json_t *json_object(void);
json_t *json_array(void);
json_t *json_string(const char *value);
json_t *json_stringn(const char *value, int len);
json_t *json_integer(long long value);
json_t *json_real(double value);
json_t *json_true(void);
json_t *json_false(void);
json_t *json_null(void);
#define json_boolean(val)      ((val) ? json_true() : json_false())

json_t *json_loadf(const char *file);
json_t *json_loads(const char *str);

int json_dumpf(json_t *json, const char *file);
json_t *json_dumps(json_t *json);

json_t *json_object_get(json_t *json, const char *key);
void json_object_set(json_t *json, const char *key, json_t *value);
void json_object_del(json_t *json, const char *key);
void json_object_clear(json_t *json);

json_t *json_array_at(json_t *json, int index);
void json_array_append(json_t *json, json_t *value);
void json_array_prepand(json_t *json, json_t *value);
void json_array_clear(json_t *json);
void json_array_del(json_t *json, int index);

void json_delete(json_t *json);

long long json_integer_value(json_t *json);
double json_real_value(json_t *json);
const char *json_string_value(json_t *json);
bool json_boolean_value(json_t *json);
void json_string_set(json_t *json, const char *str, int len);
void json_string_append(json_t *json, const char *str, int len);
void json_string_append_byte(json_t *json, char c);
json_iter_t *json_iter_new(json_t *json);
json_t *json_iter_next(json_iter_t *iter);
void json_iter_delete(json_iter_t *iter);

#ifdef __cplusplus
}
#endif

#endif
