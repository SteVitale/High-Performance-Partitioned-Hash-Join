
#include <iostream>
#include <vector>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <fstream>
#include <chrono>
#include <cuda_runtime.h>
#include "Hash_Analysis/distribution.hpp"

#define ITERATIONS 7  // odd => clean median center
#define BLOCK_SIZE 256 // multiple of 32 (warp size)

// Called from the GPU
// Plain Murmur3 finalizer fmix32 
__device__ inline uint32_t hash_murmur3_32_device(uint64_t key, uint32_t p_exp) {
    uint32_t h = static_cast<uint32_t>(key ^ (key >> 32));

    h ^= h >> 16;
    h *= 0x85ebca6bU;
    h ^= h >> 13;
    h *= 0xc2b2ae35U;
    h ^= h >> 16;

    return h >> (32 - p_exp);
}

// One Thread per Key
__global__ void hash_kernel(const uint64_t* __restrict__ keys, uint32_t* __restrict__ parts, uint32_t p_exp, size_t N)
{
    size_t idx = static_cast<size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
    if (idx < N)
        parts[idx] = hash_murmur3_32_device(keys[idx], p_exp); //call device function 
}

// Cuda error checking
#define CUDA_CHECK(call)                                                    \
    do {                                                                    \
        cudaError_t _e = (call);                                            \
        if (_e != cudaSuccess) {                                            \
            std::cerr << "CUDA error: " << cudaGetErrorString(_e)           \
                      << "  (" << __FILE__ << ":" << __LINE__ << ")\n";     \
            std::exit(EXIT_FAILURE);                                        \
        }                                                                   \
    } while (0)

auto start(){
    return std::chrono::steady_clock::now();
}

auto stop(std::chrono::steady_clock::time_point start){
    auto end = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(end - start).count();
}

int main(int argc, char* argv[]) {

    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <millions_N> <exponent_P>\n";
        return 1;
    }

    // Parsing arguments N_millions P_exponent
    const size_t   N     { static_cast<size_t>(std::stod(argv[1]) * 1'000'000) };
    const uint32_t p_exp { static_cast<uint32_t>(std::stoi(argv[2])) };
    const uint32_t P     { 1U << p_exp };
    const uint64_t SEED  { 42 };

    // Generate keys on host (same as CPU versions)
    std::vector<uint64_t> keys_id(generate_keys(N, P, SEED, DistType::DIST_UNIFORM));
    std::vector<uint32_t> parts_id(N, 0);

    // Allocate device memory
    uint64_t* d_keys  = nullptr;
    uint32_t* d_parts = nullptr;
    CUDA_CHECK(cudaMalloc(&d_keys,  N * sizeof(uint64_t)));
    CUDA_CHECK(cudaMalloc(&d_parts, N * sizeof(uint32_t)));

    // Grid configuration
    int numBlocks = static_cast<int>((N + BLOCK_SIZE - 1) / BLOCK_SIZE);

    // FROM HOST TO DEVICE
    auto start_1 = start();
    CUDA_CHECK(cudaMemcpy(d_keys, keys_id.data(), N * sizeof(uint64_t), cudaMemcpyHostToDevice));
    auto stop_1 = stop(start_1);

    // Warm Up - Not measured
    hash_kernel<<<numBlocks, BLOCK_SIZE>>>(d_keys, d_parts, p_exp, N);
    CUDA_CHECK(cudaDeviceSynchronize()); 

    // Execution of the Kernel in the GPU
    std::vector<double> time_iterations(ITERATIONS, 0.0);
    for (size_t iter = 0; iter < ITERATIONS; ++iter) {
        auto t0 = start();

        hash_kernel<<<numBlocks, BLOCK_SIZE>>>(d_keys, d_parts, p_exp, N);
        CUDA_CHECK(cudaDeviceSynchronize()); // wait for kernel to finish

        time_iterations[iter] = stop(t0);
    }

    // FROM DEVICE TO HOST
    auto start_2 = start();
    CUDA_CHECK(cudaMemcpy(parts_id.data(), d_parts, N * sizeof(uint32_t), cudaMemcpyDeviceToHost));
    auto stop_2 = stop(start_2);

    // Free Resources allocated before
    cudaFree(d_keys);
    cudaFree(d_parts);

    // Median
    std::sort(time_iterations.begin(), time_iterations.end());
    double median = time_iterations[ITERATIONS / 2];

    // Standard Deviation
    double mean = std::accumulate(time_iterations.begin(), time_iterations.end(), 0.0) / ITERATIONS;
    double sq_sum = 0.0;
    for (double t : time_iterations) sq_sum += (t - mean) * (t - mean);
    double stddev = std::sqrt(sq_sum / ITERATIONS);

    double throughput = N / median;

    uint64_t checksum = 0;
    for (size_t i = 0; i < N; ++i)
        checksum += parts_id[i];

    // Output (same format as CPU versions)
    std::cout << std::fixed << std::setprecision(6);
    std::cout << "Median    : " << median             << " s\n";
    std::cout << "Stddev    : " << stddev             << " s\n";
    std::cout << "Throughput : " << throughput / 1e6 << " million-elements/s\n";
    std::cout << "Checksum <" << N << "> <" << P << "> : " << checksum << "\n";

    // Time to Transfer To / From Device
    std::cout << "Transfer_H2D  : " << stop_1 << " s\n";
    std::cout << "Transfer_D2H  : " << stop_2 << " s\n";
    std::cout << "Transfer_TOT  : " << stop_1 + stop_2 << " s\n";

    // Dump first min(N, 10000) elements for correctness check
    size_t dump_size = std::min(N, static_cast<size_t>(10000));
    std::string filename = std::string(argv[0]) + "_output.txt";
    std::ofstream f(filename);
    for (size_t i = 0; i < dump_size; ++i)
        f << parts_id[i] << "\n";

    return 0;
}
