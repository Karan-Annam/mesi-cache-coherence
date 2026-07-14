#!/usr/bin/env python3
"""Plot spinlock scalability: total bus transactions vs. number of competing
cores. Reads analysis/lock_scalability.csv. Falls back to ASCII if matplotlib is
unavailable.
"""
import csv, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
CSV = os.path.join(HERE, "lock_scalability.csv")


def main():
    if not os.path.exists(CSV):
        print("no lock_scalability.csv (run model_main first)")
        return 0
    cores, med, lo, hi = [], [], [], []
    with open(CSV) as f:
        for r in csv.DictReader(f):
            cores.append(int(r["cores"]))
            med.append(float(r["median_bus_txns_per_acq"]))
            lo.append(float(r["min_bus_txns_per_acq"]))
            hi.append(float(r["max_bus_txns_per_acq"]))
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        plt.figure(figsize=(7, 4))
        err = [[m - l for m, l in zip(med, lo)],
               [h - m for m, h in zip(med, hi)]]
        plt.errorbar(cores, med, yerr=err, marker="s", capsize=4, color="purple")
        plt.xlabel("competing cores")
        plt.ylabel("bus transactions per lock acquisition")
        plt.title("TAS spinlock coherence traffic (7-trial median/range)")
        plt.grid(True, alpha=0.3); plt.tight_layout()
        out = os.path.join(HERE, "..", "docs", "img", "lock_scalability.png")
        plt.savefig(out)
        print("wrote", out)
    except Exception as e:
        print("[matplotlib unavailable: %s] ASCII summary:" % e)
        mx = max(med) or 1
        for c, m, l, h in zip(cores, med, lo, hi):
            bar = "#" * int(50 * m / mx)
            print(f"  cores={c}  txns/acq={m:.4f} [{l:.4f}, {h:.4f}] |{bar}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
