#pragma once

uint64_t murmur_hash_64(const void *key, uint32_t len, uint64_t seed);
uint64_t string_hash(const void *string);