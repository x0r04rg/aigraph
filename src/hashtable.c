#include "collections.h"
#include <malloc.h>
#include <assert.h>
#include <string.h>

struct hashtable_entry
{
    size_t hash;
    int next, prev;
};

struct find_entry_result
{
    int index, prev, iterations;
};

// source: https://probablydance.com/2018/06/16/fibonacci-hashing-the-optimization-that-the-world-forgot-or-a-better-alternative-to-integer-modulo/
static inline int hash_to_index(struct hashtable *ht, size_t hash)
{
    // IMPORTANT: this will fail on non 64-bit platforms
    return (int)(11400714819323198485ull * hash >> (sizeof(hash) * 8 - ht->log_capacity));
}

struct find_entry_result find_entry(struct hashtable *ht, size_t bucket, size_t hash, void *key)
{
    struct find_entry_result result = { .index = -1, .prev = -1, .iterations = 0 };
    int start = ht->buckets[bucket];
    for (int i = start; i != -1; i = ht->entries[i].next)
    {
        result.iterations += 1;
        if (ht->entries[i].hash == hash && ht->equals(hashtable_get_key(ht, i), key))
        {
            result.index = i;
            break;
        }
        result.prev = i;
    }
    return result;
}

static void init_buffer(struct hashtable *ht, size_t log_capacity)
{
    assert(log_capacity);
    assert(log_capacity < 31);
    ht->log_capacity = log_capacity;
    size_t capacity = 1 << ht->log_capacity;
    size_t element_size = sizeof(struct hashtable_entry) + ht->key_size + ht->value_size;
    ht->buffer = log_capacity > 0 ? malloc(capacity * (sizeof(*ht->buckets) + element_size)) : NULL;
    ht->buckets = ht->buffer;
    ht->entries = (struct hashtable_entry*)(ht->buckets + capacity);
    ht->keys = ht->entries + capacity;
    ht->values = ((char*)ht->keys) + ht->key_size * capacity;
    for (size_t i = 0; i < capacity; ++i)
    {
        ht->buckets[i] = -1;
        ht->entries[i].next = -1;
        ht->entries[i].prev = -1;
    }
}

static void grow(struct hashtable *ht);

static void *insert(struct hashtable *ht, size_t hash, void *key, void *value)
{
    size_t bucket = hash_to_index(ht, hash);
    struct find_entry_result r = find_entry(ht, bucket, hash, key);

    if (r.index != -1)
    {
        memcpy(hashtable_get_value(ht, r.index), value, ht->value_size);
        return hashtable_get_value(ht, r.index);
    }
    
    size_t capacity = 1 << ht->log_capacity;
    if (r.iterations >= ht->log_capacity || ht->size == capacity)
    {
        float load_factor = (float)ht->size / (float)capacity;
        if (load_factor > 0.4f)
        {
            grow(ht);
            return insert(ht, hash, key, value);
        }
    }

    int index = ht->size;
    struct hashtable_entry *entry = &ht->entries[index];
    
    entry->hash = hash;
    entry->next = ht->buckets[bucket];
    entry->prev = -1;
    if (entry->next != -1) ht->entries[entry->next].prev = index;
    memcpy(hashtable_get_key(ht, index), key, ht->key_size);
    memcpy(hashtable_get_value(ht, index), value, ht->value_size);

    ht->buckets[bucket] = index;
    ht->size += 1;

    return hashtable_get_value(ht, index);
}

static void grow(struct hashtable *ht)
{
    void *old_buffer = ht->buffer;
    int old_count = ht->size;
    struct hashtable_entry *entry = ht->entries;
    char *key = ht->keys;
    char *value = ht->values;

    init_buffer(ht, ht->log_capacity + 1);

    for (int i = 0; i < old_count; ++i)
    {
        insert(ht, entry->hash, key, value);
        entry += 1;
        key += ht->key_size;
        value += ht->value_size;
    }

    free(old_buffer);
}

struct hashtable hashtable_create(size_t key_size, size_t val_size, int log_capacity, hash_func hash, equals_func equals)
{
    assert(log_capacity >= 0);
    assert(key_size > 0);
    assert(val_size > 0);
    assert(hash);
    assert(equals);
    struct hashtable ht;
    ht.key_size = key_size;
    ht.value_size = val_size;
    ht.log_capacity = 0;
    ht.hash = hash;
    ht.equals = equals;
    ht.size = 0;
    if (log_capacity) init_buffer(&ht, log_capacity);
    return ht;
}

void hashtable_destroy(struct hashtable *ht)
{
    if (ht->buffer) free(ht->buffer);
}

void *hashtable_insert(struct hashtable *ht, void *key, void *value)
{
    if (ht->log_capacity == 0) init_buffer(ht, 3);

    size_t hash = ht->hash(key);
    return insert(ht, hash, key, value);
}

void *hashtable_get(struct hashtable *ht, void *key)
{
    size_t hash = ht->hash(key);
    size_t bucket = hash_to_index(ht, hash);
    struct find_entry_result r = find_entry(ht, bucket, hash, key);
    if (r.index != -1) return hashtable_get_value(ht, r.index);
    return NULL;
}

bool hashtable_remove(struct hashtable *ht, void *key)
{
    size_t hash = ht->hash(key);
    size_t bucket = hash_to_index(ht, hash);
    struct find_entry_result r = find_entry(ht, bucket, hash, key);
    
    if (r.index == -1) return false;

    /* delete entry from linked list leaving hole in array */
    struct hashtable_entry *entry = &ht->entries[r.index];
    if (entry->prev == -1)
    {
        ht->buckets[bucket] = entry->next;
        if (entry->next != -1) ht->entries[entry->next].prev = -1;
    }
    else
    {
        ht->entries[entry->prev].next = entry->next;
        if (entry->next != -1) ht->entries[entry->next].prev = entry->prev;
    }

    ht->size -= 1;

    if (r.index == ht->size) return true;

    /* insert last entry to the newly created hole */
    struct hashtable_entry *last = &ht->entries[ht->size];
    if (last->prev == -1)
    {
        ht->buckets[hash_to_index(ht, last->hash)] = r.index;
    }
    else
    {
        ht->entries[last->prev].next = r.index;
    }
    memcpy(entry, last, sizeof *entry);
    memcpy(hashtable_get_key(ht, r.index), hashtable_get_key(ht, ht->size), ht->key_size);
    memcpy(hashtable_get_value(ht, r.index), hashtable_get_value(ht, ht->size), ht->value_size);

    return true;
}