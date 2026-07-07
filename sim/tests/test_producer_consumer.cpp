// Producer/consumer sharing pattern.
#include "../test_framework.hpp"
#include "../workloads.hpp"

using namespace mesi;

static State st(MesiSystem& s, int core, uint32_t addr) {
    return s.bus().controller(core)->line_state(line_of(addr));
}

TEST("producer-consumer delivers all data and ends in shared state") {
    MesiSystem s(2);
    uint32_t buf = 0x10000;
    uint32_t flag = 0x20000;
    const int words = 8;
    std::vector<uint32_t> out;
    producer_consumer(s, buf, words, flag, out);

    CHECK_EQ((int)out.size(), words);
    for (int i = 0; i < words; ++i)
        CHECK_EQ(out[i], (long long)(0xC0DE0000u + i));

    // After the consumer reads, producer's lines must have downgraded M->S and
    // the consumer holds them shared too.
    CHECK(st(s, 0, flag) == State::S);
    CHECK(st(s, 1, flag) == State::S);
    CHECK(st(s, 0, buf) == State::S);
    CHECK(st(s, 1, buf) == State::S);
}

TEST("producer-consumer flag read observes the producer's flag write") {
    MesiSystem s(2);
    uint32_t buf = 0x30000, flag = 0x40000;
    std::vector<uint32_t> out;
    producer_consumer(s, buf, 4, flag, out);
    CHECK_EQ(s.read(1, flag), 1);
}
