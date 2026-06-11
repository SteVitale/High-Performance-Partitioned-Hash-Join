#!/bin/bash
# =============================================================================
# weak_scaling.sh — Weak Scaling Benchmark
# Usage: ./weak_scaling.sh [N_PER_RANK] [NODES] [SKEW_FLAG]
#
# Weak scaling efficiency = T(1 node) / T(P nodes)  — ideal: 1.0
# The 1-node baseline is saved to /tmp/weak_baseline.txt on the first call
# and reused by subsequent calls within the same job.
# =============================================================================

# Fixed parameters
SEED=42
P_LOCAL=512
RUNS=3

# CLI parameters
N_PER_RANK="${1:-3000000}"
NODES="${2:-1}"
SKEW_FLAG="${3:-}"

if [ -z "$SLURM_NTASKS_PER_NODE" ]; then
    echo "ERROR: SLURM_NTASKS_PER_NODE not set — must run inside a SLURM job"
    exit 1
fi

TASKS_PER_NODE=$SLURM_NTASKS_PER_NODE
TASKS=$((NODES * TASKS_PER_NODE))

# Global load scales linearly with tasks — each rank always processes N_PER_RANK records
WEAK_N=$((N_PER_RANK * TASKS))

# MAX_KEY scales with WEAK_N to keep collision density per rank constant (ratio = 100)
MAX_KEY=$((WEAK_N / 100))

OUT_TMP="/tmp/weak_run.txt"
BASELINE_FILE="/tmp/weak_baseline.txt"   # persists across calls within the same job

echo -e "\n========================================================"
echo " WEAK SCALING: Nodes=$NODES | Tasks=$TASKS | N/rank=$N_PER_RANK"
echo " Global N=$WEAK_N | Max-Key=$MAX_KEY"
echo -e "========================================================"

# =============================================================================
# MPI runs
# =============================================================================
T_TOT=0; T_GEN=0; T_MC=0; T_MN=0; T_MJ=0

for r in $(seq 1 $RUNS); do
    srun -N $NODES -n $TASKS --ntasks-per-node $TASKS_PER_NODE --mpi=pmix ./mpi \
        -nr $WEAK_N -ns $WEAK_N \
        -seed $SEED -max-key $MAX_KEY \
        -p_per_rank $P_LOCAL $SKEW_FLAG > "$OUT_TMP"

    TIME=$(grep     "^time_sec="       "$OUT_TMP" | cut -d= -f2)
    TIME_GEN=$(grep  "^Generation : "  "$OUT_TMP" | awk '{print $3}')
    TIME_MACRO_C=$(grep "^Preparation : "   "$OUT_TMP" | awk '{print $3}')
    TIME_MACRO_N=$(grep "^Communication : " "$OUT_TMP" | awk '{print $3}')
    TIME_MICRO_C=$(grep "^Local Join : "    "$OUT_TMP" | awk '{print $4}')

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

# =============================================================================
# Weak scaling efficiency — baseline is the 1-node run (saved on first call)
# =============================================================================
if [ "$NODES" -eq 1 ]; then
    echo "$A_TOT" > "$BASELINE_FILE"
    EFFICIENCY="1.000  (baseline)"
else
    if [ -f "$BASELINE_FILE" ]; then
        T_BASE=$(cat "$BASELINE_FILE")
        EFFICIENCY=$(awk "BEGIN {printf \"%.3f\", $T_BASE / $A_TOT}")
    else
        EFFICIENCY="n/a  (baseline not found — run 1-node first)"
    fi
fi

echo -e "--------------------------------------------------------"
echo "  MPI time (avg)         : $A_TOT s"
echo "  Weak scaling efficiency: $EFFICIENCY  (ideal: 1.000)"
echo "  Net  Comp              : $A_MC s"
echo "  Net  Comm              : $A_MN s"
echo "  Local Join             : $A_MJ s"
echo -e "========================================================\n"

rm -f "$OUT_TMP"