// Reader storm: many readers, then a single writer.
#include "../test_framework.hpp"
#include "../workloads.hpp"

using namespace mesi;

static State st(MesiSystem& s, int core, uint32_t addr) {
    return s.bus().controller(core)->line_state(line_of(addr));
}

TEST("reader storm: only BusRd, ends with all cores Shared") {
    MesiSystem s(4);
    uint32_t A = 0x8000;
    s.bus().memory().write(A, 0xABCD);
    s.bus().register_addr(A);
    reader_storm(s, A);

    // One BusRd per core, no exclusive/upgrade traffic, no invalidations.
    CHECK_EQ(s.perf().bus_rd_count, 4);
    CHECK_EQ(s.perf().bus_rdx_count, 0);
    CHECK_EQ(s.perf().bus_upgr_count, 0);
    for (int c = 0; c < 4; ++c) {
        CHECK(st(s, c, A) == State::S);
        CHECK_EQ(s.read(c, A), 0xABCD);
    }
}

TEST("writer after reader storm invalidates every reader") {
    MesiSystem s(4);
    uint32_t A = 0x9000;
    s.bus().register_addr(A);
    reader_storm(s, A);             // cores 0..3 all Shared
    s.write(0, A, 0x1234);          // core0 S->M via BusUpgr, invalidate 1..3

    CHECK(st(s, 0, A) == State::M);
    for (int c = 1; c < 4; ++c) CHECK(st(s, c, A) == State::I);
    CHECK_EQ(s.perf().bus_upgr_count, 1);
    // cores 1,2,3 each received an invalidation.
    CHECK_EQ(s.perf().inv_count[1] + s.perf().inv_count[2] + s.perf().inv_count[3], 3);
}
