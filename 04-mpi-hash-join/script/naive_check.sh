#!/bin/bash
# =============================================================================
# naive_check.sh — Simple correctness check
# =============================================================================
NR=500
NS=500
RANKS=4        
P_LOCAL=2          
SEED=42
MAX_KEY=50
SKEW_FLAG="${1:-}" 
P_GLOBAL=$((RANKS * P_LOCAL))



make cleanall > /dev/null && make > /dev/null

# Execution
srun -n 1 ./seq -nr $NR -ns $NS -seed $SEED -max-key $MAX_KEY -p $P_GLOBAL $SKEW_FLAG > seq_out.txt 2>/dev/null
srun --mpi=pmix -n $RANKS ./mpi -nr $NR -ns $NS -seed $SEED -max-key $MAX_KEY -p_per_rank $P_LOCAL $SKEW_FLAG > mpi_out.txt 2>/dev/null
srun --mpi=pmix --cpu-bind=none -n $RANKS ./hybrid -nr $NR -ns $NS -seed $SEED -max-key $MAX_KEY -p_per_rank $P_LOCAL $SKEW_FLAG > hybrid_out.txt 2>/dev/null

# Printing all results (standard and naive)
echo "=== SEQUENTIAL DETECTED METRICS ==="
grep -E "join|checksum" seq_out.txt
echo ""
echo "=== MPI DETECTED METRICS ==="
grep -E "join|checksum" mpi_out.txt
echo ""
echo "=== HYBRID DETECTED METRICS ==="
grep -E "join|checksum" hybrid_out.txt
echo "==================================="


grep -E "^(join_count|checksum1|checksum2)=" seq_out.txt > seq_metrics.txt
grep -E "^(join_count|checksum1|checksum2)=" mpi_out.txt > mpi_metrics.txt
grep -E "^(join_count|checksum1|checksum2)=" hybrid_out.txt > hybrid_metrics.txt


if diff -q seq_metrics.txt mpi_metrics.txt > /dev/null 2>&1 \
    && diff -q seq_metrics.txt hybrid_metrics.txt > /dev/null 2>&1; then
    echo " SUCCESS: Everything matches perfectly!"
else
    echo " FAIL: Mismatch found between sequential and MPI!"
fi

rm -f seq_out.txt mpi_out.txt hybrid_out.txt seq_metrics.txt mpi_metrics.txt hybrid_metrics.txt