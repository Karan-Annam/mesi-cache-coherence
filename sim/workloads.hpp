// Workload library: coherence stimulus patterns measured by the counters.
// These run as a single deterministic interleaving — they're about coherence
// traffic, not races (the concurrency-sensitive tests use real threads).
#pragma once
#include <cstdint>
#include <vector>
#include "../model/mesi_system.hpp"

namespace mesi {

// Ping-pong: two cores alternately write the same line. Worst case for MESI —
// every write invalidates the other core's copy (continuous M<->I ping-pong).
inline void ping_pong(MesiSystem& s, uint32_t addr, int iters) {
    for (int i = 0; i < iters; ++i) {
        s.write(0, addr, (uint32_t)(2 * i));
        s.write(1, addr, (uint32_t)(2 * i + 1));
    }
}

// False sharing: core0 hammers word a0, core1 hammers word a1. If a0 and a1 are
// on the same 64-byte line, every write invalidates the other core even though
// the two words are logically independent.
inline void false_sharing(MesiSystem& s, uint32_t a0, uint32_t a1, int iters) {
    for (int i = 0; i < iters; ++i) {
        s.write(0, a0, (uint32_t)i);
        s.write(1, a1, (uint32_t)i);
    }
}

// Producer-consumer: core 0 fills a data buffer then sets a flag; core 1 spins
// on the flag, then reads the buffer. Exercises M->S downgrades and S sharing.
inline void producer_consumer(MesiSystem& s, uint32_t buf_base, int words,
                              uint32_t flag_addr, std::vector<uint32_t>& out) {
    // Producer.
    for (int i = 0; i < words; ++i)
        s.write(0, buf_base + 4 * i, (uint32_t)(0xC0DE0000 + i));
    s.write(0, flag_addr, 1);
    // Consumer: read flag (will be 1), then read buffer.
    (void)s.read(1, flag_addr);
    out.clear();
    for (int i = 0; i < words; ++i)
        out.push_back(s.read(1, buf_base + 4 * i));
}

// Reader storm: every core reads the same address. First reader gets E, the
// rest go S; no BusRdX should occur.
inline void reader_storm(MesiSystem& s, uint32_t addr) {
    for (int c = 0; c < s.n_cores(); ++c)
        (void)s.read(c, addr);
}

} // namespace mesi
