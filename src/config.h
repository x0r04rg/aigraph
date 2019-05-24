#ifndef AIGRAPH_H
#define AIGRAPH_H

#include <stddef.h>
#include "json.h"
#include "allocator.h"
#include "json_helper.h"
#include "collections.h"
#include "hash.h"

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
    struct json_value_s *json;
    struct hashtable node_by_name;
    void *buffer;
};

static inline int
streq(const char *str1, const char *str2)
{
    return strcmp(str1, str2) == 0;
}

static void
config_load_from_json(struct config *conf, struct json_value_s *json)
{
    memset(conf, 0, sizeof *conf);

    struct linear_allocator a = allocator_linear_create(256);
    conf->json = json_deep_copy(json);
    struct json_object_s *jconf = conf->json->payload;

    for (struct json_object_element_s *field = jconf->start; field != NULL; field = field->next)
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
                conf->enums[i].name = enum_item->name->string;

                assert(enum_item->value->type == json_type_array);
                struct json_array_s *values = enum_item->value->payload;

                conf->enums[i].count = (int)values->length;
                conf->enums[i].values = mem_calloc(&a, values->length, sizeof(char*));

                int j = 0;
                for (struct json_array_element_s *val = values->start; val != NULL; val = val->next)
                {
                    assert(val->value->type == json_type_string);
                    struct json_string_s *str = val->value->payload;
                    conf->enums[i].values[j] = str->string;
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

                conf->nodes[i].name = node_item->name->string;

                struct json_object_s *node_obj = node_item->value->payload;
                for (struct json_object_element_s *node_field = node_obj->start; node_field != NULL; node_field = node_field->next)
                {
                    const char *key = node_field->name->string;
                    if (streq(key, "category"))
                    {
                        assert(node_field->value->type == json_type_string);
                        struct json_string_s *name = node_field->value->payload;
                        conf->nodes[i].category = name->string;
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

                            conf->nodes[i].props[j].name = item->name->string;

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

                            conf->nodes[i].inputs[j].name = item->name->string;
                            conf->nodes[i].inputs[j].type = type->string;
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

                            conf->nodes[i].outputs[j].name = item->name->string;
                            conf->nodes[i].outputs[j].type = type->string;
                        }
                    }
                }
                ++i;
            }
        }
    }
    
    conf->node_by_name = hashtable_create(sizeof(char*), sizeof(struct node_info*), ((int)log2(conf->node_count)) + 1, strhash, streq);
    for (int i = 0; i < conf->node_count; ++i)
    {
        struct node_info *node = &conf->nodes[i];
        hashtable_insert(&conf->node_by_name, &node->name, &node);
    }

    conf->buffer = a.buffer;
}

static void
config_cleanup(struct config *conf)
{
    if (conf->buffer) free(conf->buffer);
    hashtable_destroy(&conf->node_by_name);
}

#endif