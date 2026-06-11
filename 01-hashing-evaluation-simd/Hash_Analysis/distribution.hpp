#ifndef DISTRIBUTION_HPP
#define DISTRIBUTION_HPP

#include <vector>
#include <random>
#include <algorithm>
#include <iostream>
#include <iomanip>

enum class DistType {
    DIST_UNIFORM,         //Key Space perfectly distribuited
    DIST_MULTIPLES,       //Key Space with each key is multiple of 64
    DIST_DUPLICATES,      //Key Space with many duplicates key
    DIST_SEQUENTIAL       //Key Space perfectly sequential 0 ... N-1  
};

struct HashStats {
    double overload_factor;  // M / ideal
    double cv;            // σ / μ  — Coefficient of Variation
};

std::vector<uint64_t> generate_keys(size_t N, uint32_t P, uint64_t seed, DistType distType){
    std::vector<uint64_t> keys(N);
    std::mt19937_64 rng(seed);

    switch(distType){
        case DistType::DIST_UNIFORM:
            //Key Space is perfectly Uniform: each key is random over 64-bit value
            {
                std::uniform_int_distribution<uint64_t> dist;
                for (size_t i = 0; i < N; ++i) 
                    keys[i] = dist(rng);
            }
            break;

        case DistType::DIST_MULTIPLES:
            //Key Space is the worst possible: each key is a multiple of 64
            for (size_t i = 0; i < N; ++i) 
                keys[i] = static_cast<uint64_t>(i + 1) * 64;
            break;
        case DistType::DIST_DUPLICATES:
            // Key Space with heavy duplicates: given N, we use only a small portion to fit the key space
            {
                std::uniform_int_distribution<uint64_t> dist;

                // Dynamic pool size (i.e. N = 50'000'000 => 50'000 unique keys)
                // std::max ensures that the pool is AT LEAST 1 element even for very small N
                size_t pool_size = std::max<size_t>(1, N / 1000); 
                
                std::vector<uint64_t> small_pool(pool_size);

                for (size_t i = 0; i < pool_size; ++i) 
                    small_pool[i] = dist(rng);
                
                for (size_t i = 0; i < N; ++i) 
                    keys[i] = small_pool[i % pool_size];
            }
            break;
        case DistType::DIST_SEQUENTIAL:
            // Sequential Keys
            for (size_t i = 0; i < N; ++i)
                keys[i] = static_cast<uint64_t>(i);
            break;
    }
    
    return keys;
}

 
// parts_id : vettore degli ID di partizione prodotti dalla hash  (valori in [0, P))
// P        : numero di partizioni
HashStats compute_stats(const std::vector<uint32_t>& parts_id, uint32_t P) {
    
    // 1. Istogramma
    std::vector<size_t> hist(P, 0);
    for (uint32_t pid : parts_id)
        hist[pid]++;
 
    const double N     = static_cast<double>(parts_id.size());
    const double ideal = N / P;   // μ = carico atteso per partizione
 
    // 2. Max Skew
    const size_t M        = *std::max_element(hist.begin(), hist.end());
    //const double max_skew = (static_cast<double>(M) - ideal) / ideal * 100.0;
    const double overload_f = static_cast<double>(M) / ideal;

    // 3. CV = σ / μ
    double sum_sq_diff = 0.0;
    for (size_t c : hist) {
        double diff = static_cast<double>(c) - ideal;
        sum_sq_diff += diff * diff;
    }
    const double variance = sum_sq_diff / P;
    const double cv       = std::sqrt(variance) / ideal;
 
    return { overload_f, cv };
}


void print_stats(const HashStats& s, const std::string& dist_name, const std::string& hash_name) {
    std::cout << std::fixed << std::setprecision(4)
              << "  Overload Factor: " << std::setw(8) << s.overload_factor << "x"
              << "   Coeff. Of Variation: "     << std::setw(8) << s.cv
              << "   [" << hash_name << " x " << dist_name << "]\n";
}

#endif