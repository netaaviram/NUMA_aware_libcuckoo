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
