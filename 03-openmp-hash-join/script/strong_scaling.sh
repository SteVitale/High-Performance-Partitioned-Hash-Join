#!/bin/bash

NR=50000000
NS=50000000
P_OPTIMAL=2048
SEED=42
MAX_KEY=500000
RUNS=5

export OMP_PLACES="cores"
export OMP_PROC_BIND="spread"

make cleanall && make par omp_loop omp_task >/dev/null

echo "====================================================================="
echo " STRONG SCALING BENCHMARK (Fixed Problem Size: N=$NR)"
echo "====================================================================="
printf "%-10s | %-15s | %-15s | %-15s\n" "Threads" "C++ Threads (s)" "OMP Loop (s)" "OMP Task (s)"
echo "---------------------------------------------------------------------"

for T in 1 2 4 8 16 24 32 40; do
    export OMP_NUM_THREADS=$T
    
    SUM_PAR=0; SUM_LOOP=0; SUM_TASK=0

    for (( i=1; i<=RUNS; i++ )); do
        T_PAR=$(./par -nr $NR -ns $NS -seed $SEED -max-key $MAX_KEY -p $P_OPTIMAL -t $T 2>/dev/null | grep "time_sec=" | cut -d'=' -f2)
        T_LOOP=$(./omp_loop -nr $NR -ns $NS -seed $SEED -max-key $MAX_KEY -p $P_OPTIMAL -t $T 2>/dev/null | grep "time_sec=" | cut -d'=' -f2)
        T_TASK=$(./omp_task -nr $NR -ns $NS -seed $SEED -max-key $MAX_KEY -p $P_OPTIMAL -t $T 2>/dev/null | grep "time_sec=" | cut -d'=' -f2)
        
        SUM_PAR=$(awk "BEGIN {print $SUM_PAR + $T_PAR}")
        SUM_LOOP=$(awk "BEGIN {print $SUM_LOOP + $T_LOOP}")
        SUM_TASK=$(awk "BEGIN {print $SUM_TASK + $T_TASK}")
    done

    MEAN_PAR=$(awk "BEGIN {printf \"%.6f\", $SUM_PAR / $RUNS}")
    MEAN_LOOP=$(awk "BEGIN {printf \"%.6f\", $SUM_LOOP / $RUNS}")
    MEAN_TASK=$(awk "BEGIN {printf \"%.6f\", $SUM_TASK / $RUNS}")

    printf "%-10s | %-15s | %-15s | %-15s\n" "$T" "$MEAN_PAR" "$MEAN_LOOP" "$MEAN_TASK"
done