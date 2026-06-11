#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include <mpi.h>
#include <omp.h>

#include "include/SimpleHashTable.hpp"
#include "include/omp_utils.hpp"

#define THREAD_NUMBER_PER_NODE 16

// MPI error check
#define CHECK_MPI(call)                                                 \
    do {                                                                \
        int _err = (call);                                              \
        if (_err != MPI_SUCCESS) {                                      \
            char _buf[MPI_MAX_ERROR_STRING]; int _len = 0;              \
            MPI_Error_string(_err, _buf, &_len);                        \
            std::cerr << "MPI error at line " << __LINE__               \
                      << ": " << std::string(_buf, _len) << '\n';       \
            MPI_Abort(MPI_COMM_WORLD, _err);                            \
        }                                                               \
    } while (0)


// CLI helpers
static bool read_arg_u64(int argc, char** argv, const std::string& flag, std::uint64_t& out) {
    for (int i = 1; i + 1 < argc; ++i)
        if (flag == argv[i]) {
            out = std::strtoull(argv[i+1], nullptr, 10);
            return true;
        }
    return false;
}

static void usage(const char* prog) {
    std::cerr
        << "Usage:\n"
        << "  " << prog << " -nr NR -ns NS -seed SEED -max-key K -p_per_rank P\n\n"
        << "Parameters:\n"
        << "  -nr          Number of records in relation R\n"
        << "  -ns          Number of records in relation S\n"
        << "  -seed        Deterministic seed\n"
        << "  -max-key     Keys are generated in [0, max-key)\n"
        << "  -p_per_rank  Number of partitions (power of two required in this reference code)\n";
}


struct Profiler {
    double generation    = 0.0;
    double preparation   = 0.0;
    double communication  = 0.0;
    double join          = 0.0;
};

// MPI type for Record
static MPI_Datatype create_mpi_record_type() {
    MPI_Datatype mpi_record;

    int count[1] = {1};
    MPI_Datatype type[1] = {MPI_UINT64_T};
    MPI_Aint disp[1];

    Record dummy;
    MPI_Aint base, key_addr;

    MPI_Get_address(&dummy, &base);
    MPI_Get_address(&dummy.key, &key_addr);

    disp[0] = key_addr - base;

    MPI_Type_create_struct(1, count, disp, type, &mpi_record);
    MPI_Type_commit(&mpi_record);

    return mpi_record;
}


// Centralized Data Generation
static std::vector<Record> generate_relation(std::size_t n, std::uint64_t seed, std::uint64_t max_key, bool is_skewed) {
    std::vector<Record> out(n);
    std::uint64_t state = seed;
    const std::uint64_t HEAVY_KEY = max_key / 3;

    for (std::size_t i = 0; i < n; ++i) {
        const std::uint64_t dice = splitmix64_next(state);
        const std::uint64_t random_val = splitmix64_next(state);
        
        std::uint64_t k = random_val;
        if (is_skewed && (dice % 100) < 50) {
            k = HEAVY_KEY;
        }
        
        out[i].key = (max_key == 0) ? k : (k % max_key);
    }
    return out;
}

static std::vector<Record> scatter_raw_data_from_root(const std::vector<Record>& global_rel, std::size_t global_n, int num_ranks, int rank, MPI_Datatype mpi_record) {
    
    // Local Chunk (Last rank takes the remainder)
    const std::size_t chunk = global_n / static_cast<std::size_t>(num_ranks);
    const std::size_t remainder = global_n % static_cast<std::size_t>(num_ranks);
    const std::size_t local_n = (rank == num_ranks - 1) ? chunk + remainder : chunk;

    std::vector<Record> local_rel(local_n);
    
    std::vector<int> sendcounts;
    std::vector<int> displs;

    // Only rank 0 compute the scatterv parameters
    if (rank == 0) {
        sendcounts.resize(num_ranks);
        displs.resize(num_ranks);
        int current_disp = 0;
        
        for (int i = 0; i < num_ranks; ++i) {
            int count = (i == num_ranks - 1) ? chunk + remainder : chunk;
            sendcounts[i] = count;
            displs[i] = current_disp;
            current_disp += count;
        }
    }

    // Distribution of Raw Data
    CHECK_MPI(MPI_Scatterv(
        rank == 0 ? global_rel.data() : nullptr, 
        rank == 0 ? sendcounts.data() : nullptr, 
        rank == 0 ? displs.data() : nullptr, 
        mpi_record,
        local_rel.data(),
        static_cast<int>(local_n),
        mpi_record,
        0, 
        MPI_COMM_WORLD
    ));

    return local_rel;
}

// Histogram 
static std::vector<int> compute_histogram(const std::vector<Record>& rel, std::uint32_t p, std::uint32_t p_exp) {
    std::vector<int> hist(p, 0);
    for (const auto& rec : rel)
        ++hist[hash_murmur3_32(rec.key, p_exp)];
    return hist;                                  
}

// Exclusive prefix sum (Templated to support both int for MPI and size_t for OpenMP)
template <typename T>
static std::vector<T> exclusive_prefix_sum(const std::vector<T>& hist) {
    std::vector<T> begin(hist.size(), 0);
    T running = 0;

    for (std::size_t i = 0; i < hist.size(); ++i) {
        begin[i]   = running;
        running += hist[i];
    }
    return begin;
}

// Scatter 
static std::vector<Record> scatter(const std::vector<Record>& rel, std::uint32_t p_exp, std::vector<int> begin) {
    std::vector<Record> out(rel.size());

    for(const auto& rec : rel) {
        const int dest = static_cast<int>(hash_murmur3_32(rec.key, p_exp));
        out[begin[dest]++] = rec;
    }
    return out;
}

static std::vector<Record> distribute_relation(const std::vector<Record>& local_rel, int num_ranks, MPI_Datatype mpi_record, Profiler& prof) {

    const std::uint32_t num_ranks_exp = static_cast<std::uint32_t>(std::log2(num_ranks));

    // ===========================================================================
    // First Phase: Preparation of the continuous data
    // ===========================================================================
    double calc_start = MPI_Wtime();
    std::vector<int> send_counts = compute_histogram(local_rel, static_cast<std::uint32_t>(num_ranks), num_ranks_exp);
    std::vector<int> sdispls = exclusive_prefix_sum(send_counts);
    std::vector<Record> send_buf = scatter(local_rel, num_ranks_exp, sdispls);
    prof.preparation += (MPI_Wtime() - calc_start);

    // ===========================================================================
    // Second Phase: Metadata Exchange
    // ===========================================================================
    std::vector<int> recv_counts(num_ranks, 0);
    
    // example
    // rank 0 send [10, 8]
    // rank 1 send [7, 12]

    double comm_start = MPI_Wtime();
    CHECK_MPI(MPI_Alltoall(
        send_counts.data(), 1, MPI_INT,  
        recv_counts.data(), 1, MPI_INT,  
        MPI_COMM_WORLD
    ));
    prof.communication += (MPI_Wtime() - comm_start);

    // rank 0 receive [10, 7]
    // rank 1 receive [8, 12]
    // ===========================================================================
    // Third Phase: recv Buffer Displacement
    // ===========================================================================

    calc_start = MPI_Wtime();
    std::vector<int> rdispls(num_ranks, 0);
    int total_recv = 0;
    
    for (int i = 0; i < num_ranks; ++i) {
        rdispls[i] = total_recv;
        total_recv += recv_counts[i]; 
    }
    
    
    // Allocation of the recv buffer
    std::vector<Record> recv_buf(total_recv);
    prof.preparation += (MPI_Wtime() - calc_start);

    // example
    // data = [0, 5, 9, 1, 2, 9] 
    // send_counts = [3, 3] (hist)
    // displacement = [0, 3] (scan) 

    // ===========================================================================
    // Fourth Phase: MPI_Alltoallv
    // ===========================================================================
    comm_start = MPI_Wtime();
    CHECK_MPI(MPI_Alltoallv(
        send_buf.data(), send_counts.data(), sdispls.data(), mpi_record, // to send
        recv_buf.data(), recv_counts.data(), rdispls.data(), mpi_record, // to receive
        MPI_COMM_WORLD
    ));
    prof.communication += (MPI_Wtime() - comm_start);

    return recv_buf;
}


static PartitionedRelation partition_relation(const std::vector<Record>& rel, std::uint32_t p, std::uint32_t p_exp, std::size_t num_thread) {
    
  
    const auto hist_res = compute_histogram_parallel(rel, p, p_exp, num_thread);

    const auto begin = exclusive_prefix_sum(hist_res.global_hist);
   
    auto local_next = compute_local_offsets(begin, hist_res.local_hists);
    auto data = scatter_partitioned_parallel(rel, p_exp, local_next, num_thread);
  

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
static JoinResult partitioned_hash_join_parallel(const std::vector<Record>& R, const std::vector<Record>& S,
                                                 std::uint32_t p, std::uint32_t p_exp, std::size_t num_thread
                                                ) {

    std::size_t num_thd = std::min((size_t)p, num_thread);

    // Phase 1: partition both relations 
    PartitionedRelation Rpart = partition_relation(R, p, p_exp, num_thd);
    PartitionedRelation Spart = partition_relation(S, p, p_exp, num_thd);

    

    // Total Accumulator for all reductions
    std::uint64_t total_join = 0;
    std::uint64_t total_chk1 = 0;
    std::uint64_t total_chk2 = 0;

    // Phase 2 + 3: local joins and global reduction
    #pragma omp parallel for schedule(dynamic,1) num_threads(num_thd) \
            default(none) shared(Rpart, Spart, p) \
            reduction(+: total_join, total_chk1, total_chk2)
    for (std::uint32_t pid = 0; pid < p; ++pid) {
        
        const JoinResult local = join_one_partition(Rpart, Spart, pid);
        
        total_join += local.join_count;
        total_chk1 += local.checksum1;
        total_chk2 += local.checksum2;
    }

    JoinResult total{total_join, total_chk1, total_chk2};



    std::free(Rpart.data);
    std::free(Spart.data);
    return total;
}

// Naive O(NR×NS) verifier — only for tiny inputs
static JoinResult naive_join_verifier(const std::vector<Record>& R,
                                      const std::vector<Record>& S) {
    JoinResult res{};
    for (const auto& r : R)
        for (const auto& s : S)
            if (r.key == s.key) {
                ++res.join_count;
                res.checksum1 += splitmix64(r.key);
                res.checksum2 += splitmix64(r.key ^ 0x9e3779b97f4a7c15ULL);
            }
    return res;
}

int main(int argc, char** argv){
    int provided;
    int rank, num_ranks;

    MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &provided);

    if (provided < MPI_THREAD_FUNNELED) {
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &num_ranks);

    // num_ranks is P_mpi — must be power of two
    if (!is_power_of_two(static_cast<std::uint32_t>(num_ranks))) {
        if (rank == 0)
            std::cerr << "Error: num_ranks must be a power of two.\n";
        MPI_Abort(MPI_COMM_WORLD, 1); 
        return 1;
    }

    // Parse CLI
    std::uint64_t nr = 0, ns = 0, seed = 0, max_key = 0, p_arg = 0;
    
    if (!read_arg_u64(argc, argv, "-nr", nr) ||
        !read_arg_u64(argc, argv, "-ns", ns) ||
        !read_arg_u64(argc, argv, "-seed", seed) ||
        !read_arg_u64(argc, argv, "-max-key", max_key) ||
        !read_arg_u64(argc, argv, "-p_per_rank", p_arg)) {

        if (rank == 0) 
            usage(argv[0]);

        MPI_Abort(MPI_COMM_WORLD, 1); return 1;
    }

    bool is_skewed = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--skewed") {
            is_skewed = true;
        }
    }


    const std::uint32_t p_per_rank = static_cast<std::uint32_t>(p_arg);
    if (!is_power_of_two(p_per_rank)) {

        if (rank == 0)
            std::cerr << "Error: -p (p_per_rank) must be a power of two.\n";
        
        MPI_Abort(MPI_COMM_WORLD, 1); return 1;
    }

    const std::size_t NR = static_cast<std::size_t>(nr);
    const std::size_t NS = static_cast<std::size_t>(ns);

    const std::uint32_t p_per_rank_exp = static_cast<std::uint32_t>(std::log2(p_per_rank));

    MPI_Datatype mpi_record = create_mpi_record_type();
    Profiler profiler_current_rank;

    // ===========================================================================
    // Step 1: Data Generation & Distribution (ScatterV)
    // ===========================================================================
    
    std::vector<Record> global_R, global_S;
    
    const double t_setup_start = MPI_Wtime();
    
    if (rank == 0) {
        global_R = generate_relation(NR, seed, max_key, is_skewed);
        global_S = generate_relation(NS, seed ^ 0xdeadebdecdeedef1ULL, max_key, is_skewed);
    }

    // ScatterV to statically divide and send data to other ranks
    auto local_R = scatter_raw_data_from_root(global_R, NR, num_ranks, rank, mpi_record);
    auto local_S = scatter_raw_data_from_root(global_S, NS, num_ranks, rank, mpi_record);
    
    // Rank 0 free the allocated memory
    if (rank == 0) {
        global_R.clear(); global_R.shrink_to_fit();
        global_S.clear(); global_S.shrink_to_fit();
    }
    
    profiler_current_rank.generation = MPI_Wtime() - t_setup_start;

    CHECK_MPI(MPI_Barrier(MPI_COMM_WORLD));

    // ===================Start of the communication between process================= 
    const double t_start = MPI_Wtime();
    // ===========================================================================


    // ===========================================================================
    // Step 2: Redistribute - each rank sends records to their
    // destination rank determined by hash(key) % num_ranks
    // ===========================================================================
    auto recv_R = distribute_relation(local_R, num_ranks, mpi_record, profiler_current_rank);
    auto recv_S = distribute_relation(local_S, num_ranks, mpi_record, profiler_current_rank);
  


    // ===========================================================================
    // Step 3: Local Partitioned Join - re-partition on received buffer
    // into p_per_rank sub_partitions to respect the overall algorithm
    // ===========================================================================
    const double t_join_start = MPI_Wtime();
    const JoinResult rank_result = partitioned_hash_join_parallel(recv_R, recv_S, p_per_rank, p_per_rank_exp, THREAD_NUMBER_PER_NODE);
    profiler_current_rank.join = MPI_Wtime() - t_join_start;

    // ===========================================================================
    // Step 4: Global Reduction
    // ===========================================================================
    std::uint64_t global_joincount  = 0;
    std::uint64_t global_checksum1  = 0;
    std::uint64_t global_checksum2  = 0;

    CHECK_MPI(MPI_Reduce(&rank_result.join_count, &global_joincount, 1, MPI_UINT64_T, MPI_SUM, 0, MPI_COMM_WORLD));
    CHECK_MPI(MPI_Reduce(&rank_result.checksum1,  &global_checksum1, 1, MPI_UINT64_T, MPI_SUM, 0, MPI_COMM_WORLD));
    CHECK_MPI(MPI_Reduce(&rank_result.checksum2,  &global_checksum2, 1, MPI_UINT64_T, MPI_SUM, 0, MPI_COMM_WORLD));

    CHECK_MPI(MPI_Barrier(MPI_COMM_WORLD));


    // ==========End of the communication between processes=======================
    const double t_end = MPI_Wtime(); double total_time = t_end - t_start;
    // ===========================================================================
    
    // Accumulators for global reduce on rank 0
    Profiler max_prof, sum_prof;

    // Reduce - MAX to take the unluckiest rank's time in each phase
    MPI_Reduce(&profiler_current_rank.generation,   &max_prof.generation,    1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&profiler_current_rank.preparation,  &max_prof.preparation,   1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&profiler_current_rank.communication,&max_prof.communication, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&profiler_current_rank.join,         &max_prof.join,          1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);


    // Reduce - SUM to take the sum in order to compute the average after
    MPI_Reduce(&profiler_current_rank.generation,   &sum_prof.generation,    1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&profiler_current_rank.preparation,  &sum_prof.preparation,   1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&profiler_current_rank.communication,&sum_prof.communication, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&profiler_current_rank.join,         &sum_prof.join,          1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
   
    MPI_Type_free(&mpi_record);
    MPI_Finalize();
    // ===============Only Master Node can print =================================
    if(rank != 0) return 0;
    // ===========================================================================

    double avg_preparation = sum_prof.preparation / num_ranks;
    double avg_join = sum_prof.join / num_ranks;

    std::cout << "NR=" << NR << " NS=" << NS 
              << " Ranks= " << num_ranks
              << " P rank=" << p_per_rank
			  << " seed=" << seed
              << " [0, " << max_key << ")\n";

    std::cout << "join_count=" << global_joincount << "\n";
    std::cout << "checksum1=" << global_checksum1 << "\n";
    std::cout << "checksum2=" << global_checksum2 << "\n";

    std::cout << "\n=== PROFILING METRICS ===\n";
    std::cout << std::fixed << std::setprecision(6);
    std::cout << "time_sec=" << total_time << "\n";

    std::cout << "\n--- Bottlenecks (Max per phase) ---\n";
    std::cout << "Generation : " << max_prof.generation << " s\n";
    std::cout << "Preparation : " << max_prof.preparation << " s\n";
    std::cout << "Communication : " << max_prof.communication << " s\n";
    std::cout << "Local Join : " << max_prof.join << " s\n";

    std::cout << "\n--- Load Imbalance Analysis ---\n";
    std::cout << "Net Compute Imbalance : " << (max_prof.preparation - avg_preparation) << " s\n";
    std::cout << "Join Compute Imbalance : " << (max_prof.join - avg_join) << " s\n";

    //Tiny debug check, only for very small datasets
    if (NR <= 500 && NS <= 500) {

        auto R = generate_relation(NR, seed, max_key, is_skewed);
        auto S = generate_relation(NS, seed ^ 0xdeadebdecdeedef1ULL, max_key, is_skewed);
        
        const JoinResult naive = naive_join_verifier(R, S);
       
        std::cout << "naive_join_count=" << naive.join_count << "\n";
        std::cout << "naive_checksum1=" << naive.checksum1 << "\n";
        std::cout << "naive_checksum2=" << naive.checksum2 << "\n";
    }


    
    return 0;
}