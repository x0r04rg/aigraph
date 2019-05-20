#ifndef AIGRAPH_H
#define AIGRAPH_H

#include <stddef.h>
#include "json.h"
#include "allocator.h"
#include "json_helper.h"

#define LEN(x) (sizeof(x)/sizeof(x)[0])

typedef enum { FIELD_INVALID, FIELD_INT, FIELD_FLOAT, FIELD_ENUM, FIELD_STRING } property_type;

struct property_info
{
    char *name;
    enum property_type type;
    short enum_type;
};

struct input_info
{
    char *name;
    char *type;
};

struct output_info
{
    char *name;
    char *type;
};

struct node_info
{
    char *name;
    char *category;
    int prop_count;
    int input_count;
    int output_count;
    struct property_info *props;
    struct input_info *inputs;
    struct output_info *outputs;
};

struct enum_info
{
    char *name;
    int count;
    char **values;
};

struct config
{
    struct node_info *nodes;
    struct enum_info *enums;
    int enum_count;
    int node_count;
    void *buffer;
};

static inline int
streq(const char *str1, const char *str2)
{
    return strcmp(str1, str2) == 0;
}

static inline char*
duplicate_string(struct allocator *a, const char *str)
{
    size_t length = strlen(str);
    char* copy = mem_alloc(a, length);
    strcpy_s(copy, length, str);
    return copy;
}

//static void
//config_load_from_json(struct config *conf, struct json_reader *r)
//{
//    struct linear_allocator a;
//    allocator_linear_init(&a, 512);
//
//    memset(conf, 0, sizeof *conf);
//
//    if (json_reader_try_push_object_by_name(r, "enums"))
//    {
//        conf->enum_count = json_reader_get_length(r);
//        if (conf->enum_count)
//        {
//            conf->enums = mem_calloc(&a, conf->enum_count, sizeof *conf->enums);
//            for (int i = 0; i < conf->enum_count; ++i)
//            {
//                struct enum_info *e = &conf->enums[i];
//                json_reader_get_name(r, &e->name);
//                if (!json_reader_push_array(r)) goto fail;
//                e->count = json_reader_get_length(r);
//                if (e->count)
//                {
//                    e->values = mem_calloc(&a, e->count, sizeof *e->values);
//                    for (int j = 0; j < e->count; ++j)
//                    {
//                        if (!json_reader_get_string(r, &e->values[j])) goto fail;
//                        json_reader_next(r);
//                    }
//                }
//                json_reader_pop_array(r);
//            }
//        }
//        json_reader_pop_array(r);
//    }
//    if (json_reader_try_push_object_by_name(r, "nodes"))
//    {
//        conf->node_count = json_reader_get_length(r);
//        if (conf->node_count)
//        {
//            conf->nodes = mem_calloc(&a, conf->enum_count, sizeof *conf->nodes);
//            for (int i = 0; i < conf->enum_count; ++i)
//            {
//
//            }
//        }
//    }
//
//    fail:
//}

static void
config_load_from_json(struct config *conf, struct json_object_s *json)
{
    struct linear_allocator a;
    allocator_linear_init(&a, 512);

    memset(conf, 0, sizeof *conf);

    for (struct json_object_element_s *field = json->start; field != NULL; field = field->next)
    {
        if (streq(field->name->string, "enums"))
        {
            assert(conf->enums == NULL);
            assert(field->value->type == json_type_object);

            struct json_object_s *enums = field->value->payload;

            conf->enum_count = (int)enums->length;
            conf->enums = mem_calloc(&a, enums->length, sizeof(struct enum_info));

            int i = 0;
            for (struct json_object_element_s *enum_item = enums->start; enum_item != NULL; enum_item = enum_item->next)
            {
                conf->enums[i].name = duplicate_string(&a, enum_item->name->string);

                assert(enum_item->value->type == json_type_array);
                struct json_array_s *values = enum_item->value->payload;

                conf->enums[i].count = (int)values->length;
                conf->enums[i].values = mem_calloc(&a, values->length, sizeof(char*));

                int j = 0;
                for (struct json_array_element_s *val = values->start; val != NULL; val = val->next)
                {
                    assert(val->value->type == json_type_string);
                    struct json_string_s *str = val->value->payload;
                    conf->enums[i].values[j] = duplicate_string(&a, str->string);
                    ++j;
                }
                ++i;
            }
        }
        else if (streq(field->name->string, "nodes"))
        {
            assert(conf->nodes == NULL);
            assert(field->value->type == json_type_object);

            struct json_object_s *nodes = field->value->payload;

            conf->node_count = (int)nodes->length;
            conf->nodes = mem_calloc(&a, nodes->length, sizeof(struct node_info));

            int i = 0;
            for (struct json_object_element_s *node_item = nodes->start; node_item != NULL; node_item = node_item->next)
            {
                assert(node_item->value->type == json_type_object);

                conf->nodes[i].name = duplicate_string(&a, node_item->name->string);

                struct json_object_s *node_obj = node_item->value->payload;
                for (struct json_object_element_s *node_field = node_obj->start; node_field != NULL; node_field = node_field->next)
                {
                    const char *key = node_field->name->string;
                    if (streq(key, "category"))
                    {
                        assert(node_field->value->type == json_type_string);
                        struct json_string_s *name = node_field->value->payload;
                        conf->nodes[i].category = duplicate_string(&a, name->string);
                    }
                    else if (streq(key, "properties"))
                    {
                        assert(node_field->value->type == json_type_object);

                        struct json_object_s *items = node_field->value->payload;
                        conf->nodes[i].prop_count = (int)items->length;
                        conf->nodes[i].props = mem_calloc(&a, items->length, sizeof(struct property_info));

                        int j = 0;
                        for (struct json_object_element_s *item = items->start; item != NULL; item = item->next)
                        {
                            assert(item->value->type == json_type_string);

                            struct json_string_s *type = item->value->payload;

                            conf->nodes[i].props[j].name = duplicate_string(&a, item->name->string);

                            if (streq(type->string, "int"))
                            {
                                conf->nodes[i].props[j].type = FIELD_INT;
                            }
                            else if (streq(type->string, "float"))
                            {
                                conf->nodes[i].props[j].type = FIELD_FLOAT;
                            }
                            else if (streq(type->string, "string"))
                            {
                                conf->nodes[i].props[j].type = FIELD_STRING;
                            }
                            else
                            {
                                /* TODO: enums are not guaranteed be loaded by the time we get here */
                                int k = 0;
                                for (k = 0; k < conf->enum_count; ++k)
                                {
                                    if (streq(conf->enums[k].name, type->string))
                                    {
                                        conf->nodes[i].props[j].type = FIELD_ENUM;
                                        conf->nodes[i].props[j].enum_type = k;
                                        break;
                                    }
                                }
                                if (k == conf->enum_count)
                                {
                                    conf->nodes[i].props[j].type = FIELD_INVALID;
                                }
                            }
                        }
                    }
                    else if (streq(key, "inputs"))
                    {
                        assert(node_field->value->type == json_type_object);

                        struct json_object_s *items = node_field->value->payload;
                        conf->nodes[i].input_count = (int)items->length;
                        conf->nodes[i].inputs = mem_calloc(&a, items->length, sizeof(struct input_info));

                        int j = 0;
                        for (struct json_object_element_s *item = items->start; item != NULL; item = item->next)
                        {
                            assert(item->value->type == json_type_string);

                            struct json_string_s *type = item->value->payload;

                            conf->nodes[i].inputs[j].name = duplicate_string(&a, item->name->string);
                            conf->nodes[i].inputs[j].type = duplicate_string(&a, type->string);
                        }
                    }
                    else if (streq(key, "outputs"))
                    {
                        assert(node_field->value->type == json_type_object);

                        struct json_object_s *items = node_field->value->payload;
                        conf->nodes[i].output_count = (int)items->length;
                        conf->nodes[i].outputs = mem_calloc(&a, items->length, sizeof(struct output_info));

                        int j = 0;
                        for (struct json_object_element_s *item = items->start; item != NULL; item = item->next)
                        {
                            assert(item->value->type == json_type_string);

                            struct json_string_s *type = item->value->payload;

                            conf->nodes[i].outputs[j].name = duplicate_string(&a, item->name->string);
                            conf->nodes[i].outputs[j].type = duplicate_string(&a, type->string);
                        }
                    }
                }
                ++i;
            }
        }
    }

    conf->buffer = a.buffer;
}

static void
config_write_to_json(struct config *conf, struct json_builder *json)
{
    json_builder_push_object_to_object(json, "enums");
    for (int i = 0; i < conf->enum_count; ++i)
    {
        json_builder_push_array_to_object(json, conf->enums[i].name);
        for (int j = 0; j < conf->enums[i].count; ++j)
        {
            json_builder_push_string_to_array(json, conf->enums[i].values[j]);
        }
        json_builder_pop_array(json);
    }
    json_builder_pop_object(json);

    json_builder_push_object_to_object(json, "nodes");
    for (int i = 0; i < conf->node_count; ++i)
    {
        struct node_info *node = &conf->nodes[i];

        json_builder_push_object_to_object(json, node->name);
        json_builder_push_string_to_object(json, node->category);

        json_builder_push_object_to_object(json, "properties");
        for (int j = 0; j < node->prop_count; ++j)
        {
            struct property_info *prop = node->props[j];
            char *type = NULL;
            switch (prop->type)
            {
            case FIELD_INT: type = "int"; break;
            case FIELD_FLOAT: type = "float"; break;
            case FIELD_STRING: type = "string"; break;
            case FIELD_ENUM: type = conf->enums[prop->enum_type]; break;
            }
            assert(type != NULL);
            json_builder_push_string_to_object(json, prop->name, type);
        }
        json_builder_pop_object(json);

        json_builder_push_object_to_object(json, "inputs");
        for (int j = 0; j < node->input_count; ++j)
        {
            json_builder_push_string_to_object(json, node->inputs[j].name, node->inputs[j].type);
        }
        json_builder_pop_object(json);

        json_builder_push_object_to_object(json, "outputs");
        for (int j = 0; j < node->output_count; ++j)
        {
            json_builder_push_string_to_object(json, node->outputs[j].name, node->outputs[j].type);
        }
        json_builder_pop_object(json);
    }
    json_builder_pop_array(json);
}

static void
config_cleanup(struct config *conf)
{
    if (conf->enums)
    {
        for (int i = 0; i < conf->enum_count; ++i)
        {
            free(conf->enums[i].name);
            for (int j = 0; j < conf->enums[i].count; ++j)
            {
                free(conf->enums[i].values[j]);
            }
        }
        free(conf->enums);
        conf->enums = NULL;
    }
    if (conf->nodes)
    {
        for (int i = 0; i < conf->node_count; ++i)
        {
            free(conf->nodes[i].name);
            if (conf->nodes[i].category) free(conf->nodes[i].category);
            for (int j = 0; j < conf->nodes[i].prop_count; ++j)
            {
                free(conf->nodes[i].props[j].name);
            }
            for (int j = 0; j < conf->nodes[i].input_count; ++j)
            {
                free(conf->nodes[i].inputs[j].name);
                free(conf->nodes[i].inputs[j].type);
            }
            for (int j = 0; j < conf->nodes[i].output_count; ++j)
            {
                free(conf->nodes[i].outputs[j].name);
                free(conf->nodes[i].outputs[j].type);
            }
        }
        free(conf->nodes);
        conf->nodes = NULL;
    }
}

#endif