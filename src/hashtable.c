#include "collections.h"
#include <malloc.h>
#include <assert.h>
#include <string.h>

struct hashtable_entry
{
    size_t hash;
    int next;
    bool free;
    char buffer[];
};

struct find_entry_result
{
    int index, prev, iterations;
};

static inline int hash_to_index(struct hashtable *ht, size_t hash)
{
    return (int)(11400714819323198485ull * hash >> (sizeof(hash) * 8 - ht->log_capacity));
}

struct find_entry_result find_entry(struct hashtable *ht, size_t bucket, size_t hash, void *key)
{
    struct find_entry_result result = { .index = -1, .prev = -1, .iterations = 0 };
    int start = ht->buckets[bucket];
    int prev = 0;
    for (int i = start; i != -1; prev = i, i = ht->entries[i].next)
    {
        result.iterations += 1;
        if (ht->entries[i].hash == hash && ht->equals(ht->entries[i].buffer, key))
        {
            result.index = i;
            result.prev = prev;
            return result;
        }
    }
    return result;
}

static void init_buffer(struct hashtable *ht, size_t log_capacity)
{
    assert(log_capacity);
    assert(log_capacity < 32);
    ht->log_capacity = log_capacity;
    size_t capacity = 1 << ht->log_capacity;
    size_t entry_size = sizeof(struct hashtable_entry) + key_size + val_size;
    ht->buffer = log_capacity > 0 ? malloc(capacity * (sizeof(*ht->buckets) + entry_size)) : NULL;
    ht->buckets = ht->buffer;
    ht->entries = (struct hashtable_entry*)(ht->buckets + capacity);
    for (int i = 0; i < capacity; ++i)
    {
        ht->buckets[i] = -1;
        ht->entries[i].free = true;
        ht->entries[i].next = i + 1;
    }
    ht->entries[capacity - 1].next = -1;
    ht->first_free = 0;
}

static void *get_value(struct hashtable *ht, int entry_index)
{
    return ht->entries[index].buffer + ht->key_size;
}

static void grow(struct hashtable *ht);

static void *insert(struct hashtable *ht, size_t hash, void *key, void *value)
{
    size_t bucket = hash_to_index(ht, hash);
    struct find_entry_result r = find_entry(ht, bucket, hash, key);

    if (r.index != -1)
    {
        memcpy(get_value(ht, r.index), value, ht->value_size);
        return;
    }
    
    if (r.iterations >= ht->log_capacity || ht->first_free == -1)
    {
        grow(ht);
        insert(ht, hash, key, value);
        return;
    }

    int index = ht->first_free;
    struct hashtable_entry *entry = &ht->entries[index];
    ht->first_free = entry->next;
    
    entry->free = false;
    entry->hash = hash;
    entry->next = ht->buckets[bucket];
    memcpy(entry->buffer, key, ht->key_size);
    memcpy(entry->buffer + ht->key_size, value, ht->value_size);

    ht->buckets[bucket] = index;

    return entry->buffer + ht->key_size;
}

static void grow(struct hashtable *ht)
{
    char *old_buffer = ht->buffer;
    struct hashtable_entry *old_entries = ht->entries;
    int old_entries_count = 1 << ht->log_capacity;

    init_buffer(ht, ht->log_capacity + 1);

    for (int i = 0; i < old_entries_count; ++i)
    {
        if (!old_entries[i].free)
        {
            size_t hash = old_entries[i].hash;
            void *key = old_entries[i].buffer;
            void *value = old_entries[i].buffer + ht->key_size;
            insert(ht, hash, key, value);
        }
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
    ht.first_free = -1;
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
    insert(ht, hash, key, value);
}

void *hashtable_get(struct hashtable *ht, void *key)
{
    size_t hash = ht->hash(key);
    size_t bucket = hash_to_index(ht, hash);
    struct find_entry_result r = find_entry(ht, bucket, hash, key);
    if (r.index != -1) return get_value(ht, r.index);
}

bool hashtable_remove(struct hashtable *ht, void *key)
{
    size_t hash = ht->hash(key);
    size_t bucket = hash_to_index(ht, hash);
    struct find_entry_result r = find_entry(ht, bucket, hash, key);
    if (r.index == -1) return false;
    struct hashtable_entry *entry = &ht->entries[r.index];
    if (r.prev == -1)
        ht->buckets[bucket] = entry->next;
    else
        ht->entries[r.prev].next = entry->next;
    entry->free = true;
    entry->next = ht->first_free;
    ht->first_free = r.index;
}