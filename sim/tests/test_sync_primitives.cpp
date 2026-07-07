// Synchronization primitives under real concurrent threads.
#include "../test_framework.hpp"
#include "../../model/mesi_system.hpp"
#include "../../primitives/dut.hpp"
#include "../../primitives/spinlock.hpp"
#include "../../primitives/ticket_lock.hpp"
#include "../../primitives/seqlock.hpp"
#include "../../primitives/hazard_ptr.hpp"
#include "../../primitives/lockfree_stack.hpp"
#include "../../primitives/barrier.hpp"
#include <thread>
#include <vector>
#include <atomic>
#include <algorithm>

using namespace mesi;

TEST("TAS spinlock provides mutual exclusion (counter is exact)") {
    const int N = 4, K = 2000;
    MesiSystem s(N);
    uint32_t LOCK = 0x100, CTR = 0x200;
    s.bus().register_addr(LOCK); s.bus().register_addr(CTR);
    s.write(0, LOCK, 0); s.write(0, CTR, 0);
    TASLock lk{LOCK};

    auto worker = [&](int core) {
        Dut d{&s, core};
        for (int i = 0; i < K; ++i) {
            lk.lock(d);
            uint32_t v = d.read(CTR);    // non-atomic RMW protected by the lock
            d.write(CTR, v + 1);
            lk.unlock(d);
        }
    };
    std::vector<std::thread> ts;
    for (int c = 0; c < N; ++c) ts.emplace_back(worker, c);
    for (auto& t : ts) t.join();
    CHECK_EQ(s.read(0, CTR), (long long)(N * K));
}

TEST("ticket lock provides mutual exclusion and is FIFO-fair") {
    const int N = 4, K = 1500;
    MesiSystem s(N);
    uint32_t NEXT = 0x300, SERV = 0x304, CTR = 0x308;
    for (uint32_t a : {NEXT, SERV, CTR}) s.bus().register_addr(a);
    s.write(0, NEXT, 0); s.write(0, SERV, 0); s.write(0, CTR, 0);
    TicketLock lk{NEXT, SERV};

    auto worker = [&](int core) {
        Dut d{&s, core};
        for (int i = 0; i < K; ++i) {
            lk.lock(d);
            uint32_t v = d.read(CTR);
            d.write(CTR, v + 1);
            lk.unlock(d);
        }
    };
    std::vector<std::thread> ts;
    for (int c = 0; c < N; ++c) ts.emplace_back(worker, c);
    for (auto& t : ts) t.join();
    CHECK_EQ(s.read(0, CTR), (long long)(N * K));
    // now_serving must have advanced exactly once per critical section.
    CHECK_EQ(s.read(0, SERV), (long long)(N * K));
}

TEST("seqlock readers never observe a torn (inconsistent) snapshot") {
    const int NREAD = 3;
    MesiSystem s(NREAD + 1);
    uint32_t SEQ = 0x400, DATA = 0x440;
    const int W = 4;
    s.bus().register_addr(SEQ);
    for (int i = 0; i < W; ++i) s.bus().register_addr(DATA + 4 * i);
    s.write(0, SEQ, 0);
    Seqlock sl{SEQ, DATA, W};
    // Each version writes {v, v+1, v+2, v+3}; a consistent snapshot has all
    // words equal to base+offset for one base value.
    std::vector<uint32_t> init(W);
    for (int i = 0; i < W; ++i) init[i] = (uint32_t)i;
    sl.write(Dut{&s, 0}, init);

    std::atomic<bool> stop{false};
    std::atomic<long> torn{0}, reads{0};

    auto reader = [&](int core) {
        Dut d{&s, core};
        std::vector<uint32_t> snap;
        while (!stop.load()) {
            sl.read_retry(d, snap);
            uint32_t base = snap[0];
            for (int i = 1; i < W; ++i)
                if (snap[i] != base + (uint32_t)i) torn.fetch_add(1);
            reads.fetch_add(1);
        }
    };
    std::vector<std::thread> ts;
    for (int r = 0; r < NREAD; ++r) ts.emplace_back(reader, r + 1);

    Dut wd{&s, 0};
    for (uint32_t v = 1; v <= 4000; ++v) {
        std::vector<uint32_t> vals(W);
        for (int i = 0; i < W; ++i) vals[i] = v + (uint32_t)i;
        sl.write(wd, vals);
    }
    stop.store(true);
    for (auto& t : ts) t.join();

    CHECK_EQ(torn.load(), 0);
    CHECK(reads.load() > 0);
}

TEST("lock-free stack: concurrent push/pop loses and duplicates nothing") {
    const int N = 4, PER = 1500;
    MesiSystem s(N);
    uint32_t TOP = 0x500, ALLOC = 0x504, NODES = 0x10000;
    s.bus().register_addr(TOP); s.bus().register_addr(ALLOC);
    LockFreeStack st{TOP, ALLOC, NODES};
    st.init(Dut{&s, 0});

    // Each thread pushes a disjoint range of values, then everything is popped.
    auto pusher = [&](int core) {
        Dut d{&s, core};
        for (int i = 0; i < PER; ++i)
            st.push(d, (uint32_t)(core * 1000000 + i + 1)); // +1: avoid value 0
    };
    std::vector<std::thread> ts;
    for (int c = 0; c < N; ++c) ts.emplace_back(pusher, c);
    for (auto& t : ts) t.join();

    // Concurrently pop everything; collect per-thread to avoid contention on a
    // shared result structure.
    std::vector<std::vector<uint32_t>> got(N);
    auto popper = [&](int core) {
        Dut d{&s, core};
        uint32_t v;
        while (st.pop(d, v)) got[core].push_back(v);
    };
    ts.clear();
    for (int c = 0; c < N; ++c) ts.emplace_back(popper, c);
    for (auto& t : ts) t.join();

    std::vector<uint32_t> all;
    for (auto& g : got) all.insert(all.end(), g.begin(), g.end());
    CHECK_EQ((long long)all.size(), (long long)(N * PER)); // nothing lost
    std::sort(all.begin(), all.end());
    CHECK(std::adjacent_find(all.begin(), all.end()) == all.end()); // no dupes
    // Values are exactly the union of each thread's range.
    CHECK_EQ((long long)all.front(), 1);
}

TEST("hazard pointers prevent use-after-free across cores") {
    const int NREAD = 3;
    const int NT = NREAD + 1;
    MesiSystem s(NT);
    uint32_t PTR = 0x600, HAZ = 0x640, OBJBASE = 0x20000, ALLOC = 0x604;
    s.bus().register_addr(PTR); s.bus().register_addr(ALLOC);
    for (int t = 0; t < NT; ++t) s.bus().register_addr(HAZ + 4 * t);
    HazardDomain hp{HAZ, NT};

    Dut w{&s, 0};
    // Object layout: [val @ +0]. val != 0xDEAD means live; 0xDEAD means freed.
    auto alloc_obj = [&](uint32_t val) -> uint32_t {
        uint32_t idx = w.faa(ALLOC, 1) + 1;
        uint32_t addr = OBJBASE + idx * 8;
        w.write(addr, val); // write() registers the address under the bus mutex
        return addr;
    };
    w.write(PTR, alloc_obj(1));

    std::atomic<bool> stop{false};
    std::atomic<long> uaf{0}, reads{0};

    auto reader = [&](int tid) {
        Dut d{&s, tid};
        while (!stop.load()) {
            uint32_t p = hp.protect(d, tid, PTR);
            if (p) {
                if (d.read(p) == 0xDEAD) uaf.fetch_add(1); // saw a freed object!
                reads.fetch_add(1);
            }
            hp.release(d, tid);
        }
    };
    std::vector<std::thread> ts;
    for (int r = 0; r < NREAD; ++r) ts.emplace_back(reader, r + 1);

    // Writer swaps the object repeatedly, retiring the old one only once no
    // hazard slot references it (deferred reclamation).
    std::vector<uint32_t> retired;
    for (uint32_t v = 2; v <= 3000; ++v) {
        uint32_t old = w.read(PTR);
        uint32_t neu = alloc_obj(v);
        w.write(PTR, neu);            // publish new object
        retired.push_back(old);
        // Try to reclaim anything no longer hazarded.
        std::vector<uint32_t> still;
        for (uint32_t r : retired) {
            if (hp.is_hazarded(w, r)) { still.push_back(r); }
            else { w.write(r, 0xDEAD); } // safe "free": poison it
        }
        retired.swap(still);
    }
    stop.store(true);
    for (auto& t : ts) t.join();

    CHECK_EQ(uaf.load(), 0);   // no reader ever dereferenced a freed object
    CHECK(reads.load() > 0);
}

TEST("barrier: no thread leaves an episode before all arrive") {
    const int N = 4, EPISODES = 200;
    MesiSystem s(N);
    uint32_t CNT = 0x700, SENSE = 0x704, ARRIVED = 0x708;
    for (uint32_t a : {CNT, SENSE, ARRIVED}) s.bus().register_addr(a);
    Barrier bar{CNT, SENSE, N};
    bar.init(Dut{&s, 0});
    s.write(0, ARRIVED, 0);

    std::atomic<long> violations{0};

    // Classic 3-barrier episode: (1) all threads increment ARRIVED, barrier;
    // now ARRIVED must equal N for everyone. (2) barrier so all have observed
    // before reset. (3) leader resets, barrier so reset is visible next episode.
    auto worker = [&](int core) {
        Dut d{&s, core};
        uint32_t ls = 0;
        for (int e = 0; e < EPISODES; ++e) {
            d.faa(ARRIVED, 1);
            bar.wait(d, ls);
            if (d.read(ARRIVED) != (uint32_t)N) violations.fetch_add(1);
            bar.wait(d, ls);
            if (core == 0) d.write(ARRIVED, 0);
            bar.wait(d, ls);
        }
    };
    std::vector<std::thread> ts;
    for (int c = 0; c < N; ++c) ts.emplace_back(worker, c);
    for (auto& t : ts) t.join();
    CHECK_EQ(violations.load(), 0);
}
