#include "snooping_bus.hpp"
#include "cache_controller.hpp"
#include <algorithm>

namespace mesi {

SnoopingBus::SnoopingBus(int n_cores) {
    perf_.resize(n_cores);
    controllers_.reserve(n_cores);
    for (int i = 0; i < n_cores; ++i)
        controllers_.push_back(new CacheController(i, this));
}

SnoopingBus::~SnoopingBus() {
    for (auto* c : controllers_) delete c;
}

void SnoopingBus::register_addr(uint32_t addr) {
    line_words_[line_of(addr)].insert(addr);
}

const std::unordered_set<uint32_t>& SnoopingBus::words_in_line(uint32_t line) const {
    auto it = line_words_.find(line);
    return it == line_words_.end() ? empty_ : it->second;
}

SnoopResult SnoopingBus::broadcast(BusTxn txn, uint32_t addr, int issuer_id) {
    // Visit all other caches and let them react to the snooped transaction.
    // Order can be randomized under race injection; correctness is unaffected
    // because each snoop reaction is independent given a serialized bus.
    std::vector<int> order;
    order.reserve(controllers_.size());
    for (int i = 0; i < (int)controllers_.size(); ++i)
        if (i != issuer_id) order.push_back(i);
    if (race_) std::shuffle(order.begin(), order.end(), rng_);

    SnoopResult agg;
    for (int i : order)
        agg |= controllers_[i]->snoop(txn, addr, issuer_id);
    return agg;
}

void SnoopingBus::account(BusTxn txn) {
    perf_.bus_cycles_busy++;
    switch (txn) {
        case BusTxn::BusRd:   perf_.bus_rd_count++;   break;
        case BusTxn::BusRdX:  perf_.bus_rdx_count++;  break;
        case BusTxn::BusUpgr: perf_.bus_upgr_count++; break;
        case BusTxn::BusWB:   perf_.bus_wb_count++;   break;
        default: break;
    }
    advance_arb();
}

} // namespace mesi
