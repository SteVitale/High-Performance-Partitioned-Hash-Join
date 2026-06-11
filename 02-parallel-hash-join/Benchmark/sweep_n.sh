#!/bin/bash
T=16
P=2048
SEED=42
MAX_KEY=500000

make par >/dev/null

echo "=================================================="
echo " SWEEP OF N (Fixed T=$T, P=$P)"
echo "=================================================="
printf "%-12s | %-15s | %-15s\n" "N (Millions)" "Total Time (s)" "Throughput (M/s)"
echo "--------------------------------------------------"

# From 1 to 150 million
for N_MILLIONS in 1 10 25 50 75 100 125 150; do
    N=$(( N_MILLIONS * 1000000 ))
    
    # 3 different execution for the mean time
    SUM_TIME=0
    for i in {1..3}; do
        TIME=$(./par -nr $N -ns $N -seed $SEED -max-key $MAX_KEY -p $P -t $T 2>/dev/null | grep "time_sec=" | cut -d'=' -f2)
        SUM_TIME=$(awk "BEGIN {print $SUM_TIME + $TIME}")
    done
    
    AVG_TIME=$(awk "BEGIN {print $SUM_TIME / 3}")
    # Throughput = (R+S) / time
    THROUGHPUT=$(awk "BEGIN {print ($N * 2 / 1000000) / $AVG_TIME}")

    printf "%-12d | %-15.6f | %-15.2f\n" $N_MILLIONS $AVG_TIME $THROUGHPUT
done
echo "=================================================="