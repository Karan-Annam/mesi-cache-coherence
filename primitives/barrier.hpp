// Centralized sense-reversing barrier.
#pragma once
#include "dut.hpp"

namespace mesi {

// Coherent words:
//   count_addr : number of threads not yet arrived (init = n)
//   sense_addr : global sense flag (flips each barrier episode)
// Each thread keeps a private `local_sense` it flips every episode. The last
// arriver resets the counter and flips the global sense, releasing the rest.
struct Barrier {
    uint32_t count_addr;
    uint32_t sense_addr;
    int n;

    void init(const Dut& d) const {
        d.write(count_addr, (uint32_t)n);
        d.write(sense_addr, 0);
    }

    // local_sense is per-thread state (passed by reference, flipped here).
    void wait(const Dut& d, uint32_t& local_sense) const {
        local_sense ^= 1u;
        uint32_t prev = d.faa(count_addr, (uint32_t)-1); // atomically decrement
        if (prev == 1) {
            // last to arrive: reset and release.
            d.write(count_addr, (uint32_t)n);
            d.fence();
            d.write(sense_addr, local_sense);
        } else {
            while (d.read(sense_addr) != local_sense) { /* spin */ }
        }
    }
};

} // namespace mesi
