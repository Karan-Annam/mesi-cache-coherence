// TSO store-buffer memory model. A per-core buffer sits between the CPU and
// coherent memory: stores park there (invisible to other cores), the owning
// core's loads check the buffer first, and MFENCE drains it fully.
//
// Drain order:
//   FIFO     — program order preserved (faithful x86-TSO). store->store ordering
//              holds, so message-passing is correctly ordered even without a
//              fence; only store->load can be reordered (the SB/Dekker litmus).
//   RELAXED  — drain may reorder stores (models a weaker store buffer). Used to
//              exhibit a message-passing violation that a fence then repairs.
#pragma once
#include <cstdint>
#include <deque>
#include <vector>
#include <algorithm>
#include <random>
#include "mesi_system.hpp"

namespace mesi {

enum class DrainMode { FIFO, RELAXED };

struct SBEntry { uint32_t addr; uint32_t val; };

// One core's store buffer. Owner-thread-only (no internal locking): all access
// is from the core that owns it.
class StoreBuffer {
public:
    StoreBuffer(MesiSystem* sys, int core, int depth = 8,
                DrainMode mode = DrainMode::FIFO, uint64_t seed = 0)
        : sys_(sys), core_(core), depth_(depth), mode_(mode), rng_(seed) {}

    int depth() const { return depth_; }
    size_t size() const { return buf_.size(); }
    bool empty() const { return buf_.empty(); }

    // CPU store: enqueue. If full, drain the oldest entry (FIFO) to make room.
    void store(uint32_t addr, uint32_t val) {
        // Coalesce: keep only the newest value per address for bypass clarity,
        // but preserve ordering by appending (older duplicate stays for drain).
        buf_.push_back({addr, val});
        if ((int)buf_.size() > depth_) drain_one();
    }

    // CPU load: bypass-read the buffer (newest matching entry) else coherent mem.
    uint32_t load(uint32_t addr) {
        for (auto it = buf_.rbegin(); it != buf_.rend(); ++it)
            if (it->addr == addr) { hits_++; return it->val; }
        return sys_->read(core_, addr);
    }

    // Drain the oldest entry into coherent memory.
    void drain_one() {
        if (buf_.empty()) return;
        size_t idx = 0;
        if (mode_ == DrainMode::RELAXED && buf_.size() > 1)
            idx = rng_() % buf_.size(); // out-of-order drain
        SBEntry e = buf_[idx];
        buf_.erase(buf_.begin() + idx);
        sys_->write(core_, e.addr, e.val);
        drained_++;
    }

    // MFENCE: drain everything (always in FIFO order, regardless of mode — a
    // fence forces full ordered propagation) and count the drain cost.
    void mfence() {
        while (!buf_.empty()) {
            SBEntry e = buf_.front();
            buf_.pop_front();
            sys_->write(core_, e.addr, e.val);
            drained_++;
            sys_->perf().sb_drain_cycles[core_]++;
        }
    }

    // Drain all (used by the harness to model eventual background propagation).
    void drain_all() { while (!buf_.empty()) drain_one(); }

    uint64_t bypass_hits() const { return hits_; }
    uint64_t drained() const { return drained_; }

private:
    MesiSystem* sys_;
    int core_;
    int depth_;
    DrainMode mode_;
    std::mt19937_64 rng_;
    std::deque<SBEntry> buf_;
    uint64_t hits_ = 0;
    uint64_t drained_ = 0;
};

// Per-core handle that routes loads/stores/mfence through a store buffer.
struct TSODut {
    MesiSystem* sys;
    StoreBuffer* sb;
    int core;

    void store(uint32_t addr, uint32_t val) { sb->store(addr, val); }
    uint32_t load(uint32_t addr) { return sb->load(addr); }
    void mfence() { sb->mfence(); }
};

} // namespace mesi
