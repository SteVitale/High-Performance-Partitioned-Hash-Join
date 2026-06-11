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

#include <omp.h>
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
                                                    std::uint32_t p_exp, std::size_t num_tasks) {
    
    std::vector<std::vector<std::size_t>> task_hists(num_tasks, std::vector<std::size_t>(p, 0));
    std::vector<std::size_t> hist(p, 0);

    size_t data_chunk = rel.size() / num_tasks;

    // 1. Open the parallel region and wake up threads
    #pragma omp parallel default(none) num_threads(num_tasks) shared(rel, p_exp, num_tasks, data_chunk, task_hists)
    {
        // 2. Only the initial Thread creates the tasks queue
        #pragma omp single nowait
        {
            // 3. Explicit task creation
            for (size_t k = 0; k < num_tasks; ++k) {
                
                // 4. Single Task Definition
                // 'k' must be firstprivate, otherwise every task will read the final value of k
                #pragma omp task firstprivate(k) default(none) \
                        shared(rel, p_exp, data_chunk, task_hists, num_tasks)
                {
                    const size_t start = k * data_chunk;
                    const size_t end = (k == num_tasks - 1) ? rel.size() : (k + 1) * data_chunk;

                    for (size_t i = start; i < end; ++i) {
                        const std::uint32_t pid = hash_murmur3_32(rel[i].key, p_exp);
                        ++task_hists[k][pid];
                    }
                }
            }
        } // end single
    } // implicit "barrier" of the parallel region 

    // Sequential reduction (over tasks instead)
    for (size_t pid = 0; pid < p; ++pid) {
        for (size_t k = 0; k < num_tasks; ++k) {
            hist[pid] += task_hists[k][pid];
        }
    }

    return {std::move(hist), std::move(task_hists)};
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
    std::vector<std::vector<std::size_t>>& task_next, 
    std::size_t num_tasks) {

    // Allocates virtual memory, so that the main thread doesn't touch the physical memory
    Record* out_ptr = static_cast<Record*>(std::malloc(rel.size() * sizeof(Record)));
    
    size_t data_chunk = rel.size() / num_tasks;

    // 1. Defining the parallel region 
    #pragma omp parallel default(none) num_threads(num_tasks) shared(rel, p_exp, task_next, out_ptr, num_tasks, data_chunk)
    {
        // 2. Only the initial Thread creates the tasks queue
        #pragma omp single nowait
        {
            // 3. Explicit creation of the tasks
            for (size_t k = 0; k < num_tasks; ++k) {
                
                // 4. Definition of one task
                #pragma omp task firstprivate(k) default(none) \
                        shared(rel, p_exp, task_next, out_ptr, num_tasks, data_chunk)
                {
                    const size_t start = k * data_chunk;
                    const size_t end = (k == num_tasks - 1) ? rel.size() : (k + 1) * data_chunk;

                    for (size_t i = start; i < end; ++i) {
                        const std::uint32_t pid = hash_murmur3_32(rel[i].key, p_exp);
                        
                        // Lock-Free writes: each task exclusively owns its task_next[k] cursor
                        out_ptr[task_next[k][pid]++] = rel[i];
                    }
                }
            }
        } // end of omp single (all tasks submitted to the queue)
    } // implicit barrier of the omp parallel region

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

struct JoinResult {
    std::uint64_t join_count = 0;
    std::uint64_t checksum1 = 0;
    std::uint64_t checksum2 = 0;
};


// ------------------------------------------------------------
// Full dynamic parallel partitioned hash join
// ------------------------------------------------------------
static JoinResult partitioned_hash_join_parallel(const std::vector<Record>& R,
                                                const std::vector<Record>& S,
                                                std::uint32_t p, std::uint32_t p_exp,
                                                std::size_t num_thread) {

    std::size_t num_thd = std::min((size_t)p, num_thread);

    // Phase 1: Partition both relations
    PartitionedRelation Rpart = partition_relation(R, p, p_exp, num_thd);
    PartitionedRelation Spart = partition_relation(S, p, p_exp, num_thd);

    TIMERSTART(join_partition);

    // Accumulators for global reduction
    std::uint64_t total_join = 0;
    std::uint64_t total_chk1 = 0;
    std::uint64_t total_chk2 = 0;

    // Heavy Skew Threshold
    // Threshold: 3x the expected uniform bucket size (3 * N/P) [HARDCODED]
    const size_t expected_avg_s_size = S.size() / p;
    const size_t SKEW_THRESHOLD = expected_avg_s_size * 3; 

    #pragma omp parallel default(none) shared(Rpart, Spart, p, SKEW_THRESHOLD, num_thd, total_join, total_chk1, total_chk2)
    {
        #pragma omp single nowait
        {
            #pragma omp taskgroup task_reduction(+: total_join, total_chk1, total_chk2)
            {
                // "main" task for each partition
                for (std::uint32_t pid = 0; pid < p; ++pid) {
                    
                    #pragma omp task firstprivate(pid) default(none) \
                            shared(Rpart, Spart, SKEW_THRESHOLD, num_thd) \
                            in_reduction(+: total_join, total_chk1, total_chk2)
                    {
                        const std::size_t r_begin = Rpart.begin[pid];
                        const std::size_t r_end = Rpart.end[pid];
                        const std::size_t s_begin = Spart.begin[pid];
                        const std::size_t s_end = Spart.end[pid];
                        
                        std::size_t r_size = r_end - r_begin;
                        std::size_t s_size = s_end - s_begin;

                        if (r_size > 0 && s_size > 0) {
                            
                            // --------------------------------------------------
                            // SKEW DETECTION
                            // If R or S are above the threshold => NestedMode
                            // --------------------------------------------------
                            if (s_size > SKEW_THRESHOLD || r_size > SKEW_THRESHOLD) {
                                
                                // 1. Build Phase (Sequential)
                                SimpleHashTable ht(r_size);
                                for (std::size_t i = r_begin; i < r_end; ++i) {
                                    ht.insert(Rpart.data[i].key);
                                }

                                // 2. Probe Phase (Parallel Through Nested Task)
                                // 4x the thread count: enough chunks to keep all threads busy on the overloaded partition
                                std::size_t num_chunks = num_thd * 4;
                                std::size_t chunk_size = (s_size + num_chunks - 1) / num_chunks;

                                // Local Accumulator for the nested_task
                                std::uint64_t nested_join = 0;
                                std::uint64_t nested_chk1 = 0;
                                std::uint64_t nested_chk2 = 0;

                                // taskgroup 
                                #pragma omp taskgroup 
                                {
                                    for(std::size_t c = 0; c < num_chunks; ++c) {
                                        
                                        std::size_t c_start = s_begin + (c * chunk_size);
                                        std::size_t c_end = std::min(s_begin + ((c + 1) * chunk_size), s_end);
                                        
                                        if (c_start >= c_end) continue;

                                        #pragma omp task firstprivate(c_start, c_end) default(none) \
                                                shared(Spart, ht, nested_join, nested_chk1, nested_chk2)
                                        {
                                            std::uint64_t local_join = 0;
                                            std::uint64_t local_chk1 = 0;
                                            std::uint64_t local_chk2 = 0;
                                            
                                            // Probe chunk
                                            for (std::size_t j = c_start; j < c_end; ++j) {
                                                const std::uint64_t s_key = Spart.data[j].key;
                                                const std::uint32_t count_r = ht.get_count(s_key);
                                                
                                                if (count_r > 0) {
                                                    local_join += count_r;
                                                    local_chk1 += splitmix64(s_key) * count_r;
                                                    local_chk2 += splitmix64(s_key ^ 0x9e3779b97f4a7c15ULL) * count_r;
                                                }
                                            }
                                            
                                            //safe reduction over "main" task space
                                            #pragma omp atomic
                                            nested_join += local_join;
                                            #pragma omp atomic
                                            nested_chk1 += local_chk1;
                                            #pragma omp atomic
                                            nested_chk2 += local_chk2;
                                        }
                                    }
                                } // End of Nested Taskgroup Barrier
                                
                                // Global Reduction over all "main" tasks
                                total_join += nested_join;
                                total_chk1 += nested_chk1;
                                total_chk2 += nested_chk2;

                            } 
                            // --------------------------------------------------
                            // STANDARD MODE (same as hashjoin_omp_task.cpp)
                            // --------------------------------------------------
                            else {
                                JoinResult local_res{};
                                SimpleHashTable ht(r_size);
                                
                                for (std::size_t i = r_begin; i < r_end; ++i) {
                                    ht.insert(Rpart.data[i].key);
                                }
                                
                                for (std::size_t j = s_begin; j < s_end; ++j) {
                                    const std::uint64_t s_key = Spart.data[j].key;
                                    const std::uint32_t count_r = ht.get_count(s_key);
                                    if (count_r > 0) {
                                        local_res.join_count += count_r;
                                        local_res.checksum1 += splitmix64(s_key) * count_r;
                                        local_res.checksum2 += splitmix64(s_key ^ 0x9e3779b97f4a7c15ULL) * count_r;
                                    }
                                }
                                
                                total_join += local_res.join_count;
                                total_chk1 += local_res.checksum1;
                                total_chk2 += local_res.checksum2;
                            }
                        }
                    }
                }
            } // End of the main taskgroup
        } 
    } 

    JoinResult total{total_join, total_chk1, total_chk2};

    TIMERSTOP(join_partition);

    std::free(Rpart.data);
    std::free(Spart.data);
    return total;
}

// ------------------------------------------------------------
// Verifier for very small inputs
// ------------------------------------------------------------
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