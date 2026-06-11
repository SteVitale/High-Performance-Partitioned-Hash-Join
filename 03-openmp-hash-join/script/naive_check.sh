#!/bin/bash
# Small data in order to trigger the if (NR <= 500)
DISTR="${1:-uniform}"
NR=500
NS=500
P=16
T=4
SEED=42
MAX_KEY=50

# --- OpenMP specific config ---
export OMP_NUM_THREADS=$T
export OMP_PLACES="${OMP_PLACES:-cores}"
export OMP_PROC_BIND="${OMP_PROC_BIND:-spread}"


make cleanall && make seq par omp_loop omp_task >/dev/null

echo ""
echo "=================================================="
echo " SEQUENTIAL VERSION CHECK"
echo "=================================================="
./seq -nr $NR -ns $NS -seed $SEED -max-key $MAX_KEY -p $P -distr $DISTR> seq_test.txt 2>/dev/null

grep -E "join_count|checksum1|checksum2" seq_test.txt

echo ""
echo "=================================================="
echo " PARALLEL VERSION CHECK (T=$T)"
echo "=================================================="
./par -nr $NR -ns $NS -seed $SEED -max-key $MAX_KEY -p $P -t $T -distr $DISTR> par_test.txt 2>/dev/null

grep -E "join_count|checksum1|checksum2" par_test.txt

echo ""
echo "=================================================="
echo " OMP LOOP VERSION CHECK (T=$T)"
echo "=================================================="
./omp_loop -nr $NR -ns $NS -seed $SEED -max-key $MAX_KEY -p $P -t $T -distr $DISTR> omp_loop_test.txt 2>/dev/null

grep -E "join_count|checksum1|checksum2" omp_loop_test.txt

echo ""
echo "=================================================="
echo " OMP TASK VERSION CHECK (T=$T)"
echo "=================================================="
./omp_task -nr $NR -ns $NS -seed $SEED -max-key $MAX_KEY -p $P -t $T -distr $DISTR> omp_task_test.txt 2>/dev/null

grep -E "join_count|checksum1|checksum2" omp_task_test.txt


echo "=================================================="

grep "^naive_" seq_test.txt > seq_naive.txt
grep "^naive_" par_test.txt > par_naive.txt
grep "^naive_" omp_loop_test.txt > omp_loop_naive.txt
grep "^naive_" omp_task_test.txt > omp_task_naive.txt

echo ""
echo "=================================================="
echo " NAIVE CORRECTNESS CHECK O(|R|*|S|)"
echo "=================================================="


# Orig == Seq && Seq == Par
if diff -q seq_naive.txt par_naive.txt > /dev/null 2>&1 && \
   diff -q par_naive.txt omp_loop_naive.txt > /dev/null 2>&1 && \
   diff -q omp_loop_naive.txt omp_task_naive.txt > /dev/null 2>&1 ; then

    echo -e "SUCCESS Naive verification passed!"
    echo ""
    cat seq_naive.txt
else

    echo -e "FAIL Naive verification failed!"
    echo ""
    echo "Differences between Seq and Par:"
    diff seq_naive.txt par_naive.txt

    echo "Differences between Seq and Omp Loop:"
    diff seq_naive.txt omp_loop_naive.txt

    echo "Differences between Seq and Omp Task:"
    diff seq_naive.txt omp_task_naive.txt
fi

rm -f seq_test.txt par_test.txt omp_loop_test.txt omp_task_test.txt\
      seq_naive.txt par_naive.txt omp_loop_naive.txt omp_task_naive.txt