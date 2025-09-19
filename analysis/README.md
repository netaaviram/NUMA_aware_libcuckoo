# Analysis Scripts & Logs

This folder contains helper scripts and output logs used to analyze memory placement and thread affinity in the NUMA-aware libcuckoo benchmark.

## Contents

- `parse_and_plot_numa.py`  
  → Parses `numa_test_*.txt` and `threadmap_*.txt` to generate the memory placement figure.

- `plot_sweep.py`  
  → Plots throughput and perf counter results from `sweep.csv`.

## Example Logs

These `.txt` files contain placement and thread mapping outputs from different `numactl` configurations:

- `numa_test_*.txt`  
  → Memory placement snapshots from `/proc/<pid>/numa_maps`.

- `threadmap_*.txt`  
  → Thread-to-CPU mapping and benchmark debug output.

Each file corresponds to a different policy (e.g. `bind0`, `interleave_all`, etc.).

## Usage

To regenerate the plots:
```bash
python parse_and_plot_numa.py
python plot_sweep.py
