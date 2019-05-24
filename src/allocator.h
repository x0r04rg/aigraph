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

struct linear_allocator allocator_linear_create(size_t capacity);
void allocator_linear_destroy(struct linear_allocator *a);
struct stack_allocator allocator_stack_create(size_t capacity);
void allocator_stack_destroy(struct stack_allocator *a);

void *mem_alloc(struct allocator *a, size_t size);
void *mem_calloc(struct allocator *a, size_t count, size_t size);
void mem_free(struct allocator *a, void *buf);