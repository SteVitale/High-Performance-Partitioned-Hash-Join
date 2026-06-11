#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <fstream>
#include <algorithm>
#include <numeric>   
#include <cmath>     

#include "Hash_Analysis/distribution.hpp"
#include <immintrin.h>

#define ITERATIONS 7 //odd => clean median center

auto start(){
    return std::chrono::steady_clock::now();
}

auto stop(std::chrono::steady_clock::time_point start){
    auto end = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(end - start).count();
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

// Vectorized Murmur3 fmix32 partition mapping using AVX intrinsics
__attribute__((target("avx"), always_inline))
inline void hash_murmur3_32_128bit(const uint64_t* keys, uint32_t* out, uint32_t p_exp) {
    // Murmur3 magic constants
    const __m128i C1 = _mm_set1_epi32(0x85ebca6b);
    const __m128i C2 = _mm_set1_epi32(0xc2b2ae35);

    // --- PHASE 1: 4-Way Unrolled Load (128-bit strict) ---
    // We load 16 keys (64-bit each) into 8 separate 128-bit registers.
    // This avoids the decoding penalty of 256-bit registers on AMD Zen 1.
    __m128i L0 = _mm_loadu_si128((const __m128i*)&keys[0]);
    __m128i L1 = _mm_loadu_si128((const __m128i*)&keys[2]);
    __m128i L2 = _mm_loadu_si128((const __m128i*)&keys[4]);
    __m128i L3 = _mm_loadu_si128((const __m128i*)&keys[6]);
    __m128i L4 = _mm_loadu_si128((const __m128i*)&keys[8]);
    __m128i L5 = _mm_loadu_si128((const __m128i*)&keys[10]);
    __m128i L6 = _mm_loadu_si128((const __m128i*)&keys[12]);
    __m128i L7 = _mm_loadu_si128((const __m128i*)&keys[14]);

    // --- PHASE 2: 64-bit to 32-bit Folding ---
    // key ^ (key >> 32). The valid 32-bit results are now in the lower half
    // of each 64-bit lane within the registers.
    L0 = _mm_xor_si128(L0, _mm_srli_epi64(L0, 32));
    L1 = _mm_xor_si128(L1, _mm_srli_epi64(L1, 32));
    L2 = _mm_xor_si128(L2, _mm_srli_epi64(L2, 32));
    L3 = _mm_xor_si128(L3, _mm_srli_epi64(L3, 32));
    L4 = _mm_xor_si128(L4, _mm_srli_epi64(L4, 32));
    L5 = _mm_xor_si128(L5, _mm_srli_epi64(L5, 32));
    L6 = _mm_xor_si128(L6, _mm_srli_epi64(L6, 32));
    L7 = _mm_xor_si128(L7, _mm_srli_epi64(L7, 32));

    // --- PHASE 3: Data Compaction ---
    // Step A: Shuffle to move the valid 32-bit results to the bottom of the registers.
    L0 = _mm_shuffle_epi32(L0, _MM_SHUFFLE(3, 1, 2, 0));
    L1 = _mm_shuffle_epi32(L1, _MM_SHUFFLE(3, 1, 2, 0));
    L2 = _mm_shuffle_epi32(L2, _MM_SHUFFLE(3, 1, 2, 0));
    L3 = _mm_shuffle_epi32(L3, _MM_SHUFFLE(3, 1, 2, 0));
    L4 = _mm_shuffle_epi32(L4, _MM_SHUFFLE(3, 1, 2, 0));
    L5 = _mm_shuffle_epi32(L5, _MM_SHUFFLE(3, 1, 2, 0));
    L6 = _mm_shuffle_epi32(L6, _MM_SHUFFLE(3, 1, 2, 0));
    L7 = _mm_shuffle_epi32(L7, _MM_SHUFFLE(3, 1, 2, 0));

    // Step B: Unpack to merge pairs of registers. 
    // We successfully compacted 8 sparsely populated registers into 4 fully saturated ones (H0-H3).
    __m128i H0 = _mm_unpacklo_epi64(L0, L1);
    __m128i H1 = _mm_unpacklo_epi64(L2, L3);
    __m128i H2 = _mm_unpacklo_epi64(L4, L5);
    __m128i H3 = _mm_unpacklo_epi64(L6, L7);

    // --- PHASE 4: Murmur3 fmix32 Avalanche Step ---
    // All math from here is purely 32-bit, perfectly fitting AVX capabilities.
    H0 = _mm_xor_si128(H0, _mm_srli_epi32(H0, 16));
    H1 = _mm_xor_si128(H1, _mm_srli_epi32(H1, 16));
    H2 = _mm_xor_si128(H2, _mm_srli_epi32(H2, 16));
    H3 = _mm_xor_si128(H3, _mm_srli_epi32(H3, 16));

    H0 = _mm_mullo_epi32(H0, C1);
    H1 = _mm_mullo_epi32(H1, C1);
    H2 = _mm_mullo_epi32(H2, C1);
    H3 = _mm_mullo_epi32(H3, C1);

    H0 = _mm_xor_si128(H0, _mm_srli_epi32(H0, 13));
    H1 = _mm_xor_si128(H1, _mm_srli_epi32(H1, 13));
    H2 = _mm_xor_si128(H2, _mm_srli_epi32(H2, 13));
    H3 = _mm_xor_si128(H3, _mm_srli_epi32(H3, 13));

    H0 = _mm_mullo_epi32(H0, C2);
    H1 = _mm_mullo_epi32(H1, C2);
    H2 = _mm_mullo_epi32(H2, C2);
    H3 = _mm_mullo_epi32(H3, C2);

    H0 = _mm_xor_si128(H0, _mm_srli_epi32(H0, 16));
    H1 = _mm_xor_si128(H1, _mm_srli_epi32(H1, 16));
    H2 = _mm_xor_si128(H2, _mm_srli_epi32(H2, 16));
    H3 = _mm_xor_si128(H3, _mm_srli_epi32(H3, 16));

    // --- PHASE 5: Partition Mapping & Store ---
    // Extract the top 'p_exp' bits to assign the final partition ID.
    H0 = _mm_srli_epi32(H0, 32 - p_exp);
    H1 = _mm_srli_epi32(H1, 32 - p_exp);
    H2 = _mm_srli_epi32(H2, 32 - p_exp);
    H3 = _mm_srli_epi32(H3, 32 - p_exp);

    // Store the final 16 partition IDs to memory.
    _mm_storeu_si128((__m128i*)&out[0],  H0);
    _mm_storeu_si128((__m128i*)&out[4],  H1);
    _mm_storeu_si128((__m128i*)&out[8],  H2);
    _mm_storeu_si128((__m128i*)&out[12], H3);
}





int main(int argc, char* argv[]){
         
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <millions_N> <exponent_P> \n";
        return 1;
    }

    const size_t N { static_cast<size_t>(std::stod(argv[1]) * 1'000'000 )}; 
    const uint32_t p_exp { static_cast<uint32_t>(std::stoi(argv[2])) };
    const uint32_t P { 1U << p_exp }; //20 => 2**20  
    const uint64_t SEED { 42 };  

    std::vector<uint64_t> keys_id(generate_keys(N, P, SEED, DistType::DIST_UNIFORM));
    std::vector<uint32_t> parts_id(N, 0); // each parts_id[i] = [0,P)

    
    const size_t N16 = N - (N % 16);

    // Warm Up - Not measured
    for (size_t i = 0; i < N16; i += 16) {
        hash_murmur3_32_128bit(&keys_id[i], &parts_id[i], p_exp);
    }
    for (size_t i = N16; i < N; ++i) {
        parts_id[i] = hash_murmur3_32(keys_id[i], p_exp);
    }

    std::vector<double> time_iterations(ITERATIONS, 0);

    for (size_t iter = 0; iter < ITERATIONS; ++iter) {
        auto time_start = start();

        for (size_t i = 0; i < N16; i += 16) {
            hash_murmur3_32_128bit(&keys_id[i], &parts_id[i], p_exp);
        }
        for (size_t i = N16; i < N; ++i) {
            parts_id[i] = hash_murmur3_32(keys_id[i], p_exp);
        }

        time_iterations[iter] = stop(time_start);
    }

    // Median
    std::sort(time_iterations.begin(), time_iterations.end());
    double median = time_iterations[ITERATIONS / 2];

    // Standard deviation
    double mean = std::accumulate(time_iterations.begin(), time_iterations.end(), 0.0) / ITERATIONS;
    double sq_sum = 0.0;
    for (double t : time_iterations) sq_sum += (t - mean) * (t - mean);
    double stddev = std::sqrt(sq_sum / ITERATIONS);

    double throughput = N / median;
    
    uint64_t checksum = 0;
    for (size_t i = 0; i < N; ++i) {
        checksum += parts_id[i];
    }
    
    // Output (extracted also from sh file)
    std::cout << std::fixed << std::setprecision(6);
    std::cout << "Median    : " << median               << " s\n";
    std::cout << "Stddev    : " << stddev               << " s\n";
    std::cout << "Throughput : " << throughput / 1e6 << " million-elements/s" << std::endl;
    std::cout << "Checksum <"<<N<<"> <"<<P<<"> : " << checksum << std::endl;
    
    // Always dump the first min(N, 10000) elements for correctness checking
    size_t dump_size = std::min(N, static_cast<size_t>(10000));
    std::string filename = std::string(argv[0]) + "_output.txt";
    std::ofstream f(filename);
    for (size_t i = 0; i < dump_size; ++i)
        f << parts_id[i] << "\n";
    
    return 0;
}