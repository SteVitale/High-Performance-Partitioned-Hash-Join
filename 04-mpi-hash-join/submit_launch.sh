#!/bin/bash
#SBATCH --output=launch_results.txt 
#SBATCH --error=launch_errors.txt  
#SBATCH --nodes=4                  
#SBATCH --ntasks=32   
#SBATCH --ntasks-per-node=8              
#SBATCH --time=00:10:00             

chmod +x ./script/launch.sh

# CLI Parameters
NR="${1:-50000000}"
NS="${2:-50000000}"
SEED="${3:-42}"
MAX_KEY="${4:-500000}"
P_LOCAL="${5:-512}"    # Partitions within each process

#SKEW_FLAG="--skewed"
SKEW_FLAG=""

./script/launch.sh $NR $NS $SEED $MAX_KEY $P_LOCAL $SKEW_FLAG

