// Drives the Verilated system from C++. Per-core read/write API: set the CPU
// port, clock until the cache says done, return the loaded value. Also reads
// back per-core MESI state (debug port) and the perf counters.
#pragma once
#include <cstdint>
#include <stdexcept>
#include "Vmesi_tb_top.h"
#include "verilated.h"

namespace mesidut {

// MESI state codes (match mesi_pkg / the C++ model).
enum { ST_I = 0, ST_S = 1, ST_E = 2, ST_M = 3 };

class Driver {
public:
    explicit Driver(Vmesi_tb_top* top) : top_(top) {}

    void reset() {
        top_->rst_n = 0;
        top_->cpu_req = 0; top_->cpu_we = 0;
        top_->dbg_addr = 0;
        // zero the FSM/SB/stub unit-test inputs
        top_->fsm_cur_state = 0; top_->fsm_pr_req = 0;
        top_->fsm_shared_in = 0; top_->fsm_snoop_txn = 0;
        top_->sb_st_valid = 0; top_->sb_drain_ready = 0; top_->sb_fence_req = 0;
        top_->sb_bypass_addr = 0; top_->sb_st_addr = 0; top_->sb_st_data = 0;
        top_->stub_enable = 0; top_->stub_base = 0; top_->stub_done = 0;
        for (int i = 0; i < 6; ++i) tick();
        top_->rst_n = 1;
        tick();
    }

    // One full clock cycle (posedge commit), inputs already set.
    void tick() {
        top_->clk = 0; top_->eval();
        top_->clk = 1; top_->eval();
    }

    // Evaluate combinational logic with clk low (so *_done / rdata are valid for
    // the transaction that will commit on the next posedge).
    void settle() { top_->clk = 0; top_->eval(); }

    static void set_bit(uint8_t& v, int i, bool b) {
        if (b) v |= (1u << i); else v &= ~(1u << i);
    }
    static bool get_bit(uint8_t v, int i) { return (v >> i) & 1u; }

    // Issue one op on `core`; block (stepping the clock) until it completes.
    uint32_t op(int core, bool we, uint32_t addr, uint32_t wdata) {
        uint8_t req = top_->cpu_req, wev = top_->cpu_we;
        set_bit(req, core, true);
        set_bit(wev, core, we);
        top_->cpu_req = req; top_->cpu_we = wev;
        top_->cpu_addr[core] = addr;
        top_->cpu_wdata[core] = wdata;

        uint32_t rd = 0;
        const int MAX = 1000;
        int c = 0;
        for (; c < MAX; ++c) {
            settle(); // comb settles with current registers
            bool done = get_bit(top_->cpu_done, core);
            rd = top_->cpu_rdata[core];
            top_->clk = 1; top_->eval(); // posedge commit
            if (done) break;
        }
        if (c == MAX) throw std::runtime_error("CPU op did not complete (deadlock)");

        // Deassert this core's request.
        req = top_->cpu_req; set_bit(req, core, false);
        top_->cpu_req = req;
        settle();
        return rd;
    }

    uint32_t read(int core, uint32_t addr) { return op(core, false, addr, 0); }
    void     write(int core, uint32_t addr, uint32_t v) { op(core, true, addr, v); }

    // Per-core MESI state for `addr` via the debug port.
    int state(int core, uint32_t addr) {
        top_->dbg_addr = addr;
        top_->eval();
        return (top_->dbg_state >> (2 * core)) & 0x3;
    }
    uint32_t dbg_data(int core, uint32_t addr) {
        top_->dbg_addr = addr;
        top_->eval();
        return top_->dbg_data[core];
    }

    // Performance counters.
    uint32_t bus_rd()   const { return top_->bus_rd_count; }
    uint32_t bus_rdx()  const { return top_->bus_rdx_count; }
    uint32_t bus_upgr() const { return top_->bus_upgr_count; }
    uint32_t bus_wb()   const { return top_->bus_wb_count; }
    uint32_t l1_hit(int core)  const { return top_->l1_hit[core]; }
    uint32_t l1_miss(int core) const { return top_->l1_miss[core]; }
    uint32_t inv_count(int core) const { return top_->inv_count[core]; }

private:
    Vmesi_tb_top* top_;
};

} // namespace mesidut
