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

AI-assisted tools were used for implementation support, debugging, and
documentation. I reviewed and modified the resulting RTL and C++ model and
validated them with the model and RTL test suites. The MESI FSM's race handling
and the sequential-consistency checker required several debugging passes.
