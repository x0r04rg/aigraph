#include "allocator.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>

static void* allocator_linear_alloc(struct allocator *base, size_t size)
{
    struct linear_allocator *a = base;
    if (a->cursor + size > a->capacity)
    {
        a->capacity = (a->capacity + 1) * 2;
        if (a->capacity < a->cursor + size) a->capacity = a->cursor + size;
        a->buffer = realloc(a->buffer, a->capacity);
    }
    void *result = ((char*)a->buffer) + a->cursor;
    a->cursor += size;
    return result;
}

static void allocator_linear_free(struct allocator *base, void *buf)
{
    /* linear allocator can't free */
}

struct linear_allocator allocator_linear_create(size_t capacity)
{
    struct linear_allocator a;
    a.base.alloc = &allocator_linear_alloc;
    a.base.free = &allocator_linear_free;
    a.base.destroy = &linear_allocator_destroy;
    a.capacity = capacity;
    a.cursor = 0;
    a.buffer = capacity > 0 ? malloc(capacity) : NULL;
    return a;
}

void allocator_linear_destroy(struct linear_allocator *a)
{
    if (a->buffer) free(a->buffer);
}

static void *allocator_stack_alloc(struct allocator *base, size_t size)
{
    size = size + sizeof(size_t);
    struct stack_allocator *a = base;
    if (a->cursor + size > a->capacity)
    {
        a->capacity = (a->capacity + 1) * 2;
        if (a->capacity < a->cursor + size) a->capacity = a->cursor + size;
        a->buffer = realloc(a->buffer, a->capacity);
    }
    void *result = ((char*)a->buffer) + a->cursor;
    *((size_t*)result) = size;
    result = ((size_t*)result) + 1;
    a->cursor += size;
    return result;
}

static void allocator_stack_free(struct allocator *base, void *buf)
{
    struct stack_allocator *a = base;
    buf = ((size_t*)buf) - 1;
    size_t size = *((size_t*)buf);

    assert((char*)buf + size == (char*)a->buffer + a->cursor);
    
    a->cursor -= size;
}

void allocator_stack_deinit(struct stack_allocator *a)
{
    if (a->buffer) free(a->buffer);
}

struct stack_allocator allocator_stack_create(size_t capacity)
{
    struct stack_allocator a;
    a.base.alloc = &allocator_stack_alloc;
    a.base.free = &allocator_stack_free;
    a.base.destroy = &allocator_stack_deinit;
    a.capacity = capacity;
    a.cursor = 0;
    a.buffer = capacity > 0 ? malloc(capacity) : NULL;
    return a;
}

void *mem_alloc(struct allocator *a, size_t size)
{
    return a->alloc(a, size);
}

void *mem_calloc(struct allocator *a, size_t count, size_t size)
{
    size_t total_size = size * count;
    void *result = mem_alloc(a, total_size);
    memset(result, 0, total_size);
    return result;
}

void mem_free(struct allocator *a, void *buf)
{
    a->free(a, buf);
}