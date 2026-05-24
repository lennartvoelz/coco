#!/usr/bin/env bash
# Sweeps the barrier benchmark across thread counts and barrier modes.
#
# Usage:   ./run_bench.sh [output.csv]
# Tunables (env vars):
#   THREADS   - space-separated list of thread counts
#               (default: 1 2 4 8 12 16 24 32 40 48)
#   ITERS     - barrier iterations per thread (default: 200000)
#   TRIALS    - repetitions per (mode,threads) cell (default: 3)
#   MODES     - barrier modes to test
#               (default: "central central_opt dissemination pthread")
#   BIN       - benchmark binary path (default: ./barrier_bench)
set -euo pipefail

OUT="${1:-results.csv}"
BIN="${BIN:-./barrier_bench}"
ITERS="${ITERS:-200000}"
TRIALS="${TRIALS:-3}"
MODES="${MODES:-central central_opt dissemination pthread}"
THREADS_DEFAULT="1 2 4 8 12 16 24 32 40 48"
THREADS="${THREADS:-$THREADS_DEFAULT}"

if [[ ! -x "$BIN" ]]; then
  echo "error: $BIN not found or not executable. Run 'make' first." >&2
  exit 1
fi

echo "mode,threads,iterations,total_ops,time_ms,avg_latency_ns,ok" > "$OUT"

for mode in $MODES; do
  for n in $THREADS; do
    for ((trial=1; trial<=TRIALS; ++trial)); do
      echo "[run] mode=$mode threads=$n trial=$trial" >&2
      "$BIN" "$mode" "$n" "$ITERS" >> "$OUT"
    done
  done
done

echo "Wrote $OUT" >&2
