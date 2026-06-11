#!/bin/bash
NR=50000000
NS=50000000
T=16
SEED=42
MAX_KEY=500000
RUNS=3 

make cleanall && make par >/dev/null

echo "================================================================"
echo " SWEEP OF P (Fixed Problem: $NR records, Threads: $T)"
echo "================================================================"
printf "%-15s | %-25s | %-15s\n" "Partitions (P)" "Avg Records/Partition" "Mean Time (s)"
echo "----------------------------------------------------------------"

for P in 512 1024 2048 4096 8192 16384 32768; do
    
    AVG_REC=$(( NR / P ))
    
    > tmp_times.txt
    for (( i=1; i<=RUNS; i++ )); do
       
        TIME=$(./par -nr $NR -ns $NS -seed $SEED -max-key $MAX_KEY -p $P -t $T 2>/dev/null | grep "time_sec=" | cut -d'=' -f2)
        echo $TIME >> tmp_times.txt
    done

    awk -v p_val="$P" -v rec="$AVG_REC" -v tot_runs="$RUNS" '{ sum += $1; } END { printf "%-15d | %-25d | %.6f\n", p_val, rec, sum/tot_runs }' tmp_times.txt
    
    rm tmp_times.txt
done
echo "================================================================"