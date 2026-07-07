// Test-and-set spinlock.
#pragma once
#include "dut.hpp"

namespace mesi {

// A TAS spinlock living at a single coherent address (`flag_addr`).
// 0 = unlocked, 1 = locked. lock() spins on a CAS; while the lock looks held it
// spins with plain reads (test-and-test-and-set) to avoid pointless BusRdX.
struct TASLock {
    uint32_t flag_addr;

    void lock(const Dut& d) const {
        for (;;) {
            if (d.cas(flag_addr, 0, 1)) return;        // acquired
            while (d.read(flag_addr) == 1) { /* spin */ } // back off on shared reads
        }
    }
    void unlock(const Dut& d) const { d.write(flag_addr, 0); }
};

} // namespace mesi
