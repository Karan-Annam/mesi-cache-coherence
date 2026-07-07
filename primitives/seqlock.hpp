// Seqlock — readers retry instead of blocking, good for read-heavy data.
#pragma once
#include <vector>
#include "dut.hpp"

namespace mesi {

// Layout in coherent memory:
//   seq_addr            : sequence counter (even = stable, odd = writer active)
//   data_base + 4*i     : data word i  (i in [0, n_words))
//
// Readers take no lock: they snapshot the sequence, copy the data, and re-check
// the sequence. If it changed (or was odd), they retry. Writers bump the
// sequence odd, write data, bump it even, with fences so the ordering holds
// even under the TSO store buffer.
struct Seqlock {
    uint32_t seq_addr;
    uint32_t data_base;
    int n_words;

    void write(const Dut& d, const std::vector<uint32_t>& vals) const {
        uint32_t s = d.read(seq_addr);
        d.write(seq_addr, s + 1); // odd: writer active
        d.fence();
        for (int i = 0; i < n_words; ++i)
            d.write(data_base + 4 * i, vals[i]);
        d.fence();
        d.write(seq_addr, s + 2); // even: stable again
    }

    // Returns true and fills `out` if a consistent snapshot was read.
    bool try_read(const Dut& d, std::vector<uint32_t>& out) const {
        uint32_t s1 = d.read(seq_addr);
        if (s1 & 1u) return false; // writer active
        d.fence();
        out.resize(n_words);
        for (int i = 0; i < n_words; ++i)
            out[i] = d.read(data_base + 4 * i);
        d.fence();
        uint32_t s2 = d.read(seq_addr);
        return s1 == s2; // no writer interrupted us
    }

    void read_retry(const Dut& d, std::vector<uint32_t>& out) const {
        while (!try_read(d, out)) { /* retry */ }
    }
};

} // namespace mesi
