# Design notes

This is a tour of how the project is put together and why. If you just want to
run it, see [BUILDING.md](BUILDING.md).

The project is built as five layers, each one sitting on the one below:

1. a C++ behavioral model of MESI (protocol correctness, checked by a
   sequential-consistency checker)
2. the same protocol in synthesizable SystemVerilog with a snooping bus
3. a workload library that stresses specific protocol behaviors
4. atomics (CAS / fetch-and-add) and synchronization primitives built on them
5. a TSO store buffer and litmus tests that show why memory fences exist

The point of doing all five in one project: most people either know the hardware
side (MESI state machines) or the software side (`std::atomic`, spinlocks) but
not how one produces the other. Building the stack end to end forces you to see,
for example, exactly why a CAS is a bus-locked read-modify-write, or why false
sharing costs what it costs.

## The protocol

Standard MESI. Every line in every L1 is Modified, Exclusive, Shared, or
Invalid. The interesting rules:

| From | Event | To | Bus |
|------|-------|----|-----|
| I | PrRd, nobody else has it | E | BusRd |
| I | PrRd, someone has it | S | BusRd |
| I | PrWr | M | BusRdX |
| S | PrWr | M | BusUpgr |
| E | PrWr | M | (silent — the whole point of E) |
| M | snooped BusRd | S | BusWB first (dirty intervention) |
| M/E/S | snooped BusRdX | I | — |
| S | snooped BusUpgr | I | — |

The correctness challenge is the races. Two caches can decide to issue a request
in the same cycle; the bus serializes them, and the loser has to react to the
winner's transaction correctly. The two that matter:

- **Simultaneous BusRdX**: both in I, both want to write. One wins arbitration,
  the other snoops the winning BusRdX and must retry from scratch.
- **BusUpgr race**: both in S, both want to upgrade. The loser sees the winning
  BusUpgr, drops to I — and now it can't just retry the BusUpgr, because it no
  longer holds the line. It has to re-issue as a full BusRdX. Getting this wrong
  is the classic MESI implementation bug.

Dirty intervention is the third tricky case: a BusRd hitting a line that some
other cache holds in M. The M-holder has to write back before the requester can
be served, or the requester reads stale memory.

## Layer 1 — the C++ model (`model/`)

`cache_controller.{hpp,cpp}` implements every transition above per core;
`snooping_bus.{hpp,cpp}` serializes transactions with a round-robin arbitration
pointer and broadcasts snoops. There's also a race-injection mode that randomizes
snoop ordering, which is how I gained confidence the race handling is right —
the SC check runs 50 times, some with injection, and any violation fails the
suite.

The piece that makes the upper layers work is `mesi_system.hpp`. It wraps the
whole model in a thread-safe facade — `read / write / cas / fetch_and_add /
fence`, serialized by a bus mutex — so **real OS threads** can hammer the
coherent memory concurrently. That's how the lock and lock-free tests get real
interleavings rather than simulated ones, while the model keeps exact coherence
accounting underneath.

The SC checker (`sc_checker.{hpp,cpp}`) verifies that every read returns the
value of the most recent write in some total order consistent with each core's
program order. Since the bus already produces a total order, the fast path just
linearizes against the recorded serialization; a brute-force search over small
traces exists mostly to validate the checker itself.

## Layer 2 — the RTL (`rtl/`)

```
mesi_top (N=4)
├── snooping_bus       arbitration + snoop aggregation + dirty writeback
├── memory_controller
└── l1_cache ×4
    ├── cache_array    direct-mapped tag/state/data, 3 read ports
    ├── mesi_fsm       pure combinational next-state/action logic
    ├── bus_interface  request issue + round-robin grant handling
    └── cpu_stub       stimulus: SEQUENTIAL / SHARED_RW / FALSE_SHARE
```

The FSM mirrors the C++ model transition for transition (same state encoding,
deliberately). The bus resolves one transaction per cycle — centralized and
simple, but it genuinely serializes, which is all the races need.

Verilator's top is a thin wrapper (`sim/mesi_tb_top.sv`) that instantiates the
real `mesi_top` plus standalone copies of `mesi_fsm`, `store_buffer`, and
`cpu_stub` for unit testing, with packed ports so the C++ testbench can poke
them cleanly. 58 checks cover every directed transition (I→E, I→S, silent E→M,
S→M via BusUpgr, I→M, and both dirty-intervention paths), the full FSM table,
the store buffer, and the stimulus modes.

Simplifications I chose and would fix in a longer project:

- Lines hold one 32-bit word. Every transition is exercised, but sub-line
  false-sharing effects are quantified in the C++ model (which tracks words
  within a line), not the RTL.
- No eviction writeback — a conflicting fill overwrites the set. Tests use
  distinct sets to stay away from it.
- The atomics (layer 4) and the end-to-end TSO demos (layer 5) run against the
  C++ model, not the RTL datapath. `store_buffer.sv` is implemented and
  unit-tested standalone, but splicing it between `cpu_stub` and `l1_cache` is
  future work, as is wiring CAS pins through the RTL pipeline.

## Layer 3 — workloads (`sim/workloads.hpp`)

Ping-pong (two cores alternately writing one line), false sharing (padded vs
unpadded), producer/consumer, reader storm, and a sense-reversing barrier. Each
exists to light up one protocol behavior in the counters. Measured numbers are
in the README; the one worth repeating is false sharing: 399 bus transactions
per 100 iterations sharing a line, 2 when padded one line apart. That ~200×
cliff at a 64-byte boundary is the whole false-sharing lesson in one number.

## Layer 4 — atomics and primitives (`primitives/`)

`MesiSystem::cas` does exactly what hardware CAS does: BusRdX to get the line in
M, hold the bus lock across the compare and the conditional write, release.
Fetch-and-add is a CAS retry loop. On top of those: a test-and-set spinlock, a
ticket lock, a seqlock, hazard pointers, and a Treiber lock-free stack — all
exercised by concurrent OS threads through the coherent model, checked for lost
or duplicated values.

## Layer 5 — TSO (`model/tso.hpp`, `rtl/store_buffer.sv`)

Each core gets a store buffer: stores enter it instead of the cache, drain
asynchronously, and loads check it first (so a core sees its own stores early).
That single structure is what makes x86-TSO weaker than sequential consistency,
and Dekker's algorithm is the litmus test: without fences, both cores read the
other's stale flag and mutual exclusion breaks **100% of the time** in this
implementation; insert MFENCE (drain the buffer before the load) and it breaks
never.

One correction I had to make while building this: the common claim that message
passing (write data, write flag / read flag, read data) needs a fence under TSO
is wrong. A FIFO store buffer preserves store→store order, so MP is correctly
ordered on real x86 without any fence — TSO's only relaxation is store→load.
The suite asserts MP *passes* under the FIFO buffer, then demonstrates the
violation under an added RELAXED drain mode (a deliberately weaker model), and
shows the fence repairing it there.

## Verification approach

Same story at every layer: a golden reference and a diff.

- The C++ model is checked by the SC checker (linearization + race injection).
- The RTL is checked against directed expectations that mirror the model's
  transition table, using the same state encoding.
- The threaded tests (layers 3–5) are run 10× back to back in CI-style to shake
  out flakiness; results were stable.

The full suite is `bash run_all.sh` — exit 0 means the model tests (38), the
demo run, the RTL testbench (58 checks), and the plots all succeeded.
