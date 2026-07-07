// One MESI L1 cache controller (behavioral).
#pragma once
#include <cstdint>
#include <unordered_map>
#include "mesi_types.hpp"

namespace mesi {

class SnoopingBus; // fwd

// A single L1 cache controller. State and data are tracked per address; the
// MESI state itself is tracked per 64-byte line (so false sharing is modeled).
class CacheController {
public:
    CacheController(int id, SnoopingBus* bus) : id_(id), bus_(bus) {}

    int id() const { return id_; }

    State line_state(uint32_t line) const {
        auto it = state_.find(line);
        return it == state_.end() ? State::I : it->second;
    }

    bool has_word(uint32_t addr) const { return data_.count(addr) != 0; }
    uint32_t peek(uint32_t addr) const {
        auto it = data_.find(addr);
        return it == data_.end() ? 0u : it->second;
    }

    void set_state(uint32_t line, State s) {
        if (s == State::I) state_.erase(line);
        else state_[line] = s;
    }
    void put_word(uint32_t addr, uint32_t v) { data_[addr] = v; }
    void drop_line_words(uint32_t line);

    // Processor-side operations (issued by the owning core through the bus).
    uint32_t processor_read(uint32_t addr);
    void processor_write(uint32_t addr, uint32_t value);

    // Bring `line_of(addr)` into M (issuing BusRdX/BusUpgr as needed) and return
    // the current word value. Used by both processor_write and atomic RMW (CAS).
    uint32_t acquire_exclusive(uint32_t addr);

    // Bus-side: snoop a transaction issued by another controller.
    SnoopResult snoop(BusTxn txn, uint32_t addr, int issuer_id);

private:
    int id_;
    SnoopingBus* bus_;
    std::unordered_map<uint32_t, State> state_;     // line -> MESI state
    std::unordered_map<uint32_t, uint32_t> data_;   // addr -> word value
};

} // namespace mesi
