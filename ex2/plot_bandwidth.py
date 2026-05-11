import csv
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker
import numpy as np

# ── Load results ──────────────────────────────────────────────────────────────
threads, avg_bw, peak_bw = [], [], []
with open("bandwidth_results.csv") as f:
    reader = csv.DictReader(f)
    for row in reader:
        threads.append(int(row["threads"]))
        avg_bw.append(float(row["avg_GBps"]))
        peak_bw.append(float(row["peak_GBps"]))

threads  = np.array(threads)
avg_bw   = np.array(avg_bw)
peak_bw  = np.array(peak_bw)

# Ideal linear scaling anchored at 1-thread result
ideal = avg_bw[0] * threads

# ── Plot ──────────────────────────────────────────────────────────────────────
fig, ax = plt.subplots(figsize=(9, 5))

ax.plot(threads, avg_bw,  "o-",  color="steelblue",   linewidth=2,
        markersize=6, label="Average bandwidth")
ax.plot(threads, peak_bw, "s--", color="darkorange",   linewidth=1.5,
        markersize=5, label="Peak bandwidth")
ax.plot(threads, ideal,   "k:",  linewidth=1.4, label="Ideal linear scaling")

ax.set_xlabel("Number of threads", fontsize=12)
ax.set_ylabel("Memory bandwidth (GB/s)", fontsize=12)
ax.set_title("Memory Bandwidth vs. Thread Count\n"
             "Apple M2 Max — 1 GiB sequential load, cacheline stride",
             fontsize=13)
ax.set_xticks(threads)
ax.set_xlim(0.5, threads[-1] + 0.5)
ax.set_ylim(0, ideal[-1] * 1.05)
ax.yaxis.set_minor_locator(ticker.MultipleLocator(10))
ax.grid(True, which="major", linestyle="--", alpha=0.5)
ax.grid(True, which="minor", linestyle=":",  alpha=0.3)
ax.legend(fontsize=10, loc="upper left")

plt.tight_layout()
plt.savefig("bandwidth_plot.png", dpi=150)
print("Saved bandwidth_plot.png")
plt.show()
