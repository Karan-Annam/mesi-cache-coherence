// Ping-pong: the coherence worst case.
#include "../test_framework.hpp"
#include "../workloads.hpp"

using namespace mesi;

TEST("ping-pong: every write is a BusRdX (near-100% invalidation rate)") {
    MesiSystem s(2);
    uint32_t A = 0x1000;
    const int iters = 50;
    ping_pong(s, A, iters);

    // 2 writes per iteration, each starting from Invalid -> BusRdX.
    CHECK_EQ(s.perf().bus_rdx_count, (long long)(2 * iters));
    // No upgrades (never written from S in ping-pong; always from I).
    CHECK_EQ(s.perf().bus_upgr_count, 0);
    // Each write after the very first triggers a dirty writeback by the loser.
    CHECK(s.perf().bus_wb_count >= (uint64_t)(2 * iters - 1));
    // Invalidations roughly one per write (minus the first write).
    uint64_t total_inv = s.perf().inv_count[0] + s.perf().inv_count[1];
    CHECK(total_inv >= (uint64_t)(2 * iters - 1));
}

TEST("ping-pong: final value is the last write and memory is coherent") {
    MesiSystem s(2);
    uint32_t A = 0x2000;
    ping_pong(s, A, 10);
    // Last write was core1 with value 2*9+1 = 19.
    CHECK_EQ(s.read(0, A), 19);
}
