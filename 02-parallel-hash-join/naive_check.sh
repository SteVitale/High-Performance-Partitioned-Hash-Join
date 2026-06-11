#!/bin/bash
# Small data in order to trigger the if (NR <= 500)
NR=500
NS=500
P=16        
T=4          
SEED=42
MAX_KEY=50 

make cleanall && make >/dev/null


echo "=================================================="
echo " ORIGINAL SEQUENTIAL VERSION CHECK"
echo "=================================================="
./seq_original -nr $NR -ns $NS -seed $SEED -max-key $MAX_KEY -p $P > seq_original_test.txt 2>/dev/null

grep -E "join_count|checksum1|checksum2" seq_original_test.txt

echo ""
echo "=================================================="
echo " SEQUENTIAL VERSION CHECK"
echo "=================================================="
./seq -nr $NR -ns $NS -seed $SEED -max-key $MAX_KEY -p $P > seq_test.txt 2>/dev/null

grep -E "join_count|checksum1|checksum2" seq_test.txt

echo ""
echo "=================================================="
echo " PARALLEL VERSION CHECK (T=$T)"
echo "=================================================="
./par -nr $NR -ns $NS -seed $SEED -max-key $MAX_KEY -p $P -t $T > par_test.txt 2>/dev/null

grep -E "join_count|checksum1|checksum2" par_test.txt

echo "=================================================="

grep "^naive_" seq_original_test.txt > seq_original_naive.txt
grep "^naive_" seq_test.txt > seq_naive.txt
grep "^naive_" par_test.txt > par_naive.txt

echo ""
echo "=================================================="
echo " NAIVE CORRECTNESS CHECK O(|R|*|S|)"
echo "=================================================="


# Orig == Seq && Seq == Par
if diff -q seq_original_naive.txt seq_naive.txt > /dev/null 2>&1 && \
   diff -q seq_naive.txt par_naive.txt > /dev/null 2>&1; then
    
    echo -e "SUCCESS Naive verification passed!"
    echo ""
    cat seq_naive.txt
else
    
    echo -e "FAIL Naive verification failed!"
    echo ""
    echo "Differences between Original and Seq:"
    diff seq_original_naive.txt seq_naive.txt
    echo "Differences between Seq and Par:"
    diff seq_naive.txt par_naive.txt
fi

rm -f seq_original_test.txt seq_test.txt par_test.txt \
      seq_original_naive.txt seq_naive.txt par_naive.txt