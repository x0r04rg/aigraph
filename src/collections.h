#pragma once

#include <stddef.h>
#include <stdbool.h>

typedef _Bool bool;

struct vector
{
    size_t size, capacity, item_size;
    char *buffer;
};

struct vector vector_create(size_t item_size, size_t capacity);
void vector_destroy(struct vector *v);
void *vector_push(struct vector *v, void *item);
void vector_pop(struct vector *v);
void *vector_get(struct vector *v, size_t i);
void *vector_set(struct vector *v, size_t i, void *item);
void vector_remove(struct vector *v, size_t i);
void vector_swap_remove(struct vector *v, size_t i);

struct hashtable_entry;

typedef size_t(*hash_func)(void*);
typedef bool(*equals_func)(void*, void*);

struct hashtable
{
    void *buffer;
    int *buckets;
    struct hashtable_entry *entries;
    void *keys, *values;
    size_t size, key_size, value_size;
    int log_capacity;
    hash_func hash;
    equals_func equals;
};

struct hashtable hashtable_create(size_t key_size, size_t val_size, int log_capacity, hash_func hash, equals_func equals);
void hashtable_destroy(struct hashtable *ht);
void *hashtable_insert(struct hashtable *ht, void *key, void *value);
void *hashtable_get(struct hashtable *ht, void *key);
bool hashtable_remove(struct hashtable *ht, void *key);
inline void *hashtable_get_key(struct hashtable *ht, int i) { return ((char*)ht->keys) + i * ht->key_size; }
inline void *hashtable_get_value(struct hashtable *ht, int i) { return ((char*)ht->values) + i * ht->value_size; }