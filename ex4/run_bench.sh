#!/usr/bin/env bash
# Sweeps the priority-queue benchmark across thread counts and lock modes.
#
# Usage:   ./run_bench.sh [output.csv]
# Tunables (env vars):
#   THREADS   - space-separated list of thread counts (default: 1 2 4 8 16 32 48)
#   OPS       - operations per thread (default: 200000)
#   TRIALS    - repetitions per (mode,threads) cell (default: 2)
#   MODES     - lock modes to test (default: "mcs mutex")
#   BIN       - benchmark binary path (default: ./pq_bench)
set -euo pipefail

OUT="${1:-results.csv}"
BIN="${BIN:-./pq_bench}"
OPS="${OPS:-200000}"
TRIALS="${TRIALS:-2}"
MODES="${MODES:-mcs mutex}"
THREADS_DEFAULT="1 2 4 8 16 32 48"
THREADS="${THREADS:-$THREADS_DEFAULT}"

if [[ ! -x "$BIN" ]]; then
  echo "error: $BIN not found or not executable. Run 'make' first." >&2
  exit 1
fi

echo "mode,threads,ops_per_thread,total_ops,pops_empty,time_ms,ops_per_sec" > "$OUT"

for mode in $MODES; do
  for n in $THREADS; do
    for ((trial=1; trial<=TRIALS; ++trial)); do
      seed=$(( 1000 * n + trial ))
      echo "[run] mode=$mode threads=$n trial=$trial" >&2
      "$BIN" "$mode" "$n" "$OPS" "$seed" >> "$OUT"
    done
  done
done

echo "Wrote $OUT" >&2
