#ifndef HASH_TO_TEST_HPP
#define HASH_TO_TEST_HPP

#include <vector>
#include <random>
#include <algorithm>
#include <iostream>
#include <iomanip>


enum class HashType{
    HASH_MODULO,
    HASH_BITWISE_AND,
    HASH_FIBONACCI,
    HASH_MURMUR3_32,
    HASH_MURMUR3_64,
    HASH_CRIPTO_TEA //Tiny-Encryption-Algorithm to test one criptography function 
};

// --- EXPERIMENTAL HASH FUNCTIONS ---
// These functions are used solely for the distribution analysis 
// phase to select the optimal design choice for the final build.

inline uint32_t hash_modulo(uint64_t key, uint32_t P) {
    return key % P;
}

inline uint32_t hash_bitwise_and(uint64_t key, uint32_t P) {
    return key & (P - 1);
}

inline uint32_t hash_fibonacci(uint64_t key, uint32_t p_exp) {
    
    const uint64_t FIB_CONST = 11400714819323198485ULL;
    return (key * FIB_CONST) >> (64 - p_exp);
}

inline uint32_t hash_murmur3_64(uint64_t key, uint32_t p_exp) {
    key ^= key >> 33;
    key *= 0xff51afd7ed558ccdULL;
    key ^= key >> 33;
    key *= 0xc4ceb9fe1a85ec53ULL;
    key ^= key >> 33;
    return key >> (64 - p_exp);
}

// Plain Murmur3 finalizer fmix32 
inline uint32_t hash_murmur3_32(uint64_t key, uint32_t p_exp) {
    uint32_t h = static_cast<uint32_t>(key ^ (key >> 32));
    
    h ^= h >> 16;
    h *= 0x85ebca6bU;
    h ^= h >> 13;
    h *= 0xc2b2ae35U;
    h ^= h >> 16;

    return h >> (32 - p_exp);
}


inline uint32_t hash_crypto_tea(uint64_t key, uint32_t p_exp) {
    uint32_t v0 = static_cast<uint32_t>(key);
    uint32_t v1 = static_cast<uint32_t>(key >> 32);

    uint32_t sum = 0;
    const uint32_t delta = 0x9E3779B9; 
    const uint32_t k0 = 0xA56BAB10, k1 = 0x00112233, k2 = 0x44556677, k3 = 0x8899AABB;
   
    for (int i = 0; i < 8; i++) {
        sum += delta;
        v0 += ((v1 << 4) + k0) ^ (v1 + sum) ^ ((v1 >> 5) + k1);
        v1 += ((v0 << 4) + k2) ^ (v0 + sum) ^ ((v0 >> 5) + k3);
    }
   
    uint64_t hashed_key = (static_cast<uint64_t>(v1) << 32) | v0;
    return static_cast<uint32_t>(hashed_key >> (64 - p_exp));
}

#endif