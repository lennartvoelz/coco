#!/usr/bin/env python3
"""Plot benchmark results for the shared counter and the priority queue.

Inputs (defaults; override on the command line):
  - counter CSV (long format from bench_counter.sh):
      impl,N,C,run,observed,ms
  - priority-queue CSV (from run_bench.sh):
      mode,threads,ops_per_thread,total_ops,pops_empty,time_ms,ops_per_sec

Usage:
  ./plot.py                              # both, default paths
  ./plot.py --counter counter_results.csv --pq results.csv
  ./plot.py --counter-only
  ./plot.py --pq-only
"""

import argparse
import csv
from collections import defaultdict
from pathlib import Path

import matplotlib.pyplot as plt

HERE = Path(__file__).parent

COUNTER_IMPLS = ["atomic", "rmw_lock", "mcs_lock", "pthread"]
COUNTER_STYLE = {
    "atomic":   {"marker": "o", "color": "tab:blue"},
    "rmw_lock": {"marker": "s", "color": "tab:red"},
    "mcs_lock": {"marker": "D", "color": "tab:purple"},
    "pthread":  {"marker": "^", "color": "tab:green"},
}
COUNTER_LABEL = {
    "atomic":   "std::atomic",
    "rmw_lock": "RMW spinlock",
    "mcs_lock": "MCS lock",
    "pthread":  "pthread mutex",
}

PQ_MODES = ["mcs", "mutex"]
PQ_STYLE = {
    "mcs":   {"marker": "D", "color": "tab:purple"},
    "mutex": {"marker": "^", "color": "tab:green"},
}
PQ_LABEL = {"mcs": "MCS lock", "mutex": "pthread mutex"}


def mean(xs):
    return sum(xs) / len(xs)


# --------------------------------------------------------------------------- #
# Shared counter
# --------------------------------------------------------------------------- #
def load_counter(csv_path):
    # samples[(impl, C, N)] = list of ms
    samples = defaultdict(list)
    with open(csv_path) as f:
        for row in csv.DictReader(f):
            key = (row["impl"], int(row["C"]), int(row["N"]))
            samples[key].append(float(row["ms"]))
    return samples


def plot_counter(csv_path, out_path):
    samples = load_counter(csv_path)
    threads = sorted({n for _, _, n in samples})
    work_sizes = sorted({c for _, c, _ in samples})

    fig, axes = plt.subplots(
        1, len(work_sizes), figsize=(5 * len(work_sizes), 4.5), sharey=False
    )
    if len(work_sizes) == 1:
        axes = [axes]

    for ax, C in zip(axes, work_sizes):
        for impl in COUNTER_IMPLS:
            ys = []
            for N in threads:
                ms_list = samples.get((impl, C, N), [])
                if not ms_list:
                    ys.append(float("nan"))
                    continue
                avg_ms = mean(ms_list)
                ys.append(C / (avg_ms / 1000.0))
            ax.plot(threads, ys, label=COUNTER_LABEL[impl], linewidth=2,
                    **COUNTER_STYLE[impl])

        ax.set_xscale("log", base=2)
        ax.set_yscale("log")
        ax.set_xticks(threads)
        ax.set_xticklabels(threads)
        ax.set_xlabel("threads (N)")
        ax.set_ylabel("updates / second")
        ax.set_title(f"C = {C:,}")
        ax.grid(True, which="both", alpha=0.3)
        ax.legend()

    fig.suptitle("Shared counter throughput: atomic vs. RMW vs. MCS vs. pthread")
    fig.tight_layout()
    fig.savefig(out_path, dpi=140)
    print(f"Wrote {out_path}")


# --------------------------------------------------------------------------- #
# Priority queue
# --------------------------------------------------------------------------- #
def load_pq(csv_path):
    # samples[(mode, threads)] = list of ops_per_sec
    samples = defaultdict(list)
    with open(csv_path) as f:
        for row in csv.DictReader(f):
            key = (row["mode"], int(row["threads"]))
            samples[key].append(float(row["ops_per_sec"]))
    return samples


def plot_pq(csv_path, out_path):
    samples = load_pq(csv_path)
    threads = sorted({n for _, n in samples})

    fig, ax = plt.subplots(figsize=(6.5, 4.5))
    for mode in PQ_MODES:
        ys = []
        for n in threads:
            ops = samples.get((mode, n), [])
            ys.append(mean(ops) if ops else float("nan"))
        ax.plot(threads, ys, label=PQ_LABEL[mode], linewidth=2, **PQ_STYLE[mode])

    ax.set_xscale("log", base=2)
    ax.set_yscale("log")
    ax.set_xticks(threads)
    ax.set_xticklabels(threads)
    ax.set_xlabel("threads")
    ax.set_ylabel("operations / second")
    ax.set_title("Priority queue throughput: MCS lock vs. pthread mutex")
    ax.grid(True, which="both", alpha=0.3)
    ax.legend()

    fig.tight_layout()
    fig.savefig(out_path, dpi=140)
    print(f"Wrote {out_path}")


# --------------------------------------------------------------------------- #
# CLI
# --------------------------------------------------------------------------- #
def main():
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--counter", default=str(HERE / "counter_results.csv"),
                   help="shared-counter CSV (default: counter_results.csv)")
    p.add_argument("--pq", default=str(HERE / "results.csv"),
                   help="priority-queue CSV (default: results.csv)")
    p.add_argument("--counter-out", default=str(HERE / "counter_throughput.png"))
    p.add_argument("--pq-out", default=str(HERE / "pq_throughput.png"))
    p.add_argument("--counter-only", action="store_true")
    p.add_argument("--pq-only", action="store_true")
    args = p.parse_args()

    if not args.pq_only:
        if Path(args.counter).exists():
            plot_counter(args.counter, args.counter_out)
        else:
            print(f"skip counter: {args.counter} not found")

    if not args.counter_only:
        if Path(args.pq).exists():
            plot_pq(args.pq, args.pq_out)
        else:
            print(f"skip pq: {args.pq} not found")


if __name__ == "__main__":
    main()
