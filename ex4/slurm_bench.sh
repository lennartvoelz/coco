#!/bin/bash
#SBATCH --job-name=pq_bench
#SBATCH --output=pq_bench_%j.out
#SBATCH --error=pq_bench_%j.err
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=48
#SBATCH --time=00:30:00
#SBATCH --exclusive

set -euo pipefail
cd "${SLURM_SUBMIT_DIR:-$PWD}"

make clean
make
./run_bench.sh "results_${SLURM_JOB_ID:-local}.csv"
