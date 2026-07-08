# Tooling

## Build and verification

- Verilator 5.x for the RTL simulation
- g++ / C++17 for the behavioral model, the testbench, and the primitives
- GNU Make and bash, one `run_all.sh` that builds and runs everything: model
  tests, the demo runner, the RTL testbench, and the plots
- Python with matplotlib for the plots (ASCII fallback if it's missing)
- git for the history

Host quirks (MSYS2 PATH ordering, a broken Perl `verilator` wrapper) are in
[docs/BUILDING.md](docs/BUILDING.md).

## AI use

Built with Claude Code from a spec and design I put together, alongside my
own work on the harder pieces, the MESI FSM's race handling and the
sequential-consistency checker in particular. I made the calls on scope and
what to verify, and went through the rest of the code closely rather than
taking it as-is.

Also fixed a real bug in the TSO litmus tests along the way: message passing
doesn't actually need a fence under a FIFO store buffer (a common
misconception), so the suite now tests that directly instead of assuming it
does. Details in [docs/DESIGN.md](docs/DESIGN.md).

Ask me about the MESI FSM, the SC checker, or the store buffer, happy to go
deeper on any of it.
