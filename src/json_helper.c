#include "json_helper.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>

struct json_builder_stack_item
{
    void *obj;
    void *last;
    bool is_array;
};

static inline struct json_builder_stack_item *stack_top(struct json_builder *b)
{
    return &b->stack[b->stack_cursor];
}

static void stack_push(struct json_builder *b, int is_array, void *obj)
{
    b->stack_cursor += 1;

    if (b->stack_cursor == b->stack_capacity)
    {
        b->stack_capacity *= 2;
        b->stack = realloc(b->stack, b->stack_capacity);
    }

    stack_top(b)->is_array = is_array;
    stack_top(b)->obj = obj;
    stack_top(b)->last = NULL;
}

static void stack_pop(struct json_builder *b)
{
    assert(b->stack_cursor > 0);
    b->stack_cursor -= 1;
}

static struct json_value_s *make_value(struct json_builder *b, enum json_type_e type, void *payload)
{
    struct json_value_s *jval = mem_alloc(b->allocator, sizeof *jval);
    jval->type = type;
    jval->payload = payload;
    return jval;
}

static void make_array_element(struct json_builder *b, struct json_value_s *jval)
{
    assert(stack_top(b)->is_array);

    struct json_array_element_s *jelem = mem_alloc(b->allocator, sizeof *jelem);
    jelem->value = jval;
    jelem->next = NULL;

    struct json_array_s *jarr = stack_top(b)->obj;

    if (jarr->start)
    {
        struct json_array_element_s *prev = stack_top(b)->last;
        prev->next = jelem;
    }
    else
    {
        jarr->start = jelem;
    }

    jarr->length += 1;
    stack_top(b)->last = jelem;
}

static void make_object_element(struct json_builder *b, char *name, struct json_value_s *jval)
{
    assert(!stack_top(b)->is_array);

    struct json_object_element_s *jelem = mem_alloc(b->allocator, sizeof *jelem);
    jelem->value = jval;
    jelem->next = NULL;
    jelem->name = mem_alloc(b->allocator, sizeof *jelem->name);
    jelem->name->string = name;
    jelem->name->string_size = strlen(name);

    struct json_object_s *jobj = stack_top(b)->obj;

    if (jobj->start)
    {
        struct json_object_element_s *prev = stack_top(b)->last;
        prev->next = jelem;
    }
    else
    {
        jobj->start = jelem;
    }

    jobj->length += 1;
    stack_top(b)->last = jelem;
}

static struct json_value_s *make_string(struct json_builder *b, char *string)
{
    struct json_string_s *jstr = mem_alloc(b->allocator, sizeof *jstr);
    jstr->string = string;
    jstr->string_size = strlen(string);
    return make_value(b, json_type_string, jstr);
}

static struct json_value_s *make_int(struct json_builder *b, int64_t num)
{
    struct json_number_s *jnum = mem_alloc(b->allocator, sizeof *jnum);
    int size = snprintf("", 0, "%d", num);
    char *string = mem_alloc(b->allocator, size);;
    snprintf(string, size, "%d", num);
    jnum->number = string;
    jnum->number_size = size;
    return make_value(b, json_type_number, jnum);
}

static struct json_value_s *make_float(struct json_builder *b, float num)
{
    struct json_number_s *jnum = mem_alloc(b->allocator, sizeof *jnum);
    int size = snprintf("", 0, "%f", num);
    char *string = mem_alloc(b->allocator, size);;
    snprintf(string, size, "%f", num);
    jnum->number = string;
    jnum->number_size = size;
    return make_value(b, json_type_number, jnum);
}

static struct json_array_s *make_array(struct json_builder *b)
{
    struct json_array_s *jarr = mem_alloc(b, sizeof *jarr);
    jarr->start = NULL;
    jarr->length = 0;
    return jarr;
}

static struct json_object_s *make_object(struct json_builder *b)
{
    struct json_object_s *jobj = mem_alloc(b->allocator, sizeof *jobj);
    jobj->start = NULL;
    jobj->length = 0;
    return jobj;
}

void json_builder_init(struct json_builder *b, struct allocator *a)
{
    b->allocator = a;
    b->stack_capacity = 10;
    b->stack = calloc(b->stack_capacity, sizeof *b->stack);
    b->stack_cursor = 0;
    b->stack[0].obj = mem_calloc(a, sizeof(struct json_object_s));
    b->stack[0].is_array = false;
}

struct json_object_s *json_builder_get_root(struct json_builder *b)
{
    return b->stack[0].obj;
}

void json_builder_deinit(struct json_builder *b)
{
    free(b->stack);
}

void json_builder_push_string_to_array(struct json_builder *b, char *string)
{
    assert(stack_top(b)->is_array);
    make_array_element(b, make_string(b, string));
}

void json_builder_push_string_to_object(struct json_builder *b, char *name, char *string)
{
    assert(!stack_top(b)->is_array);
    make_object_element(b, name, make_string(b, string));
}

void json_builder_push_int_to_array(struct json_builder *b, int64_t num)
{
    assert(stack_top(b)->is_array);
    make_array_element(b, make_int(b, num));
}

void json_builder_push_int_to_object(struct json_builder *b, char *name, int64_t num)
{
    assert(!stack_top(b)->is_array);
    make_object_element(b, make_int(b, num));
}

void json_builder_push_float_to_array(struct json_builder *b, float num)
{
    assert(stack_top(b)->is_array);
    make_array_element(b, make_float(b, num));
}

void json_builder_push_float_to_object(struct json_builder *b, char *name, float num)
{
    assert(!stack_top(b)->is_array);
    make_object_element(b, make_float(b, num));
}

void json_builder_push_array_to_array(struct json_builder *b)
{
    assert(stack_top(b)->is_array);
    struct json_array_s *jarr = make_array(b);
    make_array_element(b, make_value(b, json_type_array, jarr));
    stack_push(b, 1, jarr);
}

void json_builder_push_array_to_object(struct json_builder *b, char *name)
{
    assert(!stack_top(b)->is_array);
    struct json_array_s *jarr = make_array(b);
    make_object_element(b, name, make_value(b, json_type_array, jarr));
    stack_push(b, 1, jarr);
}

void json_builder_push_object_to_array(struct json_builder *b)
{
    assert(stack_top(b)->is_array);
    struct json_object_s *jarr = make_object(b);
    make_array_element(b, make_value(b, json_type_object, jarr));
    stack_push(b, 1, jarr);
}

void json_builder_push_object_to_object(struct json_builder *b, char *name)
{
    assert(!stack_top(b)->is_array);
    struct json_object_s *jarr = make_object(b);
    make_object_element(b, name, make_value(b, json_type_object, jarr));
    stack_push(b, 1, jarr);
}

void json_builder_pop_array(struct json_builder *b)
{
    assert(stack_top(b)->is_array);
    stack_pop(b);
}

void json_builder_pop_object(struct json_builder *b)
{
    assert(!stack_top(b)->is_array);
    stack_pop(b);
}

struct json_value_s *json_find_by_name(struct json_object_s *object, char *name)
{
    for (struct json_object_element *it = object->start; it != NULL; it = it->next)
    {
        if (strcmp(name, it->name->string) == 0) return it->value;
    }
    return NULL;
}

bool json_get_int(struct json_value_s *value, int *out)
{
    if (value->type != json_type_number) return false;
    *out = atoi(((struct json_number_s*)value->payload)->number);
    return true;
}

bool json_get_float(struct json_value_s *value, float *out)
{
    if (value->type != json_type_number) return false;
    *out = atof(((struct json_number_s*)value->payload)->number);
    return true;
}

bool json_get_string(struct json_value_s *value, char **out)
{
    if (value->type != json_type_string) return false;
    struct json_string_s *jstr = value->payload;
    *out = jstr->string;
    return true;
}

bool json_copy_string(struct json_value_s *value, char **out, struct allocator *a)
{
    if (value->type != json_type_string) return false;
    struct json_string_s *jstr = value->payload;
    *out = mem_alloc(a, jstr->string_size);
    strcpy_s(*out, jstr->string_size, jstr->string);
    return true;
}

bool json_get_int_by_name(struct json_object_s *object, char *name, int *out)
{
    struct json_value_s *value = json_find_by_name(object, name);
    if (value == NULL) return false;
    return json_get_int(value, out);
}

bool json_get_float_by_name(struct json_object_s *object, char *name, float *out)
{
    struct json_value_s *value = json_find_by_name(object, name);
    if (value == NULL) return false;
    return json_get_float(value, out);
}

bool json_get_string_by_name(struct json_object_s *object, char *name, char **out)
{
    struct json_value_s *value = json_find_by_name(object, name);
    if (value == NULL) return false;
    return json_get_string(value, out);
}

static size_t json_get_size(struct json_value_s *json)
{
    size_t result = sizeof *json;
    switch (json->type)
    {
    case json_type_number:
    {
        struct json_number_s *jnum = json->payload;
        result += sizeof *jnum;
        result += jnum->number_size;
    } break;
    case json_type_string:
    {
        struct json_string_s *jstr = json->payload;
        result += sizeof *jstr;
        result += jstr->string_size;
    } break;
    case json_type_array:
    {
        struct json_array_s *jarr = json->payload;
        result += jarr;
        for (struct json_array_element_s *it = jarr->start; it != NULL; it = it->next)
        {
            result += sizeof *it;
            result += json_get_size(it->value);
        }
    } break;
    case json_type_object:
    {
        struct json_object_s *jobj = json->payload;
        result += sizeof *jobj;
        for (struct json_object_element_s *it = jobj->start; it != NULL; it = it->next)
        {
            result += sizeof *it;
            result += sizeof *it->name;
            result += it->name->string_size;
            result += json_get_size(it->value);
        }
    } break;
    }
    return result;
}

static void *buffer_copy(size_t size, char *src, char **buffer_ptr)
{
    void *result = *buffer_ptr;
    memcpy(*buffer_ptr, src, size);
    *buffer_ptr += size;
    return result;
}

static struct json_value_s *json_make_copy(struct json_value_s *json, char **buffer)
{
    struct json_value_s *value = buffer_copy(sizeof *json, json, buffer);
    switch (json->type)
    {
    case json_type_number:
    {
        struct json_number_s *jsrc, *jnum;
        jsrc = json->payload;
        jnum = buffer_copy(sizeof *jsrc, jsrc, buffer);
        jnum->number = buffer_copy(jsrc->number_size, jsrc->number, buffer);
    } break;
    case json_type_string:
    {
        struct json_string_s *jsrc, *jstr;
        jsrc = json->payload;
        jstr = buffer_copy(sizeof *jsrc, jsrc, buffer);
        jstr->string = buffer_copy(jsrc->string_size, jsrc->string, buffer);
    } break;
    case json_type_array:
    {
        struct json_array_s *jsrc, *jarr;
        jsrc = json->payload;
        jarr = buffer_copy(sizeof *jsrc, jsrc, buffer);
        struct json_array_element_s **prev = &jarr->start;
        for (struct json_array_element_s *it = jsrc->start; it != NULL; it = it->next)
        {
            struct json_array_element_s *jel = buffer_copy(sizeof *it, it, buffer);
            jel->value = json_make_copy(it->value, buffer);
            *prev = jel;
            prev = &jel->next;
        }
        *prev = NULL;
    } break;
    case json_type_object:
    {
        struct json_object_s *jsrc, *jobj;
        jsrc = json->payload;
        jobj = buffer_copy(sizeof *jsrc, jsrc, buffer);
        struct json_object_element_s **prev = &jobj->start;
        for (struct json_object_element_s *it = jsrc->start; it != NULL; it = it->next)
        {
            struct json_object_element_s *jel = buffer_copy(sizeof *it, it, buffer);
            jel->name = buffer_copy(sizeof *it->name, it->name, buffer);
            jel->name->string = buffer_copy(it->name->string_size, it->name->string, buffer);
            jel->value = json_make_copy(it->value, buffer);
            *prev = jel;
            prev = &jel->next;
        }
        *prev = NULL;
    } break;
    }
    return value;
}

struct json_value_s *json_deep_copy(struct json_value_s *json)
{
    size_t size = json_get_size(json);
    char *buffer = malloc(size);
    return json_make_copy(json, &buffer);
}