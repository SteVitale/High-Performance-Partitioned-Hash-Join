#!/bin/bash
#SBATCH --output=naive_results.txt
#SBATCH --error=naive_errors.txt
#SBATCH --nodes=1                  
#SBATCH --ntasks=4                  
#SBATCH --time=00:00:15          


#SKEW_FLAG="--skewed"
SKEW_FLAG=""

chmod +x ./script/naive_check.sh
./script/naive_check.sh $SKEW_FLAG
