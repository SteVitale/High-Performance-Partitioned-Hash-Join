#!/bin/bash
# =============================================================================
# launch.sh — Run sequential, parallel and OpenMP loop hashjoin and compare
#
# Usage:
#   ./launch.sh [DISTR] [NR] [NS] [SEED] [MAX_KEY] [P] [T]
#
# Defaults:
#   DISTR   = uniform    (uniform, multiples, step_skew, heavy_skew)
#   NR      = 50000000   (50M records in R)
#   NS      = 50000000   (50M records in S)
#   SEED    = 42
#   MAX_KEY = 500000
#   P       = 2048       (number of partitions, must be power of 2)
#   T       = 16         (number of threads) [only for parallel/omp]
# =============================================================================

# --- Helpers ---
print_header() {
    echo ""
    echo -e "══════════════════════════════════════════════════════════════════════════"
    echo -e "$1"
    echo -e "══════════════════════════════════════════════════════════════════════════"
}

# ensure fresh binaries
make cleanall && make seq par omp_loop omp_task > /dev/null

# --- Parameters (DISTR is the first parameter, allowing distribution analysis in a simpler way) ---
DISTR="${1:-uniform}"
NR="${2:-50000000}"
NS="${3:-50000000}"
SEED="${4:-42}"
MAX_KEY="${5:-500000}"
P="${6:-2048}"
T="${7:-16}"

# Number of Runs 
RUNS=5

# --- OpenMP specific config ---
export OMP_NUM_THREADS=$T
export OMP_PLACES="${OMP_PLACES:-cores}"
export OMP_PROC_BIND="${OMP_PROC_BIND:-spread}"
export OMP_DISPLAY_ENV=TRUE 

# --- Paths ---
SEQ="./seq"
PAR="./par"
OMP_LOOP="./omp_loop"
OMP_TASK="./omp_task"

OUT_SEQ="out_seq.txt"
OUT_PAR="out_par.txt"
OUT_OMP_LOOP="out_omp_loop.txt"
OUT_OMP_TASK="out_omp_task.txt"

PROF_SEQ="profiling_seq.txt"
PROF_PAR="profiling_par.txt"
PROF_OMP_LOOP="profiling_omp_loop.txt"
PROF_OMP_TASK="profiling_omp_task.txt"

print_header "BENCHMARK [NR=$NR, NS=$NS, P=$P, T=$T, DISTR=$DISTR, RUNS=$RUNS]"

# =============================================================================
# EXECUTIONS & AVERAGING
# =============================================================================

print_header "1. SEQUENTIAL BASELINE (Averaging $RUNS runs)"
SUM_SEQ=0
for (( i=1; i<=RUNS; i++ )); do
    $SEQ -nr $NR -ns $NS -seed $SEED -max-key $MAX_KEY -p $P -distr $DISTR > "$OUT_SEQ" 2> "$PROF_SEQ"
    T_VAL=$(grep "^time_sec=" "$OUT_SEQ" | cut -d= -f2)
    SUM_SEQ=$(awk "BEGIN {print $SUM_SEQ + $T_VAL}")
done
TIME_SEQ=$(awk "BEGIN {printf \"%.6f\", $SUM_SEQ / $RUNS}")
cat $OUT_SEQ

print_header "2. C++ THREADS (T = $T | Averaging $RUNS runs)"
SUM_PAR=0
for (( i=1; i<=RUNS; i++ )); do
    $PAR -nr $NR -ns $NS -seed $SEED -max-key $MAX_KEY -p $P -t $T -distr $DISTR > "$OUT_PAR" 2> "$PROF_PAR"
    T_VAL=$(grep "^time_sec=" "$OUT_PAR" | cut -d= -f2)
    SUM_PAR=$(awk "BEGIN {print $SUM_PAR + $T_VAL}")
done
TIME_PAR=$(awk "BEGIN {printf \"%.6f\", $SUM_PAR / $RUNS}")
cat $OUT_PAR

print_header "3. OMP LOOP (T = $T | Averaging $RUNS runs)"
SUM_OMP_LOOP=0
for (( i=1; i<=RUNS; i++ )); do
    $OMP_LOOP -nr $NR -ns $NS -seed $SEED -max-key $MAX_KEY -p $P -t $T -distr $DISTR > "$OUT_OMP_LOOP" 2> "$PROF_OMP_LOOP"
    T_VAL=$(grep "^time_sec=" "$OUT_OMP_LOOP" | cut -d= -f2)
    SUM_OMP_LOOP=$(awk "BEGIN {print $SUM_OMP_LOOP + $T_VAL}")
done
TIME_OMP_LOOP=$(awk "BEGIN {printf \"%.6f\", $SUM_OMP_LOOP / $RUNS}")
cat $OUT_OMP_LOOP

print_header "4. OMP TASK (T = $T | Averaging $RUNS runs)"
SUM_OMP_TASK=0
for (( i=1; i<=RUNS; i++ )); do
    $OMP_TASK -nr $NR -ns $NS -seed $SEED -max-key $MAX_KEY -p $P -t $T -distr $DISTR > "$OUT_OMP_TASK" 2> "$PROF_OMP_TASK"
    T_VAL=$(grep "^time_sec=" "$OUT_OMP_TASK" | cut -d= -f2)
    SUM_OMP_TASK=$(awk "BEGIN {print $SUM_OMP_TASK + $T_VAL}")
done
TIME_OMP_TASK=$(awk "BEGIN {printf \"%.6f\", $SUM_OMP_TASK / $RUNS}")
cat $OUT_OMP_TASK

# =============================================================================
# CORRECTNESS CHECK
# =============================================================================
print_header "CORRECTNESS CHECK"

# Extract key fields only (exclude time_sec which will differ)
grep -E "^(join_count|checksum1|checksum2)=" "$OUT_SEQ" > /tmp/_seq_check.txt
grep -E "^(join_count|checksum1|checksum2)=" "$OUT_PAR" > /tmp/_par_check.txt
grep -E "^(join_count|checksum1|checksum2)=" "$OUT_OMP_LOOP" > /tmp/_omp_loop_check.txt
grep -E "^(join_count|checksum1|checksum2)=" "$OUT_OMP_TASK" > /tmp/_omp_task_check.txt

# Compare SEQ vs PAR
if diff -q /tmp/_seq_check.txt /tmp/_par_check.txt > /dev/null 2>&1; then
    echo -e " PARALLEL matches SEQUENTIAL"
else
    echo -e " MISMATCH — Parallel outputs differ from Sequential"
fi

# Compare SEQ vs OMP Loop
if diff -q /tmp/_seq_check.txt /tmp/_omp_loop_check.txt > /dev/null 2>&1; then
    echo -e " OMP_LOOP matches SEQUENTIAL"
else
    echo -e " MISMATCH — OMP Loop outputs differ from Sequential"
fi

# Compare SEQ vs OMP Task
if diff -q /tmp/_seq_check.txt /tmp/_omp_task_check.txt > /dev/null 2>&1; then
    echo -e " OMP_TASK matches SEQUENTIAL"
else
    echo -e " MISMATCH — OMP Task outputs differ from Sequential"
fi

echo ""
echo "Values:"
cat /tmp/_seq_check.txt

# =============================================================================
# SPEEDUP (USING AVERAGED TIMES)
# =============================================================================
print_header "PERFORMANCE SUMMARY (AVERAGE)"

SPEEDUP_PAR=$(awk "BEGIN { printf \"%.3f\", $TIME_SEQ / $TIME_PAR }")
SPEEDUP_OMP_LOOP=$(awk "BEGIN { printf \"%.3f\", $TIME_SEQ / $TIME_OMP_LOOP }")
SPEEDUP_OMP_TASK=$(awk "BEGIN { printf \"%.3f\", $TIME_SEQ / $TIME_OMP_TASK }")

EFF_PAR=$(awk "BEGIN { printf \"%.1f\", ($TIME_SEQ / $TIME_PAR) / $T * 100 }")
EFF_OMP_LOOP=$(awk "BEGIN { printf \"%.1f\", ($TIME_SEQ / $TIME_OMP_LOOP) / $T * 100 }")
EFF_OMP_TASK=$(awk "BEGIN { printf \"%.1f\", ($TIME_SEQ / $TIME_OMP_TASK) / $T * 100 }")

echo -e "  Sequential : ${TIME_SEQ} s"
echo -e "  C++ Threads: ${TIME_PAR} s \t(Speedup: ${SPEEDUP_PAR}x, Eff: ${EFF_PAR}%)"
echo -e "  OMP Loop   : ${TIME_OMP_LOOP} s \t(Speedup: ${SPEEDUP_OMP_LOOP}x, Eff: ${EFF_OMP_LOOP}%)"
echo -e "  OMP Task   : ${TIME_OMP_TASK} s \t(Speedup: ${SPEEDUP_OMP_TASK}x, Eff: ${EFF_OMP_TASK}%)"

echo ""