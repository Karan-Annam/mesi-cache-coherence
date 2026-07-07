#include "sc_checker.hpp"
#include <unordered_map>
#include <algorithm>
#include <sstream>
#include <vector>

namespace mesi {

SCResult check_serial_order(const Trace& trace_in) {
    // Sort by global timestamp to recover the serialization order.
    Trace t = trace_in;
    std::sort(t.begin(), t.end(),
              [](const TraceEvent& a, const TraceEvent& b) {
                  return a.timestamp < b.timestamp;
              });

    std::unordered_map<uint32_t, uint32_t> mem; // current value per address (init 0)
    for (const auto& e : t) {
        if (e.op == OpType::Write) {
            mem[e.addr] = e.value;
        } else { // Read
            uint32_t expect = mem.count(e.addr) ? mem[e.addr] : 0u;
            if (e.value != expect) {
                std::ostringstream os;
                os << "SC violation: core " << e.core_id << " read addr 0x"
                   << std::hex << e.addr << std::dec << " got " << e.value
                   << " but most-recent-write value is " << expect
                   << " (ts=" << e.timestamp << ")";
                return {false, os.str()};
            }
        }
    }
    return {true, "trace is sequentially consistent (" +
                      std::to_string(t.size()) + " ops)"};
}

namespace {
// Backtracking search over interleavings that respect per-core program order.
struct Searcher {
    // Per-core event lists, each already in program order.
    std::vector<std::vector<TraceEvent>> cores;
    std::vector<size_t> idx;

    bool dfs(std::unordered_map<uint32_t, uint32_t>& mem) {
        bool all_done = true;
        for (size_t c = 0; c < cores.size(); ++c) {
            if (idx[c] >= cores[c].size()) continue;
            all_done = false;
            const TraceEvent& e = cores[c][idx[c]];

            if (e.op == OpType::Read) {
                uint32_t cur = mem.count(e.addr) ? mem[e.addr] : 0u;
                if (cur != e.value) continue; // can't place this read here
                idx[c]++;
                if (dfs(mem)) { idx[c]--; return true; }
                idx[c]--;
            } else { // Write
                uint32_t saved = mem.count(e.addr) ? mem[e.addr] : 0u;
                bool had = mem.count(e.addr) != 0;
                mem[e.addr] = e.value;
                idx[c]++;
                if (dfs(mem)) { idx[c]--; return true; }
                idx[c]--;
                if (had) mem[e.addr] = saved; else mem.erase(e.addr);
            }
        }
        return all_done; // reached only when every core is exhausted
    }
};
} // namespace

bool exists_sc_order(const Trace& trace) {
    // Bucket events by core, preserving program order.
    int max_core = -1;
    for (const auto& e : trace) max_core = std::max(max_core, e.core_id);
    if (max_core < 0) return true;

    Searcher s;
    s.cores.resize(max_core + 1);
    for (const auto& e : trace) s.cores[e.core_id].push_back(e);
    for (auto& v : s.cores)
        std::sort(v.begin(), v.end(),
                  [](const TraceEvent& a, const TraceEvent& b) {
                      return a.program_idx < b.program_idx;
                  });
    s.idx.assign(s.cores.size(), 0);

    std::unordered_map<uint32_t, uint32_t> mem;
    return s.dfs(mem);
}

} // namespace mesi
