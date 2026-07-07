// Per-core handle onto the coherent memory. A primitive running on core `c`
// constructs Dut{sys, c} and does all its reads/writes/CAS/fences through it,
// so every access goes through the MESI model and shows up in the counters.
#pragma once
#include <cstdint>
#include "../model/mesi_system.hpp"

namespace mesi {

struct Dut {
    MesiSystem* sys;
    int core;

    uint32_t read(uint32_t addr) const { return sys->read(core, addr); }
    void write(uint32_t addr, uint32_t v) const { sys->write(core, addr, v); }

    // compare-and-swap; returns true if the swap happened.
    bool cas(uint32_t addr, uint32_t expected, uint32_t new_val) const {
        return sys->cas(core, addr, expected, new_val).success;
    }
    CasResult cas_full(uint32_t addr, uint32_t expected, uint32_t new_val) const {
        return sys->cas(core, addr, expected, new_val);
    }

    // fetch-and-add; returns the previous value.
    uint32_t faa(uint32_t addr, uint32_t delta) const {
        return sys->fetch_and_add(core, addr, delta);
    }

    void fence() const { sys->fence(core); }
};

} // namespace mesi
