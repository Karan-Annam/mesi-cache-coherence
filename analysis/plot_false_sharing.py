#!/usr/bin/env python3
"""Plot false-sharing overhead: bus transactions vs. byte offset between the two
cores' target words. Shows the cliff at the 64-byte cache-line boundary.

Reads analysis/false_sharing.csv (produced by model_main). If matplotlib is not
installed, prints an ASCII summary instead so the script always succeeds.
"""
import csv, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
CSV = os.path.join(HERE, "false_sharing.csv")


def load(path):
    rows = []
    with open(path) as f:
        for r in csv.DictReader(f):
            rows.append({k: int(float(v)) for k, v in r.items()})
    return rows


def main():
    if not os.path.exists(CSV):
        print("no false_sharing.csv (run model_main first)")
        return 0
    rows = load(CSV)
    offs = [r["offset_bytes"] for r in rows]
    txns = [r["total_bus_txns"] for r in rows]
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        plt.figure(figsize=(7, 4))
        plt.plot(offs, txns, marker="o")
        plt.axvline(64, color="r", linestyle="--", label="64B cache line")
        plt.xlabel("offset between core0/core1 words (bytes)")
        plt.ylabel("total bus transactions")
        plt.title("False sharing: the 64-byte cliff")
        plt.legend(); plt.grid(True, alpha=0.3); plt.tight_layout()
        out = os.path.join(HERE, "..", "docs", "img", "false_sharing.png")
        plt.savefig(out)
        print("wrote", out)
    except Exception as e:
        print("[matplotlib unavailable: %s] ASCII summary:" % e)
        mx = max(txns) or 1
        for o, t in zip(offs, txns):
            bar = "#" * int(50 * t / mx)
            print(f"  off={o:4d}B  txns={t:5d} |{bar}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
