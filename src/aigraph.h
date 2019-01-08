#ifndef AIGRAPH_H
#define AIGRAPH_H

#include <stddef.h>

#define LEN(x) (sizeof(x)/sizeof(x)[0])

typedef short node_type;

struct node_base
{
    struct node_base *next;
    node_type type;
};

struct compiled_graph
{
    size_t size; /* size of data after this struct */
    struct node_base *first;
};

struct sum
{
    struct node_base base;
    float *in0, *in1;
    float out;
};

struct sum3
{
    struct node_base base;
    float *in0, *in1, *in2;
    float out;
};

struct negate
{
    struct node_base base;
    float *in, out;
};

struct play_anim
{
    struct node_base;
    int anim_id;
    float *in;
};

typedef enum { FIELD_INT, FIELD_FLOAT, FIELD_ENUM } field_type;

struct field_info
{
    char *name;
    size_t offset;
    field_type type;
    short enum_type;
};

struct input_info
{
    char *name;
    size_t offset;
};

struct output_info
{
    char *name;
    size_t offset;
};

struct node_info
{
    char *name;
    char *category;
    int size;
    int field_count;
    int input_count;
    int output_count;
    struct field_info *fields;
    struct input_info *inputs;
    struct output_info *outputs;
};

struct enum_info
{
    int count;
    char **values;
};

struct config
{
    struct node_info *nodes;
    struct enum_info *enums;
    int enum_count;
    int node_count;
};

struct input_info sum_inputs[] = 
{ 
    {.name = "in0", .offset = offsetof(struct sum, in0)},
    {.name = "in1", .offset = offsetof(struct sum, in1)}
};
struct output_info sum_outputs[] = 
{
    {.name = "out", .offset = offsetof(struct sum, out)}
};
struct input_info sum3_inputs[] = 
{ 
    {.name = "in0", .offset = offsetof(struct sum3, in0)},
    {.name = "in1", .offset = offsetof(struct sum3, in1)},
    {.name = "in2", .offset = offsetof(struct sum3, in2)}
};
struct output_info sum3_outputs[] = 
{
    {.name = "out", .offset = offsetof(struct sum3, out)}
};
struct input_info negate_inputs[] = 
{
    {.name = "in", .offset = offsetof(struct negate, in)},
};
struct output_info negate_outputs[] = 
{
    {.name = "out", .offset = offsetof(struct negate, out)}
};
struct input_info play_anim_inputs[] = 
{
    {.name = "in", .offset = offsetof(struct play_anim, in)}
};
struct field_info play_anim_fields[] =
{
    {.name = "animation", .offset = offsetof(struct play_anim, anim_id),
     .type = FIELD_ENUM, .enum_type = 0 /* animation */ }
};
#define INPUTS(x) .inputs = x, .input_count = LEN(x)
#define OUTPUTS(x) .outputs = x, .output_count = LEN(x)
#define FIELDS(x) .fields = x, .field_count = LEN(x)
const struct node_info default_nodes[] = 
{
    {.name = "sum", .category = "math", .size = sizeof(struct sum), 
     INPUTS(sum_inputs), OUTPUTS(sum_outputs)},
    {.name = "sum3", .category = "math", .size = sizeof(struct sum3),
     INPUTS(sum3_inputs), OUTPUTS(sum3_outputs)},
    {.name = "negate", .category = "math", .size = sizeof(struct negate),
     INPUTS(negate_inputs), OUTPUTS(negate_outputs)},
    {.name = "play_anim", .category = "control", .size = sizeof(struct play_anim),
     INPUTS(play_anim_inputs), FIELDS(play_anim_fields)}
};
#undef INPUTS
#undef OUTPUTS
#undef FIELDS

const char *animation_enum[] = {"idle", "wave_hand"};
#define ENUM(x) {.count = LEN(x), .values = x}
const struct enum_info default_enums[] = 
{
    ENUM(animation_enum)
};
#undef ENUM

static void
config_init_default(struct config *conf)
{
    conf->nodes = default_nodes;
    conf->enums = default_enums;
    conf->node_count = LEN(default_nodes);
    conf->enum_count = LEN(default_enums);
}

static void
config_cleanup(struct config *conf)
{
    if (conf->nodes != default_nodes) free(conf->nodes);
    if (conf->enums != default_enums) free(conf->enums);
}

#endif