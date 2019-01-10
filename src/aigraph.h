#ifndef AIGRAPH_H
#define AIGRAPH_H

#include <stddef.h>

#define LEN(x) (sizeof(x)/sizeof(x)[0])

typedef short node_type;

typedef enum { FIELD_INT, FIELD_FLOAT, FIELD_ENUM } property_type;

struct property_info
{
    char *name;
    property_type type;
    short enum_type;
};

struct input_info
{
    char *name;
};

struct output_info
{
    char *name;
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

static struct input_info sum_inputs[] = 
{ 
    {.name = "in0"},
    {.name = "in1"}
};
static struct output_info sum_outputs[] = 
{
    {.name = "out"}
};
static struct input_info sum3_inputs[] = 
{ 
    {.name = "in0"},
    {.name = "in1"},
    {.name = "in2"}
};
static struct output_info sum3_outputs[] = 
{
    {.name = "out"}
};
static struct input_info negate_inputs[] = 
{
    {.name = "in"},
};
static struct output_info negate_outputs[] = 
{
    {.name = "out"}
};
static struct input_info play_anim_inputs[] = 
{
    {.name = "in"}
};
static struct property_info play_anim_props[] =
{
    {.name = "animation", .type = FIELD_ENUM, .enum_type = 0 /* animation */ }
};
#define INPUTS(x) .inputs = x, .input_count = LEN(x)
#define OUTPUTS(x) .outputs = x, .output_count = LEN(x)
#define PROPS(x) .props = x, .prop_count = LEN(x)
static struct node_info default_nodes[] = 
{
    {.name = "sum", .category = "math", INPUTS(sum_inputs), OUTPUTS(sum_outputs)},
    {.name = "sum3", .category = "math", INPUTS(sum3_inputs), OUTPUTS(sum3_outputs)},
    {.name = "negate", .category = "math", INPUTS(negate_inputs), OUTPUTS(negate_outputs)},
    {.name = "play_anim", .category = "control", INPUTS(play_anim_inputs), PROPS(play_anim_props)}
};
#undef INPUTS
#undef OUTPUTS
#undef PROPS

static char *animation_enum[] = {"idle", "wave_hand"};
#define ENUM(x) {.count = LEN(x), .values = x}
static struct enum_info default_enums[] = 
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