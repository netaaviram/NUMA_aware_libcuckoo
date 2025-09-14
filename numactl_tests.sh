#!/usr/bin/env bash
set -euo pipefail

OPS=0
THREADS=20
CAPACITY=24
PREFILL=100

run_case () {
  local tag="$1"
  shift
  echo "=============================================="
  echo ">>> Running test: $tag"
  echo "=============================================="

  # Background run to capture NUMA maps
  "$@" ./universal_benchmark \
    --reads 100 --initial-capacity $CAPACITY --prefill $PREFILL \
    --total-ops $OPS --num-threads $THREADS &
  pid=$!
  sleep 0.2
  echo "$tag" > "numa_test_${tag}.txt"
  grep -E 'heap|anon' /proc/$pid/numa_maps >> "numa_test_${tag}.txt" || true
  wait $pid

  # Foreground run to capture thread→CPU mapping and JSON output
  "$@" ./universal_benchmark \
    --reads 100 --initial-capacity $CAPACITY --prefill $PREFILL \
    --total-ops $OPS --num-threads $THREADS \
    2>&1 | tee "threadmap_${tag}.txt"

  echo ">>> Finished test: $tag"
  echo
}

# === Test 1: default (no numactl) ===
run_case default

# === Test 2: bind0 (cpunodebind=0, membind=0) ===
run_case bind0 numactl --cpunodebind=0 --membind=0

# === Test 3: interleave all nodes ===
run_case interleave_all numactl --interleave=all

# === Test 4: membind=0, cpunodebind=3 ===
run_case mem0_cpu3 numactl --cpunodebind=3 --membind=0

# === Test 5: membind=3, cpunodebind=0 ===
run_case mem3_cpu0 numactl --cpunodebind=0 --membind=3

# === Test 6: interleave only nodes 0 and 3 ===
run_case interleave_0_3 numactl --interleave=0,3

# === Test 7: cpunodebind=1, interleave=0,1 ===
run_case cpu1_interleave_0_1 numactl --cpunodebind=1 --interleave=0,1
