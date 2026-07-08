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

I wrote the RTL and the C++ model with AI assistance, then debugged it
myself and read through it to modify things as needed, the MESI FSM's race
handling and the sequential-consistency checker in particular took a few
passes to get right.

Ask me about the MESI FSM, the SC checker, or the store buffer, happy to go
deeper on any of it.
