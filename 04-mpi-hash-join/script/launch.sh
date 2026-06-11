#!/bin/bash
# =============================================================================
# launch.sh — Sequential vs Distributed (averaged across different runs)
#
# Usage:
#   ./launch.sh [NR] [NS] [SEED] [MAX_KEY] [P_LOCAL]
#
# Example:
#   ./launch.sh 50000000 50000000 42 500000 256
# =============================================================================

print_header() {
    echo ""
    echo -e "════════════════════════════════════════════════════════"
    echo -e "$1"
    echo -e "════════════════════════════════════════════════════════"
}

make cleanall > /dev/null
make > /dev/null || { echo "Compilation Error"; exit 1; }

# CLI Parameters
NR="${1:-50000000}"
NS="${2:-50000000}"
SEED="${3:-42}"
MAX_KEY="${4:-500000}"
P_LOCAL="${5:-512}"    # Partitions within each process
SKEW_FLAG="${6:-}"

NODES=$SLURM_JOB_NUM_NODES
TASKS=$SLURM_NTASKS
P_GLOBAL=$((TASKS * P_LOCAL))

OUT_SEQ="/tmp/out_seq.txt"
OUT_MPI="/tmp/out_mpi.txt"
RUNS=3

print_header "BENCHMARK [N=$NR, Nodes=$NODES, Tasks=$TASKS, P_Local=$P_LOCAL]"

# =============================================================================
# 1. SEQUENTIAL RUN (Baseline)
# =============================================================================
print_header "SEQUENTIAL BASELINE (Averaged across $RUNS runs)"
TOTAL_TIME_SEQ=0

for i in $(seq 1 $RUNS); do
    # The sequential code runs on a single core of the current node
    srun -N 1 -n 1 --time 00:03:00 ./seq -nr $NR -ns $NS -seed $SEED -max-key $MAX_KEY -p $P_GLOBAL $SKEW_FLAG > "$OUT_SEQ"
    TIME=$(grep "^time_sec=" "$OUT_SEQ" | cut -d= -f2)
    TOTAL_TIME_SEQ=$(awk "BEGIN {print $TOTAL_TIME_SEQ + $TIME}")
    echo "  Run $i: $TIME s"
done

AVG_TIME_SEQ=$(awk "BEGIN { printf \"%.6f\", $TOTAL_TIME_SEQ / $RUNS }")
echo "  -> Average Sequential version: $AVG_TIME_SEQ s"

# =============================================================================
# 2. MPI RUN (Slurm srun)
# =============================================================================
print_header "MPI VERSION (Averaged across $RUNS runs)"
TOTAL_TIME_MPI=0

TOTAL_GEN=0
TOTAL_MACRO_CALC=0
TOTAL_MACRO_NET=0
TOTAL_MICRO_CALC=0
TOTAL_IMB_MACRO=0
TOTAL_IMB_MICRO=0

for i in $(seq 1 $RUNS); do
    srun --mpi=pmix --time 00:03:00 -N $NODES -n $TASKS ./mpi -nr $NR -ns $NS -seed $SEED -max-key $MAX_KEY -p_per_rank $P_LOCAL $SKEW_FLAG > "$OUT_MPI"
    
    TIME=$(grep "^time_sec=" "$OUT_MPI" | cut -d= -f2)
    TOTAL_TIME_MPI=$(awk "BEGIN {print $TOTAL_TIME_MPI + $TIME}")

    TIME_GEN=$(grep "^Generation : " "$OUT_MPI" | awk '{print $3}')
    TIME_MACRO_C=$(grep "^Preparation : " "$OUT_MPI" | awk '{print $3}')
    TIME_MACRO_N=$(grep "^Communication : " "$OUT_MPI" | awk '{print $3}')
    TIME_MICRO_C=$(grep "^Local Join : " "$OUT_MPI" | awk '{print $4}')

    IMB_MACRO=$(grep "^Net Compute Imbalance" "$OUT_MPI" | awk '{print $5}')
    IMB_MICRO=$(grep "^Join Compute Imbalance" "$OUT_MPI" | awk '{print $5}')

    TOTAL_GEN=$(awk "BEGIN {print $TOTAL_GEN + $TIME_GEN}")
    TOTAL_MACRO_CALC=$(awk "BEGIN {print $TOTAL_MACRO_CALC + $TIME_MACRO_C}")
    TOTAL_MACRO_NET=$(awk "BEGIN {print $TOTAL_MACRO_NET + $TIME_MACRO_N}")
    TOTAL_MICRO_CALC=$(awk "BEGIN {print $TOTAL_MICRO_CALC + $TIME_MICRO_C}")

    TOTAL_IMB_MACRO=$(awk "BEGIN {print $TOTAL_IMB_MACRO + $IMB_MACRO}")
    TOTAL_IMB_MICRO=$(awk "BEGIN {print $TOTAL_IMB_MICRO + $IMB_MICRO}")
    
    echo "  Run $i: $TIME s"
done # 

AVG_TIME_MPI=$(awk "BEGIN { printf \"%.6f\", $TOTAL_TIME_MPI / $RUNS }")

AVG_GEN=$(awk "BEGIN { printf \"%.6f\", $TOTAL_GEN / $RUNS }")
AVG_MACRO_C=$(awk "BEGIN { printf \"%.6f\", $TOTAL_MACRO_CALC / $RUNS }")
AVG_MACRO_N=$(awk "BEGIN { printf \"%.6f\", $TOTAL_MACRO_NET / $RUNS }")
AVG_MICRO_C=$(awk "BEGIN { printf \"%.6f\", $TOTAL_MICRO_CALC / $RUNS }")

AVG_IMB_MACRO=$(awk "BEGIN { printf \"%.6f\", $TOTAL_IMB_MACRO / $RUNS }")
AVG_IMB_MICRO=$(awk "BEGIN { printf \"%.6f\", $TOTAL_IMB_MICRO / $RUNS }")

echo "  -> Average MPI version (Network included): $AVG_TIME_MPI s"

# =============================================================================
# 3. CORRECTNESS CHECK
# =============================================================================
print_header "CORRECTNESS CHECK"

# Extract counters and checksums, ignoring the differing execution times
grep -E "^(join_count|checksum1|checksum2)=" "$OUT_SEQ" > /tmp/_seq_check.txt
grep -E "^(join_count|checksum1|checksum2)=" "$OUT_MPI" > /tmp/_mpi_check.txt

if diff -q /tmp/_seq_check.txt /tmp/_mpi_check.txt > /dev/null 2>&1; then
    echo -e " CORRECT : join_count and checksums match 100%"
    echo ""
    echo "--- seq version Output ---"
    cat /tmp/_seq_check.txt
    echo "--- mpi version Output ---"
    cat /tmp/_mpi_check.txt

else
    echo -e " ERROR : Results differ."
    echo ""
    echo "--- Sequential Output ---"
    cat /tmp/_seq_check.txt
    echo "--- MPI Output ---"
    cat /tmp/_mpi_check.txt
fi

# =============================================================================
# 4. HPC METRICS CALCULATION (Speedup and Efficiency)
# =============================================================================
print_header "METRICS COMPARISON"

SPEEDUP=$(awk "BEGIN { printf \"%.3f\", $AVG_TIME_SEQ / $AVG_TIME_MPI }")


echo -e "  Sequential time : ${AVG_TIME_SEQ} s"
echo -e "  MPI time        : ${AVG_TIME_MPI} s"
echo -e "  Speedup         : ${SPEEDUP}x"
echo ""

print_header "MPI Phase Breakdown (Max per phase across nodes) "

echo "  Centralized Data Setup (Excluded) : ${AVG_GEN} s"
echo "  Network Distribution Compute      : ${AVG_MACRO_C} s"
echo "  Network Distribution MPI (Net)    : ${AVG_MACRO_N} s"
echo "  Local Partition & Join Compute    : ${AVG_MICRO_C} s"

echo ""
echo "  Net Compute Imbalance            : ${AVG_IMB_MACRO} s"
echo "  Join Compute Imbalance           : ${AVG_IMB_MICRO} s"
echo ""

echo ""