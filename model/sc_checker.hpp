// Sequential-consistency checker for operation traces.
#pragma once
#include <string>
#include "trace.hpp"

namespace mesi {

struct SCResult {
    bool ok = true;
    std::string detail;
};

// Fast linearization check: the model records operations in their global bus
// serialization order (timestamps strictly increase, and each core's ops appear
// in program order). That recorded order is therefore a valid SC linearization
// *candidate*. This verifies it satisfies the read-from-latest-write rule:
// every read returns the value of the most recent write to the same address in
// serialization order. A failure here is a genuine coherence violation.
SCResult check_serial_order(const Trace& trace);

// General checker: exhaustively search for *some* total order that (a) respects
// each core's program order and (b) satisfies read-from-latest-write. Intended
// for small traces; used to validate the checker logic itself. Returns true if
// such an order exists.
bool exists_sc_order(const Trace& trace);

} // namespace mesi
