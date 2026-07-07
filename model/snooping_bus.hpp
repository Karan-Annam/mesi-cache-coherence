// The snooping bus plus memory and the line registry. Serializes every
// coherence transaction and broadcasts it to all caches.
#pragma once
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <random>
#include "mesi_types.hpp"
#include "memory.hpp"
#include "perf.hpp"

namespace mesi {

class CacheController;

class SnoopingBus {
public:
    explicit SnoopingBus(int n_cores);
    ~SnoopingBus();

    int n_cores() const { return (int)controllers_.size(); }
    CacheController* controller(int id) { return controllers_[id]; }
    Memory& memory() { return mem_; }
    PerfCounters& perf() { return perf_; }
    const PerfCounters& perf() const { return perf_; }

    // Registry of every word address ever touched within a line. Used to model
    // full-line transfers (fetch loads all known words; writeback stores them).
    void register_addr(uint32_t addr);
    const std::unordered_set<uint32_t>& words_in_line(uint32_t line) const;

    // Broadcast a transaction to all caches except the issuer. The aggregated
    // result reports whether any other cache held a copy (had_copy) and whether
    // any supplied dirty data (intervened).
    SnoopResult broadcast(BusTxn txn, uint32_t addr, int issuer_id);

    // Count a bus transaction in the perf counters and advance arbitration.
    void account(BusTxn txn);

    // Round-robin arbitration pointer (advanced after each grant). Exposed for
    // tests that want to observe fairness.
    int arb_ptr() const { return arb_ptr_; }
    void advance_arb() { arb_ptr_ = (arb_ptr_ + 1) % n_cores(); }

    // Race-injection knob: when enabled, snoop responses are processed in a
    // randomized order to expose ordering-sensitive bugs (the model stays
    // correct because the bus still serializes whole transactions).
    void enable_race_injection(uint64_t seed) { race_ = true; rng_.seed(seed); }
    bool race_injection() const { return race_; }

private:
    std::vector<CacheController*> controllers_;
    Memory mem_;
    PerfCounters perf_;
    std::unordered_map<uint32_t, std::unordered_set<uint32_t>> line_words_;
    std::unordered_set<uint32_t> empty_;
    int arb_ptr_ = 0;
    bool race_ = false;
    std::mt19937_64 rng_;
};

} // namespace mesi
