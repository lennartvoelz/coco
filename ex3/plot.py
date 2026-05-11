#!/usr/bin/env python3
"""Plot updates/sec vs. thread count for atomic, rmw spinlock, and mutex."""

import csv
from collections import defaultdict
from pathlib import Path

import matplotlib.pyplot as plt

HERE = Path(__file__).parent
CSV_PATH = HERE / "results.csv"
OUT_PATH = HERE / "throughput.png"

IMPLS = ["atomic", "rmw_lock", "mutex"]
STYLE = {
    "atomic":   {"marker": "o", "color": "tab:blue"},
    "rmw_lock": {"marker": "s", "color": "tab:red"},
    "mutex":    {"marker": "^", "color": "tab:green"},
}
LABEL = {"atomic": "std::atomic", "rmw_lock": "RMW spinlock", "mutex": "std::mutex"}


def load(csv_path):
    # samples[(impl, C, N)] = list of ms
    samples = defaultdict(list)
    with open(csv_path) as f:
        for row in csv.DictReader(f):
            key = (row["impl"], int(row["C"]), int(row["N"]))
            samples[key].append(float(row["ms"]))
    return samples


def mean(xs):
    return sum(xs) / len(xs)


def main():
    samples = load(CSV_PATH)
    threads = sorted({n for _, _, n in samples})
    work_sizes = sorted({c for _, c, _ in samples})

    fig, axes = plt.subplots(
        1, len(work_sizes), figsize=(5 * len(work_sizes), 4.5), sharey=False
    )
    if len(work_sizes) == 1:
        axes = [axes]

    for ax, C in zip(axes, work_sizes):
        for impl in IMPLS:
            ys = []
            for N in threads:
                ms_list = samples.get((impl, C, N), [])
                if not ms_list:
                    ys.append(float("nan"))
                    continue
                avg_ms = mean(ms_list)
                ups = C / (avg_ms / 1000.0)
                ys.append(ups)
            ax.plot(threads, ys, label=LABEL[impl], linewidth=2, **STYLE[impl])

        ax.set_xscale("log", base=2)
        ax.set_yscale("log")
        ax.set_xticks(threads)
        ax.set_xticklabels(threads)
        ax.set_xlabel("threads (N)")
        ax.set_ylabel("updates / second")
        ax.set_title(f"C = {C:,}")
        ax.grid(True, which="both", alpha=0.3)
        ax.legend()

    fig.suptitle("Shared counter throughput: atomic vs. RMW spinlock vs. mutex")
    fig.tight_layout()
    fig.savefig(OUT_PATH, dpi=140)
    print(f"Wrote {OUT_PATH}")


if __name__ == "__main__":
    main()
