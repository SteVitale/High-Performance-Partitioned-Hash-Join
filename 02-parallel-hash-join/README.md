# COMPILATION, EXECUTION and EVALUATION

The entire pipeline has been fully automated to ensure **readability** and **reproducibility**.

**To compile, run and evaluate all implementations at once**, use the `launch.sh` script (ensure the correct permissions with `chmod +x launch.sh` and also in all other .sh files). The script takes exactly the same arguments as the baseline code (NR, NS, SEED, MAX-KEY, P) and the parallel version takes also T argument for the number of threads.

```bash
------------------------------------------------------------------------------------------------------
# General Command:
./launch.sh [NR] [NS] [SEED] [MAX_KEY] [P] [T]

# Example:
./launch.sh 15000000 16000000 47 65400 2048 16
------------------------------------------------------------------------------------------------------
# Command used for SPM Cluster:
srun --time 00:05:00 ./launch.sh 15000000 16000000 47 65400 2048 16
------------------------------------------------------------------------------------------------------
# Default Run (If no arguments are passed, it defaults to):
# N = 50,000,000 | P = 2048 | T = 16 | SEED = 42 | MAX_KEY = 500,000
srun --time 00:05:00 ./launch.sh
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
════════════════════════════════════════════════════════
BENCHMARK [NR=15000000, NS=16000000, P=2048, T=16]
════════════════════════════════════════════════════════

════════════════════════════════════════════════════════
SEQUENTIAL BASELINE
════════════════════════════════════════════════════════
NR=15000000 NS=16000000 P=2048 seed=47 [0, 65400)
join_count=3669723161
checksum1=18174694292745652976
checksum2=13885619727587441884
time_sec=1.233131

════════════════════════════════════════════════════════
PARALLEL VERSION (T = 16)
════════════════════════════════════════════════════════
NR=15000000 NS=16000000 P=2048 seed=47 [0, 65400)
join_count=3669723161
checksum1=18174694292745652976
checksum2=13885619727587441884
time_sec=0.086761

════════════════════════════════════════════════════════
CORRECTNESS CHECK
════════════════════════════════════════════════════════
CORRECT — join_count and checksums match

Sequential:
join_count=3669723161
checksum1=18174694292745652976
checksum2=13885619727587441884

Parallel:
join_count=3669723161
checksum1=18174694292745652976
checksum2=13885619727587441884

════════════════════════════════════════════════════════
SPEEDUP
════════════════════════════════════════════════════════
  seq time : 1.233131 s
  par time : 0.086761 s
  speedup  : 14.213x
  efficiency: 88.8%
```

### Naive Check Script
I've included a simple script named `naive_check.sh` which can be used to check the `naive_join_verifier` function in all version of hash join.
The script makes `diff` over `seq`, `seq_original` and `par` naive checksums.


### Benchmark Scripts
I've included also the folder `/Benchmark/` which contains different bash script used to evaluate all the metrics described in the report pdf.

The scripts are the following
* **breakdown_times.sh** : Captures the execution time of individual internal phases (Histogram, Scatter, Join, etc.).
* **strong_scaling.sh** : Evaluates speedup and efficiency with a fixed problem size while varying threads.
* **weak_scaling.sh** : Evaluates performance while proportionally scaling both threads and the dataset size.
* **sweep_n.sh** : Analyzes data scalability and throughput up to 150+ million records.
* **sweep_p.sh** : Determines the optimal number of partitions ($P$) for cache locality.
