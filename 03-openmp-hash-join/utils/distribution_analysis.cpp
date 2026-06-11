#include <iostream>
#include <vector>
#include <fstream>
#include <string>

#include "include/distribution.hpp" 

// ------------------------------------------------------------
// Plain Murmur3 finalizer fmix32 From Modulo 01
// ------------------------------------------------------------
static inline uint32_t hash_murmur3_32(uint64_t key, uint32_t p_exp) {
    uint32_t h = static_cast<uint32_t>(key ^ (key >> 32));
    
    h ^= h >> 16;
    h *= 0x85ebca6bU;
    h ^= h >> 13;
    h *= 0xc2b2ae35U;
    h ^= h >> 16;

    return h >> (32 - p_exp);
}

int main() {
    // ---------------------------------------------------------
    // Hardcoded Parameters
    // ---------------------------------------------------------
    const size_t N = 50000000;         // 50 Milioni
    const uint64_t MAX_KEY = 500000;
    const uint32_t P = 2048;           // 
    const uint32_t P_EXP = 11;         // 2^11 = 2048
    const uint64_t SEED = 42;
    
    // Raw data bins (1000 bins => each one contains 500 keys)
    const uint32_t NUM_BINS = 1000;    

    std::vector<std::string> dist_names = {"uniform", "multiples", "step_skew", "heavy_skew"};
    
    // Matrix for save the result
    std::vector<std::vector<size_t>> data_csv(4, std::vector<size_t>(NUM_BINS, 0));
    std::vector<std::vector<size_t>> part_csv(4, std::vector<size_t>(P, 0));


    for (int d = 0; d < 4; ++d) {
        std::cout << " - COMPUTING: " << dist_names[d] << "..." << std::flush;
        

        // Generate data 
        std::vector<Record> rel = generate_relation(N, SEED, MAX_KEY, parse_dist_type(dist_names[d]));

        for (size_t i = 0; i < N; ++i) {
            uint32_t key = rel[i].key;

            // A) Compute Bins for raw data
            // normalize from 0 to BINS-1
            uint32_t bin_idx = (static_cast<uint64_t>(key) * NUM_BINS) / MAX_KEY;

            // safe check
            if (bin_idx >= NUM_BINS) bin_idx = NUM_BINS - 1; 

            data_csv[d][bin_idx]++;

            // B) Compute Hash Value (Murmur3 fmix32)
            uint32_t pid = hash_murmur3_32(key, P_EXP);
            part_csv[d][pid]++;
        }
        std::cout << " -----------------------------------\n";
    }

    // ---------------------------------------------------------
    // Creation of CSV
    // ---------------------------------------------------------

    // 1. Raw Data
    std::ofstream f_data("data_distribution.csv");
    f_data << "BinID,Uniform,Multiples,StepSkew,HeavySkew\n";
    for (uint32_t b = 0; b < NUM_BINS; ++b) {
        f_data << b << "," 
               << data_csv[0][b] << ","
               << data_csv[1][b] << ","
               << data_csv[2][b] << ","
               << data_csv[3][b] << "\n";
    }
    f_data.close();

    // 2. Hash Distribution
    std::ofstream f_part("partition_distribution.csv");
    f_part << "PartitionID,Uniform,Multiples,StepSkew,HeavySkew\n";
    for (uint32_t p = 0; p < P; ++p) {
        f_part << p << "," 
               << part_csv[0][p] << ","
               << part_csv[1][p] << ","
               << part_csv[2][p] << ","
               << part_csv[3][p] << "\n";
    }
    f_part.close();

    std::cout << "Created 'data_distribution.csv' and 'partition_distribution.csv'\n";
    return 0;
}


