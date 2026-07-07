#include "cache_controller.hpp"
#include "snooping_bus.hpp"

namespace mesi {

void CacheController::drop_line_words(uint32_t line) {
    for (uint32_t a : bus_->words_in_line(line))
        data_.erase(a);
}

// Helper: pull every known word of `line` from memory into this cache.
// Models a full 64-byte line fill.
static void fill_line(CacheController* self, SnoopingBus* bus, uint32_t line) {
    for (uint32_t a : bus->words_in_line(line))
        self->put_word(a, bus->memory().read(a));
}

// Helper: write every known word of `line` we hold back to memory (BusWB).
static void writeback_line(CacheController* self, SnoopingBus* bus, uint32_t line) {
    for (uint32_t a : bus->words_in_line(line))
        if (self->has_word(a))
            bus->memory().write(a, self->peek(a));
    bus->memory().accesses++;
}

uint32_t CacheController::processor_read(uint32_t addr) {
    bus_->register_addr(addr);
    uint32_t line = line_of(addr);
    State st = line_state(line);
    auto& perf = bus_->perf();

    if (st != State::I) {
        // Read hit — M/E/S all serve locally with no bus transaction.
        perf.l1_hit[id_]++;
        return peek(addr);
    }

    // Read miss (I): issue BusRd.
    perf.l1_miss[id_]++;
    perf.stall_cycles[id_]++;
    SnoopResult sr = bus_->broadcast(BusTxn::BusRd, addr, id_);
    bus_->account(BusTxn::BusRd);

    // After any dirty intervention, memory is current — fill the line.
    fill_line(this, bus_, line);
    // If another cache still holds the line, we are Shared; else Exclusive.
    set_state(line, sr.had_copy ? State::S : State::E);
    return peek(addr);
}

uint32_t CacheController::acquire_exclusive(uint32_t addr) {
    bus_->register_addr(addr);
    uint32_t line = line_of(addr);
    State st = line_state(line);
    auto& perf = bus_->perf();

    switch (st) {
        case State::M:
            // Already owned dirty — no bus.
            perf.l1_hit[id_]++;
            break;

        case State::E:
            // Silent upgrade E -> M, no bus transaction.
            perf.l1_hit[id_]++;
            set_state(line, State::M);
            break;

        case State::S:
            // Upgrade with BusUpgr, invalidate other S copies.
            perf.l1_hit[id_]++;
            perf.stall_cycles[id_]++;
            bus_->broadcast(BusTxn::BusUpgr, addr, id_);
            bus_->account(BusTxn::BusUpgr);
            set_state(line, State::M);
            break;

        case State::I:
            // Write miss: BusRdX to obtain exclusive ownership; others invalidate.
            perf.l1_miss[id_]++;
            perf.stall_cycles[id_]++;
            bus_->broadcast(BusTxn::BusRdX, addr, id_);
            bus_->account(BusTxn::BusRdX);
            fill_line(this, bus_, line);   // load current line from memory
            set_state(line, State::M);
            break;
    }
    return peek(addr);
}

void CacheController::processor_write(uint32_t addr, uint32_t value) {
    acquire_exclusive(addr);
    put_word(addr, value);
}

SnoopResult CacheController::snoop(BusTxn txn, uint32_t addr, int issuer_id) {
    (void)issuer_id;
    uint32_t line = line_of(addr);
    State st = line_state(line);
    SnoopResult res;
    if (st == State::I) return res; // we don't hold the line — nothing to do
    res.had_copy = true;
    auto& perf = bus_->perf();

    switch (txn) {
        case BusTxn::BusRd:
            // Another cache reads the line.
            if (st == State::M) {
                // Must write back dirty data first, then downgrade to S.
                writeback_line(this, bus_, line);
                bus_->account(BusTxn::BusWB);
                set_state(line, State::S);
                res.intervened = true;
            } else if (st == State::E) {
                set_state(line, State::S); // E -> S (clean, no writeback)
            } // S -> S: no change
            break;

        case BusTxn::BusRdX:
            // Another cache wants exclusive ownership — we must invalidate.
            if (st == State::M) {
                writeback_line(this, bus_, line);
                bus_->account(BusTxn::BusWB);
                res.intervened = true;
            }
            set_state(line, State::I);
            drop_line_words(line);
            perf.inv_count[id_]++;
            break;

        case BusTxn::BusUpgr:
            // Another S-holder is upgrading to M — our copy (S/E) is invalidated.
            set_state(line, State::I);
            drop_line_words(line);
            perf.inv_count[id_]++;
            break;

        default:
            break;
    }
    return res;
}

} // namespace mesi
