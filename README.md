
This repo reproduces our experiments showing that the libcuckoo benchmark’s scalability is bottlenecked by NUMA‑unaware placement. We:

Build libcuckoo’s universal_benchmark in Release mode.

Run 7 configurations using numactl to control CPU & memory placement.

Capture memory page placement (/proc/<pid>/numa_maps) and thread→CPU mapping (benchmark stdout).

Convert the logs to CSVs and plot a normalized “memory vs threads” stacked‑bar figure.

Run a perf sweep (threads 1…N) across policies and plot throughput + perf counters.

Tested on Ubuntu‑like systems. The example machine is a 4‑socket Intel Xeon E5‑4669 v4 (Broadwell‑EX), 22 cores per socket, 4 NUMA nodes (0–3), SMT disabled.

0) Prerequisites
# Build tools & perf

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake numactl linux-tools-common linux-tools-generic
```

# Python plotting stack
```bash
python3 -m pip install --user pandas matplotlib
```

If you prefer conda: 
```bash
conda install -y pandas matplotlib.
```

1) Clone and build libcuckoo
```bash
git clone https://github.com/efficient/libcuckoo.git
cd libcuckoo
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j
```

The benchmark we use will be at:

libcuckoo/build/tests/universal-benchmark/universal_benchmark


For convenience below, symlink it into a working dir:
```bash
mkdir -p ~/numa-libcuckoo && cd ~/numa-libcuckoo
ln -s ../libcuckoo/build/tests/universal-benchmark/universal_benchmark .
```
2) Capture hardware & NUMA topology (one time)

We record the CPU→NUMA mapping for later parsing:
```bash
lscpu > lscpu.txt
numactl --hardware > numactl_hardware.txt
```

On our reference system:
• 4 sockets × 22 cores = 88 cores (no SMT)
• NUMA nodes 0–3
• Node 0 CPUs: 0–10,44–54; Node 1: 11–21,55–65; Node 2: 22–32,66–76; Node 3: 33–43,77–87


3) Run the 7 NUMA placement tests

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

