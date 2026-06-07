#!/usr/bin/env bash
#SBATCH --job-name=parallel_scan
#SBATCH --output=parallel_scan_%j.out
#SBATCH --error=parallel_scan_%j.err
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=48
#SBATCH --exclusive
#SBATCH --time=00:30:00
#SBATCH --partition=coco-exercise

set -euo pipefail
cd "${SLURM_SUBMIT_DIR:-.}"

# Bring TBB (and a recent g++) into the env — adjust spec names to whatever
# `spack find` shows on this cluster. Without TBB on LIBRARY_PATH the link
# step fails with "cannot find -ltbb", and at runtime it'd fail to load
# libtbb.so.
source "${SPACK_ROOT:-/opt/spack}/share/spack/setup-env.sh" 2>/dev/null || true
spack load intel-tbb 2>/dev/null || spack load tbb

# Build (no-op if up to date).
make parallel_scan

# Sweep parameters — override via `sbatch --export=ALL,SIZE=…,TRIALS=…`.
OUT="scan_results_${SLURM_JOB_ID:-local}.csv"
SEED="${SEED:-42}"
SIZE="${SIZE:-1048576}"          # must be a power of two (Blelloch scan)
TRIALS="${TRIALS:-3}"
THREADS=(${THREADS:-1 2 4 8 16 32 48})

echo "threads,size,stl_avg_ms,custom_avg_ms,match" > "$OUT"

for n in "${THREADS[@]}"; do
  for ((trial=1; trial<=TRIALS; ++trial)); do
    echo "[run] threads=$n trial=$trial size=$SIZE" >&2
    ./parallel_scan "$SEED" "$SIZE" "$n" >> "$OUT"
  done
done

echo "Wrote $OUT" >&2
