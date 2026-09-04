#!/bin/bash
#SBATCH --job-name=q3-bitonic
#SBATCH --nodes=1
#SBATCH --ntasks-per-node=8
#SBATCH --cpus-per-task=1
#SBATCH --mem-per-cpu=4G
#SBATCH --time=00:45:00
#SBATCH --output=bitonic_%j.log
#SBATCH --error=bitonic_%j.err
#SBATCH --partition=debug

module load hpcx-2.7.0/hpcx-ompi    # confirm the exact module name with `module avail` on your cluster

cd "$(dirname "$0")"
bash run.sh
