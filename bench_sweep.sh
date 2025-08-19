#!/usr/bin/env bash
set -euo pipefail

BENCH=./universal_benchmark
TOTAL_OPS=5000000           # adjust if needed (bigger = smoother)
READS=100                   # read-only mix for stability
CAP=24
SEED=42

THREADS_MAX=88              # adjust to your machine; 22 per socket × 4 sockets
THREADS_STEP=2              # coarser step keeps runtime reasonable

# configs to test: name | numactl-prefix
cfg() { echo "$1|$2"; }
CONFIGS=(
  "$(cfg bind_n0           "numactl --cpunodebind=0 --membind=0")"
  "$(cfg bind_n1           "numactl --cpunodebind=1 --membind=1")"
  "$(cfg bind_n2           "numactl --cpunodebind=2 --membind=2")"
  "$(cfg bind_n3           "numactl --cpunodebind=3 --membind=3")"
  "$(cfg cpu0_mem1         "numactl --cpunodebind=0 --membind=1")"
  "$(cfg cpu0_mem2         "numactl --cpunodebind=0 --membind=2")"
  "$(cfg cpu1_mem0         "numactl --cpunodebind=1 --membind=0")"
  "$(cfg cpu2_mem0         "numactl --cpunodebind=2 --membind=0")"
  "$(cfg cpu3_mem2         "numactl --cpunodebind=3 --membind=2")"
  "$(cfg default           "")"
  "$(cfg interleave_all    "numactl --interleave=all")"
)

echo "config,threads,throughput,cycles,instructions,cache_references,cache_misses,branches,branch_misses" > sweep.csv

for C in "${CONFIGS[@]}"; do
  NAME="${C%%|*}"; PREFIX="${C##*|}"
  for T in $(seq 1 $THREADS_STEP $THREADS_MAX); do
    CMD="$PREFIX $BENCH --reads $READS --initial-capacity $CAP --prefill 100 --total-ops $TOTAL_OPS --num-threads $T --seed $SEED"
    # perf stat CSV output; program JSON to stdout
    PERF=$(perf stat -x, -e cycles,instructions,cache-references,cache-misses,branches,branch-misses \
           bash -lc "$CMD" 2>&1 >/tmp/bench_stdout.json)
    # parse benchmark throughput from its JSON
    THR=$(python3 - <<'PY'
import json,sys
with open("/tmp/bench_stdout.json") as f:
    j=json.load(f)
print(j["output"]["throughput"]["value"])
PY
)
    # pick perf values by name (last column has event name)
    get() { awk -F, -v k="$1" '$NF==k{gsub(/ /,"",$1); print $1}' <<< "$PERF"; }
    echo "$NAME,$T,$THR,$(get cycles),$(get instructions),$(get cache-references),$(get cache-misses),$(get branches),$(get branch-misses)" >> sweep.csv
    echo "$NAME T=$T  thr=$THR"
  done
done
