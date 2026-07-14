#!/usr/bin/env python3
"""Plot the TSO Dekker/SB litmus violation rate with and without fences.
Reads analysis/tso_litmus.csv. Falls back to ASCII if matplotlib is unavailable.
"""
import csv, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
CSV = os.path.join(HERE, "tso_litmus.csv")


def main():
    if not os.path.exists(CSV):
        print("no tso_litmus.csv (run model_main first)")
        return 0
    labels, rates = [], []
    with open(CSV) as f:
        for r in csv.DictReader(f):
            labels.append("with fence" if int(r["fenced"]) else "no fence (TSO)")
            rates.append(float(r["violation_rate"]))
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        plt.figure(figsize=(6, 4))
        plt.bar(labels, rates, color=["crimson", "seagreen"])
        plt.ylabel("Dekker mutual-exclusion violation rate")
        plt.title("Controlled TSO store-buffer litmus")
        plt.ylim(0, 1.05); plt.grid(True, axis="y", alpha=0.3); plt.tight_layout()
        out = os.path.join(HERE, "..", "docs", "img", "tso_litmus.png")
        plt.savefig(out)
        print("wrote", out)
    except Exception as e:
        print("[matplotlib unavailable: %s] ASCII summary:" % e)
        for l, r in zip(labels, rates):
            bar = "#" * int(50 * r)
            print(f"  {l:18s} rate={r:.3f} |{bar}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
