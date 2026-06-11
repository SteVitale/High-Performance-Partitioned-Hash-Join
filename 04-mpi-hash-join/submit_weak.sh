#!/bin/bash
#SBATCH --job-name=weak_scale
#SBATCH --output=weak_results.txt 
#SBATCH --error=weak_errors.txt  
#SBATCH --nodes=8                  
#SBATCH --ntasks=128
#SBATCH --ntasks-per-node=16              
#SBATCH --time=00:15:00


make cleanall > /dev/null
make > /dev/null || { echo "Compilation Error!"; exit 1; }

chmod +x ./script/weak_scaling.sh

N_PER_RANK=3000000 # 3 milion

#SKEW_FLAG="--skewed"
SKEW_FLAG=""


# Launch the weak scaling campaign on 1, 2, 4, and 8 nodes
./script/weak_scaling.sh $N_PER_RANK 1 $SKEW_FLAG
./script/weak_scaling.sh $N_PER_RANK 2 $SKEW_FLAG
./script/weak_scaling.sh $N_PER_RANK 4 $SKEW_FLAG
./script/weak_scaling.sh $N_PER_RANK 8 $SKEW_FLAG