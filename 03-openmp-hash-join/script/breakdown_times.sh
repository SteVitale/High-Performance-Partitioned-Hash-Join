#!/bin/bash
DISTR="${1:-uniform}"
P=2048
T=16
SEED=42
MAX_KEY=500000
SIZE=50000000
RUNS=5

export OMP_NUM_THREADS=$T
export OMP_PLACES="${OMP_PLACES:-cores}"
export OMP_PROC_BIND="${OMP_PROC_BIND:-spread}"

make cleanall && make seq par omp_loop omp_task >/dev/null

echo "=================================================================================================="
echo " BREAKDOWN EXECUTION (AVERAGED OVER $RUNS RUNS | N=$SIZE, P=$P, T=$T, DISTR=$DISTR)"
echo "=================================================================================================="
printf "%-15s | %-15s | %-15s | %-15s | %-15s\n" "Phase" "Sequential (ms)" "Parallel (ms)" "OMP Loop (ms)" "OMP Task (ms)"
echo "--------------------------------------------------------------------------------------------------"

rm -f log_*.txt

# Run each binary RUNS times, appending output with >>
for (( i=1; i<=RUNS; i++ )); do
    ./seq -nr $SIZE -ns $SIZE -seed $SEED -max-key $MAX_KEY -p $P -distr $DISTR >> log_out_seq.txt 2>> log_err_seq.txt
    ./par -nr $SIZE -ns $SIZE -seed $SEED -max-key $MAX_KEY -p $P -t $T -distr $DISTR >> log_out_par.txt 2>> log_err_par.txt
    ./omp_loop -nr $SIZE -ns $SIZE -seed $SEED -max-key $MAX_KEY -p $P -t $T -distr $DISTR >> log_out_omp_loop.txt 2>> log_err_omp_loop.txt
    ./omp_task -nr $SIZE -ns $SIZE -seed $SEED -max-key $MAX_KEY -p $P -t $T -distr $DISTR >> log_out_omp_task.txt 2>> log_err_omp_task.txt
done

# Extract phase times and compute averages via awk
for phase in histogram prefix_sum scatter_part join_partition; do
    SEQ_TIME=$(grep "elapsed time ($phase)" log_err_seq.txt | awk -v runs=$RUNS '{sum+=$5} END {printf "%.4f", sum/runs}')
    PAR_TIME=$(grep "elapsed time ($phase)" log_err_par.txt | awk -v runs=$RUNS '{sum+=$5} END {printf "%.4f", sum/runs}')
    OMP_LOOP_TIME=$(grep "elapsed time ($phase)" log_err_omp_loop.txt | awk -v runs=$RUNS '{sum+=$5} END {printf "%.4f", sum/runs}')
    OMP_TASK_TIME=$(grep "elapsed time ($phase)" log_err_omp_task.txt | awk -v runs=$RUNS '{sum+=$5} END {printf "%.4f", sum/runs}')

    LABEL=${phase/time_/}
    printf "%-15s | %15s | %15s | %15s | %15s\n" "$LABEL" "${SEQ_TIME:-0}" "${PAR_TIME:-0}" "${OMP_LOOP_TIME:-0}" "${OMP_TASK_TIME:-0}"
done

# Average total execution times
SEQ_TOT=$(grep "time_sec=" log_out_seq.txt | cut -d'=' -f2 | awk -v runs=$RUNS '{sum+=$1} END {printf "%.6f", sum/runs}')
PAR_TOT=$(grep "time_sec=" log_out_par.txt | cut -d'=' -f2 | awk -v runs=$RUNS '{sum+=$1} END {printf "%.6f", sum/runs}')
OMP_LOOP_TOT=$(grep "time_sec=" log_out_omp_loop.txt | cut -d'=' -f2 | awk -v runs=$RUNS '{sum+=$1} END {printf "%.6f", sum/runs}')
OMP_TASK_TOT=$(grep "time_sec=" log_out_omp_task.txt | cut -d'=' -f2 | awk -v runs=$RUNS '{sum+=$1} END {printf "%.6f", sum/runs}')

echo "--------------------------------------------------------------------------------------------------"
printf "%-15s | %15s | %15s | %15s | %15s\n" "Total (sec)" "$SEQ_TOT" "$PAR_TOT" "$OMP_LOOP_TOT" "$OMP_TASK_TOT"
echo ""

rm -f log_*.txt