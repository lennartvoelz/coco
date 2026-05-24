#!/usr/bin/env bash
# Sweeps the shared-counter benchmark across thread counts and work sizes.
#
# Usage:   ./bench_counter.sh [output.csv]
# Tunables (env vars):
#   THREADS   - space-separated list of thread counts (default: 1 2 4 8 16 32 48)
#   WORK      - space-separated list of total-increment counts (default: 1000000 10000000)
#   REPEATS   - repetitions per (N,C) cell (default: 2)
#   BIN       - benchmark binary path (default: ./mcs)
set -euo pipefail

cd "$(dirname "$0")"

OUT="${1:-counter_results.csv}"
BIN="${BIN:-./mcs}"
REPEATS="${REPEATS:-2}"
THREADS_DEFAULT="1 2 4 8 16 32 48"
WORK_DEFAULT="1000000 10000000"
THREADS="${THREADS:-$THREADS_DEFAULT}"
WORK="${WORK:-$WORK_DEFAULT}"

if [[ ! -x "$BIN" ]]; then
  echo "error: $BIN not found or not executable. Run 'make' first." >&2
  exit 1
fi

echo "impl,N,C,run,observed,ms" > "$OUT"

for C in $WORK; do
  for N in $THREADS; do
    for r in $(seq 1 "$REPEATS"); do
      # mcs binary prints: N,C,a_obs,l_obs,m_obs,p_obs,a_ms,l_ms,m_ms,p_ms
      out=$("$BIN" "$N" "$C")
      IFS=',' read -r _ _ a_obs l_obs m_obs p_obs a_ms l_ms m_ms p_ms <<< "$out"
      echo "atomic,$N,$C,$r,$a_obs,$a_ms"     >> "$OUT"
      echo "rmw_lock,$N,$C,$r,$l_obs,$l_ms"   >> "$OUT"
      echo "mcs_lock,$N,$C,$r,$m_obs,$m_ms"   >> "$OUT"
      echo "pthread,$N,$C,$r,$p_obs,$p_ms"    >> "$OUT"

      printf "  C=%d N=%d run=%d  atomic=%sms  rmw=%sms  mcs=%sms  pthread=%sms\n" \
        "$C" "$N" "$r" "$a_ms" "$l_ms" "$m_ms" "$p_ms"
    done
  done
done

echo
echo "Wrote $OUT"
