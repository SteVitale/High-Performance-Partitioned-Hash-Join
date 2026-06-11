#!/bin/bash
# =============================================================================
# run nested_analysis.sh
# Isolated benchmark to test Nested Tasks against Heavy Skew.
# =============================================================================

N=50000000
P=2048
T=16
SEED=42
MAX_KEY=500000
DISTR="heavy_skew"
RUNS=3

export OMP_NUM_THREADS=$T
export OMP_PLACES="cores"
export OMP_PROC_BIND="spread"

echo "=========================================================================="
echo " NESTED TASKS vs HEAVY SKEW"
make cleanall >/dev/null 2>&1
make seq omp_loop omp_task omp_nested >/dev/null 2>&1



echo "=========================================================================="
echo " Running Benchmarks (N=$N, T=$T, DISTR=$DISTR) - Averaging $RUNS runs"
echo "=========================================================================="

rm -f ext_log_*.txt

for (( i=1; i<=RUNS; i++ )); do
    echo -ne "\r Executing Run $i / $RUNS..."
    ./seq -nr $N -ns $N -seed $SEED -max-key $MAX_KEY -p $P -distr $DISTR >> ext_log_out_seq.txt 2>> ext_log_err_seq.txt
    ./omp_loop -nr $N -ns $N -seed $SEED -max-key $MAX_KEY -p $P -t $T -distr $DISTR >> ext_log_out_loop.txt 2>> ext_log_err_loop.txt
    ./omp_task -nr $N -ns $N -seed $SEED -max-key $MAX_KEY -p $P -t $T -distr $DISTR >> ext_log_out_task.txt 2>> ext_log_err_task.txt
    ./omp_nested -nr $N -ns $N -seed $SEED -max-key $MAX_KEY -p $P -t $T -distr $DISTR >> ext_log_out_nested.txt 2>> ext_log_err_nested.txt
done
echo ""

# ==========================================
# CORRECTNESS CHECK (From the last run)
# ==========================================
echo "--- CORRECTNESS CHECK ---"

# Extract key fields only (exclude time_sec which will differ)
grep -E "^(join_count|checksum1|checksum2)=" ext_log_out_seq.txt > /tmp/_seq_check_ext.txt
grep -E "^(join_count|checksum1|checksum2)=" ext_log_out_loop.txt > /tmp/_loop_check_ext.txt
grep -E "^(join_count|checksum1|checksum2)=" ext_log_out_task.txt > /tmp/_task_check_ext.txt
grep -E "^(join_count|checksum1|checksum2)=" ext_log_out_nested.txt > /tmp/_nested_check_ext.txt

# Compare SEQ vs OMP Loop
if diff -q /tmp/_seq_check_ext.txt /tmp/_loop_check_ext.txt > /dev/null 2>&1; then
    echo -e " OMP_LOOP matches SEQUENTIAL"
else
    echo -e " MISMATCH — OMP Loop outputs differ from Sequential"
fi

# Compare SEQ vs OMP Task
if diff -q /tmp/_seq_check_ext.txt /tmp/_task_check_ext.txt > /dev/null 2>&1; then
    echo -e " OMP_TASK matches SEQUENTIAL"
else
    echo -e " MISMATCH — OMP Task outputs differ from Sequential"
fi

# Compare SEQ vs OMP Nested
if diff -q /tmp/_seq_check_ext.txt /tmp/_nested_check_ext.txt > /dev/null 2>&1; then
    echo -e " OMP_NESTED matches SEQUENTIAL"
else
    echo -e " MISMATCH — OMP Nested outputs differ from Sequential"
fi

echo ""
echo "Values:"
cat /tmp/_seq_check_ext.txt
echo ""

# ==========================================
# PHASE BREAKDOWN & AVERAGES
# ==========================================
echo "--- PHASE BREAKDOWN (Milliseconds) ---"
printf "%-15s | %-15s | %-15s | %-15s\n" "Phase" "OMP Loop (ms)" "OMP Task (ms)" "OMP Nested (ms)"
echo "--------------------------------------------------------------------------"

for phase in histogram prefix_sum scatter_part join_partition; do
    LOOP_TIME=$(grep "elapsed time ($phase)" ext_log_err_loop.txt | awk -v runs=$RUNS '{sum+=$5} END {printf "%.2f", sum/runs}')
    TASK_TIME=$(grep "elapsed time ($phase)" ext_log_err_task.txt | awk -v runs=$RUNS '{sum+=$5} END {printf "%.2f", sum/runs}')
    NESTED_TIME=$(grep "elapsed time ($phase)" ext_log_err_nested.txt | awk -v runs=$RUNS '{sum+=$5} END {printf "%.2f", sum/runs}')

    LABEL=${phase/time_/}
    printf "%-15s | %15s | %15s | %15s\n" "$LABEL" "${LOOP_TIME}" "${TASK_TIME}" "${NESTED_TIME}"
done

# ==========================================
# TOTAL EXECUTION TIME & SPEEDUP
# ==========================================
SEQ_TOT=$(grep "time_sec=" ext_log_out_seq.txt | cut -d'=' -f2 | awk -v runs=$RUNS '{sum+=$1} END {printf "%.4f", sum/runs}')
LOOP_TOT=$(grep "time_sec=" ext_log_out_loop.txt | cut -d'=' -f2 | awk -v runs=$RUNS '{sum+=$1} END {printf "%.4f", sum/runs}')
TASK_TOT=$(grep "time_sec=" ext_log_out_task.txt | cut -d'=' -f2 | awk -v runs=$RUNS '{sum+=$1} END {printf "%.4f", sum/runs}')
NESTED_TOT=$(grep "time_sec=" ext_log_out_nested.txt | cut -d'=' -f2 | awk -v runs=$RUNS '{sum+=$1} END {printf "%.4f", sum/runs}')

SP_LOOP=$(awk "BEGIN {printf \"%.2fx\", $SEQ_TOT / $LOOP_TOT}")
SP_TASK=$(awk "BEGIN {printf \"%.2fx\", $SEQ_TOT / $TASK_TOT}")
SP_NESTED=$(awk "BEGIN {printf \"%.2fx\", $SEQ_TOT / $NESTED_TOT}")

echo "--------------------------------------------------------------------------"
printf "%-15s | %15s | %15s | %15s\n" "Total (sec)" "${LOOP_TOT} s" "${TASK_TOT} s" "${NESTED_TOT} s"
printf "%-15s | %15s | %15s | %15s\n" "Speedup" "${SP_LOOP}" "${SP_TASK}" "${SP_NESTED}"
echo "=========================================================================="

# Cleanup
rm -f ext_log_*.txt