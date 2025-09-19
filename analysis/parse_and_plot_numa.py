#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import re
import pathlib
from collections import defaultdict

import pandas as pd
import matplotlib.pyplot as plt


# -----------------------
# Helpers
# -----------------------
def parse_cpu_to_node_map(path: pathlib.Path) -> dict[int, int]:
    """
    Parse `numactl --hardware` output to map CPU id -> NUMA node id.
    Expected lines like: "node 0 cpus: 0 1 2 3 ..."
    """
    nodes: dict[int, int] = {}
    if not path.exists():
        raise FileNotFoundError(
            f"Missing {path}. Run `numactl --hardware > {path.name}` in this directory."
        )
    with path.open("r", encoding="utf-8") as f:
        for line in f:
            m = re.match(r"node\s+(\d+)\s+cpus:\s+(.+)", line.strip())
            if m:
                node = int(m.group(1))
                cpus = [int(x) for x in m.group(2).split()]
                for c in cpus:
                    nodes[c] = node
    if not nodes:
        raise ValueError(
            f"No CPU→node lines found in {path}. Check the contents format."
        )
    return nodes


def stacked_bar(df: pd.DataFrame, cols, title, ylabel, out_png, out_pdf=None):
    """Simple stacked bar plot over df['config'] for the given columns."""
    if df.empty:
        print(f"[plot] Skipped {title}: no data.")
        return

    plt.figure(figsize=(10, 5))
    bottoms = pd.Series([0] * len(df), index=df.index, dtype=float)

    for col in cols:
        vals = df[col].fillna(0).astype(float)
        plt.bar(df["config"], vals, bottom=bottoms, label=col)
        bottoms = bottoms + vals

    plt.title(title)
    plt.ylabel(ylabel)
    plt.xlabel("Config")
    plt.xticks(rotation=45, ha="right")
    plt.legend(title="NUMA node", ncol=4, frameon=False)
    plt.tight_layout()
    plt.savefig(out_png, dpi=150)
    if out_pdf:
        plt.savefig(out_pdf)
    plt.close()
    print(f"[plot] Saved {out_png}" + (f" and {out_pdf}" if out_pdf else ""))


def make_percent_df(df: pd.DataFrame, cols: list[str], rename_map: dict[str, str]) -> pd.DataFrame:
    """Create a new dataframe with config + percent columns derived from cols."""
    if df.empty:
        return df
    out = df[["config"]].copy()
    totals = df[cols].sum(axis=1).replace(0, 1)  # avoid div-by-zero
    for old in cols:
        new = rename_map[old]
        out[new] = (df[old] / totals) * 100.0
    return out


# -----------------------
# Main
# -----------------------
def main():
    root = pathlib.Path(".").resolve()

    # --- CPU->node map from numactl --hardware
    nodes = parse_cpu_to_node_map(root / "numactl_hardware.txt")

    # --- 1) Memory placement (sum N0/N1/N2/N3 counts across all anon/heap segments)
    mem_rows = []
    for p in sorted(root.glob("numa_test_*.txt")):
        conf = p.stem.replace("numa_test_", "")
        counts = defaultdict(int)
        for line in p.read_text().splitlines():
            for m in re.finditer(r"N(\d+)=(\d+)", line):
                counts[f"N{m.group(1)}"] += int(m.group(2))
        mem_rows.append(
            {
                "config": conf,
                "N0_pages": counts["N0"],
                "N1_pages": counts["N1"],
                "N2_pages": counts["N2"],
                "N3_pages": counts["N3"],
            }
        )
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
        thr_rows.append(
            {
                "config": conf,
                "N0_threads": counts["N0"],
                "N1_threads": counts["N1"],
                "N2_threads": counts["N2"],
                "N3_threads": counts["N3"],
            }
        )
    df_thr = pd.DataFrame(thr_rows).sort_values("config")
    df_thr.to_csv("Thread_Allocation_per_NUMA_Node.csv", index=False)

    print("Wrote Extended_NUMA_Allocation_Results.csv and Thread_Allocation_per_NUMA_Node.csv")

    # -----------------------
    # Plots
    # -----------------------
    # Memory (counts)
    mem_cols = ["N0_pages", "N1_pages", "N2_pages", "N3_pages"]
    stacked_bar(
        df_mem,
        mem_cols,
        title="NUMA Memory Allocation (pages)",
        ylabel="Pages",
        out_png="numa_memory_pages_stacked.png",
        out_pdf="numa_memory_pages_stacked.pdf",
    )

    # Memory (percent)
    mem_rename = {c: c.replace("pages", "pct_pages") for c in mem_cols}
    df_mem_pct = make_percent_df(df_mem, mem_cols, mem_rename)
    stacked_bar(
        df_mem_pct,
        list(mem_rename.values()),
        title="NUMA Memory Allocation (percent)",
        ylabel="Percent (%)",
        out_png="numa_memory_percent_stacked.png",
        out_pdf="numa_memory_percent_stacked.pdf",
    )

    # Threads (counts)
    thr_cols = ["N0_threads", "N1_threads", "N2_threads", "N3_threads"]
    stacked_bar(
        df_thr,
        thr_cols,
        title="NUMA Thread Placement (count)",
        ylabel="Threads",
        out_png="numa_threads_stacked.png",
        out_pdf="numa_threads_stacked.pdf",
    )

    # Threads (percent)
    thr_rename = {c: c.replace("threads", "pct_threads") for c in thr_cols}
    df_thr_pct = make_percent_df(df_thr, thr_cols, thr_rename)
    stacked_bar(
        df_thr_pct,
        list(thr_rename.values()),
        title="NUMA Thread Placement (percent)",
        ylabel="Percent (%)",
        out_png="numa_threads_percent_stacked.png",
        out_pdf="numa_threads_percent_stacked.pdf",
    )


if __name__ == "__main__":
    main()
