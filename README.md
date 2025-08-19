NUMA locality & performance with libcuckoo

This repo reproduces our experiments showing that the libcuckoo benchmark’s scalability is bottlenecked by NUMA‑unaware placement. We:

Build libcuckoo’s universal_benchmark in Release mode.

Run 7 configurations using numactl to control CPU & memory placement.

Capture memory page placement (/proc/<pid>/numa_maps) and thread→CPU mapping (benchmark stdout).

Convert the logs to CSVs and plot a normalized “memory vs threads” stacked‑bar figure.

Run a perf sweep (threads 1…N) across policies and plot throughput + perf counters.

Tested on Ubuntu‑like systems. The example machine is a 4‑socket Intel Xeon E5‑4669 v4 (Broadwell‑EX), 22 cores per socket, 4 NUMA nodes (0–3), SMT disabled.

0) Prerequisites
# Build tools & perf
sudo apt-get update
sudo apt-get install -y build-essential cmake numactl linux-tools-common linux-tools-generic

# Python plotting stack
python3 -m pip install --user pandas matplotlib


If you prefer conda: conda install -y pandas matplotlib.

1) Clone and build libcuckoo
git clone https://github.com/efficient/libcuckoo.git
cd libcuckoo
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j


The benchmark we use will be at:

libcuckoo/build/tests/universal-benchmark/universal_benchmark


For convenience below, symlink it into a working dir:

mkdir -p ~/numa-libcuckoo && cd ~/numa-libcuckoo
ln -s ../libcuckoo/build/tests/universal-benchmark/universal_benchmark .

2) Capture hardware & NUMA topology (one time)

We record the CPU→NUMA mapping for later parsing:

lscpu > lscpu.txt
numactl --hardware > numactl_hardware.txt


On our reference system:
• 4 sockets × 22 cores = 88 cores (no SMT)
• NUMA nodes 0–3
• Node 0 CPUs: 0–10,44–54; Node 1: 11–21,55–65; Node 2: 22–32,66–76; Node 3: 33–43,77–87

3) Run the 7 NUMA placement tests

These commands save (a) memory placement from /proc/<pid>/numa_maps and (b) the benchmark’s thread→CPU lines.

We keep the small “initialization” workload used in the locality experiments you saw above (so --total-ops 0). For perf sweeps we’ll set --total-ops later.

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


You should now have files like:

numa_test_*.txt
threadmap_*.txt

4) Convert logs → CSVs & plot “Memory vs Threads”

Create parse_and_plot_numa.py:

import re, json, sys, pathlib
import pandas as pd
from collections import defaultdict

root = pathlib.Path(".")

# --- CPU->node map from numactl --hardware
nodes = { }  # cpu -> node
with open("numactl_hardware.txt") as f:
    for line in f:
        m = re.match(r"node\s+(\d+)\s+cpus:\s+(.+)", line.strip())
        if m:
            node = int(m.group(1))
            cpus = [int(x) for x in m.group(2).split()]
            for c in cpus:
                nodes[c] = node

# --- 1) Memory placement (sum N0/N1/N2/N3 counts across all anon/heap segments)
mem_rows = []
for p in sorted(root.glob("numa_test_*.txt")):
    conf = p.stem.replace("numa_test_", "")
    counts = defaultdict(int)
    for line in p.read_text().splitlines():
        for m in re.finditer(r"N(\d+)=(\d+)", line):
            counts[f"N{m.group(1)}"] += int(m.group(2))
    mem_rows.append({
        "config": conf,
        "N0_pages": counts["N0"], "N1_pages": counts["N1"],
        "N2_pages": counts["N2"], "N3_pages": counts["N3"]
    })
df_mem = pd.DataFrame(mem_rows).sort_values("config")
df_mem.to_csv("Extended_NUMA_Allocation_Results.csv", index=False)

# --- 2) Thread placement (prefill/mix lines → CPU → node)
thr_rows = []
for p in sorted(root.glob("threadmap_*.txt")):
    conf = p.stem.replace("threadmap_", "")
    cpus = []
    for line in p.read_text().splitlines():
        m = re.search(r"\[(?:prefill|mix)\s+t\d+\]\s+cpu=(\d+)", line)
        if m:
            cpus.append(int(m.group(1)))
    counts = defaultdict(int)
    for cpu in cpus:
        n = nodes.get(cpu, -1)
        if n >= 0:
            counts[f"N{n}"] += 1
    thr_rows.append({
        "config": conf,
        "N0_threads": counts["N0"], "N1_threads": counts["N1"],
        "N2_threads": counts["N2"], "N3_threads": counts["N3"]
    })
df_thr = pd.DataFrame(thr_rows).sort_values("config")
df_thr.to_csv("Thread_Allocation_per_NUMA_Node.csv", index=False)

print("Wrote Extended_NUMA_Allocation_Results.csv and Thread_Allocation_per_NUMA_Node.csv")


Run it:

python3 parse_and_plot_numa.py


Now create plot_numa_memory_vs_threads.py (the cleaned, publication-quality plot):

import pandas as pd
import matplotlib.pyplot as plt

df_mem = pd.read_csv("Extended_NUMA_Allocation_Results.csv").set_index("config")
df_thr = pd.read_csv("Thread_Allocation_per_NUMA_Node.csv").set_index("config")

order = ["default","bind0","interleave_all","interleave_0_3","mem0_cpu3","mem3_cpu0","cpu1_interleave_0_1"]
nodes = ["N0","N1","N2","N3"]
colors = {"N0":"#1f77b4","N1":"#ff7f0e","N2":"#2ca02c","N3":"#d62728"}

df_mem = df_mem.loc[order]
df_thr = df_thr.loc[order]

df_mem_pct = df_mem.div(df_mem.sum(axis=1), axis=0)*100
df_thr_pct = df_thr.div(df_thr.sum(axis=1), axis=0)*100

def plot_stacked(ax, data, title):
    left = [0]*len(data)
    x = range(len(data))
    for n in nodes:
        ax.bar(x, data[n + ("_pages" if "pages" in data.columns[0] else "_threads")],
               bottom=left, color=colors[n], width=0.6, label=f"Node {n[-1]}" if title.startswith("Memory") else None)
        left = [l + d for l, d in zip(left, data[n + ("_pages" if "pages" in data.columns[0] else "_threads")])]
    ax.set_ylim(0,100); ax.set_ylabel("Percent of Total (%)")
    ax.set_title(title, fontsize=11)
    ax.set_xticks(list(x)); ax.set_xticklabels(order, rotation=25, ha='right')

fig, axes = plt.subplots(2,1, figsize=(13,6), sharex=True, gridspec_kw=dict(hspace=0.25))
plot_stacked(axes[0], df_mem_pct, "Memory placement (pages)")
plot_stacked(axes[1], df_thr_pct, "Thread placement (20 threads)")
axes[0].legend(title="NUMA node", ncol=4, loc="lower center", bbox_to_anchor=(0.5,1.15))
fig.suptitle("NUMA locality: Memory vs. Thread Affinity (Normalized)", y=1.03)
fig.tight_layout()
plt.savefig("memory_vs_threads.png", dpi=150)
plt.show()


Run:

python3 plot_numa_memory_vs_threads.py


This produces memory_vs_threads.png, the figure you’ll cite as “Figure 2”.

5) Perf sweep (throughput + counters) and plot

We now benchmark a real workload by setting --total-ops to a large value and sweeping threads across sockets. We collect throughput from the benchmark JSON and perf counters via perf stat.

Create bench_sweep.sh:

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


Make it executable and run:

chmod +x bench_sweep.sh
./bench_sweep.sh


This produces sweep.csv.

Plot it with plot_sweep.py:

import pandas as pd
import matplotlib.pyplot as plt

df = pd.read_csv("sweep.csv")

# Derived metrics
df["ipc"] = df["instructions"] / df["cycles"]
df["cache_miss_rate"] = df["cache_misses"] / df["cache_references"].clip(lower=1)

# Choose a subset/order like the figure in the write-up
order = ["bind_n0","bind_n1","bind_n2","bind_n3",
         "cpu0_mem1","cpu0_mem2","cpu1_mem0","cpu2_mem0","cpu3_mem2",
         "default","interleave_all"]
df["config"] = pd.Categorical(df["config"], categories=order, ordered=True)

fig, axs = plt.subplots(2,2, figsize=(16,7))
for name, sub, ax in [
    ("Throughput (ops/s)", df.pivot(index="threads", columns="config", values="throughput"), axs[0,0]),
    ("Instructions per Cycle", df.pivot(index="threads", columns="config", values="ipc"), axs[0,1]),
    ("Cache-miss Rate", df.pivot(index="threads", columns="config", values="cache_miss_rate"), axs[1,0]),
    ("Branch-misses", df.pivot(index="threads", columns="config", values="branch_misses"), axs[1,1]),
]:
    sub = sub[order]
    for c in sub.columns:
        ax.plot(sub.index, sub[c], marker="o", ms=2, lw=1, label=c)
    ax.set_title(name); ax.set_xlabel("Threads"); ax.grid(True, alpha=.2)
axs[0,0].legend(ncol=2, fontsize=8)
fig.suptitle("NUMA sweep – perf counters", fontsize=14)
fig.tight_layout()
plt.savefig("numa_sweep_perf.png", dpi=150)
plt.show()


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

8) Repository layout (suggested)
README.md
universal_benchmark -> ../libcuckoo/build/tests/universal-benchmark/universal_benchmark

# topology
lscpu.txt
numactl_hardware.txt

# placement experiments
numa_test_*.txt
threadmap_*.txt
parse_and_plot_numa.py
Extended_NUMA_Allocation_Results.csv
Thread_Allocation_per_NUMA_Node.csv
memory_vs_threads.png

# perf sweep
bench_sweep.sh
sweep.csv
plot_sweep.py
numa_sweep_perf.png
