/* -*- encoding: utf-8; -*- */
/* -*- c-mode -*- */
/* File-name:    <json.c> */
/* Author:       <Xsoda> */
/* Create:       <Wednesday November 20 23:13:25 2013> */
/* Time-stamp:   <Thursday November 21, 10:47:33 2013> */
/* Mail:         <Xsoda@Live.com> */
#define _CRT_SECURE_NO_WARNINGS
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>
#include "json.h"
#include "list.h"
#include "avl-inl.h"

typedef struct _pair_t pair_t;
typedef struct _string_t string_t;
typedef struct _json_parser_t json_parser_t;
#ifdef _MSC_VER
#define FUNC_ATTR static __inline
#else
#define FUNC_ATTR static inline
#endif

struct _json_parser_t
{
   const char *pos;
   unsigned long lineno;
   char *errmsg;
};

struct _pair_t
{
   json_t *key;
   json_t *value;
   struct avl_node entry;
};

struct _json_t
{
   json_type type;
   union
   {
      struct avl_tree object;
      list_head(, _json_t) array;
      long long integer;
      double real;
      struct 
      {
         char *ptr;
         int length;
         int size;
      } string;
   };
   list_entry(_json_t) entry;
};

typedef enum { JSON_ITER_ARRAY, JSON_ITER_OBJECT } json_iter;
struct _json_iter_t
{
   json_iter type;
   void *next;
   json_t *json;
};

int object_cmp(const struct avl_node *a, const struct avl_node *b)
{
   pair_t *pa, *pb;
   pa = container_of(a, pair_t, entry);
   pb = container_of(b, pair_t, entry);
   return strcmp(pa->key->string.ptr, pb->key->string.ptr);
}

#define json_init(value, _type)                     \
   do {                                             \
      memset((value), 0, sizeof(json_t));           \
      (value)->type = _type;                        \
   } while (0)

void json_string_append_byte(json_t *json, char c)
{
   if (!json_is_string(json)) return;
   if (json->string.size == json->string.length + 1)
   {
      json->string.size *= 2;
      json->string.ptr = (char *)realloc(json->string.ptr, json->string.size);
      if (!json->string.ptr) return;
   }
   json->string.ptr[json->string.length++] = c;
   json->string.ptr[json->string.length] = 0;
}
void json_string_set(json_t *json, const char *str, int len)
{
   if (!json_is_string(json) || str == NULL) return;
   if (len == -1) len = strlen(str);
   if (len < json->string.size)
   {
      json->string.ptr = (char *)realloc(json->string.ptr, len + 1);
      json->string.size = len + 1;
   }
   json->string.length = len;
   memcpy(json->string.ptr, str, len);
   json->string.ptr[json->string.length + 1] = 0;
}
void json_string_append(json_t *json, const char *str, int len)
{
   if (!json_is_string(json)) return;
   if (len == -1) len = strlen(str);
   while (len >= json->string.size - json->string.length)
   {
      json->string.size *= 2;
      json->string.ptr = (char *)realloc(json->string.ptr, json->string.size);
      if (!json->string.ptr) return;
   }
   memcpy(json->string.ptr + json->string.length, str, len);
   json->string.length += len;
   json->string.ptr[json->string.length] = 0;
}

FUNC_ATTR json_t *json_parse(json_parser_t *parser);

FUNC_ATTR int hex_value(char c)
{
   if (c >= '0' && c <= '9') return c - '0';
   if (c >= 'a' && c <= 'f') return c - 'a' + 0x0a;
   if (c >= 'A' && c <= 'F') return c - 'A' + 0x0a;
   return -1;
}

FUNC_ATTR void json_parser_error(json_parser_t *parser, const char *fmt, ...)
{
   va_list ap;
   fprintf(stderr, "json parse error [line %d]: ", parser->lineno);
   va_start(ap, fmt);
   fprintf(stderr, fmt, ap);
   va_end(ap);
   fprintf(stderr, "\n");
}

FUNC_ATTR void skip_space(json_parser_t *parser)
{
   while (isspace(parser->pos[0]))
   {
      if (parser->pos[0] == '\n')
      {
         parser->lineno++;
      }
      else if (parser->pos[0] == '\r')
      {
         if (parser->pos[1] == '\n')
            parser->pos++;
         parser->lineno++;
      }
      parser->pos++;
   }
}

FUNC_ATTR void skip_comment(json_parser_t *parser)
{
   if (parser->pos[0] == '/' && parser->pos[1] == '*')
   {
      parser->pos += 2;

      if (parser->pos[0] == 0)
      {
         json_parser_error(parser, "unterminated comment");
         return;
      }

      while (parser->pos[0] != '*' && parser->pos[1] != '/')
      {
         if (parser->pos[1] == 0)
         {
            json_parser_error(parser, "unterminated comment");
            return;
         }
         if (parser->pos[0] == '\n')
         {
            parser->lineno++;
            parser->pos++;
         }
         else if (parser->pos[0] == '\r')
         {
            if (parser->pos[1] == '\n')
               parser->pos++;
            parser->lineno++;
            parser->pos++;
         }
         parser->pos++;
      }
      parser->pos += 2;
   }
   else if (parser->pos[0] == '/' && parser->pos[1] == '/')
   {
      parser->pos += 2;
      while (parser->pos[0] != '\r' || parser->pos[0] != '\n')
      {
         if (parser->pos[0] == '\n')
         {
            parser->lineno++;
            parser->pos++;
            break;
         }
         else if (parser->pos[0] == '\r')
         {
            if (parser->pos[1] == '\n')
               parser->pos++;
            parser->lineno++;
            parser->pos++;
            break;
         }
         if (parser->pos[0] == 0)
            break;
         parser->pos++;
      }
   }
}
json_t *json_object(void)
{
   json_t *value;
   if ((value = (json_t *)malloc(sizeof(json_t))) == NULL)
      return NULL;
   json_init(value, JSON_OBJECT);
   avl_init(&value->object);
   return value;
}

json_type json_typeof(json_t *json)
{
   return json == NULL ? JSON_NONE : json->type;
}
json_t *json_array(void)
{
   json_t *value;
   if ((value = (json_t *)malloc(sizeof(json_t))) == NULL)
      return NULL;
   json_init(value, JSON_ARRAY);
   list_init(&value->array);
   return value;
}

json_t *json_string(const char *cstr)
{
   json_t *value;
   if ((value = (json_t *)malloc(sizeof(json_t))) == NULL)
      return NULL;
   json_init(value, JSON_STRING);
   value->string.ptr = (char *)calloc(32, sizeof(char));
   value->string.size = 32;
   json_string_append(value, cstr, -1);
   return value;
}

json_t *json_stringn(const char *cstr, int len)
{
   json_t *value;
   if ((value = (json_t *)malloc(sizeof(json_t))) == NULL)
      return NULL;
   json_init(value, JSON_STRING);
   value->string.ptr = (char *)calloc(32, sizeof(char));
   value->string.size = 32;
   json_string_append(value, cstr, len);
   return value;
}

json_t *json_integer(long long integer)
{
   json_t *value;
   if ((value = (json_t *)malloc(sizeof(json_t))) == NULL)
      return NULL;
   json_init(value, JSON_INTEGER);
   value->integer = integer;
   return value;
}

json_t *json_real(double real)
{
   json_t *value;
   if ((value = (json_t *)malloc(sizeof(json_t))) == NULL)
      return NULL;
   json_init(value, JSON_REAL);
   value->real = real;
   return value;
}

json_t *json_true(void)
{
   json_t *value;
   if ((value = (json_t *)malloc(sizeof(json_t))) == NULL)
      return NULL;
   json_init(value, JSON_TRUE);
   return value;
}

json_t *json_false(void)
{
   json_t *value;
   if ((value = (json_t *)malloc(sizeof(json_t))) == NULL)
      return NULL;
   json_init(value, JSON_FALSE);
   return value;
}

json_t *json_null(void)
{
   json_t *value;
   if ((value = (json_t *)malloc(sizeof(json_t))) == NULL)
      return NULL;
   json_init(value, JSON_NULL);
   return value;
}

FUNC_ATTR long unescape_char(json_parser_t *parser)
{
   switch (*++parser->pos) 
   {
   case '\\': parser->pos++; return '\\';
   case '/':  parser->pos++; return  '/';
   case '\'': parser->pos++; return '\'';
   case '"':  parser->pos++; return '\"';
   case 'a':  parser->pos++; return '\a';
   case 'b':  parser->pos++; return '\b';
   case 'f':  parser->pos++; return '\f';
   case 'n':  parser->pos++; return '\n';
   case 'r':  parser->pos++; return '\r';
   case 't':  parser->pos++; return '\t';
   case '0':  parser->pos++; return '\0';
   case 'x':
      parser->pos++;
      if (isxdigit(parser->pos[0]) && isxdigit(parser->pos[1]))
      {
         parser->pos += 2;
         return (0xFF & (hex_value(parser->pos[-1]) << 4) | hex_value(parser->pos[-2]));
      }
      else
      {
         json_parser_error(parser, "invalid escape sequence \\x%c%c", parser->pos[0], parser->pos[1]);
         return -1;
      }
   default:
      json_parser_error(parser, "invalid escape sequence \\%c", *parser->pos);
      return -1;
   }
}
FUNC_ATTR json_t *parse_string(json_parser_t *parser)
{
   char start;
   long c;
   json_t *string;
   start = *parser->pos;
   parser->pos++;
   string = json_string("");
   while (*parser->pos != start)
   {
      if (*parser->pos == 0)
      {
         json_parser_error(parser, "end of input before closing \\%c in string", start);
         json_delete(string);
         return NULL;
      }
      if (*parser->pos == '\\')
      {
         if (parser->pos[1] == '\r')
         {
            parser->pos += 2;
            if (*parser->pos == '\n')
               parser->pos++;
         }
         else if (parser->pos[1] == '\n')
         {
            parser->pos += 2;
         }
         else
         {
            c = unescape_char(parser);
            if (c < 0)
            {
               json_delete(string);
               return NULL;
            }
            json_string_append_byte(string, (char)c);
         }
      }
      else
      {
         json_string_append_byte(string, *parser->pos++);
      }
   }
   parser->pos++;
   return string;
}

FUNC_ATTR json_t *read_key(json_parser_t *parser)
{
   json_t *key;
   key = NULL;
   if (*parser->pos
       && ((*parser->pos >= 'a' && *parser->pos <= 'z')
           || (*parser->pos >= 'A' && *parser->pos <= 'Z')
           || *parser->pos == '$'
           || *parser->pos == '_'))
   {
      key = json_string("");
      json_string_append_byte(key, *parser->pos);
      parser->pos++;
   }
   while (*parser->pos
          && ((*parser->pos >= 'a' && *parser->pos <= 'z')
              || (*parser->pos >= 'A' && *parser->pos <= 'Z')
              || (*parser->pos >= '0' && *parser->pos <= '9')
              || *parser->pos == '$'
              || *parser->pos == '_'
              || *parser->pos == '-'))
   {
      json_string_append_byte(key, *parser->pos);
      parser->pos++;
   }
   return key;
}

FUNC_ATTR json_t  *parse_object(json_parser_t *parser)
{
   json_t *key;
   json_t *value;
   json_t *object;
   parser->pos++;
   value = NULL;
   skip_space(parser);
   if (*parser->pos == 0)
   {
      json_parser_error(parser, "unexpected end of input in dictionary", NULL);
      json_delete(value);
      return NULL;
   }
   object = json_object();
   while (*parser->pos != '}')
   {
      if (*parser->pos == 0)
      {
         json_parser_error(parser, "unexpected end of input in dictionary", NULL);
         json_delete(value);
         return NULL;
      }
      skip_space(parser);
      if (*parser->pos == '/') 
      {
         skip_comment(parser);
         skip_space(parser);
      }
      if (*parser->pos == '}') break;
      key = read_key(parser);
      skip_space(parser);
      if (*parser->pos != ':') 
      {
         json_parser_error(parser, "expected ':' after dictionary key", NULL);
         json_delete(key);
         json_delete(object);
         return NULL;
      }
      parser->pos++;
      value = json_parse(parser);
      if (value == NULL)
      {
         json_parser_error(parser, "expected value after key in dictionary", NULL);
         json_delete(key);
         json_delete(object);
         return NULL;
      }
      json_object_set(object, key->string.ptr, value);
      json_delete(key);
      skip_space(parser);
      if (*parser->pos == ',') parser->pos++;
      else if (*parser->pos == '}') break;
      else
      {
         json_parser_error(parser, "expected '}' or ',' in dictionary", NULL);
         json_delete(object);
         return NULL;
      }
   }
   parser->pos++;
   return object;
}

FUNC_ATTR json_t *parse_array(json_parser_t *parser)
{
   json_t *value;
   json_t *array;
   parser->pos++;
   skip_space(parser);
   if (*parser->pos == 0)
   {
      json_parser_error(parser, "unexpected end of input in array", NULL);
      return NULL;
   }
   array = json_array();
   while (*parser->pos != ']')
   {
      value = json_parse(parser);
      if (value == NULL)
      {
         json_delete(array);
         return NULL;
      }
      json_array_append(array, value);
      skip_space(parser);
      if (*parser->pos == ',') parser->pos++;
      else if (*parser->pos == ']') break;
      else 
      {
         json_parser_error(parser, "expected ']' or ',' in array", NULL);
         json_delete(array);
         return NULL;
      }
   }
   parser->pos++;
   return array;
}

FUNC_ATTR json_t *parse_number(json_parser_t *parser)
{
   char *endptr;
   long long integer;
   json_t *value = json_integer(0);
   integer = _strtoi64(parser->pos, &endptr, 10);
   if (*endptr == 'e' || *endptr == 'E' || *endptr == '.')
   {
      double real = strtod(parser->pos, &endptr);
      value->type = JSON_REAL;
      value->real = real;
   }
   else
   {
      value->type = JSON_INTEGER;
      value->integer = integer;
   }
   parser->pos = endptr;
   return value;
}

FUNC_ATTR json_t *parse_true(json_parser_t *parser)
{
   if (memcmp(parser->pos, "true", 4) == 0)
   {
      parser->pos += 4;
      return json_true();
   }
   else
   {
      json_parser_error(parser, "expected 'true'");
      return NULL;
   }
}

FUNC_ATTR json_t *parse_false(json_parser_t *parser)
{
   if (memcmp(parser->pos, "false", 5) == 0)
   {
      parser->pos += 5;
      return json_false();
   }
   else
   {
      json_parser_error(parser, "expected 'false'");
      return NULL;
   }
}

FUNC_ATTR json_t *parse_null(json_parser_t *parser)
{
   if (memcmp(parser->pos, "null", 4) == 0)
   {
      parser->pos += 4;
      return json_null();
   }
   else
   {
      json_parser_error(parser, "expected 'null'");
      return NULL;
   }
}

FUNC_ATTR json_t *json_parse(json_parser_t *parser)
{
   skip_space(parser);
   switch (parser->pos[0])
   {
   case '{': return parse_object(parser);
   case '[': return parse_array(parser);
   case '\"':
   case '\'': return parse_string(parser);
   case '-':
   case '+':
   case '.':
   case '0':
   case '1': 
   case '2': 
   case '3':
   case '4':
   case '5':
   case '6': 
   case '7': 
   case '8': 
   case '9': return parse_number(parser);
   case 't': return parse_true(parser);
   case 'f': return parse_false(parser);
   case 'n': return parse_null(parser);
   case '/': skip_comment(parser); return json_parse(parser);
   default: return NULL;
   }
}

json_t *json_object_get(json_t *json, const char *key)
{
   pair_t pair, *find;
   struct avl_node *node;
   avl_index_t where;
   if (!json_is_object(json))
      return NULL;
   pair.key = json_string(key);
   if (pair.key == NULL) return NULL;
   node = avl_find(&json->object, &pair.entry, &where, object_cmp);
   json_delete(pair.key);
   if (node)
   {
      find = container_of(node, pair_t, entry);
      return find->value;
   }
   return NULL;
}

json_t *json_array_at(json_t *json, int index)
{
   json_t *elm;
   if (!json_is_array(json) || index < 0) return NULL;
   elm = list_first(&json->array);
   while (elm && index)
   {
      elm = list_next(elm, entry);
      index--;
   }
   return elm;
}

void json_object_set(json_t *json, const char *key, json_t *value)
{
   pair_t *insert, *pair;
   struct avl_node *find;
   avl_index_t where;
   if (!json_is_object(json))
      return;
   if ((insert = (pair_t *)malloc(sizeof(pair_t))) == NULL)
      return;
   insert->key = json_string(key);
   if (insert->key == NULL)
   {
      free(insert);
      return;
   }
   find = avl_find(&json->object, &insert->entry, &where, object_cmp);
   if (find)
   {
      pair = container_of(find, pair_t, entry);
      json_delete(pair->value);
      json_delete(insert->key);
      pair->value = value;
      free(insert);
   }
   else
   {
      insert->value = value;
      avl_insert(&json->object, &insert->entry, where);
   }
}

void json_object_clear(json_t *json)
{
   pair_t *pair;
   struct avl_node *var, *tmp;
   if (!json_is_object(json))
      return;
   avl_foreach_safe(&json->object, var, tmp)
   {
      pair = container_of(var, pair_t, entry);
      avl_remove(&json->object, var);
      json_delete(pair->value);
      json_delete(pair->key);
      free(pair);
   }
}

void json_array_clear(json_t *json)
{
   json_t *elm, *tmp;
   if (!json_is_array(json)) return;
   list_foreach_safe(elm, &json->array, entry, tmp)
   {
      list_remove(&json->array, elm, entry);
      json_delete(elm);
   }
}

void json_delete(json_t *json)
{
   switch(json_typeof(json))
   {
   case JSON_NONE:
      return;
   case JSON_OBJECT:
      json_object_clear(json);
      break;
   case JSON_ARRAY:
      json_array_clear(json);
      break;
   case JSON_STRING:
      free(json->string.ptr);
      break;
   default:
      break;
   }
   free(json);
}

void json_array_append(json_t *json, json_t *value)
{
   if (!json_is_array(json) || !value) return;
   list_insert_tail(&json->array, value, entry);
}

void json_object_del(json_t *json, const char *key)
{
   pair_t *find, pair;
   struct avl_node *node;
   avl_index_t where;
   if (!json_is_object(json))
      return;
   pair.key = json_string(key);
   if (pair.key) return;
   node = avl_find(&json->object, &pair.entry, &where, object_cmp);
   if (node)
   {
      find = container_of(node, pair_t, entry);
      json_delete(find->value);
      json_delete(find->key);
      free(find);
   }
   json_delete(pair.key);
}

void json_array_prepand(json_t *json, json_t *value)
{
   if (!json_is_array(json) || !value) return;
   list_insert_head(&json->array, value, entry);
}

void json_array_del(json_t *json, int index)
{
   json_t *elm = json_array_at(json, index);
   if (elm)
   {
      list_remove(&json->array, elm, entry);
      json_delete(elm);
   }
}

long long json_integer_value(json_t *json)
{
   if (json_is_integer(json)) return json->integer;
   if (json_is_real(json)) return (long long)json->real;
   return 0;
}

double json_real_value(json_t *json)
{
   if (json_is_real(json)) return json->real;
   if (json_is_integer(json)) return (double)json->integer;
   return 0.0;
}

const char *json_string_value(json_t *json)
{
   if (json_is_string(json)) return json->string.ptr;
   return "";
}

int json_string_length(json_t *json)
{
   if (json_is_string(json)) return json->string.length;
   return 0;
}

bool json_boolean_value(json_t *json)
{
   return json_is_true(json) ? true : false;
}

json_t *json_loadf(const char *file)
{
   FILE *fp;
   json_t *json;
   char *content;
   int length;
   if ((fp = fopen(file, "rb")) == NULL)
      return NULL;
   fseek(fp, 0, SEEK_SET);
   fseek(fp, 0, SEEK_END);
   length = ftell(fp);
   fseek(fp, 0, SEEK_SET);
   if ((content = (char *)malloc(length + 1)) == NULL)
      return NULL;
   fread(content, sizeof(char), length, fp);
   json = json_loads(content);
   free(content);
   fclose(fp);
   return json;
}

json_t *json_loads(const char *str)
{
   json_parser_t parser;
   parser.lineno = 0;
   parser.pos = str;
   return json_parse(&parser);
}

int json_dumpb(json_t *json, json_t *string, int tab)
{
   pair_t *pair;
   json_t *elm;
   struct avl_node *var;
   char buf[256];
   int i;
   switch (json_typeof(json))
   {
   case JSON_NONE:
      return -1;
   case JSON_OBJECT:
      json_string_append_byte(string, '{');
      tab += 4;
      avl_foreach(&json->object, var)
      {
         pair = container_of(var, pair_t, entry);
         json_string_append_byte(string, '\n');
         for (i = 0; i < tab; i++)
            json_string_append_byte(string, ' ');
         json_string_append(string, pair->key->string.ptr, pair->key->string.length);
         json_string_append(string, ": ", 2);
         if (json_dumpb(pair->value, string, tab) < 0) return -1;
         json_string_append_byte(string, ',');
      }
      string->string.length -= 1;
      string->string.ptr[string->string.length] = 0;
      json_string_append_byte(string, '\n');
      tab -= 4;
      for (i = 0; i < tab; i++)
         json_string_append_byte(string, ' ');
      json_string_append_byte(string, '}');
      break;
   case JSON_ARRAY:
      json_string_append_byte(string, '[');
      tab += 4;
      list_foreach(elm, &json->array, entry)
      {
         json_string_append_byte(string, '\n');
         for (i = 0; i < tab; i++)
            json_string_append_byte(string, ' ');
         if (json_dumpb(elm, string, tab) < 0) return -1;
         json_string_append_byte(string, ',');
      }
      string->string.length -= 1;
      string->string.ptr[string->string.length] = 0;
      json_string_append_byte(string, '\n');
      tab -= 4;
      for (i = 0; i < tab; i++)
         json_string_append_byte(string, ' ');
      json_string_append_byte(string, ']');
      break;
   case JSON_STRING:
      json_string_append_byte(string, '\'');
      for (i = 0; i < json->string.length; i++)
      {
          char c = json->string.ptr[i];
          switch (c)
          {
          case '\b': json_string_append_byte(string, '\\'); json_string_append_byte(string, 'b'); break;
          case '\f': json_string_append_byte(string, '\\'); json_string_append_byte(string, 'f'); break;
          case '\n': json_string_append_byte(string, '\\'); json_string_append_byte(string, 'n'); break;
          case '\r': json_string_append_byte(string, '\\'); json_string_append_byte(string, 'r'); break;
          case '\t': json_string_append_byte(string, '\\'); json_string_append_byte(string, 't'); break;
          case '\'': json_string_append_byte(string, '\\'); json_string_append_byte(string, '\''); break;
          case '\"': json_string_append_byte(string, '\\'); json_string_append_byte(string, '\"'); break;
          case '\\': json_string_append_byte(string, '\\'); json_string_append_byte(string, '\\'); break;
          case '\0': json_string_append_byte(string, '\\'); json_string_append_byte(string, '\0'); break;
          default:
             if (c > 31 && c < 127) json_string_append_byte(string, c);
             else
             {
                json_string_append(string, "\\x", 2);
                json_string_append_byte(string, "0123456789abcdef"[(c >> 4) & 0xf]);
                json_string_append_byte(string, "0123456789abcdef"[c & 0xf]);
             }
          }
      }
      json_string_append_byte(string, '\'');
      break;
   case JSON_INTEGER:
      sprintf(buf, "%lld", json->integer);
      json_string_append(string, buf, -1);
      break;
   case JSON_REAL:
      sprintf(buf, "%lf", json->real);
      json_string_append(string, buf, -1);
      break;
   case JSON_TRUE:
      json_string_append(string, "true", 4);
      break;
   case JSON_FALSE:
      json_string_append(string, "false", 5);
      break;
   case JSON_NULL:
      json_string_append(string, "null", 4);
      break;
   default:
      string->string.ptr[0] = 0;
      string->string.length = 0;
      return -1;
   }
   return 0;
}

int json_dumpf(json_t *json, const char *file)
{
   json_t *string;
   FILE *fp;
   string = json_dumps(json);
   fp = fopen(file, "wb+");
   fwrite(json_string_value(string), sizeof(char), json_string_length(string), fp);
   fclose(fp);
   json_delete(string);
   return -1;
}

json_t *json_dumps(json_t *json)
{
   json_t *string;
   string = json_string("");
   json_dumpb(json, string, 0);
   return string;
}

json_iter_t *json_iter_new(json_t *json)
{
   json_iter_t *iter;
   if ((iter = malloc(sizeof(json_iter_t))) == NULL)
      return NULL;
   iter->json = json;
   if (json_is_object(json))
   {
      iter->type = JSON_ITER_OBJECT;
      iter->next = avl_first(&json->object);
   }
   else if (json_is_array(json))
   {
      iter->type = JSON_ITER_ARRAY;
      iter->next = list_first(&json->array);
   }
   else
   {
      free(iter);
      iter = NULL;
   }
   return iter;
}

json_t *json_iter_next(json_iter_t *iter)
{
   json_t *json;
   pair_t *pair;
   struct avl_node *node;
   if (iter)
   {
      if (iter->type == JSON_ITER_ARRAY)
      {
         if (iter->next)
         {
            json = (json_t *)iter->next;
            iter->next = list_next(json, entry);
            return json;
         }
         else
            return NULL;
      }
      else if (iter->type == JSON_ITER_OBJECT)
      {
         if (iter->next)
         {
            pair = (pair_t *)iter->next;
            node = avl_walk(&iter->json->object, &pair->entry, AVL_AFTER);
            iter->next = container_of(node, pair_t, entry);
            return pair->key;
         }
         else
            return NULL;
      }
   }
   return NULL;
}

void json_iter_delete(json_iter_t *iter)
{
   if (iter) free(iter);
}

#if 0
int main()
{
   json_t *json, *l;
   json = json_loadf("test.json");
   l = json_object_get(json, "launch");
   json_dumpf(json, "dump.json");
   json_delete(json);
   return 1;
}
#endif