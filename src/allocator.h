#pragma once

#include <stddef.h>

struct allocator
{
    void *(*alloc)(struct allocator*, size_t size);
    void (*free)(struct allocator*, void*);
};

struct linear_allocator
{
    struct allocator base;
    size_t cursor, capacity;
    void *buffer;
};

struct stack_allocator
{
    struct allocator base;
    size_t cursor, capacity;
    void *buffer;
};

void allocator_linear_init(struct linear_allocator *a, size_t capacity);
void allocator_linear_deinit(struct linear_allocator *a);
void allocator_stack_init(struct stack_allocator *a, size_t capacity);
void allocator_stack_deinit(struct stack_allocator *a);

void *mem_alloc(struct allocator *a, size_t size);
void *mem_calloc(struct allocator *a, size_t count, size_t size);
void mem_free(struct allocator *a, void *buf);