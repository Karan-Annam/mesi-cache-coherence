// Demo runner: drives the workloads, the SC checker, and race injection, and
// writes the CSVs the analysis scripts plot. Non-zero exit if a check fails.
#include <cstdio>
#include <cstdint>
#include <vector>
#include <thread>
#include <random>
#include <fstream>
#include <algorithm>
#include <atomic>
#include "mesi_system.hpp"
#include "sc_checker.hpp"
#include "tso.hpp"
#include "../sim/workloads.hpp"
#include "../primitives/dut.hpp"
#include "../primitives/spinlock.hpp"

using namespace mesi;

static int g_fail = 0;
static void expect(bool c, const char* what) {
    if (!c) { std::printf("  [model_main] CHECK FAILED: %s\n", what); g_fail = 1; }
}

// Run a randomized read/write workload across `n` cores using real threads,
// with tracing on, then verify the produced trace is sequentially consistent.
static void sc_stress(int n, int ops_per_core, uint64_t seed, bool race) {
    MesiSystem s(n);
    s.enable_tracing(true);
    if (race) s.enable_race_injection(seed ^ 0x9E3779B97F4A7C15ull);

    // Pre-register a small address space so lines are known to the bus.
    const int NADDR = 8;
    std::vector<uint32_t> addrs;
    for (int i = 0; i < NADDR; ++i) { uint32_t a = 0x100 + 4u * i; addrs.push_back(a); s.bus().register_addr(a); }

    auto worker = [&](int core) {
        std::mt19937_64 rng(seed + core * 1000 + 7);
        for (int i = 0; i < ops_per_core; ++i) {
            uint32_t a = addrs[rng() % NADDR];
            if (rng() & 1) s.write(core, a, (uint32_t)((core << 16) | i));
            else (void)s.read(core, a);
        }
    };
    std::vector<std::thread> ts;
    for (int c = 0; c < n; ++c) ts.emplace_back(worker, c);
    for (auto& t : ts) t.join();

    SCResult r = check_serial_order(s.trace());
    expect(r.ok, r.detail.c_str());
}

// Emit the false-sharing cliff: total bus transactions vs. the byte offset
// between the two cores' target words. Below 64 bytes (same line) traffic is
// high; at/above 64 bytes it collapses.
static void emit_false_sharing_csv() {
    std::ofstream f("analysis/false_sharing.csv");
    f << "offset_bytes,total_bus_txns,bus_rdx,inv_total\n";
    for (uint32_t off = 4; off <= 128; off += 4) {
        MesiSystem s(2);
        uint32_t a0 = 0x4000;
        uint32_t a1 = 0x4000 + off;
        false_sharing(s, a0, a1, 100);
        uint64_t inv = s.perf().inv_count[0] + s.perf().inv_count[1];
        f << off << "," << s.perf().total_bus_txns() << ","
          << s.perf().bus_rdx_count << "," << inv << "\n";
    }
    std::printf("  wrote analysis/false_sharing.csv\n");
}

// Spinlock bus traffic vs. number of competing cores.
static void emit_lock_scalability_csv() {
    std::ofstream f("analysis/lock_scalability.csv");
    f << "cores,acquisitions_per_trial,trials,median_bus_txns_per_acq,"
         "min_bus_txns_per_acq,max_bus_txns_per_acq,median_total_bus_txns\n";
    constexpr int K = 500;
    constexpr int TRIALS = 7;
    for (int n = 1; n <= 4; ++n) {
        std::vector<uint64_t> totals;
        for (int trial = 0; trial < TRIALS; ++trial) {
            MesiSystem s(n);
            uint32_t LOCK = 0x100, CTR = 0x200;
            s.bus().register_addr(LOCK); s.bus().register_addr(CTR);
            s.write(0, LOCK, 0); s.write(0, CTR, 0);
            TASLock lk{LOCK};
            std::atomic<int> ready{0};
            std::atomic<bool> go{false};
            auto worker = [&](int core) {
                Dut d{&s, core};
                ready.fetch_add(1, std::memory_order_release);
                while (!go.load(std::memory_order_acquire)) std::this_thread::yield();
                for (int i = 0; i < K; ++i) {
                    lk.lock(d);
                    uint32_t v = d.read(CTR); d.write(CTR, v + 1);
                    lk.unlock(d);
                }
            };
            std::vector<std::thread> ts;
            for (int c = 0; c < n; ++c) ts.emplace_back(worker, c);
            while (ready.load(std::memory_order_acquire) < n) std::this_thread::yield();
            go.store(true, std::memory_order_release);
            for (auto& t : ts) t.join();
            expect(s.read(0, CTR) == (uint32_t)(n * K),
                   "lock scalability counter exact");
            totals.push_back(s.perf().total_bus_txns());
        }
        std::sort(totals.begin(), totals.end());
        const double denom = double(n * K);
        const uint64_t median = totals[TRIALS / 2];
        f << n << "," << n * K << "," << TRIALS << ","
          << median / denom << "," << totals.front() / denom << ","
          << totals.back() / denom << "," << median << "\n";
    }
    std::printf("  wrote analysis/lock_scalability.csv\n");
}

// Dekker/SB litmus violation rate vs. store-buffer depth and fence on/off.
static void emit_tso_litmus_csv() {
    std::ofstream f("analysis/tso_litmus.csv");
    f << "fenced,trials,violations,violation_rate\n";
    const int trials = 500;
    for (int fenced = 0; fenced <= 1; ++fenced) {
        MesiSystem s(2);
        uint32_t F0 = 0x2000, F1 = 0x2040;
        s.bus().register_addr(F0); s.bus().register_addr(F1);
        long viol = 0;
        for (int t = 0; t < trials; ++t) {
            s.write(0, F0, 0); s.write(0, F1, 0);
            StoreBuffer sb0(&s, 0), sb1(&s, 1);
            TSODut d0{&s, &sb0, 0}, d1{&s, &sb1, 1};
            uint32_t r0 = 1, r1 = 1;
            std::thread th0([&]{ d0.store(F0,1); if (fenced) d0.mfence(); r0 = d0.load(F1); });
            std::thread th1([&]{ d1.store(F1,1); if (fenced) d1.mfence(); r1 = d1.load(F0); });
            th0.join(); th1.join();
            if (r0 == 0 && r1 == 0) ++viol;
        }
        f << fenced << "," << trials << "," << viol << ","
          << ((double)viol / trials) << "\n";
        if (fenced) expect(viol == 0, "fenced Dekker has zero violations");
        else expect(viol > 0, "unfenced Dekker exhibits violations");
    }
    std::printf("  wrote analysis/tso_litmus.csv\n");
}

int main() {
    std::printf("==== MESI behavioral model demo ====\n");

    // 1) Ping-pong summary.
    {
        MesiSystem s(2);
        ping_pong(s, 0x1000, 100);
        std::printf("[ping-pong x100]\n%s\n", s.perf().to_string().c_str());
        expect(s.perf().bus_rdx_count == 200, "ping-pong BusRdX == 200");
    }

    // 2) Reader storm summary.
    {
        MesiSystem s(4);
        s.bus().register_addr(0x8000);
        reader_storm(s, 0x8000);
        std::printf("[reader-storm 4 cores] bus_rd=%llu bus_rdx=%llu\n",
                    (unsigned long long)s.perf().bus_rd_count,
                    (unsigned long long)s.perf().bus_rdx_count);
        expect(s.perf().bus_rd_count == 4 && s.perf().bus_rdx_count == 0,
               "reader storm: 4 BusRd, 0 BusRdX");
    }

    // 3) Sequential consistency under concurrency (with and without race inject).
    std::printf("[SC checker] running concurrent stress (this exercises real threads)...\n");
    for (uint64_t seed = 1; seed <= 25; ++seed) {
        sc_stress(4, 200, seed, /*race=*/false);
        sc_stress(4, 200, seed, /*race=*/true);
    }
    std::printf("  SC verified across 50 concurrent runs (with & without race injection)\n");

    // 4) Analysis data.
    emit_false_sharing_csv();
    emit_lock_scalability_csv();
    emit_tso_litmus_csv();

    if (g_fail) { std::printf("model_main: FAILURES PRESENT\n"); return 1; }
    std::printf("model_main: all internal checks passed\n");
    return 0;
}
