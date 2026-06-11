#!/bin/bash
# =============================================================================
# strong_scaling.sh — Strong Scaling Benchmark
# Usage: ./strong_scaling.sh [GLOBAL_N] [NODES] [SKEW_FLAG]
# =============================================================================

# Fixed parameters
SEED=42
MAX_KEY=500000
P_LOCAL=512
RUNS=3

# CLI parameters
STRONG_N="${1:-50000000}"
NODES="${2:-1}"
SKEW_FLAG="${3:-}"


TASKS_PER_NODE=$SLURM_NTASKS_PER_NODE
TASKS=$((NODES * TASKS_PER_NODE))
P_GLOBAL=$((TASKS * P_LOCAL))   # total partitions for sequential (must match MPI total)

OUT_TMP="/tmp/strong_run.txt"
OUT_SEQ="/tmp/strong_seq.txt"

echo -e "\n========================================================"
echo " STRONG SCALING: N=$STRONG_N | Nodes=$NODES | Tasks=$TASKS"
echo -e "========================================================"

# =============================================================================
# Sequential baseline — run once, same N and parameters
# =============================================================================
T_SEQ=0
for r in $(seq 1 $RUNS); do
    srun -N 1 -n 1 --cpu-bind=none ./seq \
        -nr $STRONG_N -ns $STRONG_N \
        -seed $SEED -max-key $MAX_KEY \
        -p $P_GLOBAL $SKEW_FLAG > "$OUT_SEQ"
    T=$(grep "^time_sec=" "$OUT_SEQ" | cut -d= -f2)
    T_SEQ=$(awk "BEGIN {print $T_SEQ + $T}")
done
A_SEQ=$(awk "BEGIN {printf \"%.6f\", $T_SEQ / $RUNS}")
echo "  [seq] Average: $A_SEQ s"

# =============================================================================
# MPI runs
# =============================================================================
T_TOT=0; T_GEN=0; T_MC=0; T_MN=0; T_MJ=0

for r in $(seq 1 $RUNS); do
    srun -N $NODES -n $TASKS --ntasks-per-node $TASKS_PER_NODE --mpi=pmix ./mpi \
        -nr $STRONG_N -ns $STRONG_N \
        -seed $SEED -max-key $MAX_KEY \
        -p_per_rank $P_LOCAL $SKEW_FLAG > "$OUT_TMP"

    TIME=$(grep     "^time_sec="      "$OUT_TMP" | cut -d= -f2)
    TIME_GEN=$(grep  "^Generation : " "$OUT_TMP" | awk '{print $3}')
    TIME_MACRO_C=$(grep "^Preparation : "  "$OUT_TMP" | awk '{print $3}')
    TIME_MACRO_N=$(grep "^Communication : " "$OUT_TMP" | awk '{print $3}')
    TIME_MICRO_C=$(grep "^Local Join : "   "$OUT_TMP" | awk '{print $4}')

    T_TOT=$(awk "BEGIN {print $T_TOT + $TIME}")
    T_GEN=$(awk "BEGIN {print $T_GEN + $TIME_GEN}")
    T_MC=$(awk  "BEGIN {print $T_MC  + $TIME_MACRO_C}")
    T_MN=$(awk  "BEGIN {print $T_MN  + $TIME_MACRO_N}")
    T_MJ=$(awk  "BEGIN {print $T_MJ  + $TIME_MICRO_C}")
    echo "  Run $r: $TIME s"
done

A_TOT=$(awk "BEGIN {printf \"%.6f\", $T_TOT / $RUNS}")
A_GEN=$(awk "BEGIN {printf \"%.6f\", $T_GEN / $RUNS}")
A_MC=$(awk  "BEGIN {printf \"%.6f\", $T_MC  / $RUNS}")
A_MN=$(awk  "BEGIN {printf \"%.6f\", $T_MN  / $RUNS}")
A_MJ=$(awk  "BEGIN {printf \"%.6f\", $T_MJ  / $RUNS}")

SPEEDUP=$(awk    "BEGIN {printf \"%.3f\", $A_SEQ / $A_TOT}")
EFFICIENCY=$(awk "BEGIN {printf \"%.3f\", ($A_SEQ / $A_TOT) / $TASKS}")

echo -e "--------------------------------------------------------"
echo "  Sequential time (avg)  : $A_SEQ s"
echo "  MPI time (avg)         : $A_TOT s"
echo "  Speedup                : ${SPEEDUP}x  (ideal: ${TASKS}x)"
echo "  Efficiency             : $EFFICIENCY"
echo "  Net Comp               : $A_MC s"
echo "  Net Comm               : $A_MN s"
echo "  Local Join             : $A_MJ s"
echo -e "========================================================\n"

rm -f "$OUT_TMP" "$OUT_SEQ"