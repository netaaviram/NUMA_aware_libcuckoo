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
