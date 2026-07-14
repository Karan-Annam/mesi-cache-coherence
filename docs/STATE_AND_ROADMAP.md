# Current State and Roadmap

## Verified now

| Component | Evidence |
|---|---|
| C++ MESI model and SC checker | 41-test C++ suite plus 50 measurement-driver stress runs |
| Atomics and synchronization primitives | concurrent correctness checks over the model |
| FIFO and relaxed store-buffer models | store-buffering, message-passing, bypass, and fence tests |
| Four-core RTL and standalone store buffer | Verilator lint and 59 directed checks |
| Measurement scripts | regenerated CSV data and three plots |

The RTL has been simulated and linted, but this repository does not currently
contain synthesis, place-and-route, timing, area, or hardware measurements.

## Next milestones

1. Connect `store_buffer.sv` between a CPU request interface and each RTL L1,
   then run the store-buffering and message-passing litmus tests end to end.
2. Add an RTL bus-lock/RMW path for CAS and demonstrate a hardware spinlock on
   the multicore top.
3. Add multiword RTL cache lines and dirty eviction writeback, with conflict
   miss and sub-line false-sharing tests.
4. Build a shared trace format and compare C++ and RTL architectural states
   after every completed transaction.
5. Synthesize a constrained FPGA target and report post-route timing and
   resource use.
6. Extend the litmus harness to SB, LB, MP, and IRIW across configurable memory
   models; compare a snooping and directory-based design at higher core counts.
