#!/usr/bin/env bash
#SBATCH --job-name=barrier_bench
#SBATCH --output=barrier_bench_%j.out
#SBATCH --error=barrier_bench_%j.err
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=48
#SBATCH --exclusive
#SBATCH --time=00:30:00
#SBATCH --partition=coco-exercise

set -euo pipefail
cd "${SLURM_SUBMIT_DIR:-.}"

# Build (no-op if up to date)
make barrier_bench

# Run the sweep. Override ITERS / TRIALS / MODES / THREADS via env if desired:
#   sbatch --export=ALL,ITERS=500000 slurm_bench.sh
./run_bench.sh barrier_results.csv
