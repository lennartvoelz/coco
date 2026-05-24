#!/bin/bash
#SBATCH --job-name=counter_bench
#SBATCH --output=counter_bench_%j.out
#SBATCH --error=counter_bench_%j.err
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=48
#SBATCH --time=00:30:00
#SBATCH --exclusive

set -euo pipefail
cd "${SLURM_SUBMIT_DIR:-$PWD}"

make clean
make
./bench_counter.sh "counter_results_${SLURM_JOB_ID:-local}.csv"
