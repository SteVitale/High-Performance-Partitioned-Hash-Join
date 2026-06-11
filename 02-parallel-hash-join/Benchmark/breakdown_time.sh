#!/bin/bash
P=2048
T=16
SEED=42
MAX_KEY=500000

make cleanall && make seq par >/dev/null

# Iteriamo su tre taglie: 500k (Small), 5M (Medium), 50M (Large)
for SIZE in 500000 5000000 50000000; do
    echo "======================================================="
    echo " BREAKDOWN EXECUTION (N=$SIZE, P=$P, T=$T)"
    echo "======================================================="
    printf "%-15s | %-15s | %-15s\n" "Phase" "Sequential (ms)" "Parallel (ms)"
    echo "-------------------------------------------------------"

    ./seq -nr $SIZE -ns $SIZE -seed $SEED -max-key $MAX_KEY -p $P > log_out_seq.txt 2> log_err_seq.txt
    ./par -nr $SIZE -ns $SIZE -seed $SEED -max-key $MAX_KEY -p $P -t $T > log_out_par.txt 2> log_err_par.txt

    for phase in histogram prefix_sum scatter_part join_partition; do
        SEQ_TIME=$(grep "elapsed time ($phase)" log_err_seq.txt | awk '{sum+=$5} END {print sum}')
        PAR_TIME=$(grep "elapsed time ($phase)" log_err_par.txt | awk '{sum+=$5} END {print sum}')
        
        LABEL=${phase/time_/} 
        # Uso un fallback a 0 nel caso il tempo sia così minuscolo da non essere stampato
        printf "%-15s | %15s | %15s\n" "$LABEL" "${SEQ_TIME:-0}" "${PAR_TIME:-0}"
    done

    SEQ_TOT=$(grep "time_sec=" log_out_seq.txt | cut -d'=' -f2)
    PAR_TOT=$(grep "time_sec=" log_out_par.txt | cut -d'=' -f2)
    echo "-------------------------------------------------------"
    printf "%-15s | %15s | %15s\n" "Total (sec)" "$SEQ_TOT" "$PAR_TOT"
    echo ""
done

rm -f log_*.txt