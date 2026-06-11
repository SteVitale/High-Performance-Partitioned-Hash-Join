# COMPILATION, EXECUTION and EVALUATION

The entire pipeline has been fully automated to ensure **readability** and **reproducibility**.

**To compile, run and evaluate all implementations at once**, use the `launch.sh` script (ensure the correct permissions with `chmod +x launch.sh` and also in all other .sh files). 
```bash
------------------------------------------------------------------------------------------------------
# General Command:
./launch.sh [DISTR] [NR] [NS] [SEED] [MAX_KEY] [P] [T] 

# Example:
./launch.sh heavy_skew 15000000 16000000 47 65400 2048 16 
------------------------------------------------------------------------------------------------------
# Command used for SPM Cluster:
srun --time 00:05:00 ./launch.sh heavy_skew 15000000 16000000 47 65400 2048 16 
------------------------------------------------------------------------------------------------------
# Default Run (If no arguments are passed, it defaults to):
# DISTR = uniform | N = 50,000,000 | P = 2048 | T = 16 | SEED = 42 | MAX_KEY = 500,000 
srun --time 00:05:00 ./launch.sh

# The --distr argument is set first on the launch.sh parsing, allowing fast comparison like this:
./launch.sh uniform
./launch.sh multiples
./launch.sh step_skew
./launch.sh heavy_skew

------------------------------------------------------------------------------------------------------
```
The script does the following things:
* **Cleans** the environment and recompiles all binaries via makefile
* **Executes** every binary (seq and par) passing the parameters
* **Parses the output** metrics and presents them in a compact readable table.
* **Checks** the join count and checksums across both versions of hash join.


### Example Output of launch.sh
```bash
(... result of make cleanall && make ...)
══════════════════════════════════════════════════════════════════════════
BENCHMARK [NR=15000000, NS=16000000, P=2048, T=16, DISTR=uniform, RUNS=5]
══════════════════════════════════════════════════════════════════════════

══════════════════════════════════════════════════════════════════════════
1. SEQUENTIAL BASELINE (Averaging 5 runs)
══════════════════════════════════════════════════════════════════════════
NR=15000000 NS=16000000 P=2048 seed=47 [0, 65400)
join_count=3669640586
checksum1=1635802993802706952
checksum2=7563415937912943481
time_sec=0.949692

══════════════════════════════════════════════════════════════════════════
2. C++ THREADS (T = 16 | Averaging 5 runs)
══════════════════════════════════════════════════════════════════════════
NR=15000000 NS=16000000 P=2048 seed=47 [0, 65400)
join_count=3669640586
checksum1=1635802993802706952
checksum2=7563415937912943481
time_sec=0.085951

══════════════════════════════════════════════════════════════════════════
3. OMP LOOP (T = 16 | Averaging 5 runs)
══════════════════════════════════════════════════════════════════════════
NR=15000000 NS=16000000 P=2048 seed=47 [0, 65400)
join_count=3669640586
checksum1=1635802993802706952
checksum2=7563415937912943481
time_sec=0.084452

══════════════════════════════════════════════════════════════════════════
4. OMP TASK (T = 16 | Averaging 5 runs)
══════════════════════════════════════════════════════════════════════════
NR=15000000 NS=16000000 P=2048 seed=47 [0, 65400)
join_count=3669640586
checksum1=1635802993802706952
checksum2=7563415937912943481
time_sec=0.085471

══════════════════════════════════════════════════════════════════════════
CORRECTNESS CHECK
══════════════════════════════════════════════════════════════════════════
 PARALLEL matches SEQUENTIAL
 OMP_LOOP matches SEQUENTIAL
 OMP_TASK matches SEQUENTIAL

Values:
join_count=3669640586
checksum1=1635802993802706952
checksum2=7563415937912943481

══════════════════════════════════════════════════════════════════════════
PERFORMANCE SUMMARY (AVERAGE)
══════════════════════════════════════════════════════════════════════════
  Sequential : 0.949258 s
  C++ Threads: 0.086377 s       (Speedup: 10.990x, Eff: 68.7%)
  OMP Loop   : 0.084574 s       (Speedup: 11.224x, Eff: 70.1%)
  OMP Task   : 0.085260 s       (Speedup: 11.134x, Eff: 69.6%)
```

### Benchmark Scripts (Script Folder)
I've included also the folder `/script/` which contains different bash scripts used to evaluate all the metrics described in the report pdf.

The scripts are the following
* **breakdown_times.sh** : Captures the execution time of individual internal phases (Histogram, Scatter, Join, etc.).
* **strong_scaling.sh** : Evaluates speedup and efficiency with a fixed problem size while varying threads.
* **weak_scaling.sh** : Evaluates performance while proportionally scaling both threads and the dataset size.
* **nested_analysis.sh** : Similar to launch.sh, but also execute the hashjoin_omp_nested.cpp implementation.
* **naive_check.sh** : Can be used to check the `naive_join_verifier` function in all version of hash join. The script makes `diff` over `seq`, and `par` naive checksums.

### ./utils/distribution_analysis.cpp
Used internally to generate the distribution plots in the report (figures 1 and 2); not required for correctness or performance evaluation. 