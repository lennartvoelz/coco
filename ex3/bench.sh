#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

OUT=results.csv
REPEATS=3

THREADS=(1 2 4 8 16)
WORK=(100000 1000000 10000000)

echo "impl,N,C,run,observed,ms" > "$OUT"

for C in "${WORK[@]}"; do
  for N in "${THREADS[@]}"; do
    for r in $(seq 1 "$REPEATS"); do
      # shared_counter_2: prints "N,C,atomic_obs,lock_obs,atomic_ms,lock_ms"
      out2=$(./counter "$N" "$C")
      IFS=',' read -r _ _ a_obs l_obs a_ms l_ms <<< "$out2"
      echo "atomic,$N,$C,$r,$a_obs,$a_ms" >> "$OUT"
      echo "rmw_lock,$N,$C,$r,$l_obs,$l_ms" >> "$OUT"

      # shared_counter_1 mutex: prints "N,C,observed,mutex_ms"
      out1=$(./counter_mutex "$N" "$C")
      IFS=',' read -r _ _ m_obs m_ms <<< "$out1"
      echo "mutex,$N,$C,$r,$m_obs,$m_ms" >> "$OUT"

      printf "  C=%d N=%d run=%d  atomic=%sms  rmw=%sms  mutex=%sms\n" \
        "$C" "$N" "$r" "$a_ms" "$l_ms" "$m_ms"
    done
  done
done

echo
echo "Wrote $OUT"
