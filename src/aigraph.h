#ifndef AIGRAPH_H
#define AIGRAPH_H

#include <stddef.h>

enum { MAX_INPUTS = 5, MAX_OUTPUTS = 5 };

typedef enum
{
    NODE_SUM,
    NODE_INVERT,
    NODE_TYPE_COUNT
} node_type;

struct input_info
{
    char *name;
    int offset;
};

struct output_info
{
    char *name;
    int offset;
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
    node_type type;
};

struct sum
{
    struct node_base;
    float *in0, *in1, out;
};

struct inertia
{
    struct node_base;
    float last;
    float *in, out;
};

struct node_info infos[NODE_TYPE_COUNT];

void fill_infos()
{
    {
        infos[NODE_SUM].name = "Sum";
        infos[NODE_SUM].size = sizeof(struct sum);
        infos[NODE_SUM].input_count = 2;
        infos[NODE_SUM].inputs[0].name = "In 1";
        infos[NODE_SUM].inputs[0].offset = offsetof(struct sum, in0);
        infos[NODE_SUM].inputs[1].name = "In 2";
        infos[NODE_SUM].inputs[1].offset = offsetof(struct sum, in1);
        infos[NODE_SUM].output_count = 1;
        infos[NODE_SUM].outputs[0].name = "Out";
        infos[NODE_SUM].outputs[0].offset = offsetof(struct sum, out);
    }
}

#endif