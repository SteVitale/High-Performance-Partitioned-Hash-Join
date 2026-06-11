#ifndef SIMPLE_HASH_TABLE_HPP
#define SIMPLE_HASH_TABLE_HPP

#include <vector>
#include <cstdint>
struct SimpleHashTable {
    // Structure of Arrays (SoA) layout for vectorization
    std::vector<std::uint64_t> keys;
    std::vector<std::uint32_t> counts;
    std::vector<std::uint8_t> occupied; // 0 = empty, 1 = occupied

    std::size_t capacity;
    std::size_t mask;

    // Constructor: takes the exact number of records this table will hold
    SimpleHashTable(std::size_t num_records) {
        // Enforce a maximum load factor of 50% to mitigate primary clustering
        std::size_t target_size = num_records * 2;
        
        // Round up to the next power of 2 for fast bitwise modulo operations
        capacity = 1;
        while (capacity < target_size) {
            capacity <<= 1;
        }
        if (capacity < 16) capacity = 16; // Minimum safe capacity
        
        // Mask used to replace (hash % capacity) with (hash & mask)
        mask = capacity - 1;

        // Zero-initialization
        keys.resize(capacity, 0);
        counts.resize(capacity, 0);
        occupied.resize(capacity, 0);
    }

    // Compute the hash value, uses the already used splitmix64
    inline std::size_t hash(std::uint64_t key) const {
        return static_cast<std::size_t>(splitmix64_mix(key));
    }


    // Insert with linear probing for collision resolution
    inline void insert(std::uint64_t key) {
        std::size_t idx = hash(key) & mask;

        // Probe until an empty slot is found
        while (occupied[idx]) {
            if (keys[idx] == key) {
                counts[idx]++; // Key match: handle duplicate by incrementing count
                return;
            }
            idx = (idx + 1) & mask; // Linear probing step
        }

        // Empty slot found, insert the new key
        occupied[idx] = 1;
        keys[idx] = key;
        counts[idx] = 1;
    }

    // Lookup with Linear Probing
    inline std::uint32_t get_count(std::uint64_t key) const {
        std::size_t idx = hash(key) & mask;

        // Probe until an empty slot is found (which means 'Miss')
        while (occupied[idx]) {
            if (keys[idx] == key) {
                return counts[idx]; // Match found, return multiplicity
            }
            idx = (idx + 1) & mask;
        }
        return 0; // Miss: The key does not exist in the build relation
    }

    static inline std::uint64_t splitmix64_mix(std::uint64_t x) {
        x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
        x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
        x = x ^ (x >> 31);
        return x;
    }
    
    static inline std::uint64_t splitmix64(std::uint64_t x) {
        return splitmix64_mix(x + 0x9e3779b97f4a7c15ULL);
    }



    // Only Used for the hashjoin_omp_nested.cpp
    inline const std::uint64_t* get_keys_array() const { return keys.data(); }
    inline const std::uint32_t* get_counts_array() const { return counts.data(); }
    inline const std::uint8_t* get_occupied_array() const { return occupied.data(); }
    inline const std::size_t get_capacity() const { return capacity;}
};

#endif