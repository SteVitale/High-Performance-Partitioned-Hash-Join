#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <fstream>
#include <algorithm>
#include <numeric>   
#include <cmath>     
#include "Hash_Analysis/distribution.hpp"

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

    // Warm-up — not measured
    for (size_t i = 0; i < N; ++i)
        parts_id[i] = hash_murmur3_32(keys_id[i], p_exp);


    std::vector<double> time_iterations(ITERATIONS, 0);

    for(size_t iter=0; iter<ITERATIONS; ++iter){
        auto time_start = start();

        for(size_t i=0; i<N; ++i)
            parts_id[i] = hash_murmur3_32(keys_id[i], p_exp);

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
