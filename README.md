# MESI Cache Coherence, from RTL to Synchronization

This project connects cache-coherence behavior to the software primitives built
on top of it. It contains an instrumented C++ MESI model, an independent
four-core SystemVerilog implementation, multithreaded synchronization tests,
and FIFO store-buffer models used to exercise TSO litmus tests.

## What is implemented

- A word-addressable C++ MESI model with `BusRd`, `BusRdX`, `BusUpgr`, dirty
  intervention, per-core counters, tracing, and a sequential-consistency checker.
- A four-core RTL system with a direct-mapped L1, MESI FSM, snooping bus, memory
  controller, performance counters, and a standalone FIFO store buffer.
- Bus-serialized CAS and fetch-and-add, then TAS/ticket locks, a seqlock, a
  barrier, a Treiber stack, and a separately tested hazard-pointer domain.
- FIFO TSO and deliberately weaker out-of-order drain models for store-buffering
  and message-passing litmus tests.

The C++ facade accepts calls from real OS threads, while a mutex gives each
modeled memory operation a total bus order. Randomized snoop visitation changes
the order in which a serialized transaction reaches peer caches; it is a stress
mode, not a model of simultaneous bus grants.

## Quick start

```bash
bash run_all.sh
```

The command builds and runs 41 C++ model/primitive tests, the measurement
driver, and 59 Verilator RTL checks, then renders the plots when matplotlib is
available. Requirements and individual targets are in
[docs/BUILDING.md](docs/BUILDING.md).

## Measured behavior

Two cores each perform 100 writes. Keeping their target words on one 64-byte
line produces 399 modeled bus transactions; placing the words on separate lines
produces 2.

![False-sharing traffic](docs/img/false_sharing.png)

For a TAS spinlock, seven trials at each core count show the median modeled bus
traffic per acquisition rising from 0.0040 with one core to 0.0455 with four.
The
error bars show the measured min/max range rather than hiding scheduler
variation.

![TAS spinlock traffic](docs/img/lock_scalability.png)

In the controlled store-buffering harness, all 500 unfenced trials expose the
permitted TSO outcome and none of 500 fenced trials do. This is a deterministic
model test, not an empirical violation rate measured on an x86 processor.

![Controlled TSO litmus](docs/img/tso_litmus.png)

The measurement driver also checks 100 ping-pong rounds (200 `BusRdX`, 199
writebacks) and a four-core reader workload (4 `BusRd`, 0 `BusRdX`). Raw data is
written under `analysis/` by `make model`.

## Design scope

The C++ and RTL implementations share the MESI transition rules but are tested
independently; this repository does not claim cycle-by-cycle differential
verification. The RTL cache stores one 32-bit payload per 64-byte line and does
not write back a dirty line on a conflicting-set eviction. Atomics and the TSO
end-to-end tests run on the C++ model. `rtl/store_buffer.sv` is verified as a
standalone unit and is not connected to the full cache datapath.

The Treiber stack uses monotonic allocation and never reuses popped nodes, so
its test avoids ABA without reclamation. The hazard-pointer domain is a separate
coherence workload rather than the stack's allocator.

See [docs/DESIGN.md](docs/DESIGN.md) for implementation details and
[docs/STATE_AND_ROADMAP.md](docs/STATE_AND_ROADMAP.md) for the next milestones.

## Repository layout

```text
model/        C++ MESI model, SC checker, and TSO store buffer
rtl/          SystemVerilog cache, FSM, bus, memory, and store buffer
sim/          C++/Verilator tests and coherence workloads
primitives/   synchronization and lock-free data-structure exercises
analysis/     CSV measurements and plot scripts
docs/         design/build notes and generated figures
```

## AI Use and Tooling

AI-assisted tools were used for implementation support, debugging, and
documentation. I reviewed the resulting C++ and RTL and validated the behavior
with the model and Verilator suites described above. Tool versions and host
notes are recorded in [TOOLING.md](TOOLING.md).
