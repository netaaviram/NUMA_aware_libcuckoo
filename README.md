# NUMA-aware optimization of libcuckoo: analyzing scalability bottlenecks for improved performance through thread and memory placement

This repo reproduces my experiments showing that the libcuckoo benchmark’s scalability is bottlenecked by NUMA-unaware placement. I:

* Build libcuckoo’s universal_benchmark in Release mode.

* Run 7 configurations using numactl to control CPU & memory placement.

* Capture memory page placement (/proc/<pid>/numa_maps) and thread→CPU mapping (benchmark stdout).

* Convert the logs to CSVs and plot a normalized “memory vs threads” stacked-bar figure.

* Run a perf sweep (threads 1…N) across policies and plot throughput + perf counters.

All tests were made on Ubuntu-like systems. The example machine is a 4-socket Intel Xeon E5-4669 v4 (Broadwell-EX), 22 cores per socket, 4 NUMA nodes (0–3), SMT disabled.

---

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

---

# 1) Clone libcuckoo
```bash
wget https://github.com/efficient/libcuckoo/archive/refs/heads/master.zip
unzip master.zip
mv libcuckoo-master libcuckoo
cd libcuckoo/tests/universal-benchmark
```

---

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

---

# 3) Run the 7 NUMA placement tests

This step measures where memory pages are allocated and which NUMA node each thread runs on under different placement policies.
We are not interested in throughput here, only validating placement behavior 

That’s why we run the benchmark with --total-ops 0: 
The benchmark still allocates memory and spawns threads, but it doesn’t perform real operations, allowing us to capture a clean snapshot of memory/thread placement without workload noise.

Run: 
```bash
mv ./NUMA_aware_libcuckoo/numactl_tests.sh ./
chmod +x numactl_tests.sh
./numactl_tests.sh
```
This will generate:

numa_test_*.txt - parsed from /proc/$pid/numa_maps, showing memory pages per NUMA node.

threadmap_*.txt - benchmark logs showing thread→CPU mapping.

Later, parse_and_plot_numa.py processes these files to produce Figure 2 in the final report.

---

# 4) Run a sweep over thread count: 
This step is the scalability profiling I ran to understand and detect bottlenecks in libcuckoo. I ran a real workload (--total-ops != 0) while sweeping the thread count across sockets and record:

* The benchmark’s throughput (from its JSON)

* Linux perf stat hardware counters (cycles, instructions, cache/branch stats)

This produces the CSV and plot for Figure 1 in the final report (throughput vs. threads, with counters).

Run the sweep:
```bash
mv ./NUMA_aware_libcuckoo/bench_sweep.sh ./
chmod +x bench_sweep.sh
./bench_sweep.sh
```
This creates a sweep.csv with the following columns:
config,threads,throughput,cycles,instructions,cache_references,cache_misses,branches,branch_misses

* NOTE: This test runs 44 different thread counts (1,3,5,7,...,87) for each of the configurations. You should expect long runtime (few hours).

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

---

# 5) Clone NUMA_aware_libcuckoo project (this repo):
```bash
wget https://github.com/netaaviram/NUMA_aware_libcuckoo/archive/refs/heads/main.zip -O numa_libcuckoo.zip
unzip numa_libcuckoo.zip
mv NUMA_aware_libcuckoo-main NUMA_aware_libcuckoo
```

---

# 6) Capture hardware & NUMA topology (one time)
We record the CPU→NUMA mapping for later parsing:
```bash
lscpu > lscpu.txt
numactl --hardware > numactl_hardware.txt
```
On the reference system:
• 4 sockets × 22 cores = 88 cores (no SMT)
• NUMA nodes 0–3
• Node 0 CPUs: 0–10,44–54; Node 1: 11–21,55–65; Node 2: 22–32,66–76; Node 3: 33–43,77–87

---

# 7) Switching to NUMA aware implementation
Replace the original benchmark file with the NUMA optimized benchmark file from NUMA_aware_libcuckoo:
```bash
rm universal_benchmark.cc
mv ./NUMA_aware_libcuckoo/universal_benchmark.cc ./
```
* Now, the new optimized version of universal benchmark is saved as universal_benchmark.cc under libcuckoo/build/tests/universal_benchmark.cc

---

# 8) Build and Compile the optimized NUMA-aware libcuckoo implementation
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

---

# 9) Perf sweep (throughput + counters) and plot
We now run the same experiment as for the original benchmark, but this time for comparison with the optimized libcuckoo benchmark.

Run:
```bash
./bench_sweep.sh
```
You can than use:
```bash
python plot_sweep.py
```
To plot Figure 4 in the write-up.

---

# Appendix

A1. Building and Installing libnuma Locally

The experiments rely on explicit NUMA memory placement via the libnuma library. Many systems ship with libnuma preinstalled, but to avoid version mismatches we build it from source in a local prefix ($HOME/numa_local).

Step-by-Step: Rebuild Guide

Set up NUMA (libnuma) from source

1.1 Create a clean install directory
```bash
mkdir -p $HOME/numa_local/build
cd       $HOME/numa_local/build
```

1.2 Download latest libnuma release (as of Aug 2025)
```bash
wget https://github.com/numactl/numactl/releases/download/v2.0.18/numactl-2.0.18.tar.gz
```

1.3 Extract
```bash
tar xf numactl-2.0.18.tar.gz
cd numactl-2.0.18
```
1.4 Configure with local prefix
```bash
./configure --prefix=$HOME/numa_local/install
```
1.5 Compile and install
```bash
make -j$(nproc)
make install
```

Verify installation

Headers: $HOME/numa_local/install/include/

Library: $HOME/numa_local/install/lib/libnuma.so

When building universal_benchmark, the -I and -L flags must point to these paths, and you must link with -lnuma.

---

A2. Understanding --total-ops

We set the benchmark’s --total-ops so that each datapoint runs ~1–2 seconds on rack-mad-04. 

This runtime produced stable throughput and repeatability while keeping the sweep tractable. 

Note that the program’s reported total_ops in the JSON reflects internal scaling (per thread/NUMA shard), so the actual executed operations are in the millions per point. 

All configurations and thread counts used the same setting to ensure fair comparison.

---

A3. Perf Counter Collection

We use Linux perf stat in CSV mode around each benchmark run to capture hardware counters. Important notes:

* perf stat counts are system-wide; background processes can slightly perturb results.

* To minimize noise, run on a dedicated node or at low system load.

* The script extracts counters like cycles, instructions, cache_references, cache_misses, branches, branch_misses.

* For deeper analysis, you can extend it to include memory bandwidth counters (e.g. uncore_imc) if supported by your CPU.

---

A4. Placement Policies in bench_sweep.sh

For clarity, the CONFIGS array in the sweep script defines several policies:

* bind_n0, bind_n1, … → Threads and memory pinned to a single NUMA node.

* cross_bind → Threads on one node, memory on another (worst-case).

* interleave → Pages striped across all nodes.

* default → OS default, often first-touch.

This variety shows exactly how sensitive libcuckoo is to NUMA awareness.

---

A5. Runtime Expectations

* NUMA placement tests (numactl_tests.sh) run quickly (seconds per config).

* Perf sweeps (bench_sweep.sh) are much heavier: for 7 configs × ~44 thread counts, expect hours of runtime on a 4-socket 88-core system. Plan accordingly.

---

A6. What the NUMA debug prints mean (optimized benchmark)

I added a few stderr debug prints to the optimized universal_benchmark to make NUMA behavior visible and easy to validate. They appear before the JSON (which stays on stdout so scripts can parse it cleanly). You can always capture them separately via 2> debug.log.

Sample output (stderr):

```bash
Detected 4 NUMA nodes
NUMA node 0 has 8388608 keys
NUMA node 1 has 8388608 keys
NUMA node 2 has 8388608 keys
NUMA node 3 has 8388608 keys
Pre-filling table shards
[prefill n3 t0] cpu=77
[prefill n1 t0] cpu=11
[prefill n0 t0] cpu=0
[prefill n2 t0] cpu=67
Running operations
[mix    n0 t0] cpu=44
[mix    n1 t0] cpu=11
[mix    n3 t0] cpu=78
[mix    n2 t0] cpu=66
```

What each line indicates:

Detected N NUMA nodes

If --numa-nodes isn’t provided, we auto-detect with numa_max_node()+1. This is the number of shards we build and the number of node-local CPU sets we’ll target.

NUMA node i has X keys

We partition the global key set evenly across nodes (round-robin). This ensures each shard (per-node table) is prefilled and exercised using its own local key subset, minimizing cross-node traffic by design.

Pre-filling table shards

We create per-node tables and insert each node’s keys on threads pinned to that node’s CPUs. This warms up placement so pages back the shard on the correct NUMA node (first-touch).

[prefill n<i> t<j>] cpu=<k>

During prefill, thread j for node i is bound to logical CPU k (reported by sched_getcpu()). CPUs listed here should belong to node i. Quick check: compare with numactl --hardware.

Running operations

Start of the measured phase. Work is divided per node and per thread, mirroring prefill placement.

[mix n<i> t<j>] cpu=<k>

During the hot loop, the mix thread for node i is still running on a CPU from node i. Seeing CPUs that don’t belong to node i suggests affinity issues.
