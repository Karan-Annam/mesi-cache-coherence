// Hazard pointers for safe memory reclamation.
#pragma once
#include <vector>
#include <cstdint>
#include "dut.hpp"

namespace mesi {

// One hazard slot per thread, all in coherent memory:
//   hazard_base + tid*4 : the pointer thread `tid` is currently protecting
// A reader publishes the pointer it is about to dereference into its slot and
// re-validates; a thread retiring an object scans every slot and only frees the
// object when no slot references it. Hazard slots are written by one core and
// read by all others — exactly the cross-core pattern that stresses MESI.
struct HazardDomain {
    uint32_t hazard_base;
    int n_threads;

    // Acquire-and-protect: load *ptr_addr, publish it, re-validate. Returns the
    // protected pointer (0 means null).
    uint32_t protect(const Dut& d, int tid, uint32_t ptr_addr) const {
        for (;;) {
            uint32_t p = d.read(ptr_addr);
            d.write(hazard_base + 4u * tid, p);
            d.fence();
            if (d.read(ptr_addr) == p) return p; // stable: safe to use
        }
    }

    void release(const Dut& d, int tid) const {
        d.write(hazard_base + 4u * tid, 0);
    }

    // Is `ptr` currently protected by any thread?
    bool is_hazarded(const Dut& d, uint32_t ptr) const {
        for (int t = 0; t < n_threads; ++t)
            if (d.read(hazard_base + 4u * t) == ptr) return true;
        return false;
    }
};

} // namespace mesi
