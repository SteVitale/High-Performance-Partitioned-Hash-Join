#!/bin/bash
BASE_RECORDS=5000000 
P_OPTIMAL=2048
SEED=42
MAX_KEY=500000
RUNS=5

make cleanall && make par >/dev/null

echo "=================================================="
echo " WEAK SCALING BENCHMARK (Fixed Work per Thread)"
echo " Base records per thread: $BASE_RECORDS"
echo "=================================================="

for T in 1 2 4 8 16 32 48 64; do
    echo "--- Testing with Threads: $T ---"
    
    # Calculate problem size for current thread count: PS(p) = p * PS(1)
    CURRENT_NR=$(( T * BASE_RECORDS ))
    CURRENT_NS=$(( T * BASE_RECORDS ))
    
    > tmp_times.txt

    for (( i=1; i<=RUNS; i++ )); do
        TIME=$(./par -nr $CURRENT_NR -ns $CURRENT_NS -seed $SEED -max-key $MAX_KEY -p $P_OPTIMAL -t $T 2>/dev/null | grep "time_sec=" | cut -d'=' -f2)
        echo $TIME >> tmp_times.txt
    done

    # Calculate Mean Time 
    awk -v t="$T" -v nr="$CURRENT_NR" -v runs="$RUNS" '{
        sum += $1; 
    } 
    END {
        mean = sum / runs;
        printf "Threads: %2d | Input Size: %8d | Mean Time: %.6f sec\n", t, nr, mean;
    }' tmp_times.txt
    
    rm tmp_times.txt
done