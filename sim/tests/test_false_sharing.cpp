// False sharing, padded vs unpadded.
#include "../test_framework.hpp"
#include "../workloads.hpp"

using namespace mesi;

TEST("unpadded false sharing generates heavy inter-core bus traffic") {
    MesiSystem s(2);
    // Two words on the SAME 64-byte line (offset 0 and 4).
    uint32_t a0 = 0x4000;
    uint32_t a1 = 0x4004;
    CHECK_EQ(line_of(a0), line_of(a1)); // confirm same line
    const int iters = 40;
    false_sharing(s, a0, a1, iters);

    // Each write must reacquire the line exclusively -> a BusRdX every time.
    CHECK_EQ(s.perf().bus_rdx_count, (long long)(2 * iters));
    uint64_t inv = s.perf().inv_count[0] + s.perf().inv_count[1];
    CHECK(inv >= (uint64_t)(2 * iters - 1));
}

TEST("padded layout eliminates inter-core coherence traffic") {
    MesiSystem s(2);
    // Two words on DIFFERENT 64-byte lines.
    uint32_t a0 = 0x5000;
    uint32_t a1 = 0x5040;
    CHECK(line_of(a0) != line_of(a1));
    const int iters = 40;
    false_sharing(s, a0, a1, iters);

    // Each core touches its own line: 1 BusRdX to acquire, then M-state hits.
    CHECK_EQ(s.perf().bus_rdx_count, 2); // one acquisition per core, total
    CHECK_EQ(s.perf().bus_upgr_count, 0);
    CHECK_EQ(s.perf().bus_wb_count, 0);  // no snoops force writebacks
    CHECK_EQ(s.perf().inv_count[0] + s.perf().inv_count[1], 0); // zero invalidations
}

TEST("false sharing overhead ratio is large (the 64-byte cliff)") {
    MesiSystem bad(2), good(2);
    false_sharing(bad, 0x6000, 0x6004, 100);   // same line
    false_sharing(good, 0x7000, 0x7040, 100);  // padded
    uint64_t bad_txn = bad.perf().total_bus_txns();
    uint64_t good_txn = good.perf().total_bus_txns();
    CHECK(bad_txn > good_txn * 20); // dramatic overhead from false sharing
}
