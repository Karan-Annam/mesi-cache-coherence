// TSO litmus tests.
//  - store-buffer bypass: a core sees its own buffered store, others don't
//  - Dekker/SB (store->load): breaks under TSO without fences, MFENCE fixes it
//  - message passing (store->store): fine under a FIFO buffer with no fence;
//    only the RELAXED drain mode reorders it, and a fence repairs that
#include "../test_framework.hpp"
#include "../../model/mesi_system.hpp"
#include "../../model/tso.hpp"
#include <thread>
#include <atomic>
#include <vector>

using namespace mesi;

TEST("store buffer: owner bypass-reads its own write before drain; others can't") {
    MesiSystem s(2);
    uint32_t A = 0x1000;
    s.bus().register_addr(A);
    s.write(0, A, 7); // initial coherent value
    StoreBuffer sb0(&s, 0), sb1(&s, 1);

    sb0.store(A, 99);                 // buffered, not yet in coherent memory
    CHECK_EQ(sb0.load(A), 99);        // owner sees its own store (bypass)
    CHECK_EQ(s.read(1, A), 7);        // other core still sees the old value
    CHECK(sb0.bypass_hits() >= 1);

    sb0.mfence();                     // drain
    CHECK_EQ(s.read(1, A), 99);       // now visible to everyone
    (void)sb1;
}

// Two-thread spin barrier helper (C++17, no std::barrier).
static void spin_barrier(std::atomic<int>& a, int n) {
    a.fetch_add(1);
    while (a.load() < n) { /* spin */ }
}

// Run the SB/Dekker litmus `trials` times; return number of violations
// (both cores entered the critical section).
static long run_dekker(bool fenced, int trials) {
    MesiSystem s(2);
    uint32_t F0 = 0x2000, F1 = 0x2040; // separate lines
    s.bus().register_addr(F0);
    s.bus().register_addr(F1);
    std::atomic<long> violations{0};

    for (int t = 0; t < trials; ++t) {
        s.write(0, F0, 0);
        s.write(0, F1, 0);
        StoreBuffer sb0(&s, 0), sb1(&s, 1);
        TSODut d0{&s, &sb0, 0}, d1{&s, &sb1, 1};
        std::atomic<int> gate{0};
        uint32_t r0 = 1, r1 = 1;

        std::thread th0([&] {
            spin_barrier(gate, 2);
            d0.store(F0, 1);
            if (fenced) d0.mfence();
            r0 = d0.load(F1);
        });
        std::thread th1([&] {
            spin_barrier(gate, 2);
            d1.store(F1, 1);
            if (fenced) d1.mfence();
            r1 = d1.load(F0);
        });
        th0.join();
        th1.join();
        if (r0 == 0 && r1 == 0) violations.fetch_add(1); // both entered CS
    }
    return violations.load();
}

TEST("Dekker under TSO WITHOUT fences violates mutual exclusion") {
    long v = run_dekker(/*fenced=*/false, 300);
    // The store-buffered flag is invisible to the other core, so both read 0.
    CHECK(v > 0);
}

TEST("Dekker WITH MFENCE restores mutual exclusion (zero violations)") {
    long v = run_dekker(/*fenced=*/true, 300);
    CHECK_EQ(v, 0);
}

// Message passing, deterministic controlled interleaving:
//   Producer: store DATA=42 ; [fence?] ; store READY=1 ; then propagate.
//   Consumer: observes after READY drains; if DATA not yet visible -> violation.
// We model the consumer observing coherent memory at the worst-case moment:
// immediately after READY becomes visible.
static bool mp_violation(DrainMode mode, bool fence_between) {
    MesiSystem s(2);
    uint32_t DATA = 0x3000, READY = 0x3040;
    s.bus().register_addr(DATA);
    s.bus().register_addr(READY);
    s.write(0, DATA, 0);
    s.write(0, READY, 0);
    StoreBuffer sb(&s, 0, 8, mode, /*seed=*/12345);

    sb.store(DATA, 42);
    if (fence_between) sb.mfence();   // force DATA out before READY is stored
    sb.store(READY, 1);

    // Propagate one entry at a time; after each drain the consumer peeks.
    bool violated = false;
    while (!sb.empty()) {
        sb.drain_one();
        uint32_t ready = s.read(1, READY);
        uint32_t data = s.read(1, DATA);
        if (ready == 1 && data != 42) violated = true; // saw flag, missed data
    }
    return violated;
}

TEST("message passing is correctly ordered under FIFO TSO (no fence needed)") {
    // FIFO store buffer preserves store->store order: READY can't overtake DATA.
    bool v = mp_violation(DrainMode::FIFO, /*fence_between=*/false);
    CHECK(!v);
}

TEST("message passing breaks under a RELAXED store buffer without a fence") {
    // With out-of-order drain, READY may become visible before DATA.
    bool any = false;
    for (int seed = 0; seed < 40 && !any; ++seed) {
        MesiSystem s(2);
        uint32_t DATA = 0x4000, READY = 0x4040;
        s.bus().register_addr(DATA); s.bus().register_addr(READY);
        s.write(0, DATA, 0); s.write(0, READY, 0);
        StoreBuffer sb(&s, 0, 8, DrainMode::RELAXED, (uint64_t)(seed * 7 + 1));
        sb.store(DATA, 42);
        sb.store(READY, 1);
        while (!sb.empty()) {
            sb.drain_one();
            if (s.read(1, READY) == 1 && s.read(1, DATA) != 42) any = true;
        }
    }
    CHECK(any); // at least one ordering exhibits the violation
}

TEST("a fence repairs message passing even under a RELAXED store buffer") {
    bool v = mp_violation(DrainMode::RELAXED, /*fence_between=*/true);
    CHECK(!v);
}
