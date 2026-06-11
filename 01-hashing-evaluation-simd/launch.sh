#!/bin/bash
ITERATIONS=7

make cleanall && make 2>/dev/null

N=${1:-50}
P=${2:-20}

SEP="============================================================"
THIN="------------------------------------------------------------"

echo ""
echo "$SEP"
echo "  Partition Mapping Benchmark"
echo "  N = ${N}M keys   |   P = 2^${P} = $((1 << P))"
echo "$SEP"

OUT_BASE=$(./baseline $N $P 2>/dev/null)
OUT_VEC=$(./autovec   $N $P 2>/dev/null)
OUT_AVX=$(./avx      $N $P 2>/dev/null)
OUT_AVX2=$(./avx2      $N $P 2>/dev/null)
OUT_CUDA=$(./cuda     $N $P 2>/dev/null)

field() { echo "$1" | grep "$2" | awk '{print $3}'; }

MED_BASE=$(field "$OUT_BASE" "Median")
MED_VEC=$(field  "$OUT_VEC"  "Median")
MED_AVX=$(field  "$OUT_AVX"  "Median")
MED_AVX2=$(field  "$OUT_AVX2"  "Median")
MED_CUDA=$(field "$OUT_CUDA" "Median")

STD_BASE=$(field "$OUT_BASE" "Stddev")
STD_VEC=$(field  "$OUT_VEC"  "Stddev")
STD_AVX=$(field  "$OUT_AVX"  "Stddev")
STD_AVX2=$(field  "$OUT_AVX2"  "Stddev")
STD_CUDA=$(field "$OUT_CUDA" "Stddev")

THR_BASE=$(field "$OUT_BASE" "Throughput")
THR_VEC=$(field  "$OUT_VEC"  "Throughput")
THR_AVX=$(field  "$OUT_AVX"  "Throughput")
THR_AVX2=$(field  "$OUT_AVX2"  "Throughput")
THR_CUDA=$(field "$OUT_CUDA" "Throughput")

CHK_BASE=$(echo "$OUT_BASE" | grep "Checksum" | awk '{print $NF}')
CHK_VEC=$(echo  "$OUT_VEC"  | grep "Checksum" | awk '{print $NF}')
CHK_AVX=$(echo  "$OUT_AVX"  | grep "Checksum" | awk '{print $NF}')
CHK_AVX2=$(echo  "$OUT_AVX2"  | grep "Checksum" | awk '{print $NF}')
CHK_CUDA=$(echo "$OUT_CUDA" | grep "Checksum" | awk '{print $NF}')

SP_VEC=$(echo  "scale=2; $MED_BASE / $MED_VEC"  | bc 2>/dev/null)
SP_AVX=$(echo  "scale=2; $MED_BASE / $MED_AVX"  | bc 2>/dev/null)
SP_AVX2=$(echo  "scale=2; $MED_BASE / $MED_AVX2"  | bc 2>/dev/null)
SP_CUDA=$(echo "scale=2; $MED_BASE / $MED_CUDA" | bc 2>/dev/null)

h2d=$(echo "$OUT_CUDA" | grep "Transfer_H2D" | awk '{printf "%.3f", $3 * 1000}')
d2h=$(echo "$OUT_CUDA" | grep "Transfer_D2H" | awk '{printf "%.3f", $3 * 1000}')
tot=$(echo "$OUT_CUDA" | grep "Transfer_TOT" | awk '{printf "%.3f", $3 * 1000}')

echo ""
echo "  PERFORMANCE  (median ± stddev, $ITERATIONS runs)"
echo "$THIN"
printf "  %-12s  median=%-14s  stddev=%-14s  throughput=%.2f MOps/s\n" \
    "baseline:" "$(printf '%.3f ms' $(echo "$MED_BASE * 1000" | bc))" \
    "$(printf '%.3f ms' $(echo "$STD_BASE * 1000" | bc))" "$THR_BASE"

printf "  %-12s  median=%-14s  stddev=%-14s  throughput=%.2f MOps/s\n" \
    "autovec:"  "$(printf '%.3f ms' $(echo "$MED_VEC * 1000" | bc))" \
    "$(printf '%.3f ms' $(echo "$STD_VEC * 1000" | bc))" "$THR_VEC"

printf "  %-12s  median=%-14s  stddev=%-14s  throughput=%.2f MOps/s\n" \
    "avx:"     "$(printf '%.3f ms' $(echo "$MED_AVX * 1000" | bc))" \
    "$(printf '%.3f ms' $(echo "$STD_AVX * 1000" | bc))" "$THR_AVX"

printf "  %-12s  median=%-14s  stddev=%-14s  throughput=%.2f MOps/s\n" \
    "avx2:"     "$(printf '%.3f ms' $(echo "$MED_AVX2 * 1000" | bc))" \
    "$(printf '%.3f ms' $(echo "$STD_AVX2 * 1000" | bc))" "$THR_AVX2"

echo "$THIN"

printf "  %-12s  median=%-14s  stddev=%-14s  throughput=%.2f MOps/s\n" \
    "cuda kernel:"     "$(printf '%.3f ms' $(echo "$MED_CUDA * 1000" | bc))" \
    "$(printf '%.3f ms' $(echo "$STD_CUDA * 1000" | bc))" "$THR_CUDA"

printf "  %-12s  H2D=%-17s D2H=%-17s Total=%s ms\n" \
    "cuda transf:" "${h2d} ms" "${d2h} ms" "${tot}"


echo "$THIN"
[ -n "$SP_VEC" ] && printf "  speedup autovec vs baseline: %.2fx\n" "$SP_VEC"
[ -n "$SP_AVX" ] && printf "  speedup avx     vs baseline: %.2fx\n" "$SP_AVX"
[ -n "$SP_AVX2" ] && printf "  speedup avx2    vs baseline: %.2fx\n" "$SP_AVX2"
[ -n "$SP_CUDA" ] && printf "  speedup cuda    vs baseline: %.2fx\n" "$SP_CUDA"

echo ""
echo "  CHECKSUM COMPARISON"
echo "$THIN"
printf "  %-12s  %s\n" "baseline:" "$CHK_BASE"
printf "  %-12s  %s\n" "autovec:"  "$CHK_VEC"
printf "  %-12s  %s\n" "avx:"     "$CHK_AVX"
printf "  %-12s  %s\n" "avx2:"     "$CHK_AVX2"
printf "  %-12s  %s\n" "cuda:"     "$CHK_CUDA"
echo "$THIN"
if [ "$CHK_BASE" == "$CHK_VEC" ] && [ "$CHK_BASE" == "$CHK_AVX2" ] && [ "$CHK_BASE" == "$CHK_AVX" ] && [ "$CHK_BASE" == "$CHK_CUDA" ]; then
    echo "  PASSED — all checksums match"
else
    echo "  FAILED — checksum mismatch detected"
fi

echo ""
echo "  CORRECTNESS  (first 10000 elements, element-by-element)"
echo "$THIN"
if diff -q baseline_output.txt autovec_output.txt > /dev/null 2>&1; then
    echo "  PASSED — baseline matches autovec"
else
    echo "  FAILED — baseline vs autovec mismatch"
fi

if diff -q baseline_output.txt avx_output.txt > /dev/null 2>&1; then
    echo "  PASSED — baseline matches avx"
else
    echo "  FAILED — baseline vs avx mismatch"
fi


if diff -q baseline_output.txt avx2_output.txt > /dev/null 2>&1; then
    echo "  PASSED — baseline matches avx2"
else
    echo "  FAILED — baseline vs avx2 mismatch"
fi

if diff -q baseline_output.txt cuda_output.txt > /dev/null 2>&1; then
    echo "  PASSED — baseline matches cuda"
else
    echo "  FAILED — baseline vs cuda mismatch"
fi


echo ""
echo "$SEP"
echo ""