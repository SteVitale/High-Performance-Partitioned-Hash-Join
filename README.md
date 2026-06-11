# High-Performance Partitioned Hash Join: An Evolutionary Architecture

**A comprehensive journey into High-Performance Computing (HPC), scaling a memory-bound database operation from SIMD micro-optimizations on a single core to a distributed Hybrid MPI+OpenMP cluster.**

---

## Overview

The Partitioned Hash Join is a fundamental, heavily memory-bound algorithm used in modern relational database engines. This project explores its optimization across different hardware paradigms, demonstrating how to overcome architectural bottlenecks at various levels of the compute stack. 

Rather than a monolithic codebase, this repository is structured as an **evolutionary roadmap**. It starts from the lowest level of CPU vectorization and GPU offloading, progresses through shared-memory NUMA optimizations, tackles dynamic load-balancing under severe data skew, and finally scales horizontally using distributed networking.

---

## The Architectural Roadmap

The project is divided into four main logical increments, each located in its respective directory.

### [01] Algorithmic Foundations & Vectorization (`/01-hashing-evaluation-simd`)
This phase focuses on the core partitioning kernel, balancing distribution quality and computational throughput.
* **Hash Function Selection:** Evaluated multiple hash algorithms, selecting the Murmur3 finalizer (`fmix32`) for its ability to guarantee perfectly balanced partitions even with low-entropy inputs.
* **SIMD Vectorization:** Implemented a manual AVX2 (256-bit) vectorization that processes a batch of 16 keys per iteration, utilizing fast in-lane shuffles to ensure 100% lane saturation. 
* **Hardware-Aware Tuning:** Addressed a specific structural bottleneck of the AMD Zen 1 microarchitecture (which physically splits 256-bit instructions) by developing a custom 128-bit unrolled implementation to maximize Instruction-Level Parallelism.
* **GPU Offloading:** Developed a massively data-parallel CUDA version, mapping one thread per 64-bit key across a 1D grid with 256-thread blocks to ensure optimal warp scheduling and Streaming Multiprocessor occupancy.

### [02] Shared-Memory Concurrency & NUMA Awareness (`/02-parallel-hash-join`)
This module scales the algorithm across physical cores on a single machine, implementing a three-step parallel pipeline: Histogram, Prefix Sum, and Scatter.
* **Lock-Free Architecture:** By pre-calculating write cursors via a global prefix sum, threads write to strictly non-overlapping regions of the shared output array, making the Scatter phase completely lock-free.
* **NUMA-Aware Memory Allocation:** Replaced standard `std::vector` allocations with `std::malloc` to prevent sequential OS zero-initialization. This allows parallel threads to trigger the Linux First-Touch policy, optimally distributing memory pages across physical NUMA nodes and mitigating the main memory bandwidth bottleneck.
* **Extreme Stress Testing:** Successfully processed 1 billion records per relation (~16 GB of raw payload) in just 8.06 seconds, maintaining a solid 10x speedup over the optimized sequential baseline.

### [03] Concurrency Models & Skew Mitigation (`/03-openmp-hash-join`)
A deep dive into OpenMP scheduling paradigms and the mitigation of Amdahl's Law under highly skewed workloads.
* **Loop vs. Task Scheduling:** Compared OpenMP Loop constructs with Task-based models, proving that while both converge at scale, tasks introduce measurable overhead when granularity is too fine.
* **Heavy Skew Bottleneck:** Demonstrated how a dataset with 50% identical keys forces a structural sequential bottleneck, stalling threads and spiking join execution times.
* **Nested Tasks for Work-Stealing:** Developed an experimental dynamic skew detection mechanism. When a thread detects a partition exceeding a $3x$ threshold, it switches to "Nested Mode", dividing the probe phase into micro-chunks and spawning nested OpenMP tasks. This successfully wakes idle threads and reduces the skewed join bottleneck by ~33%.

### [04] Distributed Clustering & Hybrid Architectures (`/04-mpi-hash-join`)
The final phase scales the architecture horizontally across multiple nodes using a Bulk Synchronous Parallel (BSP) model on MPI.
* **Two-Level Partitioning:** Implemented Macro-Partitioning for network routing (`MPI_Alltoallv`) and Micro-Partitioning for local node processing
* **Network Contention Analysis:** Identified a critical communication bottleneck at 128 ranks (16 tasks/node), where dense intra-node traffic caused the network to consume 96% of the total execution time.
* **Hybrid MPI+OpenMP Resolution:** Resolved the structural bottleneck by shifting to a Hybrid architecture (1 MPI Rank per node + 16 OpenMP Threads). This reduced the global communication matrix from 128x128 down to 8x8, dropping communication time from 0.993s to just 0.116s (an 88.3% reduction) while fully utilizing physical CPU cores.

---

## Key Performance Highlights

* **Sub-linear Core Scaling:** ~13x speedup in the core computational Join phase utilizing 16 physical cores.
* **Massive Throughput:** System throughput plateaus at ~375 Million records per second on a single NUMA node under uniform distributions.
* **Hybrid Efficiency:** Scaling out to 8 nodes, the Hybrid MPI+OpenMP model achieved a 3.36x absolute speedup compared to the standard pure MPI configuration.

---

## Build & Run Instructions

*(Note: Navigate to the specific directory for detailed, per-module compilation scripts and cluster submission details).*

**Prerequisites:**
* GCC (with AVX2 support)
* NVIDIA CUDA Toolkit
* OpenMP
* MPI (e.g., OpenMPI or MPICH)

```bash