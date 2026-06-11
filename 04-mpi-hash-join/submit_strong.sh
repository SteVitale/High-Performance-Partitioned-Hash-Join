#!/bin/bash
#SBATCH --job-name=strong_scale
#SBATCH --output=strong_results.txt 
#SBATCH --error=strong_errors.txt  
#SBATCH --nodes=8                  
#SBATCH --ntasks=8
#SBATCH --ntasks-per-node=1
#SBATCH --time=00:15:00



make cleanall > /dev/null
make > /dev/null || { echo "Compilation Error!"; exit 1; }

chmod +x ./script/strong_scaling.sh

#SKEW_FLAG="--skewed"
SKEW_FLAG=""
GLOBAL_N=50000000


# Launch the strong scaling campaign on 1, 2, 4, and 8 nodes
./script/strong_scaling.sh $GLOBAL_N 1 $SKEW_FLAG
./script/strong_scaling.sh $GLOBAL_N 2 $SKEW_FLAG
./script/strong_scaling.sh $GLOBAL_N 4 $SKEW_FLAG
./script/strong_scaling.sh $GLOBAL_N 8 $SKEW_FLAG