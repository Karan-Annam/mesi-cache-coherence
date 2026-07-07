// Validates the SC checker itself, then checks SC of real concurrent runs.
#include "../test_framework.hpp"
#include "../../model/mesi_system.hpp"
#include "../../model/sc_checker.hpp"
#include <thread>
#include <random>

using namespace mesi;

TEST("checker accepts a valid serialized trace") {
    Trace t;
    // ts order: write A=1, read A=1, write A=2, read A=2
    t.push_back({0, OpType::Write, 0x10, 1, 0, 0});
    t.push_back({1, OpType::Read,  0x10, 1, 1, 0});
    t.push_back({0, OpType::Write, 0x10, 2, 2, 1});
    t.push_back({1, OpType::Read,  0x10, 2, 3, 1});
    CHECK(check_serial_order(t).ok);
}

TEST("checker rejects a stale read (manufactured violation)") {
    Trace t;
    t.push_back({0, OpType::Write, 0x10, 1, 0, 0});
    t.push_back({0, OpType::Write, 0x10, 2, 1, 1});
    t.push_back({1, OpType::Read,  0x10, 1, 2, 0}); // reads 1 but latest is 2
    CHECK(!check_serial_order(t).ok);
}

TEST("exhaustive checker finds a valid interleaving for message passing") {
    // Core0: W data=42 (po0), W flag=1 (po1)
    // Core1: R flag=1 (po0), R data=42 (po1)
    // A valid SC order exists: data=42, flag=1, R flag=1, R data=42.
    Trace t;
    t.push_back({0, OpType::Write, 0xD, 42, 0, 0});
    t.push_back({0, OpType::Write, 0xF, 1,  1, 1});
    t.push_back({1, OpType::Read,  0xF, 1,  2, 0});
    t.push_back({1, OpType::Read,  0xD, 42, 3, 1});
    CHECK(exists_sc_order(t));
}

TEST("exhaustive checker rejects an impossible (non-SC) outcome") {
    // Core1 reads flag=1 but data=0 — impossible under SC if core0 wrote data
    // before flag. No program-order-respecting interleaving yields this.
    Trace t;
    t.push_back({0, OpType::Write, 0xD, 42, 0, 0});
    t.push_back({0, OpType::Write, 0xF, 1,  1, 1});
    t.push_back({1, OpType::Read,  0xF, 1,  2, 0});
    t.push_back({1, OpType::Read,  0xD, 0,  3, 1}); // saw flag=1 but data=0
    CHECK(!exists_sc_order(t));
}

TEST("real concurrent execution is sequentially consistent") {
    const int N = 4;
    MesiSystem s(N);
    s.enable_tracing(true);
    std::vector<uint32_t> addrs;
    for (int i = 0; i < 6; ++i) { uint32_t a = 0x200 + 4u * i; addrs.push_back(a); s.bus().register_addr(a); }

    auto worker = [&](int core) {
        std::mt19937_64 rng(99 + core);
        for (int i = 0; i < 300; ++i) {
            uint32_t a = addrs[rng() % addrs.size()];
            if (rng() & 1) s.write(core, a, (uint32_t)((core << 20) | i));
            else (void)s.read(core, a);
        }
    };
    std::vector<std::thread> ts;
    for (int c = 0; c < N; ++c) ts.emplace_back(worker, c);
    for (auto& t : ts) t.join();

    SCResult r = check_serial_order(s.trace());
    CHECK(r.ok);
}
