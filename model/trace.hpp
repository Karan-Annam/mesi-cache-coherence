// Memory-operation trace events for the SC checker.
#pragma once
#include <cstdint>
#include <vector>

namespace mesi {

enum class OpType : uint8_t { Read, Write };

struct TraceEvent {
    int core_id;
    OpType op;
    uint32_t addr;
    uint32_t value;       // value read or written
    uint64_t timestamp;   // global serialization order index
    uint64_t program_idx; // per-core program order index
};

using Trace = std::vector<TraceEvent>;

} // namespace mesi
