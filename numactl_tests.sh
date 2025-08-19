# === Test 1: default (no numactl) ===
./universal_benchmark --reads 100 --initial-capacity 24 --prefill 100 --total-ops 0 --num-threads 20 &
pid=$!; sleep 0.2
echo "default" > numa_test_default.txt
grep -E 'heap|anon' /proc/$pid/numa_maps >> numa_test_default.txt
wait $pid
./universal_benchmark --reads 100 --initial-capacity 24 --prefill 100 --total-ops 0 --num-threads 20 \
2>&1 | tee threadmap_default.txt

# === Test 2: bind0 (cpunodebind=0, membind=0) ===
numactl --cpunodebind=0 --membind=0 ./universal_benchmark --reads 100 --initial-capacity 24 --prefill 100 --total-ops 0 --num-threads 20 &
pid=$!; sleep 0.2
echo "bind0" > numa_test_bind0.txt
grep -E 'heap|anon' /proc/$pid/numa_maps >> numa_test_bind0.txt
wait $pid
numactl --cpunodebind=0 --membind=0 ./universal_benchmark --reads 100 --initial-capacity 24 --prefill 100 --total-ops 0 --num-threads 20 \
2>&1 | tee threadmap_bind0.txt

# === Test 3: interleave all nodes ===
numactl --interleave=all ./universal_benchmark --reads 100 --initial-capacity 24 --prefill 100 --total-ops 0 --num-threads 20 &
pid=$!; sleep 0.2
echo "interleave_all" > numa_test_interleave_all.txt
grep -E 'heap|anon' /proc/$pid/numa_maps >> numa_test_interleave_all.txt
wait $pid
numactl --interleave=all ./universal_benchmark --reads 100 --initial-capacity 24 --prefill 100 --total-ops 0 --num-threads 20 \
2>&1 | tee threadmap_interleave_all.txt

# === Test 4: membind=0, cpunodebind=3 ===
numactl --cpunodebind=3 --membind=0 ./universal_benchmark --reads 100 --initial-capacity 24 --prefill 100 --total-ops 0 --num-threads 20 &
pid=$!; sleep 0.2
echo "mem0_cpu3" > numa_test_mem0_cpu3.txt
grep -E 'heap|anon' /proc/$pid/numa_maps >> numa_test_mem0_cpu3.txt
wait $pid
numactl --cpunodebind=3 --membind=0 ./universal_benchmark --reads 100 --initial-capacity 24 --prefill 100 --total-ops 0 --num-threads 20 \
2>&1 | tee threadmap_mem0_cpu3.txt

# === Test 5: membind=3, cpunodebind=0 ===
numactl --cpunodebind=0 --membind=3 ./universal_benchmark --reads 100 --initial-capacity 24 --prefill 100 --total-ops 0 --num-threads 20 &
pid=$!; sleep 0.2
echo "mem3_cpu0" > numa_test_mem3_cpu0.txt
grep -E 'heap|anon' /proc/$pid/numa_maps >> numa_test_mem3_cpu0.txt
wait $pid
numactl --cpunodebind=0 --membind=3 ./universal_benchmark --reads 100 --initial-capacity 24 --prefill 100 --total-ops 0 --num-threads 20 \
2>&1 | tee threadmap_mem3_cpu0.txt

# === Test 6: interleave only nodes 0 and 3 ===
numactl --interleave=0,3 ./universal_benchmark --reads 100 --initial-capacity 24 --prefill 100 --total-ops 0 --num-threads 20 &
pid=$!; sleep 0.2
echo "interleave_0_3" > numa_test_interleave_0_3.txt
grep -E 'heap|anon' /proc/$pid/numa_maps >> numa_test_interleave_0_3.txt
wait $pid
numactl --interleave=0,3 ./universal_benchmark --reads 100 --initial-capacity 24 --prefill 100 --total-ops 0 --num-threads 20 \
2>&1 | tee threadmap_interleave_0_3.txt

# === Test 7: cpunodebind=1, interleave=0,1 ===
numactl --cpunodebind=1 --interleave=0,1 ./universal_benchmark --reads 100 --initial-capacity 24 --prefill 100 --total-ops 0 --num-threads 20 &
pid=$!; sleep 0.2
echo "cpu1_interleave_0_1" > numa_test_cpu1_interleave_0_1.txt
grep -E 'heap|anon' /proc/$pid/numa_maps >> numa_test_cpu1_interleave_0_1.txt
wait $pid
numactl --cpunodebind=1 --interleave=0,1 ./universal_benchmark --reads 100 --initial-capacity 24 --prefill 100 --total-ops 0 --num-threads 20 \
2>&1 | tee threadmap_cpu1_interleave_0_1.txt
