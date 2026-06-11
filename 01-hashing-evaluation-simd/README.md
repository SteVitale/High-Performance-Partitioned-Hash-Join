# Compilation, Execution and Evaluation

The entire pipeline has been fully automated to ensure **readability** and **reproducibility**.

**To compile, run and evaluate all implementations at once**, use the `launch.sh` script (ensure the right permission with `chmod +x launch.sh`). The script takes two optional arguments: `N` (millions of keys) and `P` (exponent for the number of partitions).

```bash
------------------------------------------------------------------------------------------------------
# General Command:
./launch.sh [N_million] [P_exponent]
# Example:
./launch.sh 100 18
------------------------------------------------------------------------------------------------------
# Command used for SPM Cluster: 
srun -N 1 --partition=gpu-excl --nodelist=node09 --time 00:00:50 ./launch.sh [N_millions] [P_exponent]
# Example:
srun -N 1 --partition=gpu-excl --nodelist=node09 --time 00:00:50 ./launch.sh 100 18
------------------------------------------------------------------------------------------------------
# Default (no args passed): 
N = 50 million, P = 2^20
------------------------------------------------------------------------------------------------------
```
The script does the following things:
* **Cleans** the environment and recompiles all binaries via makefile
* **Executes** every binary (Baseline, Autovec, AVX2, CUDA) passing the `N` and `P` parameters.
* **Parses the output** metrics and presents them in a compact readable table.
* **Checks** the global checksum across all versions to ensure macro-correctness.
* **Runs a `diff`** command on the `[name]_output.txt` files (containing the first 10.000 elements) to guarantee element-by-element bitwise correctness.

If preferred, each component can be compiled individually via makefile (in this case, args `N` and `P` are mandatory):
* **make baseline**: Produces `baseline` executable.
* **make autovec** : Produces `autovec`. It also generates a `vec_report.txt` containing the compiler's vectorization proofs.
* **make avx** : Produces `avx` version, using only 128bit registers.
* **make avx2**: Produces the `avx2` version, using also 256bit registers.
* **make cuda**: Produces the `cuda` executable.

### Standardized I/O of Executables
The input format is standardized across all executables. For example, running `./baseline 50 20` sets `N = 50.000.000` and `P = 2^20`. To ease parsing and comparison, every binary produces a strictly formatted output:

```bash
Median     : 0.054523 s     # median of the duration time across 7 runs
Stddev     : 0.002898 s     # standard deviation of 7 runs
Throughput : 550.222221 million-elements/s 
Checksum <50000000> <1048576> : 15728352510506 # <N> <P> : CHECKSUM
```

Additionally, each binary dumps the first 10.000 mapped partitions into `[name]_output.txt` file for the automated diff verification 

### Example Output of launch.sh
```bash
(... result of make cleanall && make ...)
============================================================
  Partition Mapping Benchmark
  N = 100M keys   |   P = 2^20 = 1048576
============================================================

  PERFORMANCE  (median ± stddev, 7 runs)
------------------------------------------------------------
  baseline:     median=216.721 ms      stddev=3.072 ms        throughput=461.42 MOps/s
  autovec:      median=86.594 ms       stddev=0.040 ms        throughput=1154.82 MOps/s
  avx:          median=85.892 ms       stddev=0.089 ms        throughput=1164.25 MOps/s
  avx2:         median=91.013 ms       stddev=0.048 ms        throughput=1098.74 MOps/s
------------------------------------------------------------
  cuda kernel:  median=1.507 ms        stddev=0.002 ms        throughput=66338.56 MOps/s
  cuda transf:  H2D=79.488 ms         D2H=52.252 ms         Total=131.740 ms
------------------------------------------------------------
  speedup autovec vs baseline: 2.50x
  speedup avx     vs baseline: 2.52x
  speedup avx2    vs baseline: 2.38x
  speedup cuda    vs baseline: 143.80x

  CHECKSUM COMPARISON
------------------------------------------------------------
  baseline:     52429384298384
  autovec:      52429384298384
  avx:          52429384298384
  avx2:         52429384298384
  cuda:         52429384298384
------------------------------------------------------------
  PASSED — all checksums match

  CORRECTNESS  (first 10000 elements, element-by-element)
------------------------------------------------------------
  PASSED — baseline matches autovec
  PASSED — baseline matches avx
  PASSED — baseline matches avx2
  PASSED — baseline matches cuda

============================================================
```

## Hash Analysis for choosing the optimal Hash Function
I've included also the folder `/Hash_Analysis` with which I've evaluated different kinds of hash functions across different ways of creating keys.
The folder contains two main files: 
* **main_test_hash_distribution.cpp** : used to evaluate the **quality** of the distribution produced by the hash function across all the partitions
* **main_test_hash_performance.cpp** : used to evaluate the **throughput** of different hash functions

The file `distribution.hpp` contains the utility functions to simulate different distributions of key space (and is called from all the binaries for generating keys).

The file `hash_to_test.hpp` contains the implementation of all the hash functions evaluated during the test.

These two tests can be executed individually after compiling them via the Makefile located inside the `/Hash_Analysis` folder.