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

# Confirmed working via `which mpicxx` / `mpirun --version` in an interactive
# session on this cluster -- do not change without re-verifying with
# `module avail` first.
module load openmpi/4.1.5

cd "$(dirname "$0")"
bash run.sh
