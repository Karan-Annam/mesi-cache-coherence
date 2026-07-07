// CAS / fetch-and-add semantics.
#include "../test_framework.hpp"
#include "../../model/mesi_system.hpp"
#include "../../primitives/dut.hpp"
#include <thread>
#include <vector>

using namespace mesi;

TEST("CAS success and failure semantics, returns old value") {
    MesiSystem s(2);
    uint32_t A = 0x1000;
    s.bus().register_addr(A);
    s.write(0, A, 5);

    CasResult r1 = s.cas(1, A, 5, 9);   // expected matches
    CHECK(r1.success);
    CHECK_EQ(r1.old_val, 5);
    CHECK_EQ(s.read(1, A), 9);

    CasResult r2 = s.cas(0, A, 5, 100); // expected stale -> fails
    CHECK(!r2.success);
    CHECK_EQ(r2.old_val, 9);
    CHECK_EQ(s.read(0, A), 9);          // unchanged
}

TEST("CAS acquires the line exclusively (BusRdX on miss)") {
    MesiSystem s(2);
    uint32_t A = 0x2000;
    s.bus().register_addr(A);
    s.read(1, A); // core1 holds E
    uint64_t rdx0 = s.perf().bus_rdx_count;
    s.cas(0, A, 0, 1); // core0 must gain exclusivity -> BusRdX, invalidates core1
    CHECK_EQ(s.perf().bus_rdx_count, (long long)rdx0 + 1);
    CHECK(s.bus().controller(1)->line_state(line_of(A)) == State::I);
}

TEST("concurrent fetch-and-add is atomic (no lost updates)") {
    const int N = 4, K = 5000;
    MesiSystem s(N);
    uint32_t CTR = 0x3000;
    s.bus().register_addr(CTR);
    s.write(0, CTR, 0);

    auto worker = [&](int core) {
        Dut d{&s, core};
        for (int i = 0; i < K; ++i) d.faa(CTR, 1);
    };
    std::vector<std::thread> ts;
    for (int c = 0; c < N; ++c) ts.emplace_back(worker, c);
    for (auto& t : ts) t.join();

    CHECK_EQ(s.read(0, CTR), (long long)(N * K));
}

TEST("fetch-and-add implemented as CAS retry loop is also atomic") {
    const int N = 4, K = 3000;
    MesiSystem s(N);
    uint32_t CTR = 0x4000;
    s.bus().register_addr(CTR);
    s.write(0, CTR, 0);

    auto worker = [&](int core) {
        Dut d{&s, core};
        for (int i = 0; i < K; ++i) {
            for (;;) {
                uint32_t old = d.read(CTR);
                if (d.cas(CTR, old, old + 1)) break;
            }
        }
    };
    std::vector<std::thread> ts;
    for (int c = 0; c < N; ++c) ts.emplace_back(worker, c);
    for (auto& t : ts) t.join();

    CHECK_EQ(s.read(0, CTR), (long long)(N * K));
}
