// Ticket spinlock — FIFO fair, unlike the TAS lock.
#pragma once
#include "dut.hpp"

namespace mesi {

// Two coherent words: next_ticket and now_serving. A thread takes a ticket with
// fetch-and-add, then waits until now_serving reaches it. Guarantees FIFO order,
// preventing the starvation a TAS lock can suffer under contention.
struct TicketLock {
    uint32_t next_ticket_addr;
    uint32_t now_serving_addr;

    uint32_t lock(const Dut& d) const {
        uint32_t my = d.faa(next_ticket_addr, 1);
        while (d.read(now_serving_addr) != my) { /* spin */ }
        return my;
    }
    void unlock(const Dut& d) const {
        d.faa(now_serving_addr, 1);
    }
};

} // namespace mesi
