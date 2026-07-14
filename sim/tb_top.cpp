// Verilator testbench. Covers the standalone mesi_fsm transition table, the
// store buffer (bypass / drain / fence), cpu_stub, and the full 4-core system
// (directed transitions, ping-pong, reader storm).
#include <cstdio>
#include <cstdint>
#include <string>
#include "Vmesi_tb_top.h"
#include "verilated.h"
#include "cpu_driver.hpp"

using namespace mesidut;

// Some Verilator runtimes reference this legacy hook at link time.
double sc_time_stamp() { return 0; }

static int g_fail = 0;
static int g_checks = 0;
static void check(bool c, const std::string& msg) {
    ++g_checks;
    if (!c) { std::printf("  [FAIL] %s\n", msg.c_str()); g_fail = 1; }
}
#define CHECK(c, m) check((c), (m))

// state code -> letter
static const char* sname(int s) {
    switch (s) { case ST_I: return "I"; case ST_S: return "S";
                 case ST_E: return "E"; case ST_M: return "M"; } return "?";
}

// ---------------- mesi_fsm unit test ----------------
static void test_fsm(Vmesi_tb_top* top) {
    std::printf("-- mesi_fsm transition table --\n");
    auto set = [&](int cur, int pr, int shared, int snoop) {
        top->fsm_cur_state = cur; top->fsm_pr_req = pr;
        top->fsm_shared_in = shared; top->fsm_snoop_txn = snoop;
        top->eval();
    };
    enum { NONE = 0, RD = 1, WR = 2 };
    enum { B_IDLE = 0, B_RD = 1, B_RDX = 2, B_UPGR = 3 };

    // processor request type
    set(ST_I, RD, 0, B_IDLE); CHECK(top->fsm_req_txn == B_RD && top->fsm_needs_bus, "I+PrRd -> BusRd");
    set(ST_S, RD, 0, B_IDLE); CHECK(top->fsm_req_txn == B_IDLE && !top->fsm_needs_bus, "S+PrRd -> hit");
    set(ST_E, RD, 0, B_IDLE); CHECK(top->fsm_req_txn == B_IDLE, "E+PrRd -> hit");
    set(ST_M, RD, 0, B_IDLE); CHECK(top->fsm_req_txn == B_IDLE, "M+PrRd -> hit");
    set(ST_I, WR, 0, B_IDLE); CHECK(top->fsm_req_txn == B_RDX, "I+PrWr -> BusRdX");
    set(ST_S, WR, 0, B_IDLE); CHECK(top->fsm_req_txn == B_UPGR, "S+PrWr -> BusUpgr");
    set(ST_E, WR, 0, B_IDLE); CHECK(top->fsm_req_txn == B_IDLE, "E+PrWr -> silent");
    set(ST_M, WR, 0, B_IDLE); CHECK(top->fsm_req_txn == B_IDLE, "M+PrWr -> hit");

    // processor next-state
    set(ST_I, RD, 0, B_IDLE); CHECK(top->fsm_pr_next == ST_E, "I+PrRd(noshare) -> E");
    set(ST_I, RD, 1, B_IDLE); CHECK(top->fsm_pr_next == ST_S, "I+PrRd(shared) -> S");
    set(ST_I, WR, 0, B_IDLE); CHECK(top->fsm_pr_next == ST_M, "I+PrWr -> M");
    set(ST_S, WR, 0, B_IDLE); CHECK(top->fsm_pr_next == ST_M, "S+PrWr -> M");
    set(ST_E, WR, 0, B_IDLE); CHECK(top->fsm_pr_next == ST_M, "E+PrWr -> M");

    // snoop transitions
    set(ST_M, NONE, 0, B_RD);   CHECK(top->fsm_snoop_next == ST_S && top->fsm_snoop_wb, "M+BusRd -> S, wb");
    set(ST_E, NONE, 0, B_RD);   CHECK(top->fsm_snoop_next == ST_S && !top->fsm_snoop_wb, "E+BusRd -> S");
    set(ST_S, NONE, 0, B_RD);   CHECK(top->fsm_snoop_next == ST_S, "S+BusRd -> S");
    set(ST_M, NONE, 0, B_RDX);  CHECK(top->fsm_snoop_next == ST_I && top->fsm_snoop_wb, "M+BusRdX -> I, wb");
    set(ST_E, NONE, 0, B_RDX);  CHECK(top->fsm_snoop_next == ST_I, "E+BusRdX -> I");
    set(ST_S, NONE, 0, B_RDX);  CHECK(top->fsm_snoop_next == ST_I, "S+BusRdX -> I");
    set(ST_S, NONE, 0, B_UPGR); CHECK(top->fsm_snoop_next == ST_I, "S+BusUpgr -> I");
}

// ---------------- store_buffer unit test ----------------
static void test_store_buffer(Vmesi_tb_top* top, Driver& drv) {
    std::printf("-- store_buffer (bypass / FIFO drain / fence) --\n");
    auto tick = [&]{ drv.tick(); };
    // push three entries: (0x10,111),(0x20,222),(0x10,333)
    auto push = [&](uint32_t a, uint32_t d) {
        top->sb_st_valid = 1; top->sb_st_addr = a; top->sb_st_data = d;
        top->sb_drain_ready = 0; tick();
    };
    push(0x10, 111); push(0x20, 222); push(0x10, 333);
    top->sb_st_valid = 0; top->eval();
    CHECK(top->sb_count == 3, "store buffer count == 3");

    // bypass: newest match wins
    top->sb_bypass_addr = 0x10; top->eval();
    CHECK(top->sb_bypass_hit && top->sb_bypass_data == 333, "bypass 0x10 -> 333 (newest)");
    top->sb_bypass_addr = 0x20; top->eval();
    CHECK(top->sb_bypass_hit && top->sb_bypass_data == 222, "bypass 0x20 -> 222");
    top->sb_bypass_addr = 0x30; top->eval();
    CHECK(!top->sb_bypass_hit, "bypass 0x30 -> miss");

    top->sb_fence_req = 1;
    top->eval();
    CHECK(!top->sb_fence_ack && top->sb_full,
          "pending fence blocks stores and waits for drain");

    // drain in FIFO order
    top->sb_drain_ready = 1;
    drv.settle();
    CHECK(top->sb_drain_valid && top->sb_drain_addr == 0x10 && top->sb_drain_data == 111,
          "drain[0] = (0x10,111)"); tick();
    drv.settle();
    CHECK(top->sb_drain_addr == 0x20 && top->sb_drain_data == 222, "drain[1] = (0x20,222)"); tick();
    drv.settle();
    CHECK(top->sb_drain_addr == 0x10 && top->sb_drain_data == 333, "drain[2] = (0x10,333)"); tick();
    drv.settle();
    CHECK(top->sb_count == 0 && top->sb_fence_ack, "buffer empty -> fence_ack");
    top->sb_fence_req = 0; top->sb_drain_ready = 0; top->eval();
    CHECK(!top->sb_fence_ack, "fence ack deasserts with fence request");
}

// ---------------- cpu_stub unit test ----------------
static void test_cpu_stub(Vmesi_tb_top* top, Driver& drv) {
    std::printf("-- cpu_stub SEQUENTIAL stimulus --\n");
    top->stub_enable = 1; top->stub_base = 0x1000; top->stub_done = 1;
    drv.settle();
    CHECK(top->stub_req && top->stub_addr == 0x1000 && top->stub_we == 0, "stub step0: 0x1000 read");
    drv.tick();
    drv.settle();
    CHECK(top->stub_addr == 0x1040 && top->stub_we == 1, "stub step1: 0x1040 write");
    drv.tick();
    drv.settle();
    CHECK(top->stub_addr == 0x1080 && top->stub_we == 0, "stub step2: 0x1080 read");
    CHECK(top->stub_issued == 2, "stub issued == 2");
    top->stub_enable = 0; top->stub_done = 0; top->eval();
}

// ---------------- full system coherence tests ----------------
static void test_system(Driver& d) {
    std::printf("-- multi-core coherence (directed) --\n");
    // distinct-set addresses (index = (addr>>6)&63), all tag 0, no set conflicts
    const uint32_t A=0x040, B=0x080, C=0x0C0, D=0x100, E=0x140, F=0x180, G=0x1C0, H=0x200;

    // I->E
    CHECK(d.read(0, A) == 0, "read A returns 0 (fresh)");
    CHECK(d.state(0, A) == ST_E, "core0 A -> E");
    // I->S
    d.read(1, A);
    CHECK(d.state(0, A) == ST_S && d.state(1, A) == ST_S, "A shared S/S");

    // E->M silent
    d.read(0, B);
    CHECK(d.state(0, B) == ST_E, "core0 B -> E");
    uint32_t rdx0 = d.bus_rdx(), upg0 = d.bus_upgr();
    d.write(0, B, 0xBB);
    CHECK(d.state(0, B) == ST_M, "core0 B -> M (silent)");
    CHECK(d.bus_rdx() == rdx0 && d.bus_upgr() == upg0, "E->M issues no bus txn");
    CHECK(d.read(0, B) == 0xBB, "B reads back 0xBB");

    // S->M via BusUpgr
    d.read(0, C); d.read(1, C);
    uint32_t upg1 = d.bus_upgr();
    d.write(0, C, 0xCC);
    CHECK(d.state(0, C) == ST_M && d.state(1, C) == ST_I, "C: core0 M, core1 I");
    CHECK(d.bus_upgr() == upg1 + 1, "S->M issues BusUpgr");

    // I->M via BusRdX
    d.read(1, D);
    uint32_t rdx1 = d.bus_rdx();
    d.write(0, D, 0xDD);
    CHECK(d.state(0, D) == ST_M && d.state(1, D) == ST_I, "D: core0 M, core1 I");
    CHECK(d.bus_rdx() == rdx1 + 1, "I->M issues BusRdX");

    // M->S dirty intervention
    d.write(0, E, 0xEE);
    CHECK(d.state(0, E) == ST_M, "E line: core0 M");
    uint32_t wb0 = d.bus_wb();
    uint32_t got = d.read(1, E);
    CHECK(got == 0xEE, "core1 reads dirty value 0xEE via intervention");
    CHECK(d.state(0, E) == ST_S && d.state(1, E) == ST_S, "E line both S after BusRd");
    CHECK(d.bus_wb() == wb0 + 1, "dirty intervention triggers writeback");

    // M->I dirty intervention on BusRdX
    d.write(0, F, 0x11);
    d.write(1, F, 0x22);
    CHECK(d.state(0, F) == ST_I && d.state(1, F) == ST_M, "F: core0 I, core1 M");
    CHECK(d.read(1, F) == 0x22, "core1 F reads 0x22");

    // Ping-pong perf
    uint32_t rdx_before = d.bus_rdx();
    const int iters = 20;
    for (int i = 0; i < iters; ++i) { d.write(0, G, 2*i); d.write(1, G, 2*i+1); }
    CHECK(d.bus_rdx() - rdx_before == (uint32_t)(2*iters), "ping-pong: each write a BusRdX");
    CHECK(d.read(0, G) == (uint32_t)(2*iters - 1), "ping-pong final value");

    // Reader storm
    uint32_t rd_before = d.bus_rd(), rdx_b2 = d.bus_rdx();
    for (int c = 0; c < 4; ++c) d.read(c, H);
    CHECK(d.bus_rd() - rd_before == 4, "reader storm: 4 BusRd");
    CHECK(d.bus_rdx() - rdx_b2 == 0, "reader storm: no BusRdX");
    for (int c = 0; c < 4; ++c) CHECK(d.state(c, H) == ST_S, std::string("reader core ") + std::to_string(c) + " -> S");
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    Vmesi_tb_top* top = new Vmesi_tb_top;
    Driver drv(top);

    std::printf("==== MESI RTL testbench (Verilator) ====\n");
    drv.reset();

    test_fsm(top);
    test_store_buffer(top, drv);
    test_cpu_stub(top, drv);
    drv.reset(); // clean state for the system tests
    test_system(drv);

    top->final();
    delete top;

    std::printf("---- RTL: %d checks, %s ----\n", g_checks, g_fail ? "FAILURES" : "all passed");
    if (g_fail) { std::printf("RTL TESTBENCH FAILED\n"); return 1; }
    std::printf("RTL TESTBENCH PASSED\n");
    return 0;
}
