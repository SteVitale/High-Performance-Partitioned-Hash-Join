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

// Vectorized Murmur3 fmix32 partition mapping using AVX2 intrinsics
inline void hash_murmur3_32_256bit(const uint64_t* keys, uint32_t* out, uint32_t p_exp) {
    const __m256i C1 = _mm256_set1_epi32(0x85ebca6b);
    const __m256i C2 = _mm256_set1_epi32(0xc2b2ae35);

    // 1. LOAD: 
    // 16 keys are loaded : 64bit x 16 = 1024bit => 4 registers of 256bit (A, B, C ,D)
    __m256i A = _mm256_loadu_si256((const __m256i*)&keys[0]);
    __m256i B = _mm256_loadu_si256((const __m256i*)&keys[4]);
    __m256i C = _mm256_loadu_si256((const __m256i*)&keys[8]);
    __m256i D = _mm256_loadu_si256((const __m256i*)&keys[12]);

    // 2. FOLD: 
    // From each key we need to extract 32 folded bits ( k = k ^ (k >> 32))
    // So we perform xor and right-shift for the reduction 64 to 32 bits
    A = _mm256_xor_si256(A, _mm256_srli_epi64(A, 32));
    B = _mm256_xor_si256(B, _mm256_srli_epi64(B, 32));
    C = _mm256_xor_si256(C, _mm256_srli_epi64(C, 32));
    D = _mm256_xor_si256(D, _mm256_srli_epi64(D, 32));

    // 3. PRE-COMPACTION 
    // The folding phase leaves 50% of the SIMD lanes filled with garbage (upper 32 bits of each 64-bit block).
    // _MM_SHUFFLE pushes the valid 32-bit hashes to the lower halves of each 128-bit lane.
    A = _mm256_shuffle_epi32(A, _MM_SHUFFLE(3, 1, 2, 0));
    B = _mm256_shuffle_epi32(B, _MM_SHUFFLE(3, 1, 2, 0));
    C = _mm256_shuffle_epi32(C, _MM_SHUFFLE(3, 1, 2, 0));
    D = _mm256_shuffle_epi32(D, _MM_SHUFFLE(3, 1, 2, 0));

    // We extract the lower 64-bits from both the lower and upper 128-bit lanes,
    // merging them into perfectly dense 128-bit registers containing four 32-bit hashes each.
    __m128i comp_A = _mm_unpacklo_epi64(_mm256_castsi256_si128(A), _mm256_extracti128_si256(A, 1));
    __m128i comp_B = _mm_unpacklo_epi64(_mm256_castsi256_si128(B), _mm256_extracti128_si256(B, 1));
    __m128i comp_C = _mm_unpacklo_epi64(_mm256_castsi256_si128(C), _mm256_extracti128_si256(C, 1));
    __m128i comp_D = _mm_unpacklo_epi64(_mm256_castsi256_si128(D), _mm256_extracti128_si256(D, 1));

    // 4. COMBINE: 
    // Now the 16 valid keys weigh 16 x 32-bit = 512-bit. 
    // We fuse the four 128-bit blocks into two fully saturated 256-bit AVX2 registers.
    __m256i H1 = _mm256_set_m128i(comp_B, comp_A);
    __m256i H2 = _mm256_set_m128i(comp_D, comp_C);
    
    // 5. MURMUR3:
    // hash function is applied simultaneously to H1 and H2 (each one contains 8 key of 32bit)
    H1 = _mm256_xor_si256(H1, _mm256_srli_epi32(H1, 16));
    H2 = _mm256_xor_si256(H2, _mm256_srli_epi32(H2, 16));

    H1 = _mm256_mullo_epi32(H1, C1);
    H2 = _mm256_mullo_epi32(H2, C1);

    H1 = _mm256_xor_si256(H1, _mm256_srli_epi32(H1, 13));
    H2 = _mm256_xor_si256(H2, _mm256_srli_epi32(H2, 13));

    H1 = _mm256_mullo_epi32(H1, C2);
    H2 = _mm256_mullo_epi32(H2, C2);

    H1 = _mm256_xor_si256(H1, _mm256_srli_epi32(H1, 16));
    H2 = _mm256_xor_si256(H2, _mm256_srli_epi32(H2, 16));

    H1 = _mm256_srli_epi32(H1, 32 - p_exp);
    H2 = _mm256_srli_epi32(H2, 32 - p_exp);

    // 6. STORE: 
    // Writes exactly 16 contiguous 32-bit partition IDs to memory
    _mm256_storeu_si256((__m256i*)&out[0], H1);
    _mm256_storeu_si256((__m256i*)&out[8], H2);
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
        hash_murmur3_32_256bit(&keys_id[i], &parts_id[i], p_exp);
    }
    for (size_t i = N16; i < N; ++i) {
        parts_id[i] = hash_murmur3_32(keys_id[i], p_exp);
    }

    std::vector<double> time_iterations(ITERATIONS, 0);

    for (size_t iter = 0; iter < ITERATIONS; ++iter) {
        auto time_start = start();

        for (size_t i = 0; i < N16; i += 16) {
            hash_murmur3_32_256bit(&keys_id[i], &parts_id[i], p_exp);
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