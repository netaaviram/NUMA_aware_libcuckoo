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
