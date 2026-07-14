// Thread-safe facade over the whole MESI model: read / write / cas /
// fetch_and_add / fence. A single bus mutex serializes operations — that both
// models the snooping bus's total order and lets real OS threads call in
// concurrently, which is how the lock/lock-free tests get real interleavings.
#pragma once
#include <cstdint>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <stdexcept>
#include <vector>
#include "snooping_bus.hpp"
#include "cache_controller.hpp"
#include "trace.hpp"

namespace mesi {

struct CasResult {
    uint32_t old_val;
    bool success;
};

class MesiSystem {
public:
    explicit MesiSystem(int n_cores) : bus_(checked_core_count(n_cores)),
                                       prog_idx_(n_cores, 0) {}

    int n_cores() const { return bus_.n_cores(); }
    SnoopingBus& bus() { return bus_; }
    PerfCounters& perf() { return bus_.perf(); }
    const PerfCounters& perf() const { return bus_.perf(); }

    void enable_tracing(bool on) {
        std::lock_guard<std::mutex> g(m_);
        tracing_ = on;
    }
    const Trace& trace() const { return trace_; }
    void clear_trace() {
        std::lock_guard<std::mutex> g(m_);
        trace_.clear();
        timestamp_ = 0;
        std::fill(prog_idx_.begin(), prog_idx_.end(), 0);
    }

    void enable_race_injection(uint64_t seed) { bus_.enable_race_injection(seed); }

    // ---- DUT memory interface (thread-safe) ----

    uint32_t read(int core, uint32_t addr) {
        validate_core(core);
        std::lock_guard<std::mutex> g(m_);
        uint32_t v = bus_.controller(core)->processor_read(addr);
        log(core, OpType::Read, addr, v);
        return v;
    }

    void write(int core, uint32_t addr, uint32_t value) {
        validate_core(core);
        std::lock_guard<std::mutex> g(m_);
        bus_.controller(core)->processor_write(addr, value);
        log(core, OpType::Write, addr, value);
    }

    // Atomic compare-and-swap. The whole read-compare-write happens while the
    // bus mutex is held (this is the RTL "bus lock" for the CAS duration).
    CasResult cas(int core, uint32_t addr, uint32_t expected, uint32_t new_val) {
        validate_core(core);
        std::lock_guard<std::mutex> g(m_);
        uint32_t old = bus_.controller(core)->acquire_exclusive(addr);
        log(core, OpType::Read, addr, old);
        bool ok = (old == expected);
        if (ok) {
            bus_.controller(core)->put_word(addr, new_val);
            log(core, OpType::Write, addr, new_val);
        }
        return {old, ok};
    }

    // Atomic fetch-and-add, built on the same bus-locked RMW window.
    uint32_t fetch_and_add(int core, uint32_t addr, uint32_t delta) {
        validate_core(core);
        std::lock_guard<std::mutex> g(m_);
        uint32_t old = bus_.controller(core)->acquire_exclusive(addr);
        log(core, OpType::Read, addr, old);
        bus_.controller(core)->put_word(addr, old + delta);
        log(core, OpType::Write, addr, old + delta);
        return old;
    }

    // MFENCE. In the base (non-TSO) model writes are not buffered, so a fence
    // is an ordering marker only; the TSO store buffer overrides this.
    void fence(int core) { validate_core(core); }

    uint64_t now() const { return timestamp_.load(); }

private:
    static int checked_core_count(int n) {
        if (n <= 0) throw std::invalid_argument("MesiSystem needs at least one core");
        return n;
    }

    void validate_core(int core) const {
        if (core < 0 || core >= n_cores())
            throw std::out_of_range("core id is outside the MESI system");
    }

    void log(int core, OpType op, uint32_t addr, uint32_t value) {
        if (!tracing_) { timestamp_++; return; }
        TraceEvent e{core, op, addr, value, timestamp_++, prog_idx_[core]++};
        trace_.push_back(e);
    }

    SnoopingBus bus_;
    std::mutex m_;
    std::atomic<uint64_t> timestamp_{0};
    bool tracing_ = false;
    Trace trace_;
    std::vector<uint64_t> prog_idx_;
};

} // namespace mesi
