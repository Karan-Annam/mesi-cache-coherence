// Shared enums and small structs for the model.
#pragma once
#include <cstdint>
#include <string>

namespace mesi {

// Cache line state. Values chosen to mirror the RTL encoding in mesi_pkg.sv:
//   M = 0b11, E = 0b10, S = 0b01, I = 0b00
enum class State : uint8_t { I = 0, S = 1, E = 2, M = 3 };

inline const char* state_str(State s) {
    switch (s) {
        case State::M: return "M";
        case State::E: return "E";
        case State::S: return "S";
        case State::I: return "I";
    }
    return "?";
}

// Bus transaction types. Mirrors bus_txn_t in mesi_pkg.sv.
enum class BusTxn : uint8_t {
    None = 0,
    BusRd = 1,   // request line for reading
    BusRdX = 2,  // request line for writing (invalidate others)
    BusUpgr = 3, // S -> M upgrade (invalidate other S copies)
    BusWB = 4    // writeback dirty line to memory
};

inline const char* txn_str(BusTxn t) {
    switch (t) {
        case BusTxn::None: return "None";
        case BusTxn::BusRd: return "BusRd";
        case BusTxn::BusRdX: return "BusRdX";
        case BusTxn::BusUpgr: return "BusUpgr";
        case BusTxn::BusWB: return "BusWB";
    }
    return "?";
}

// Processor request kind.
enum class PrReq : uint8_t { None = 0, PrRd = 1, PrWr = 2 };

// Aggregated result of snooping one transaction against the other caches.
struct SnoopResult {
    bool had_copy = false;   // some other cache held the line (M/E/S) when snooped
    bool intervened = false; // some other cache supplied dirty data (was in M)

    SnoopResult& operator|=(const SnoopResult& o) {
        had_copy |= o.had_copy;
        intervened |= o.intervened;
        return *this;
    }
};

// Cache geometry: 64-byte lines. The model stores one 32-bit word per address
// but tracks which line each address belongs to, so false sharing behaves
// exactly as it would with real 64-byte lines.
constexpr uint32_t LINE_BYTES = 64;

inline uint32_t line_of(uint32_t addr) { return addr & ~(LINE_BYTES - 1); }

} // namespace mesi
