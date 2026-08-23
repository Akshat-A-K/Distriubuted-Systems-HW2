#!/usr/bin/env bash
#SBATCH --job-name=q4-triangle-benchmark
#SBATCH --nodes=4
#SBATCH --ntasks-per-node=2
#SBATCH --cpus-per-task=1
#SBATCH --mem-per-cpu=4G
#SBATCH --time=02:00:00
#SBATCH --output=q4_%j.log
#SBATCH --error=q4_%j.err
#SBATCH --partition=debug

set -euo pipefail

module load hpcx-2.7.0/hpcx-ompi

SUBMIT_DIR="${SLURM_SUBMIT_DIR:?}"
if [[ -f "$SUBMIT_DIR/HW2/Q4/run.sh" ]]; then
	cd "$SUBMIT_DIR/HW2/Q4"
else
	cd "$SUBMIT_DIR"
fi

echo "========================================="
echo "SLURM Job ID: $SLURM_JOB_ID"
echo "Allocated nodes: $SLURM_NNODES"
echo "Total tasks: $SLURM_NTASKS"
echo "Node list: $SLURM_NODELIST"
echo "========================================="
echo ""

bash run.sh

echo ""
echo "========================================="
echo "Benchmark completed!"
echo "Results saved in: $PWD/results"
echo "========================================="
