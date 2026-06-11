#!/bin/bash
# =============================================================================
# launch.sh — Run sequential and parallel hashjoin and compare results
#
# Usage:
#   ./launch.sh [NR] [NS] [SEED] [MAX_KEY] [P] [T] 
#
# Defaults:
#   NR      = 50000000   (50M records in R)
#   NS      = 50000000   (50M records in S)
#   SEED    = 42
#   MAX_KEY = 500000
#   P       = 2048       (number of partitions, must be power of 2)
#   T       = 16         (number of threads) [only for parallel]
# =============================================================================
 

# --- Helpers ---
print_header() {
    echo ""
    echo -e "════════════════════════════════════════════════════════"
    echo -e "$1"
    echo -e "════════════════════════════════════════════════════════"
}
 
make cleanall && make 

# --- Parameters (override via CLI or use defaults) ---
NR="${1:-50000000}"
NS="${2:-50000000}"
SEED="${3:-42}"
MAX_KEY="${4:-500000}"
P="${5:-2048}"
T="${6:-16}" 

# --- Paths ---
SEQ="./seq"
PAR="./par"
OUT_SEQ="out_seq.txt"
OUT_PAR="out_par.txt"
PROF_SEQ="profiling_seq.txt"
PROF_PAR="profiling_par.txt"


print_header "BENCHMARK [NR=$NR, NS=$NS, P=$P, T=$T]"

print_header "SEQUENTIAL BASELINE"
$SEQ -nr $NR -ns $NS -seed $SEED -max-key $MAX_KEY -p $P \
    > "$OUT_SEQ" \
    2> "$PROF_SEQ"

cat $OUT_SEQ

print_header "PARALLEL VERSION (T = "$T")"
$PAR -nr $NR -ns $NS -seed $SEED -max-key $MAX_KEY -p $P -t $T \
    > "$OUT_PAR" \
    2> "$PROF_PAR"
cat $OUT_PAR

# =============================================================================
# CORRECTNESS CHECK
# =============================================================================
print_header "CORRECTNESS CHECK"
 
# Extract key fields only (exclude time_sec which will differ)
grep -E "^(join_count|checksum1|checksum2)=" "$OUT_SEQ" > /tmp/_seq_check.txt
grep -E "^(join_count|checksum1|checksum2)=" "$OUT_PAR" > /tmp/_par_check.txt
 
if diff -q /tmp/_seq_check.txt /tmp/_par_check.txt > /dev/null 2>&1; then
    echo -e "CORRECT — join_count and checksums match"

else
    echo -e "MISMATCH — outputs differ!"
fi

echo ""
echo "Sequential:"
cat /tmp/_seq_check.txt
echo ""
echo "Parallel:"
cat /tmp/_par_check.txt


# =============================================================================
# SPEEDUP
# =============================================================================
print_header "SPEEDUP"
 
TIME_SEQ=$(grep "^time_sec=" "$OUT_SEQ" | cut -d= -f2)
TIME_PAR=$(grep "^time_sec=" "$OUT_PAR" | cut -d= -f2)
SPEEDUP=$(awk "BEGIN { printf \"%.3f\", $TIME_SEQ / $TIME_PAR }")
EFFICIENCY=$(awk "BEGIN { printf \"%.1f\", ($TIME_SEQ / $TIME_PAR) / $T * 100 }")

echo -e "  seq time : ${TIME_SEQ} s"
echo -e "  par time : ${TIME_PAR} s"
echo -e "  speedup  : ${SPEEDUP}x"
echo -e "  efficiency: ${EFFICIENCY}%"

echo ""