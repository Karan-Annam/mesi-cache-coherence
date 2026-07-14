# Implementation Walkthrough

This is a file-level map of the project. Protocol and verification details are
in [DESIGN.md](DESIGN.md).

## Coherence model

- `model/cache_controller.cpp`: processor reads/writes, fills, upgrades, and
  snoop transitions for each modeled core.
- `model/snooping_bus.cpp`: transaction accounting, peer snoop broadcast, and
  optional randomized peer visitation.
- `model/mesi_system.hpp`: thread-safe facade and bus-serialized atomics.
- `model/sc_checker.cpp`: serialized-history and small-trace SC validation.
- `model/perf.hpp`: transaction, invalidation, and store-buffer counters.

The model stores all words registered within a 64-byte line. A dirty
intervention writes those values to memory before the requesting cache fills,
which keeps separate addresses on the same modeled line coherent.

## RTL datapath

```text
mesi_top
|-- l1_cache x4
|   |-- cache_array
|   |-- mesi_fsm
|   `-- bus_interface
|-- snooping_bus
|-- memory_controller
`-- perf_counters
```

`rtl/mesi_fsm.sv` is combinational. The cache and bus interface retain request
state and sequence fills, upgrades, and snoops. `sim/mesi_tb_top.sv` exposes the
four-core system plus standalone FSM, store-buffer, and CPU-stimulus instances
to `sim/tb_top.cpp`.

The RTL cache uses 32-bit payloads at 64-byte-spaced line addresses. The C++
model, rather than the RTL datapath, provides the sub-line false-sharing
measurement and the atomic/TSO integration tests.

## Software primitives

`primitives/dut.hpp` adapts a core ID and `MesiSystem` to the primitive API.
CAS and fetch-and-add retain the model's bus lock for the complete RMW. Lock,
seqlock, barrier, and stack tests then run from ordinary C++ threads.

The stack allocates every pushed node at a fresh address and never reclaims it.
`primitives/hazard_ptr.hpp` is a separate protected-pointer exercise and is not
the stack's reclamation layer.

## Measurement path

`model/model_main.cpp` regenerates the CSV files in `analysis/`. The lock plot
uses seven synchronized trials per core count, normalizes transactions by the
number of acquisitions, and plots median plus min/max. The false-sharing and
TSO plots use deterministic directed workloads. Plot scripts fall back to an
ASCII report if matplotlib is unavailable.
