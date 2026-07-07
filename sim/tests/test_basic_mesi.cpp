// Directed MESI state-transition tests.
#include "../test_framework.hpp"
#include "../../model/mesi_system.hpp"

using namespace mesi;

static State st(MesiSystem& s, int core, uint32_t addr) {
    return s.bus().controller(core)->line_state(line_of(addr));
}

TEST("I->E on read miss with no sharers") {
    MesiSystem s(2);
    uint32_t A = 0x1000;
    s.write(0, A, 0); // seed memory via core0 then evict by sharing? simpler: direct
    // Reset: use a fresh system so core1 reads a line nobody else holds.
    MesiSystem s2(2);
    s2.bus().memory().write(0x2000, 42);
    s2.bus().register_addr(0x2000);
    uint32_t v = s2.read(1, 0x2000);
    CHECK_EQ(v, 42);
    CHECK(st(s2, 1, 0x2000) == State::E); // exclusive: nobody else has it
}

TEST("I->S on read miss when another cache already holds the line") {
    MesiSystem s(3);
    s.bus().memory().write(0x3000, 7);
    s.bus().register_addr(0x3000);
    CHECK_EQ(s.read(0, 0x3000), 7);
    CHECK(st(s, 0, 0x3000) == State::E);
    CHECK_EQ(s.read(1, 0x3000), 7);
    // Now both share.
    CHECK(st(s, 0, 0x3000) == State::S);
    CHECK(st(s, 1, 0x3000) == State::S);
}

TEST("E->M silent upgrade on write (no bus transaction)") {
    MesiSystem s(2);
    uint32_t A = 0x4000;
    s.bus().memory().write(A, 1);
    s.bus().register_addr(A);
    s.read(0, A); // -> E
    CHECK(st(s, 0, A) == State::E);
    uint64_t before = s.perf().total_bus_txns();
    s.write(0, A, 99); // E->M silent
    CHECK(st(s, 0, A) == State::M);
    CHECK_EQ(s.perf().total_bus_txns(), (long long)before); // no new bus txn
    CHECK_EQ(s.read(0, A), 99);
}

TEST("S->M via BusUpgr invalidates other sharer") {
    MesiSystem s(2);
    uint32_t A = 0x5000;
    s.bus().memory().write(A, 5);
    s.bus().register_addr(A);
    s.read(0, A);
    s.read(1, A); // both S
    CHECK(st(s, 0, A) == State::S);
    CHECK(st(s, 1, A) == State::S);
    uint64_t up_before = s.perf().bus_upgr_count;
    s.write(0, A, 123); // S->M, BusUpgr, core1 invalidated
    CHECK(st(s, 0, A) == State::M);
    CHECK(st(s, 1, A) == State::I);
    CHECK_EQ(s.perf().bus_upgr_count, (long long)up_before + 1);
    CHECK(s.perf().inv_count[1] >= 1);
}

TEST("I->M write miss issues BusRdX and invalidates sharers") {
    MesiSystem s(2);
    uint32_t A = 0x6000;
    s.bus().memory().write(A, 0);
    s.bus().register_addr(A);
    s.read(1, A); // core1 has E
    CHECK(st(s, 1, A) == State::E);
    uint64_t rdx_before = s.perf().bus_rdx_count;
    s.write(0, A, 77); // core0 I->M, BusRdX
    CHECK(st(s, 0, A) == State::M);
    CHECK(st(s, 1, A) == State::I);
    CHECK_EQ(s.perf().bus_rdx_count, (long long)rdx_before + 1);
    CHECK_EQ(s.read(0, A), 77);
}

TEST("M->S dirty intervention: BusRd snoop forces writeback") {
    MesiSystem s(2);
    uint32_t A = 0x7000;
    s.bus().register_addr(A);
    s.write(0, A, 555); // core0: I->M (BusRdX), memory stale
    CHECK(st(s, 0, A) == State::M);
    uint64_t wb_before = s.perf().bus_wb_count;
    uint32_t v = s.read(1, A); // BusRd: core0 writes back, both go S
    CHECK_EQ(v, 555);
    CHECK(st(s, 0, A) == State::S);
    CHECK(st(s, 1, A) == State::S);
    CHECK_EQ(s.perf().bus_wb_count, (long long)wb_before + 1);
    CHECK_EQ(s.bus().memory().read(A), 555); // memory now current
}

TEST("M->I dirty intervention on BusRdX") {
    MesiSystem s(2);
    uint32_t A = 0x8000;
    s.bus().register_addr(A);
    s.write(0, A, 321);          // core0 M
    s.write(1, A, 654);          // core1 BusRdX -> core0 writes back + invalidates
    CHECK(st(s, 0, A) == State::I);
    CHECK(st(s, 1, A) == State::M);
    CHECK_EQ(s.read(1, A), 654);
}

TEST("read-your-own-writes consistency across many ops") {
    MesiSystem s(4);
    for (uint32_t a = 0x9000; a < 0x9000 + 16 * 4; a += 4) s.bus().register_addr(a);
    for (int core = 0; core < 4; ++core)
        for (uint32_t a = 0x9000; a < 0x9000 + 16 * 4; a += 4)
            s.write(core, a, core * 100 + a);
    // Last writer wins per address; re-read should reflect the most recent.
    for (uint32_t a = 0x9000; a < 0x9000 + 16 * 4; a += 4)
        CHECK_EQ(s.read(0, a), 3 * 100 + a); // core 3 wrote last
}
