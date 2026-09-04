#!/bin/bash
#SBATCH --job-name=q8-weather
#SBATCH --nodes=1
#SBATCH --ntasks-per-node=8
#SBATCH --cpus-per-task=1
#SBATCH --mem-per-cpu=4G
#SBATCH --time=00:45:00
#SBATCH --output=weather_%j.log
#SBATCH --error=weather_%j.err
#SBATCH --partition=debug

module load openmpi/4.1.5

cd "$(dirname "$0")"
bash run.sh
