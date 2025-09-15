#!/usr/bin/env bash
# bench_sweep_perf_pow2.sh — run universal_benchmark with perf; threads = powers of two
set -euo pipefail

# ---- knobs (override via env) ----
BENCH=${BENCH:-./universal_benchmark}
TOTAL_OPS=${TOTAL_OPS:-75}
READS=${READS:-100}
CAP=${CAP:-24}
SEED=${SEED:-42}

THREADS_MAX=${THREADS_MAX:-88}    # upper bound; sweep will stop at the largest 2^k <= THREADS_MAX
EVENTS=${EVENTS:-cycles,instructions,cache-references,cache-misses,branches,branch-misses}
CONFIGS_FILTER=${CONFIGS_FILTER:-} # e.g., "bind_n0,default"
LOGDIR=${LOGDIR:-logs}
# ----------------------------------

cfg() { echo "$1|$2"; }
CONFIGS=(
  "$(cfg bind_n0        "numactl --cpunodebind=0 --membind=0")"
  "$(cfg bind_n1        "numactl --cpunodebind=1 --membind=1")"
  "$(cfg bind_n2        "numactl --cpunodebind=2 --membind=2")"
  "$(cfg bind_n3        "numactl --cpunodebind=3 --membind=3")"
  "$(cfg cpu0_mem1      "numactl --cpunodebind=0 --membind=1")"
  "$(cfg cpu0_mem2      "numactl --cpunodebind=0 --membind=2")"
  "$(cfg cpu1_mem0      "numactl --cpunodebind=1 --membind=0")"
  "$(cfg cpu2_mem0      "numactl --cpunodebind=2 --membind=0")"
  "$(cfg cpu3_mem2      "numactl --cpunodebind=3 --membind=2")"
  "$(cfg default        "")"
  "$(cfg interleave_all "numactl --interleave=all")"
)

filter_ok() {
  local name="$1"
  [[ -z "$CONFIGS_FILTER" ]] && return 0
  IFS=',' read -r -a arr <<<"$CONFIGS_FILTER"
  for it in "${arr[@]}"; do [[ "$name" == "$it" ]] && return 0; done
  return 1
}

mkdir -p "$LOGDIR"

for C in "${CONFIGS[@]}"; do
  NAME="${C%%|*}"
  PREFIX="${C##*|}"
  filter_ok "$NAME" || continue

  # sweep threads as powers of two: 1,2,4,8,16,...
  T=1
  while [[ $T -le $THREADS_MAX ]]; do
    CMD="$PREFIX $BENCH \
      --reads $READS \
      --initial-capacity $CAP \
      --prefill 100 \
      --total-ops $TOTAL_OPS \
      --num-threads $T \
      --seed $SEED"

    echo
    echo "=== Running config=$NAME threads=$T ==="
    echo "CMD: $CMD"

    JSON="$LOGDIR/${NAME}_T${T}.json"
    PERF="$LOGDIR/${NAME}_T${T}_perf.txt"

    # Print to terminal AND save to files:
    set +e
    perf stat -x, -e "$EVENTS" \
      bash -lc "$CMD" \
      1> >(tee "$JSON") \
      2> >(tee "$PERF" >&2)
    rc=$?
    set -e

    # If the benchmark aborts (e.g., someone changes flags), keep sweeping
    if [[ $rc -ne 0 ]]; then
      echo "WARN: run failed (rc=$rc) for config=$NAME T=$T — continuing..." >&2
    fi

    T=$(( T * 2 ))
  done
done

echo
echo "Done. Logs saved under: $LOGDIR/"
