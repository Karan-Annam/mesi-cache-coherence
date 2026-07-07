// Performance counters (same set as perf_counters.sv).
#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <sstream>

namespace mesi {

struct PerfCounters {
    uint64_t bus_cycles_busy = 0; // transactions issued (1 cycle each in this model)
    uint64_t bus_rd_count = 0;
    uint64_t bus_rdx_count = 0;
    uint64_t bus_upgr_count = 0;
    uint64_t bus_wb_count = 0;

    std::vector<uint64_t> inv_count;     // per-core invalidations received
    std::vector<uint64_t> l1_hit;        // per-core L1 hits
    std::vector<uint64_t> l1_miss;       // per-core L1 misses
    std::vector<uint64_t> stall_cycles;  // per-core cycles "stalled" on the bus
    std::vector<uint64_t> sb_drain_cycles; // per-core store-buffer drain cycles (TSO)

    void resize(int n) {
        inv_count.assign(n, 0);
        l1_hit.assign(n, 0);
        l1_miss.assign(n, 0);
        stall_cycles.assign(n, 0);
        sb_drain_cycles.assign(n, 0);
    }

    uint64_t total_bus_txns() const {
        return bus_rd_count + bus_rdx_count + bus_upgr_count + bus_wb_count;
    }

    std::string to_string() const {
        std::ostringstream os;
        os << "bus_rd=" << bus_rd_count
           << " bus_rdx=" << bus_rdx_count
           << " bus_upgr=" << bus_upgr_count
           << " bus_wb=" << bus_wb_count
           << " total_txn=" << total_bus_txns();
        for (size_t i = 0; i < l1_hit.size(); ++i) {
            os << "\n  core" << i
               << ": hit=" << l1_hit[i]
               << " miss=" << l1_miss[i]
               << " inv=" << inv_count[i]
               << " stall=" << stall_cycles[i];
        }
        return os.str();
    }

    // CSV row helper for analysis scripts.
    std::string csv_row(const std::string& tag) const {
        std::ostringstream os;
        os << tag << "," << bus_rd_count << "," << bus_rdx_count << ","
           << bus_upgr_count << "," << bus_wb_count << "," << total_bus_txns();
        return os.str();
    }
};

} // namespace mesi
