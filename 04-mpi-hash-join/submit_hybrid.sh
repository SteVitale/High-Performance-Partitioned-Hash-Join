#!/bin/bash
#SBATCH --job-name=hybrid
#SBATCH --nodes=8                  
#SBATCH --ntasks-per-node=1        
#SBATCH --cpus-per-task=16                    
#SBATCH --time=00:05:00            
#SBATCH --output=hybrid_results.txt 
#SBATCH --error=hybrid_errors.txt  

export OMP_NUM_THREADS=16
export OMP_PROC_BIND=spread
export OMP_PLACES=cores

# CLI Parameters
NR="${1:-50000000}"
NS="${2:-50000000}"
SEED="${3:-42}"
MAX_KEY="${4:-500000}"
P_LOCAL="${5:-2048}"


#SKEW_FLAG="--skewed"
SKEW_FLAG=""


make cleanall >/dev/null && make hybrid >/dev/null

srun --mpi=pmix --cpu-bind=none -N 8 -n 8 --ntasks-per-node 1 \
        ./hybrid -nr $NR -ns $NS -seed $SEED -max-key $MAX_KEY -p_per_rank $P_LOCAL $SKEW_FLAG> hybrid_raw.txt 2>hybrid_raw_err.txt


echo "========================================================================"
echo "                 HYBRID MPI+OMP EXECUTION SUMMARY                       "
echo "========================================================================"
echo "   SETUP:"
echo "    Nodes       : $SLURM_JOB_NUM_NODES"
echo "    MPI Ranks   : 8 (1 per Node)"
echo "    OMP Threads : $OMP_NUM_THREADS per Rank"
echo ""
echo "   PROBLEM:"
echo "    NR=$NR | NS=$NS | Max-Key=$MAX_KEY | P_Local=$P_LOCAL"
echo "------------------------------------------------------------------------"
echo "   RESULTS:"
grep -E "^(join_count|checksum1|checksum2)=" hybrid_raw.txt | sed 's/^/    /'
echo "------------------------------------------------------------------------"
echo "   PERFORMANCE:"
grep -E "^time_sec=" hybrid_raw.txt | sed 's/^/    /'
echo ""
grep -A 4 "Bottlenecks" hybrid_raw.txt | sed 's/^/    /'
echo ""
grep -A 2 "Imbalance" hybrid_raw.txt | sed 's/^/    /'
echo "========================================================================"


rm -f hybrid_raw.txt hybrid_raw_err.txt
