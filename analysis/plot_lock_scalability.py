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
    cores, txns = [], []
    with open(CSV) as f:
        for r in csv.DictReader(f):
            cores.append(int(r["cores"]))
            txns.append(int(float(r["total_bus_txns"])))
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        plt.figure(figsize=(7, 4))
        plt.plot(cores, txns, marker="s", color="purple")
        plt.xlabel("competing cores")
        plt.ylabel("total bus transactions")
        plt.title("TAS spinlock: coherence traffic vs. contention")
        plt.grid(True, alpha=0.3); plt.tight_layout()
        out = os.path.join(HERE, "lock_scalability.png")
        plt.savefig(out)
        print("wrote", out)
    except Exception as e:
        print("[matplotlib unavailable: %s] ASCII summary:" % e)
        mx = max(txns) or 1
        for c, t in zip(cores, txns):
            bar = "#" * int(50 * t / mx)
            print(f"  cores={c}  txns={t:6d} |{bar}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
