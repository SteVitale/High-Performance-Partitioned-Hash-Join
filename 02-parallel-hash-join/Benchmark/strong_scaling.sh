#!/bin/bash

# Configuration for Strong Scaling (Fixed Problem Size)
NR=50000000
NS=50000000
P_OPTIMAL=2048
SEED=42
MAX_KEY=500000
RUNS=5

make cleanall && make par >/dev/null

echo "=================================================="
echo " STRONG SCALING BENCHMARK (Fixed Problem Size)"
echo " NR=$NR, NS=$NS, Partitions=$P_OPTIMAL"
echo "=================================================="

# Loop through powers of 2 for thread counts
for T in 1 2 4 8 16 32 48 64; do
    echo "--- Testing with Threads: $T ---"
    > tmp_times.txt

    # Run the benchmark multiple times to reduce OS jitter
    for (( i=1; i<=RUNS; i++ )); do
        # Extract only the final execution time in seconds
        TIME=$(./par -nr $NR -ns $NS -seed $SEED -max-key $MAX_KEY -p $P_OPTIMAL -t $T 2>/dev/null | grep "time_sec=" | cut -d'=' -f2)
        echo $TIME >> tmp_times.txt
    done

    # Calculate Mean using awk
    awk -v t="$T" -v runs="$RUNS" '{
        sum += $1; 
    } 
    END {
        mean = sum / runs;
        printf "Threads: %d | Mean Time: %.6f sec\n", t, mean;
    }' tmp_times.txt
    
    rm tmp_times.txt
done