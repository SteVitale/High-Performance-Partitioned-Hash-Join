# COMPILATION - EXECUTION - EVALUATION

To ensure maximum **readability** and **reproducibility**, the entire pipeline has been fully automated. 

It's possible to either compile the binaries manually using the provided `Makefile`, or let the SLURM submission scripts handle everything (compilation, execution, and output parsing) automatically.

---

### 1. Manual Compilation 
The project uses a standard Makefile. If you want to compile the binaries manually to test them outside of SLURM jobs:
```bash
make seq      # Compiles only the Sequential baseline (g++)
make mpi      # Compiles only the Distributed MPI version (mpicxx)
make hybrid   # Compiles only the Hybrid MPI + OpenMP version (mpicxx + fopenmp)
```

### 2. Automated Execution via Script
To run the code on the cluster, specific `submit_*.sh` scripts are provided. These scripts automatically handle the `sbatch` resource allocation, compile the code, run the binaries with `srun`, and parse the raw output into a clean, readable summary.

Ensure the scripts have execution permissions (`chmod +x submit_*.sh script/*.sh`).

**Examples:**
```bash
------------------------------------------------------------------------------------------------------
# General Command 
sbatch submit_<name>.sh [NR] [NS] [SEED] [MAX_KEY] [P_LOCAL]
cat <name>_results.txt 

# Example (Running launch.sh with custom parameters):
sbatch submit_launch.sh 15000000 16000000 42 65400 512
cat launch_results.txt
------------------------------------------------------------------------------------------------------
# Default Run (If no arguments are passed, they default to):
# NR = 50,000,000 | NS = 50,000,000 | SEED = 42 | MAX_KEY = 500000 | P_LOCAL = 512
sbatch submit_launch.sh 
------------------------------------------------------------------------------------------------------
```

---

### Available Submission Scripts

* **`submit_launch.sh`**: Allocates **4 nodes** (32 total tasks, 8 per node). It wraps the internal `script/launch.sh` to run broader evaluations across different versions. Output is saved to `launch_results.txt`.
* **`submit_hybrid.sh`**: Allocates **8 nodes** (1 MPI rank per node, 16 OpenMP threads per rank). It automatically cleans, compiles `make hybrid`, executes it, and outputs a highly formatted summary in `hybrid_results.txt`.
* **`submit_naive.sh`**: Allocates **1 node** (4 tasks). Calls `script/naive_check.sh` to run a strict correctness check (`diff`) between the sequential and parallel naive checksums. Output is saved to `naive_results.txt`.
* **`submit_strong.sh`**: Allocates **8 nodes**. Calls `script/strong_scaling.sh` to run a strong scaling analysis, iterating the execution on an increasing number of nodes (1, 2, 4, 8). Output is saved to `strong_results.txt`.
* **`submit_weak.sh`**: Allocates **8 nodes**. Calls `script/weak_scaling.sh` to run a weak scaling analysis, iterating the execution on an increasing number of nodes (1, 2, 4, 8) and proportionally increasing the problem size. Output is saved to `weak_results.txt`.

---

### MPI Ranks Configuration

As described in the report, two different configurations were evaluated with this setup (8 tasks per node and 16 tasks per node). To choose which one to use, simply modify the following lines in the submit files:

```bash
#SBATCH --nodes=4                  
#SBATCH --ntasks=32   
#SBATCH --ntasks-per-node=8
```

These parameters are read internally by all the scripts using SLURM environmental variables:
```bash
NODES=$SLURM_JOB_NUM_NODES
TASKS=$SLURM_NTASKS
TASKS_PER_NODE=$SLURM_NTASKS_PER_NODE
```

### Skewness Tests

As described in the last section of the report, a brief experiment was conducted using heavy skew distribution. To replicate the results obtained, just modify the `SKEW_FLAG` in any submit file from '' to `--skewed` as showed below.

```bash
#SKEW_FLAG="--skewed"
SKEW_FLAG=""
```
---

### Example Output (`launch_results.txt`)
When you run `sbatch submit_launch.sh`, instead of dealing with raw MPI logs, the script processes the standard output to present this clean overview:

```text
════════════════════════════════════════════════════════
BENCHMARK [N=50000000, Nodes=4, Tasks=32, P_Local=512]
════════════════════════════════════════════════════════

════════════════════════════════════════════════════════
SEQUENTIAL BASELINE (Averaged across 5 runs)
════════════════════════════════════════════════════════
  Run 1: 4.000073 s
  Run 2: 4.002286 s
  Run 3: 4.047101 s
  Run 4: 4.031518 s
  Run 5: 4.051674 s
  -> Average Sequential version: 4.026540 s

════════════════════════════════════════════════════════
MPI VERSION (Averaged across 5 runs)
════════════════════════════════════════════════════════
  Run 1: 0.475211 s
  Run 2: 0.645770 s
  Run 3: 0.658777 s
  Run 4: 1.406229 s
  Run 5: 0.620570 s
  -> Average MPI version (Network included): 0.761312 s

════════════════════════════════════════════════════════
CORRECTNESS CHECK
════════════════════════════════════════════════════════
 CORRECT : join_count and checksums match 100%

--- seq version Output ---
join_count=4999983291
checksum1=8587968115490242416
checksum2=14013769443579975969
--- mpi version Output ---
join_count=4999983291
checksum1=8587968115490242416
checksum2=14013769443579975969

════════════════════════════════════════════════════════
METRICS COMPARISON
════════════════════════════════════════════════════════
  Sequential time : 4.026540 s
  MPI time        : 0.761312 s
  Speedup         : 5.289x


════════════════════════════════════════════════════════
MPI Phase Breakdown (Max per phase across nodes)
════════════════════════════════════════════════════════
  Centralized Data Setup (Excluded) : 2.304680 s
  Network Distribution Compute      : 0.052621 s
  Network Distribution MPI (Net)    : 0.644102 s
  Local Partition & Join Compute    : 0.082036 s

  Net Compute Imbalance            : 0.008939 s
  Join Compute Imbalance           : 0.011536 s
```

---

### Project Structure Details

**`./cpp/` Folder:**
* **`hashjoin_seq.cpp`**: The original sequential baseline of the partitioned hash join used in the previous modules.
* **`hashjoin_mpi.cpp`**: The distributed implementation of the partitioned hash join.
* **`hashjoin_hybrid.cpp`**: The hybrid (MPI + OpenMP) implementation of the partitioned hash join.

**`./executed_script/` Folder:**
* Contains execution summaries from the different scripts, which were used to populate the project report with relevant and grounded performance metrics.

**`./include/` Folder:**
* **`SimpleHashTable.hpp`**: The same flat hash table used across all modules.
* **`omp_utils.hpp`**: All the parallel functions developed in the module 3 used in the hybrid version of this module.