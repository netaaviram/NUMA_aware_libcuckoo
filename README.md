# MUMA-aware optimization of libcuckoo: analyzing scalability bottlenecks for improved performance through thread and memory placement

This repo reproduces my experiments showing that the libcuckoo benchmark’s scalability is bottlenecked by NUMA‑unaware placement. I:

* Build libcuckoo’s universal_benchmark in Release mode.

* Run 7 configurations using numactl to control CPU & memory placement.

* Capture memory page placement (/proc/<pid>/numa_maps) and thread→CPU mapping (benchmark stdout).

* Convert the logs to CSVs and plot a normalized “memory vs threads” stacked‑bar figure.

* Run a perf sweep (threads 1…N) across policies and plot throughput + perf counters.

All tests were made on Ubuntu‑like systems. The example machine is a 4‑socket Intel Xeon E5‑4669 v4 (Broadwell‑EX), 22 cores per socket, 4 NUMA nodes (0–3), SMT disabled.

# 0) Prerequisites
If you are running on an other system but the rack-mad-04 of TAU, you need to install prequisits: 
# Build tools & perf

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake numactl linux-tools-common linux-tools-generic
```

# Python plotting stack
```bash
python3 -m pip install --user pandas matplotlib
```

# 1) Clone libcuckoo
```bash
wget https://github.com/efficient/libcuckoo/archive/refs/heads/master.zip
unzip master.zip
mv libcuckoo-master libcuckoo
cd libcuckoo/tests/universal-benchmark
```

# 2) Build libcuckoo
```bash
g++ -std=c++17 -O3 -pthread \
  -I.. \
  -I../.. \
  -I"$HOME/numa_local/install/include" \
  -DLIBCUCKOO \
  -DTABLE=LIBCUCKOO \
  -DTABLE_TYPE=libcuckoo::cuckoohash_map \
  -DKEY=uint64_t \
  -DVALUE=uint64_t \
  universal_benchmark.cc \
  -o universal_benchmark \
  -L"$HOME/numa_local/install/lib" \
  -Wl,-rpath,"$HOME/numa_local/install/lib" \
  -lnuma
```
We are using -lnuma flag to make sure linking against the NUMA library, which is required for memory placements.

# 3) Run a sweep over thread count to measure and obtain performance different memory placement configurations: 
This step is the scalability profiling I ran to understand and detect bottlenecks in libcuckoo. We run a real workload (--total-ops != 0) while sweeping the thread count across sockets and record:

* the benchmark’s throughput (from its JSON)

* Linux perf stat hardware counters (cycles, instructions, cache/branch stats)

* This produces the CSV and plot for Figure 1 in the final report (throughput vs. threads, with counters).

Run the sweep:
```bash
mv ./NUMA_aware_libcuckoo/bench_sweep.sh ./
chmod +x bench_sweep.sh
./bench_sweep.sh
```
This creates a sweep.csv with the following columns:
config,threads,throughput,cycles,instructions,cache_references,cache_misses,branches,branch_misses

Then plot using:
```bash
python plot_sweep.py
```
What the script does (briefly):

A. Defines a set of NUMA policies (CONFIGS) such as full bind per node, cross-CPU/memory binds, interleave, and default.

B. Sweeps threads = 1, 1+STEP, …, THREADS_MAX for each policy.

C. For each run it executes:
```bash
[numactl …] ./universal_benchmark \
    --reads 100 --initial-capacity 24 --prefill 100 \
    --total-ops $TOTAL_OPS --num-threads $T --seed $SEED
```
perf stat wraps the run and outputs counters in CSV; the script extracts:

* throughput from the benchmark JSON

* counters from perf lines

* writes a single line to sweep.csv

# 4) Clone NUMA_aware_libcuckoo project (this repo):
```bash
wget https://github.com/netaaviram/NUMA_aware_libcuckoo/archive/refs/heads/main.zip -O numa_libcuckoo.zip
unzip numa_libcuckoo.zip
mv NUMA_aware_libcuckoo-main NUMA_aware_libcuckoo
```

# 5) Replace the original benchmark file with the NUMA optimized benchmark file from NUMA_aware_libcuckoo
```bash
rm universal_benchmark.cc
mv ./NUMA_aware_libcuckoo/universal_benchmark.cc ./
```
* Now, the new optimized version of universal benchmark is saved as universal_benchmark.cc under libcuckoo/build/tests/universal_benchmark.cc

# 6) Build and Compile the optimized NUMA-aware libcuckoo implementation
```bash
g++ -std=c++17 -O3 -pthread \
  -I.. \
  -I../.. \
  -I"$HOME/numa_local/install/include" \
  -DLIBCUCKOO \
  -DTABLE=LIBCUCKOO \
  -DTABLE_TYPE=libcuckoo::cuckoohash_map \
  -DKEY=uint64_t \
  -DVALUE=uint64_t \
  universal_benchmark.cc \
  -o universal_benchmark \
  -L"$HOME/numa_local/install/lib" \
  -Wl,-rpath,"$HOME/numa_local/install/lib" \
  -lnuma
```

# 7) Capture hardware & NUMA topology (one time)

We record the CPU→NUMA mapping for later parsing:
```bash
lscpu > lscpu.txt
numactl --hardware > numactl_hardware.txt
```
On the reference system:
• 4 sockets × 22 cores = 88 cores (no SMT)
• NUMA nodes 0–3
• Node 0 CPUs: 0–10,44–54; Node 1: 11–21,55–65; Node 2: 22–32,66–76; Node 3: 33–43,77–87

# 8) Run the 7 NUMA placement tests

This step measures where memory pages are allocated and which NUMA node each thread runs on under different placement policies.
We are not interested in throughput here, only in placement behavior.

That’s why we run the benchmark with --total-ops 0:

* The benchmark still allocates memory and spawns threads, but it doesn’t perform real operations, allowing us to capture a clean snapshot of memory/thread placement without workload noise.

Run: 
```bash
mv ./NUMA_aware_libcuckoo/numactl_tests.sh ./
chmod +x numactl_tests.sh
./numactl_tests.sh
```
This will generate:
numa_test_*.txt — parsed from /proc/$pid/numa_maps, showing memory pages per NUMA node.
threadmap_*.txt — benchmark logs showing thread→CPU mapping.

Later, parse_and_plot_numa.py processes these files to produce Figure 2 in the final report.

# 9) Perf sweep (throughput + counters) and plot
We now run the same experiment as for the original benchmark, but this time for comparison with the optimized libcuckoo benchmark:
Run:
```bash
./bench_sweep.sh
```
You can than use:
```bash
python plot_sweep.py
```
To plot Figure 4 in the write-up.

