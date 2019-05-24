#pragma once

#include "json.h"
#include "allocator.h"
#include <stdint.h>
#include <stdbool.h>

//typedef _Bool bool;

struct json_builder_stack_item;
//struct json_reader_stack_item;

struct json_builder
{
    struct allocator *allocator;
    struct json_builder_stack_item *stack;
    size_t stack_cursor;
    size_t stack_capacity;
};

//struct json_reader
//{
//    struct json_object_s *root;
//    struct json_reader_stack_item *stack;
//    size_t stack_cursor;
//    size_t stack_capacity;
//};

void json_builder_init(struct json_builder *b, struct allocator *a);
struct json_object_s *json_builder_get_root(struct json_builder *b);
void json_builder_deinit(struct json_builder *b);
void json_builder_push_array_to_array(struct json_builder *b);
void json_builder_push_object_to_array(struct json_builder *b);
void json_builder_push_string_to_array(struct json_builder *b, char *string);
void json_builder_push_int_to_array(struct json_builder *b, int64_t num);
void json_builder_push_float_to_array(struct json_builder *b, float num);
void json_builder_push_array_to_object(struct json_builder *b, char *name);
void json_builder_push_object_to_object(struct json_builder *b, char *name);
void json_builder_push_string_to_object(struct json_builder *b, char *name, char *string);
void json_builder_push_int_to_object(struct json_builder *b, char *name, int64_t num);
void json_builder_push_float_to_object(struct json_builder *b, char *name, float num);
void json_builder_pop_array(struct json_builder *b);
void json_builder_pop_object(struct json_builder *b);

//void json_reader_init(struct json_reader *r, struct json_object_s *json);
//void json_reader_deinit(struct json_reader *r);
//bool json_reader_get_string_from_array(struct json_reader *r, char **out);
//bool json_reader_get_int_from_array(struct json_reader *r, int *out);
//bool json_reader_get_float_from_array(struct json_reader *r, float *out);
//bool json_reader_get_array_from_array(struct json_reader *r);
//bool json_reader_get_object_from_array(struct json_reader *r);
//bool json_reader_get_string_from_object(struct json_reader *r, char *name, char **out);
//bool json_reader_get_int_from_object(struct json_reader *r, char *name, int *out);
//bool json_reader_get_float_from_object(struct json_reader *r, char *name, float *out);
//bool json_reader_get_array_from_object(struct json_reader *r, char *name);
//bool json_reader_get_object_from_object(struct json_reader *r, char *name);
//void json_object_pop_array(struct json_reader *r);
//void json_reader_pop_object(struct json_reader *r);

bool json_get_int(struct json_value_s *value, int *out);
bool json_get_float(struct json_value_s *value, float *out);
bool json_get_string(struct json_value_s *value, char **out);
bool json_copy_string(struct json_value_s *value, char **out, struct allocator *a);
bool json_get_int_by_name(struct json_object_s *object, char *name, int *out);
bool json_get_float_by_name(struct json_object_s *object, char *name, float *out);
bool json_get_string_by_name(struct json_object_s *object, char *name, char **out);
struct json_value_s *json_get_value_by_name(struct json_object_s *object, char *name);
struct json_array_s *json_get_array_by_name(struct json_object_s *object, char *name);
struct json_object_s *json_get_object_by_name(struct json_object_s *object, char *name);
struct json_value_s *json_deep_copy(struct json_value_s *json);