#ifndef OMP_UTILS_HPP
#define OMP_UTILS_HPP


#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
#include "omp.h"


// ------------------------------------------------------------
// Partitioned relation metadata
// ------------------------------------------------------------

struct Record {
    std::uint64_t key{};
};


// Partitioned relation metadata
struct PartitionedRelation {
    Record* data; //instead of std::vector<Record>
    std::vector<std::size_t> begin;
    std::vector<std::size_t> end;
};

// Join result
struct JoinResult {
    std::uint64_t join_count = 0;
    std::uint64_t checksum1  = 0;
    std::uint64_t checksum2  = 0;
};

struct HistResult {
    std::vector<std::size_t> global_hist;
    std::vector<std::vector<std::size_t>> local_hists;
};

static bool is_power_of_two(std::uint32_t x) {
    return x != 0 && (x & (x - 1U)) == 0;
}

// ------------------------------------------------------------
// DETERMINISTIC UNIFORM DISTRIBUTION 
// ------------------------------------------------------------
static inline std::uint64_t splitmix64_mix(std::uint64_t x) {
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    x = x ^ (x >> 31);
    return x;
}
static inline std::uint64_t splitmix64(std::uint64_t x) {
    return splitmix64_mix(x + 0x9e3779b97f4a7c15ULL);
}
static inline std::uint64_t splitmix64_next(std::uint64_t& state) {
    state += 0x9e3779b97f4a7c15ULL;
    return splitmix64_mix(state);
}

// Plain Murmur3 finalizer fmix32 
static inline uint32_t hash_murmur3_32(uint64_t key, uint32_t p_exp) {
    uint32_t h = static_cast<uint32_t>(key ^ (key >> 32));
    
    h ^= h >> 16;
    h *= 0x85ebca6bU;
    h ^= h >> 13;
    h *= 0xc2b2ae35U;
    h ^= h >> 16;

    return h >> (32 - p_exp);
}


// ------------------------------------------------------------
// Histogram
// ------------------------------------------------------------
static HistResult compute_histogram_parallel(const std::vector<Record>& rel, 
                                                    std::uint32_t p, 
                                                    std::uint32_t p_exp, std::size_t num_thread) {
    

    // local_hists[i] => local histogram of i-thread
    std::vector<std::vector<std::size_t>> local_hists(num_thread, std::vector<std::size_t>(p, 0));
    
    // hist => global output array returned
    std::vector<std::size_t> hist(p, 0);

    // static chunking of the dataset into the number of threads
    #pragma omp parallel for schedule(static) num_threads(num_thread) \
            default(none) shared(rel, p_exp, local_hists)
    for (size_t i = 0; i < rel.size(); ++i) {
        const int t = omp_get_thread_num();
        const std::uint32_t pid = hash_murmur3_32(rel[i].key, p_exp);
        ++local_hists[t][pid];
    }

    // Sequential Reduction as the hashjoin_par version
    for (size_t pid = 0; pid < p; ++pid) {
        for (size_t t = 0; t < num_thread; ++t) {
            hist[pid] += local_hists[t][pid];
        }
    }

    return {std::move(hist), std::move(local_hists)};
}


// ------------------------------------------------------------
// Vector of write cursors for each thread
// ------------------------------------------------------------
static std::vector<std::vector<std::size_t>> compute_local_offsets(
    const std::vector<std::size_t>& global_begin,
    const std::vector<std::vector<std::size_t>>& local_hists) {

    size_t num_thd = local_hists.size();
    size_t p = global_begin.size();
    
    // Write Cursors Matrix (#Threads x #Partitions):
    // local_next[t][pid] will hold the exact array index where thread 't' must 
    // begin writing its elements for partition 'pid'.
    std::vector<std::vector<std::size_t>> local_next(num_thd, std::vector<std::size_t>(p, 0));
    
    // Iterate over partition (columns)
    for (size_t pid = 0; pid < p; ++pid) {
        
        // Initialize the cursor at the global starting boundary of the current partition
        size_t current_offset = global_begin[pid];
        
        // Assign the cursor to each thread sequentially and advance it by the thread's exact payload size
        for (size_t t = 0; t < num_thd; ++t) {
            local_next[t][pid] = current_offset;
            
            // "Reserve" the memory chunk for thread 't' by advancing the offset.
            // This ensures thread 't+1' writes strictly after thread 't' finishes its chunk.
            current_offset += local_hists[t][pid]; 
        }
    }
    
    return local_next;
}


// ------------------------------------------------------------
// Scatter into a partitioned array
// ------------------------------------------------------------
static Record* scatter_partitioned_parallel(
    const std::vector<Record>& rel,
    std::uint32_t p_exp,
    std::vector<std::vector<std::size_t>>& local_next, 
    std::size_t num_thread) {

    // Allocates virtual memory, so that the main thread doesn't touch the physical memory
    Record* out_ptr = static_cast<Record*>(std::malloc(rel.size() * sizeof(Record)));
    

    #pragma omp parallel for schedule(static) num_threads(num_thread) \
            default(none) shared(rel, p_exp, local_next, out_ptr)
    for (size_t i = 0; i < rel.size(); ++i) {

        const int t = omp_get_thread_num();
        const std::uint32_t pid = hash_murmur3_32(rel[i].key, p_exp);

        // Lock-Free Writing (each thread writes in its [t] space)
        out_ptr[local_next[t][pid]++] = rel[i];
    }

    return out_ptr;
}


#endif