#!/usr/bin/env bash
set -euo pipefail

BENCH=./universal_benchmark
TOTAL_OPS=5000000   # adjust if needed
READS=100
CAP=24
SEED=42

THREADS_MAX=88      # 22 cores × 4 sockets
THREADS_STEP=2

# configs to test: name | numactl-prefix
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

for C in "${CONFIGS[@]}"; do
  NAME="${C%%|*}"
  PREFIX="${C##*|}"

  for T in $(seq 1 $THREADS_STEP $THREADS_MAX); do
    CMD="$PREFIX $BENCH \
      --reads $READS \
      --initial-capacity $CAP \
      --prefill 100 \
      --total-ops $TOTAL_OPS \
      --num-threads $T \
      --seed $SEED"

    echo "=== Running config=$NAME threads=$T ==="
    echo "CMD: $CMD"
    $CMD
    echo
  done
done
