#ifndef AIGRAPH_H
#define AIGRAPH_H

#include <stddef.h>

enum { MAX_INPUTS = 5, MAX_OUTPUTS = 5 };

typedef enum
{
    NODE_SUM,
    NODE_NEGATE,
    NODE_SUM3,
    NODE_TYPE_COUNT
} node_type;

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
    int size;
    int input_count;
    int output_count;
    struct input_info inputs[MAX_INPUTS];
    struct output_info outputs[MAX_OUTPUTS];
};

struct node_base
{
    struct node_base *next;
    node_type type;
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

struct compiled_graph
{
    size_t size;
    struct node_base *first;
};

static struct node_info* 
fill_infos()
{
#define INPUT(field) infos[t].inputs[infos[t].input_count].name = #field; \
    infos[t].inputs[infos[t].input_count++].offset = ((size_t)&it.field) - ((size_t)&it);

#define OUTPUT(field) infos[t].outputs[infos[t].output_count].name = #field; \
    infos[t].outputs[infos[t].output_count++].offset = ((size_t)&it.field) - ((size_t)&it);

#define NODE(type, tag, ...) \
{ \
    struct type it; \
    node_type t = tag; \
    infos[tag].name = #type; \
    infos[tag].size = sizeof(struct type); \
    infos[tag].input_count = 0; \
    infos[tag].output_count = 0; \
    __VA_ARGS__\
}

    struct node_info *infos = malloc(NODE_TYPE_COUNT * sizeof(struct node_info));

    NODE(sum, NODE_SUM, INPUT(in0) INPUT(in1) OUTPUT(out));
    NODE(sum3, NODE_SUM3, INPUT(in0) INPUT(in1) INPUT(in2) OUTPUT(out));
    NODE(negate, NODE_NEGATE, INPUT(in) OUTPUT(out));

    return infos;
#undef INPUT
#undef OUTPUT
#undef NODE
}

#endif