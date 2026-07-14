# Design Notes

## MESI transitions

Each cache line is Modified, Exclusive, Shared, or Invalid. The model and RTL
implement the same processor and snoop rules:

| Current state | Event | Next state | Bus action |
|---|---|---|---|
| I | processor read, no peer copy | E | `BusRd` |
| I | processor read, peer has copy | S | `BusRd` |
| I | processor write | M | `BusRdX` |
| S | processor write | M | `BusUpgr` |
| E | processor write | M | none |
| M | snooped `BusRd` | S | dirty writeback |
| E | snooped `BusRd` | S | none |
| M/E/S | snooped `BusRdX` | I | M writes back |
| S | snooped `BusUpgr` | I | none |

Dirty intervention writes an M-state value to memory before the requesting
cache fills. A losing shared copy that snoops another core's `BusUpgr` becomes
Invalid, so any later write must request ownership with `BusRdX`.

## C++ model

`cache_controller.cpp` owns the per-core state transitions.
`snooping_bus.cpp` broadcasts one transaction to the peer controllers, updates
performance counters, and advances an observable arbitration pointer. The
behavioral API does not queue simultaneous requesters or use that pointer to
select a winner: `MesiSystem` serializes each complete read, write, or atomic
operation with one mutex. Threads can call the API concurrently, but their
modeled memory operations therefore have a total order.

The optional race-injection mode shuffles peer snoop visitation within that
serialized transaction. It catches accidental dependence on visitation order;
it does not create overlapping bus transactions.

The model registers every touched word within a 64-byte line, allowing a line
transfer or writeback to carry all known words. That is how the false-sharing
workload distinguishes adjacent words even though the RTL payload is narrower.

Tracing records the serialized operations and per-core program indices. The SC
checker validates reads against that order and also includes a brute-force
checker for small synthetic traces.

## RTL

`mesi_top.sv` instantiates four `l1_cache` blocks, the snooping bus, memory
controller, and counters. Each L1 contains a direct-mapped cache array and a
combinational `mesi_fsm`; request sequencing lives in the cache and bus
interface modules.

The Verilator wrapper also instantiates standalone copies of the MESI FSM,
store buffer, and CPU stimulus block. Directed checks cover the transition
table, dirty intervention, cache and bus behavior, the stimulus modes, and the
store-buffer FIFO, bypass, simultaneous push/pop, full, reset, and fence paths.

The RTL and C++ transition tables are intentionally aligned, but the current
testbench checks each implementation against directed expectations rather than
running a cycle-by-cycle differential comparison.

The current RTL cache has 64 direct-mapped sets and one 32-bit payload per
64-byte-addressed line. A conflicting fill does not write an evicted dirty line
back to memory. Tests avoid that conflict case. The standalone RTL store buffer
is not inserted between the CPU stimulus and L1 datapath.

## Atomics and synchronization

`MesiSystem::cas` and `fetch_and_add` acquire exclusive ownership and hold the
model's bus mutex over the complete read-modify-write. Fetch-and-add is a direct
bus-serialized RMW, not a CAS retry loop.

The primitive tests exercise TAS and ticket locks, a seqlock, a
sense-reversing barrier, and a Treiber stack from multiple OS threads. The stack
uses a monotonically increasing node allocator; popped nodes are not recycled,
which avoids ABA in this bounded test without providing memory reclamation.

`HazardDomain` is tested separately. Threads publish a protected pointer in
coherent memory, revalidate the source pointer, and clear their slots after use.
It demonstrates the coherence pattern used by hazard pointers but is not wired
into the Treiber stack.

## Store buffers and TSO

`model/tso.hpp` gives each modeled core an owner-thread-only store buffer.
Stores enqueue, loads search newest-to-oldest for a matching address, and an
MFENCE drains all entries in FIFO order. FIFO drain preserves store-to-store
order while allowing a later load to observe memory before an older store has
become visible to another core.

The store-buffering/Dekker harness deliberately schedules stores and loads so
the TSO-permitted outcome occurs in all 500 unfenced trials. Draining both
buffers at the fence removes that outcome in all 500 fenced trials. These
counts verify the modeled mechanism; they should not be interpreted as a
probability measured on physical hardware.

Message passing succeeds under FIFO drain because the data store reaches memory
before the flag store. A separate `RELAXED` mode randomly drains entries out of
order to model a weaker memory system; that mode can violate message passing,
and a full ordered drain repairs it.

`rtl/store_buffer.sv` implements the FIFO subset. While `fence_req` is asserted,
younger stores are blocked and `fence_ack` rises only after all older entries
have drained.

## Verification

`bash run_all.sh` performs four stages:

1. 41 C++ model, workload, atomic, primitive, and TSO tests.
2. The measurement driver and its internal assertions.
3. A Verilator build followed by 59 directed RTL checks.
4. Three analysis scripts; they write PNGs when matplotlib is installed and
   print the measured values otherwise.

`make lint` runs Verilator's lint-only path separately. There is currently no
FPGA/ASIC synthesis, post-route timing, power, or area result in this repository.
