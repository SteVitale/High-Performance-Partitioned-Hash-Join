#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <vector>
#include <cmath>
#include <string_view>
#include <thread>

#include "include/profiling_utils.hpp"
#include "include/SimpleHashTable.hpp"
#include "include/distribution.hpp"



struct HistResult {
    std::vector<std::size_t> global_hist;
    std::vector<std::vector<std::size_t>> local_hists;
};

// ------------------------------------------------------------
// Utility: command-line parsing
// ------------------------------------------------------------
static bool read_arg_u64(int argc, char** argv, const std::string& name, std::uint64_t& out) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (name == argv[i]) {
            out = std::strtoull(argv[i + 1], nullptr, 10);
            return true;
        }
    }
    return false;
}

static bool read_arg_distr(int argc, char** argv, DistType& out) {
    for (int i = 1; i + 1 < argc; ++i) {
       
        if (std::string_view(argv[i]) == "-distr") {
            try {
                out = parse_dist_type(argv[i+1]);
            } catch(const std::invalid_argument& e) {
                std::cerr << e.what() << "\n";
                return false;
            }
            return true;
        }
    }
    
    return false; 
}

static void usage(const char* prog) {
    std::cerr
        << "Usage:\n"
        << "  " << prog << " -nr NR -ns NS -seed SEED -max-key K -p P\n\n"
        << "Parameters:\n"
        << "  -nr         Number of records in relation R\n"
        << "  -ns         Number of records in relation S\n"
        << "  -seed       Deterministic seed\n"
        << "  -max-key    Keys are generated in [0, max-key)\n"
        << "  -p          Number of partitions (power of two required in this reference code)\n"
        << "  [-t]        Number of Threads (Default: nproc)\n"
        << "  [-distr]    Type of Data Distribution (uniform, multiples, heavy_skew, step_skew)";
}
static bool is_power_of_two(std::uint32_t x) {
    return x != 0 && (x & (x - 1U)) == 0;
}



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

// ------------------------------------------------------------
// Histogram
// ------------------------------------------------------------
static HistResult compute_histogram(const std::vector<Record>& rel, 
                                                    std::uint32_t p, 
                                                    std::uint32_t p_exp, std::size_t num_thread) {
    
    // local_hists[i] => local histogram of i-thread
    std::vector<std::vector<std::size_t>> local_hists(num_thread, std::vector<std::size_t>(p, 0));
    
    // hist => global output array returned
    std::vector<std::size_t> hist(p, 0);

    // Fixed Threads sharing local_hists
    std::vector<std::thread> threads;
    threads.reserve(num_thread);

    // Static Partitioning Data
    size_t data_chunk = rel.size() / num_thread;

    // Calculating the workload for each thread 
    for(size_t t = 0; t < num_thread; ++t) {
        threads.emplace_back([&, t]() {
            // Each thread reads a distinct chunk of the input relation 'rel'
            // and populates its private row in the 'local_hists' vector

            const size_t start = t * data_chunk;
            const size_t end = (t == num_thread-1) ? rel.size() : (t+1) * data_chunk;

            for(size_t i = start; i < end; ++i){
                const std::uint32_t pid = hash_murmur3_32(rel[i].key, p_exp);
                ++local_hists[t][pid];
            }
        });
    }

    // Wait for all threads to finish their work
    for (auto& th : threads) th.join();

    // Sequential reduction (sum over local_hists)
    for (size_t pid = 0; pid < p; ++pid)
        for (size_t t = 0; t < num_thread; ++t)
            hist[pid] += local_hists[t][pid];

    return {std::move(hist), std::move(local_hists)};
}

// ------------------------------------------------------------
// Prefix sum (exclusive scan)
// ------------------------------------------------------------
static std::vector<std::size_t> exclusive_prefix_sum(const std::vector<std::size_t>& hist) {
    std::vector<std::size_t> begin(hist.size(), 0);

    std::size_t running = 0;
    for (std::size_t p = 0; p < hist.size(); ++p) {
        begin[p] = running;
        running += hist[p];
    }
    return begin;
}

// ------------------------------------------------------------
// Vector of write cursors for each thread
// ------------------------------------------------------------

/*
The following function computes the write cursors for the parallel Scatter phase.
This function calculates the exact starting memory index for every thread and every
partition.

Takes in input:
 1. An array of size P containing the global starting index for each partition
 2. A T x P matrix where local_hists[t][p] is the number of records thread 't' will write to partition 'p'.
*/

// EXAMPLE

/*
 * Computes the exact array index where each thread must start writing.
 *
 * SCENARIO: 2 Threads (T0, T1) writing into 2 Partitions (P0, P1).
 *
 * 1. INPUT 'global_begin' (Where each partition starts in the final array):
 * - P0 starts at index 0.
 * - P1 starts at index 12 (since P0 total size is 5 + 7 = 12).
 *
 * 2. INPUT 'local_hists' (How many items each thread has per partition):
 * - T0 has 5 items for P0, and 3 items for P1.
 * - T1 has 7 items for P0, and 6 items for P1.
 *
 * OUTPUT 'local_next' (The exact starting indices for each thread):
 * - For P0:
 * T0 starts at index 0  (writes 5 items: 0 to 4).
 * T1 starts at index 5  (writes 7 items: 5 to 11).
 *
 * - For P1:
 * T0 starts at index 12 (writes 3 items: 12 to 14).
 * T1 starts at index 15 (writes 6 items: 15 to 20).
 *
 * For each partition pid:
 * local_next[0][pid] = global_begin[pid]
 * local_next[1][pid] = global_begin[pid] + local_hists[0][pid]
 * local_next[2][pid] = global_begin[pid] + local_hists[0][pid] + local_hists[1][pid]
 * ...
 * 
 * 
 * This offset calculation ensures threads never overwrite each other's data,
 * allowing 100% lock-free parallel writes.
 */
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
    
    // To test the std::vector behavior (main thread triggers the numa first-touch policy)
    //std::memset(out_ptr, 1, rel.size() * sizeof(Record));

    std::size_t num_thd = local_next.size(); 
    size_t data_chunk = rel.size() / num_thd;

    std::vector<std::thread> threads;
    threads.reserve(num_thd);

    for (size_t t = 0; t < num_thd; ++t) {
        threads.emplace_back([&, t]() {
            const size_t start = t * data_chunk;
            const size_t end = (t == num_thd - 1) ? rel.size() : (t + 1) * data_chunk;

            for (size_t i = start; i < end; ++i) {
                const std::uint32_t pid = hash_murmur3_32(rel[i].key, p_exp);

                // Lock-Free Writing using the private cursor of the thread t (local_next[t][pid])
                out_ptr[local_next[t][pid]++] = rel[i];
            }
        });
    }

    for (auto& th : threads) th.join();

    return out_ptr;
}


// ------------------------------------------------------------
// Partitioned relation metadata
// ------------------------------------------------------------
struct PartitionedRelation {
    Record* data; //instead of std::vector<Record>
    std::vector<std::size_t> begin;
    std::vector<std::size_t> end;
};

// ------------------------------------------------------------
// Full partitioning pipeline for one relation
// ------------------------------------------------------------
static PartitionedRelation partition_relation(const std::vector<Record>& rel, std::uint32_t p, std::uint32_t p_exp, std::size_t num_thread) {
    
    TIMERSTART(histogram);
    const auto hist_res = compute_histogram(rel, p, p_exp, num_thread);
    TIMERSTOP(histogram);

    TIMERSTART(prefix_sum);
    const auto begin = exclusive_prefix_sum(hist_res.global_hist);
    TIMERSTOP(prefix_sum);

    TIMERSTART(scatter_part);
    auto local_next = compute_local_offsets(begin, hist_res.local_hists);
    auto data = scatter_partitioned_parallel(rel, p_exp, local_next, num_thread);
    TIMERSTOP(scatter_part);


    std::vector<std::size_t> end(p, 0);
    for (std::uint32_t pid = 0; pid < p; ++pid) {
        end[pid] = begin[pid] + hist_res.global_hist[pid];
    }

    return PartitionedRelation{
        .data = data,
        .begin = begin,
        .end = end
    };
}

// ------------------------------------------------------------
// Join result
// ------------------------------------------------------------
struct JoinResult {
    std::uint64_t join_count = 0;
    std::uint64_t checksum1 = 0;
    std::uint64_t checksum2 = 0;
};

// ------------------------------------------------------------
// Local join on one partition
// ------------------------------------------------------------
static JoinResult join_one_partition(const PartitionedRelation& Rpart,
                                     const PartitionedRelation& Spart,
                                     std::uint32_t pid) {
    JoinResult result{};

    const std::size_t r_begin = Rpart.begin[pid];
    const std::size_t r_end = Rpart.end[pid];
    const std::size_t s_begin = Spart.begin[pid];
    const std::size_t s_end = Spart.end[pid];

    if (r_begin == r_end || s_begin == s_end) {
        return result;
    }

    // --- 1. BUILD PHASE (on R) ---
    std::size_t r_size = r_end - r_begin;
    SimpleHashTable ht(r_size); // Pre Allocating the perfect size

    for (std::size_t i = r_begin; i < r_end; ++i) {
        ht.insert(Rpart.data[i].key);
    }

    // --- 2. PROBE PHASE (on S) ---
    for (std::size_t j = s_begin; j < s_end; ++j) {
        const std::uint64_t s_key = Spart.data[j].key;
        
        const std::uint32_t count_r = ht.get_count(s_key);

        if (count_r > 0) {
            result.join_count += count_r;
            result.checksum1 += splitmix64(s_key) * count_r;
            result.checksum2 += splitmix64(s_key ^ 0x9e3779b97f4a7c15ULL) * count_r;
        }
    }

    return result;
}

// ------------------------------------------------------------
// Full Parallel partitioned hash join
// ------------------------------------------------------------
static JoinResult partitioned_hash_join_parallel(const std::vector<Record>& R,
                                                 const std::vector<Record>& S,
                                                 std::uint32_t p, std::uint32_t p_exp,
                                                 std::size_t num_thread
                                                ) {

    std::size_t num_thd = std::min((size_t)p, num_thread);

    // Phase 1: partition both relations
    PartitionedRelation Rpart = partition_relation(R, p, p_exp, num_thd);
    PartitionedRelation Spart = partition_relation(S, p, p_exp, num_thd);

    // Phase 2 + 3: local joins and global reduction
    
    TIMERSTART(join_partition);
    
    std::vector<std::thread> threads;
    threads.reserve(num_thd);

    std::vector<JoinResult> local_results(num_thd);
    std::atomic<std::uint32_t> next_pid{0};

    for (size_t t = 0; t < num_thd; ++t) {
        threads.emplace_back([&, t]() {
            JoinResult thread_total{};
            
            while (true) {
                std::uint32_t pid = next_pid.fetch_add(1, std::memory_order_relaxed);
                
                if (pid >= p) {
                    break;
                }

                const JoinResult local = join_one_partition(Rpart, Spart, pid);
                thread_total.join_count += local.join_count;
                thread_total.checksum1 += local.checksum1;
                thread_total.checksum2 += local.checksum2;
            }
            
            local_results[t] = thread_total;
        });
    }

    // Wait for all threads to finish the work on their partitions
    for (auto& th : threads) {
        th.join();
    }

    // Global reduction: summing over local results of the threads
    JoinResult total{};
    for (const auto& res : local_results) {
        total.join_count += res.join_count;
        total.checksum1 += res.checksum1;
        total.checksum2 += res.checksum2;
    }
    TIMERSTOP(join_partition);

    std::free(Rpart.data);
    std::free(Spart.data);
    return total;
}

// ------------------------------------------------------------
// Verifier for very small inputs
// ------------------------------------------------------------
//
// This is useful only for debugging and correctness testing on tiny
// datasets. It checks all pairs directly, so its complexity is O(|R|*|S|).
//
static JoinResult naive_join_verifier(const std::vector<Record>& R,
                                      const std::vector<Record>& S) {
    JoinResult result{};

    for (const auto& r : R) {
        for (const auto& s : S) {
            if (r.key == s.key) {
                result.join_count += 1;
				result.checksum1 += splitmix64(r.key);
				result.checksum2 += splitmix64(r.key ^ 0x9e3779b97f4a7c15ULL);
            }
        }
    }
    return result;
}

// ------------------------------------------------------------
// Main
// ------------------------------------------------------------
int main(int argc, char** argv) {

    std::uint64_t nr = 0, ns = 0, seed = 0, max_key = 0, p = 0;

    if (!read_arg_u64(argc, argv, "-nr", nr) ||
        !read_arg_u64(argc, argv, "-ns", ns) ||
        !read_arg_u64(argc, argv, "-seed", seed) ||
        !read_arg_u64(argc, argv, "-max-key", max_key) ||
        !read_arg_u64(argc, argv, "-p", p)) {
        usage(argv[0]);
        return 1;
    }

    if (p > std::numeric_limits<std::uint32_t>::max()) {
        std::cerr << "Error: P too large.\n";
        return 1;
    }

    const std::uint32_t P = static_cast<std::uint32_t>(p);
    
	// The power-of-two constraint on P is only due to the default
	// mapping used here. It may be removed if the chosen partition
	// function correctly handles arbitrary P
    if (!is_power_of_two(P)) {
        std::cerr << "Error: in this reference implementation, P must be a power of two.\n";
        return 1;
    }

    if (P < 2) { //avoid P=1 that is technically a power of 2 (2^0 = 1)
        std::cerr << "Error: P must be at least 2.\n";
        return 1;
    }
    
    const std::uint32_t P_exp = static_cast<std::uint32_t>(std::log2(P)); //log2 for power of 2 

    const std::size_t NR = static_cast<std::size_t>(nr);
    const std::size_t NS = static_cast<std::size_t>(ns);

    std::uint64_t t = 0;
    const std::size_t num_thread = read_arg_u64(argc, argv, "-t", t) 
                                 ? static_cast<std::size_t>(t) 
                                 : std::thread::hardware_concurrency();


    // CLI Distribution Parsing | Default := UNIFORM DISTRIBUTION
    DistType dist_type = DistType::UNIFORM;
    read_arg_distr(argc, argv, dist_type);

    // Deterministic generation.
    // We use two different seeds so that R and S are not identical.
    const auto R = generate_relation(NR, seed, max_key, dist_type);
    const auto S = generate_relation(NS, seed ^ 0xdeadebdecdeedef1ULL, max_key, dist_type);

    
    // Time only the join pipeline, not input generation.
    const auto t0 = std::chrono::steady_clock::now();
    const JoinResult result = partitioned_hash_join_parallel(R, S, P, P_exp, num_thread);
    const auto t1 = std::chrono::steady_clock::now();

    const double sec = std::chrono::duration<double>(t1 - t0).count();

    std::cout << "NR=" << NR << " NS=" << NS << " P=" << P
			  << " seed=" << seed
              << " [0, " << max_key << ")\n";

    std::cout << "join_count=" << result.join_count << "\n";
    std::cout << "checksum1=" << result.checksum1 << "\n";
    std::cout << "checksum2=" << result.checksum2 << "\n";

    std::cout << std::fixed << std::setprecision(6);
    std::cout << "time_sec=" << sec << "\n";

    //Tiny debug check, only for very small datasets
    if (NR <= 500 && NS <= 500) {
        const JoinResult naive = naive_join_verifier(R, S);
        std::cout << "naive_join_count=" << naive.join_count << "\n";
        std::cout << "naive_checksum1=" << naive.checksum1 << "\n";
        std::cout << "naive_checksum2=" << naive.checksum2 << "\n";
    }

    return 0;
}
