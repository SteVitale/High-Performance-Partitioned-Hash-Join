#ifndef DISTRIBUTION_HPP
#define DISTRIBUTION_HPP

#include <vector>
#include <cstdint>
#include <string>
#include <stdexcept>

// Supported Distribution Types
enum class DistType {
    UNIFORM,
    MULTIPLES,
    SINGLE_HEAVY_SKEW,
    STEP_SKEW
};

// Command-line parser
inline DistType parse_dist_type(const std::string& str) {
    if (str == "uniform") return DistType::UNIFORM;
    if (str == "multiples") return DistType::MULTIPLES;
    if (str == "heavy_skew") return DistType::SINGLE_HEAVY_SKEW;
    if (str == "step_skew") return DistType::STEP_SKEW;
    throw std::invalid_argument("Error: Unknown distribution type '" + str + "'. Use uniform, multiples, heavy_skew or step_skew.");
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


struct Record {
    std::uint64_t key{};
};

static std::vector<Record> generate_relation(std::size_t n, std::uint64_t seed, std::uint64_t max_key, DistType dist_type) {
    std::vector<Record> out(n);
    std::uint64_t state = seed;

    // Fixed Special Key (in order to avoid random and allow correctness checks)
    const std::uint64_t HEAVY_KEY = max_key / 3;

    for (std::size_t i = 0; i < n; ++i) {
        // Always extract two values to keep the consistency between R and S
        const std::uint64_t dice = splitmix64_next(state);
        const std::uint64_t random_val = splitmix64_next(state);
        std::uint64_t k = 0;

        switch (dist_type) {
            case DistType::UNIFORM:
                k = random_val;
                break;
            case DistType::MULTIPLES:
                k = (random_val / 1000ULL) * 1000ULL; 
                break;
            case DistType::SINGLE_HEAVY_SKEW:
                k = ((dice % 100) < 50) ? HEAVY_KEY : random_val;
                break;
            case DistType::STEP_SKEW: {
                std::uint64_t pct = dice % 100;
                
                // Divide the key space into 4 contiguous tiers
                std::uint64_t r1 = max_key / 20;               // Tier 1: 5% of the key space
                std::uint64_t r2 = (max_key * 3) / 20;         // Tier 2: 15% of the key space
                std::uint64_t r3 = (max_key * 3) / 10;         // Tier 3: 30% of the key space
                std::uint64_t r4 = max_key - (r1 + r2 + r3);   // Tier 4: remaining 50%
                
                if (pct < 40) {
                    // 40% of tuples mapped to the first 5% of the key space
                    k = (r1 > 0) ? (random_val % r1) : 0;
                } 
                else if (pct < 70) {
                    // 30% of the tuples mapped to the next 15% of the key space
                    k = r1 + ((r2 > 0) ? (random_val % r2) : 0);
                } 
                else if (pct < 90) {
                    // 20% of the tuples mapped to the next 30% of the key space
                    k = (r1 + r2) + ((r3 > 0) ? (random_val % r3) : 0);
                } 
                else {
                    // 10% of the tuples spread in the remaining 50%
                    k = (r1 + r2 + r3) + ((r4 > 0) ? (random_val % r4) : 0);
                }
                break;
            }
        }
        out[i].key = (max_key == 0) ? k : (k % max_key);
    }
    
    return out;
}


#endif // DISTRIBUTION_HPP