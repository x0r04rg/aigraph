#include "collections.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

struct vector vector_create(size_t item_size, size_t capacity)
{
    assert(item_size > 0);
    assert(capacity >= 0);
    struct vector result;
    result.capacity = capacity;
    result.item_size = item_size;
    result.size = 0;
    result.buffer = capacity > 0 ? malloc(item_size * capacity) : NULL;
}

void vector_destroy(struct vector *v)
{
    assert(v);
    if (v->buffer) free(v->buffer);
}

void *vector_push(struct vector *v, void *item)
{
    assert(v);
    if (v->size == v->capacity)
    {
        v->capacity = (v->capacity + 1) * 2;
        v->buffer = realloc(v->buffer, v->capacity * v->item_size);
    }
    v->size += 1;
    return vector_set(v, v->size - 1, item);
}

void vector_pop(struct vector *v)
{
    assert(v->size > 0);
    v->size -= 1;
}

void *vector_get(struct vector *v, size_t i)
{
    assert(v);
    assert(i < v->size);
    return v->buffer + i * v->item_size;
}

void *vector_set(struct vector *v, size_t i, void *item)
{
    assert(v);
    assert(item);
    void *dest = vector_get(v, i);
    memcpy(dest, item, v->item_size);
    return dest;
}

void vector_remove(struct vector *v, size_t i)
{
    assert(v);
    assert(i < v->size);
    char *item = vector_get(v, i);
    memmove(item, item + v->item_size, (v->size - i - 1) * v->item_size);
}

void vector_swap_remove(struct vector *v, size_t i)
{
    assert(v);
    assert(i < v->size);
    if (i != v->size - 1)
    {
        char *item = vector_get(v, i);
        char *last = vector_get(v, v->size - 1);
        memcpy(item, last, v->item_size);
    }
    vector_pop(v);
}