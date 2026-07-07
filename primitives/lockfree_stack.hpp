// Treiber stack (lock-free push/pop via CAS on the head pointer).
#pragma once
#include "dut.hpp"

namespace mesi {

// All state lives in coherent memory so the CAS goes through the MESI model:
//   top_addr            : head pointer (node address, 0 = empty)
//   alloc_addr          : monotonic node allocator counter
//   node_base + idx*8   : node idx -> [ next @ +0, val @ +4 ]
// Node index 0 is reserved as the null sentinel, so allocation starts at 1.
// Allocation is monotonic (popped nodes are never reused), which sidesteps the
// ABA problem for the concurrent push/pop correctness test.
struct LockFreeStack {
    uint32_t top_addr;
    uint32_t alloc_addr;
    uint32_t node_base;

    void init(const Dut& d) const {
        d.write(top_addr, 0);
        d.write(alloc_addr, 0);
    }

    void push(const Dut& d, uint32_t value) const {
        uint32_t idx = d.faa(alloc_addr, 1) + 1;       // fresh node
        uint32_t naddr = node_base + idx * 8;
        d.write(naddr + 4, value);
        for (;;) {
            uint32_t old_top = d.read(top_addr);
            d.write(naddr, old_top);                    // node->next = top
            if (d.cas(top_addr, old_top, naddr)) return;
        }
    }

    // Returns true and sets `out` if a value was popped; false if empty.
    bool pop(const Dut& d, uint32_t& out) const {
        for (;;) {
            uint32_t old_top = d.read(top_addr);
            if (old_top == 0) return false;             // empty
            uint32_t next = d.read(old_top);            // node->next
            if (d.cas(top_addr, old_top, next)) {
                out = d.read(old_top + 4);              // node->val
                return true;
            }
        }
    }
};

} // namespace mesi
