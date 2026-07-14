// Public API validation and trace-reset behavior.
#include "../test_framework.hpp"
#include "../../model/mesi_system.hpp"
#include "../../model/tso.hpp"
#include "../../primitives/hazard_ptr.hpp"
#include <stdexcept>

using namespace mesi;

TEST("MESI system rejects invalid core counts and core ids") {
    bool bad_count = false;
    try { MesiSystem invalid(0); }
    catch (const std::invalid_argument&) { bad_count = true; }
    CHECK(bad_count);

    MesiSystem s(2);
    bool bad_core = false;
    try { (void)s.read(2, 0x100); }
    catch (const std::out_of_range&) { bad_core = true; }
    CHECK(bad_core);
}

TEST("clearing a trace resets timestamp and per-core program indexes") {
    MesiSystem s(2);
    s.enable_tracing(true);
    s.write(0, 0x100, 1);
    s.read(1, 0x100);
    CHECK(!s.trace().empty());
    s.clear_trace();
    s.read(1, 0x100);
    CHECK_EQ(s.trace().size(), 1);
    CHECK_EQ(s.trace()[0].timestamp, 0);
    CHECK_EQ(s.trace()[0].program_idx, 0);
}

TEST("store buffer and hazard domain reject invalid configuration") {
    MesiSystem s(2);
    bool bad_depth = false;
    try { StoreBuffer sb(&s, 0, 0); }
    catch (const std::invalid_argument&) { bad_depth = true; }
    CHECK(bad_depth);

    bool bad_hazards = false;
    try { HazardDomain hp(0x100, 0); }
    catch (const std::invalid_argument&) { bad_hazards = true; }
    CHECK(bad_hazards);
}
