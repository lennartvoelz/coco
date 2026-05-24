#!/usr/bin/env python3
"""Aggregate barrier benchmark results and produce a table + plot.

Reads CSV produced by run_bench.sh (columns:
    mode,threads,iterations,total_ops,time_ms,avg_latency_ns,ok)
groups trials by (mode, threads), and emits:
  - <prefix>.txt : pivot table of median latency per (mode, threads)
  - <prefix>.png : line plot, one curve per mode, shaded min/max band

Usage: ./plot.py [results.csv] [output_prefix]
  Defaults: results.csv, barrier_latency
"""
import csv
import sys
from collections import defaultdict
from statistics import median

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt


MODE_ORDER = ["central", "central_opt", "dissemination", "pthread"]
MODE_LABEL = {
    "central":       "Central (spin)",
    "central_opt":   "Central (wait/notify)",
    "dissemination": "Dissemination",
    "pthread":       "pthread_barrier",
}
MARKERS = {"central": "o", "central_opt": "s", "dissemination": "^", "pthread": "D"}


def main():
    csv_path   = sys.argv[1] if len(sys.argv) > 1 else "results.csv"
    out_prefix = sys.argv[2] if len(sys.argv) > 2 else "barrier_latency"

    # latencies[mode][threads] = [avg_latency_ns, ...] across trials
    latencies = defaultdict(lambda: defaultdict(list))
    with open(csv_path) as f:
        for row in csv.DictReader(f):
            if row.get("ok") != "1":
                print(f"warning: skipping failed run: {row}", file=sys.stderr)
                continue
            latencies[row["mode"]][int(row["threads"])].append(
                float(row["avg_latency_ns"])
            )

    modes = [m for m in MODE_ORDER if m in latencies]
    extras = [m for m in latencies if m not in MODE_ORDER]
    modes += sorted(extras)
    all_threads = sorted({t for d in latencies.values() for t in d})

    # --- Text pivot table (median per cell) ---
    table_path = f"{out_prefix}.txt"
    col_w = 18
    with open(table_path, "w") as out:
        header = f"{'threads':>10}" + "".join(f"  {MODE_LABEL.get(m, m):>{col_w}}" for m in modes)
        out.write(header + "\n")
        out.write("-" * len(header) + "\n")
        for n in all_threads:
            line = f"{n:>10}"
            for m in modes:
                vals = latencies[m].get(n)
                line += f"  {median(vals):>{col_w}.1f}" if vals else f"  {'-':>{col_w}}"
            out.write(line + "\n")
        out.write("\n(units: ns; cell value is median of trials)\n")

    # --- Plot ---
    fig, ax = plt.subplots(figsize=(8.5, 5.5))
    for m in modes:
        ts = sorted(latencies[m].keys())
        meds = [median(latencies[m][t]) for t in ts]
        mins = [min(latencies[m][t])    for t in ts]
        maxs = [max(latencies[m][t])    for t in ts]
        line, = ax.plot(ts, meds, marker=MARKERS.get(m, "o"), label=MODE_LABEL.get(m, m))
        ax.fill_between(ts, mins, maxs, alpha=0.15, color=line.get_color())

    ax.set_xlabel("Threads")
    ax.set_ylabel("Avg. barrier latency (ns)")
    ax.set_title("Average barrier latency vs. thread count")
    ax.set_yscale("log")
    ax.grid(True, which="both", linestyle=":", alpha=0.6)
    ax.set_xticks(all_threads)
    ax.legend(loc="best")
    fig.tight_layout()
    plot_path = f"{out_prefix}.png"
    fig.savefig(plot_path, dpi=150)

    print(f"Wrote {table_path} and {plot_path}", file=sys.stderr)


if __name__ == "__main__":
    main()
