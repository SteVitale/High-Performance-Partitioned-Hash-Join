#include <iostream>
#include <vector>
#include <cassert>

#include "distribution.hpp"
#include "hash_to_test.hpp"


std::vector<uint32_t> generatePartitions(std::vector<uint64_t>& keys, uint32_t P, HashType hashType){
    assert((P & (P-1)) == 0); //P power of 2

    uint32_t p_exp = __builtin_ctz(P); // logaritmo buildtin

    std::vector<uint32_t> parts_id(keys.size(), 0);

    switch (hashType){
        case HashType::HASH_MODULO:
            for(size_t i=0; i<keys.size(); ++i)
                parts_id[i] = hash_modulo(keys[i], P);
            break;

        case HashType::HASH_BITWISE_AND:
            for(size_t i=0; i<keys.size(); ++i)
                parts_id[i] = hash_bitwise_and(keys[i], P);
            break;

        case HashType::HASH_FIBONACCI:
            for (size_t i = 0; i < keys.size(); ++i)
                parts_id[i] = hash_fibonacci(keys[i], p_exp);
            break;

        case HashType::HASH_MURMUR3_64:
            for (size_t i = 0; i < keys.size(); ++i)
                parts_id[i] = hash_murmur3_64(keys[i], p_exp);
            break;
        
        case HashType::HASH_CRIPTO_TEA:
            for (size_t i = 0; i < keys.size(); ++i)
                parts_id[i] = hash_crypto_tea(keys[i], p_exp);
            break;
    }

    return parts_id;
}

int main(){

    // 1. Hardcoded params for testing
    const size_t N = 50'000'000;
    const uint32_t p_exp = 20;
    const uint32_t P = 1U << p_exp;
    const uint64_t SEED = 47;

    std::cout << "Starting Hash Benchmark (N=" << N << ", P=" << P << ")\n";

    // 2. Setup enum data
    DistType dists[] = {DistType::DIST_UNIFORM, DistType::DIST_MULTIPLES, DistType::DIST_DUPLICATES, DistType::DIST_SEQUENTIAL};
    std::string dist_names[] = {"Uniform", "Multiples", "Duplicates", "Sequential"};

    HashType hashes[] = {
                        HashType::HASH_MODULO, HashType::HASH_BITWISE_AND, 
                        HashType::HASH_FIBONACCI, HashType::HASH_MURMUR3_64, 
                        HashType::HASH_CRIPTO_TEA
                    };
    std::string hash_names[] = {"Modulo", "Bitwise_AND", "Fibonacci", "Murmur3", "CriptoTEA"};

    // 3. Loop for each distribution pattern
    for(size_t d = 0; d < 4; ++d) {
        std::cout << "\n========================================\n";
        std::cout << "GENERATING DISTRIBUTION: " << dist_names[d] << "\n";
        std::cout << "========================================\n";
        
        std::vector<uint64_t> keys = generate_keys(N, P, SEED, dists[d]);

        for(size_t h = 0; h < 5; ++h) {
            std::cout << "\n---> Testing Hash: " << hash_names[h] << "\n";
            
            std::vector<uint32_t> parts = generatePartitions(keys, P, hashes[h]);
            
            auto stats = compute_stats(parts, P);
            print_stats(stats, dist_names[d], hash_names[h]);
        }
    }

    return 0;
}