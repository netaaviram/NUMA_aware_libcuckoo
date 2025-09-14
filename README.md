
This repo reproduces our experiments showing that the libcuckoo benchmark’s scalability is bottlenecked by NUMA‑unaware placement. We:

Build libcuckoo’s universal_benchmark in Release mode.

Run 7 configurations using numactl to control CPU & memory placement.

Capture memory page placement (/proc/<pid>/numa_maps) and thread→CPU mapping (benchmark stdout).

Convert the logs to CSVs and plot a normalized “memory vs threads” stacked‑bar figure.

Run a perf sweep (threads 1…N) across policies and plot throughput + perf counters.

Tested on Ubuntu‑like systems. The example machine is a 4‑socket Intel Xeon E5‑4669 v4 (Broadwell‑EX), 22 cores per socket, 4 NUMA nodes (0–3), SMT disabled.

0) Prerequisites
1) If you are running on an other system but the rack-mad-04 of TAU, you need to install prequisits: 
# Build tools & perf

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake numactl linux-tools-common linux-tools-generic
```

# Python plotting stack
```bash
python3 -m pip install --user pandas matplotlib
```
1) Clone NUMA_aware_libcuckoo project (this repo):
```bash
wget https://github.com/netaaviram/NUMA_aware_libcuckoo/archive/refs/heads/main.zip -O numa_libcuckoo.zip
unzip numa_libcuckoo.zip
mv NUMA_aware_libcuckoo-main NUMA_aware_libcuckoo
```

2) Clone libcuckoo
```bash
wget https://github.com/efficient/libcuckoo/archive/refs/heads/master.zip
unzip master.zip
mv libcuckoo-master libcuckoo
cd libcuckoo/tests/universal-benchmark
```

3) Replace the original benchmark file with the NUMA optimized benchmark file from NUMA_aware_libcuckoo
```bash
mv universal_benchmark.cc ./universal_benchmark_backup.cc
mv ../../../NUMA_aware_libcuckoo ./universal_benchmark.cc
```
* Now, the new optimized version of universal benchmark is saved as universal_benchmark.cc under libcuckoo/build/tests/universal_benchmark.cc

3) Build and Compile libcuckoo
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

The benchmark will be at:

4) Capture hardware & NUMA topology (one time)

We record the CPU→NUMA mapping for later parsing:
```bash
lscpu > lscpu.txt
numactl --hardware > numactl_hardware.txt
```

On the reference system:
• 4 sockets × 22 cores = 88 cores (no SMT)
• NUMA nodes 0–3
• Node 0 CPUs: 0–10,44–54; Node 1: 11–21,55–65; Node 2: 22–32,66–76; Node 3: 33–43,77–87

5) Run the 7 NUMA placement tests

These commands save (a) memory placement from /proc/<pid>/numa_maps and (b) the benchmark’s thread→CPU lines.

We keep the small “initialization” workload used in the locality experiments you saw above (so --total-ops 0). For perf sweeps we’ll set --total-ops later.

--> Run numactl_tests.sh

You should now have files like:
```bash
numa_test_*.txt
threadmap_*.txt
```
--> Run parse_and_plot_numa.py:
```bash
python3 parse_and_plot_numa.py
```
-->Run python3 parse_and_plot_numa.py:
```bash
python python3 parse_and_plot_numa.py
```
5) Perf sweep (throughput + counters) and plot

We now benchmark a real workload by setting --total-ops to a large value and sweeping threads across sockets. We collect throughput from the benchmark JSON and perf counters via perf stat.

--> Run bench_sweep.sh

Make it executable and run:
```bash
chmod +x bench_sweep.sh
./bench_sweep.sh
```

This produces sweep.csv.

--> Plot it with plot_sweep.py:
```bash
python plot_sweep.py
```

You now have numa_sweep_perf.png—this is “Figure 1” in the write‑up.

6) How to cite the figures in your report

Figure 1: numa_sweep_perf.png — throughput/IPC/cache‑miss/branch‑miss across configurations & threads.

Figure 2: memory_vs_threads.png — normalized memory vs. thread placement across NUMA nodes.

7) Notes & troubleshooting

If perf says “not found”: sudo apt-get install linux-tools-$(uname -r)

If jq is not installed, we do not use it (the scripts parse JSON with Python).

If your machine has a different NUMA layout, the parser reads it from numactl --hardware, so no hard‑coding is required.

For long runs, increase TOTAL_OPS and/or adjust THREADS_STEP.

For exact reproduction of our “bind0 peaks at ~22 threads” observation, confirm your socket has 22 cores (see lscpu).
