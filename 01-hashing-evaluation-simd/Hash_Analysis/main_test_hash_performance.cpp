// main_hash_test_performance.cpp
//
// Measures pure computational throughput (MOps/s) of 5 hash functions
// across different memory tiers: L1 → L2 → L3 → RAM.
//
// Output: CSV to stdout  (redirect to file with > results.csv)

#include <iostream>
#include <vector>
#include <chrono>
#include <string>
#include <cstdint>
#include <numeric>

#include "distribution.hpp"
#include "hash_to_test.hpp"

// Anti-DCE sink
// Volatile prevents the compiler from eliminating the kernel loop
// even under -O3. We XOR-accumulate into it after every run.
static volatile uint64_t dce_sink = 0;

using Clock   = std::chrono::high_resolution_clock;
using Seconds = std::chrono::duration<double>;

static inline double elapsed_sec(Clock::time_point t0, Clock::time_point t1) {
    return Seconds(t1 - t0).count();
}

double run_kernel(const std::vector<uint64_t>& keys,
                  std::vector<uint32_t>&       parts,
                  HashType                     hash_type,
                  uint32_t                     P,
                  uint32_t                     p_exp)
{
    const size_t N = keys.size();

    auto t0 = Clock::now();

    switch (hash_type) {
        case HashType::HASH_MODULO:
            for (size_t i = 0; i < N; ++i)
                parts[i] = hash_modulo(keys[i], P);
            break;

        case HashType::HASH_BITWISE_AND:
            for (size_t i = 0; i < N; ++i)
                parts[i] = hash_bitwise_and(keys[i], P);
            break;

        case HashType::HASH_FIBONACCI:
            for (size_t i = 0; i < N; ++i)
                parts[i] = hash_fibonacci(keys[i], p_exp);
            break;

        case HashType::HASH_MURMUR3_64:
            for (size_t i = 0; i < N; ++i)
                parts[i] = hash_murmur3_64(keys[i], p_exp);
            break;
        
        case HashType::HASH_MURMUR3_32:
            for (size_t i = 0; i < N; ++i)
                parts[i] = hash_murmur3_32(keys[i], p_exp);
            break;

        case HashType::HASH_CRIPTO_TEA:
            for (size_t i = 0; i < N; ++i)
                parts[i] = hash_crypto_tea(keys[i], p_exp);
            break;
    }

    auto t1 = Clock::now();

    // Anti-DCE: force the compiler to materialise the output array.
    // We sum a few elements and push them into the volatile sink.
    uint64_t tmp = static_cast<uint64_t>(parts[0])
             ^ static_cast<uint64_t>(parts[N - 1]);
    dce_sink = dce_sink ^ tmp;

    return elapsed_sec(t0, t1);
}

int main()
{
    //Fixed parameters
    constexpr uint32_t P_EXP = 20;
    constexpr uint32_t P     = 1U << P_EXP;   // 1,048,576
    constexpr uint64_t SEED  = 47;
    constexpr int      REPS  = 9;   // odd → clean median

    // node09 - AMD EPYC 7301
    //   N =     1 000  →    12 KB  (L1d = 32 KB per core)
    //   N =    10 000  →   120 KB  (L1d boundary)
    //   N =   100 000  →   1.2 MB  (L2 = 512 KB per core → già L2 miss)
    //   N = 1 000 000  →    12 MB  (L3 boundary)
    //   N =10 000 000  →   120 MB  (RAM)
    //   N =50 000 000  →   600 MB  (RAM, stable)
    const std::vector<size_t> N_sizes = {
        1'000,
        10'000,
        100'000,
        1'000'000,
        10'000'000,
        50'000'000,
        100'000'000
    };

    struct HashEntry {
        HashType    type;
        std::string name;
    };

    const std::vector<HashEntry> hashes = {
        { HashType::HASH_MODULO,      "Modulo"      },
        { HashType::HASH_BITWISE_AND, "Bitwise_AND" },
        { HashType::HASH_FIBONACCI,   "Fibonacci"   },
        { HashType::HASH_MURMUR3_32,  "Murmur3_32"  },
        { HashType::HASH_MURMUR3_64,  "Murmur3_64"  },
        { HashType::HASH_CRIPTO_TEA,  "CriptoTEA"   },
    };

    //CSV header
    std::cout << "N,HashFunction,Throughput_MOps\n";

    // Outer loop: N sizes
    for (size_t N : N_sizes) {

        // Allocate and generate ONCE per N, shared across all hashes.
        // This ensures every hash sees the same input data.
        std::vector<uint64_t> keys  = generate_keys(N, P, SEED, DistType::DIST_UNIFORM);
        std::vector<uint32_t> parts(N, 0);

        //Inner loop: hash functions
        for (const auto& h : hashes) {

            // Warm-up: one un-timed run to populate caches and
            // train the branch predictor before we start measuring.
            run_kernel(keys, parts, h.type, P, P_EXP);

            // Timed repetitions
            std::vector<double> times(REPS);
            for (int r = 0; r < REPS; ++r)
                times[r] = run_kernel(keys, parts, h.type, P, P_EXP);

            // Median (sort in-place — REPS is tiny, cost negligible)
            std::sort(times.begin(), times.end());
            double median_sec = times[REPS / 2];

            double throughput_mops = (static_cast<double>(N) / median_sec) / 1e6;

            std::cout << N               << ","
                      << h.name          << ","
                      << throughput_mops << "\n";
        }
    }

    // Flush the sink to stderr so the compiler cannot dead-strip it
    std::cerr << "[dce_sink=" << dce_sink << "]\n";

    return 0;
}