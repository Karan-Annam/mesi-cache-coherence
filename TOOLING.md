# Tooling

What actually went into building, verifying, and writing this up.

## Build and verification

- Verilator 5.x for the RTL simulation
- g++ / C++17 for the behavioral model, the testbench, and the primitives
- GNU Make and bash, wired up as one `run_all.sh` that builds and runs
  everything: model tests, the demo runner, the RTL testbench, and the plots
- Python with matplotlib for the analysis plots, with an ASCII fallback so
  the suite still passes cleanly if matplotlib isn't installed
- git for the actual history of the work

Host-specific gotchas (MSYS2 PATH ordering, a broken Perl `verilator`
wrapper) are covered in [docs/BUILDING.md](docs/BUILDING.md).

## AI-assisted coding

I used Claude Code as a coding tool throughout this project, the way a lot of
people now pair-program with an LLM. I wrote the spec, ran the build, and
hand-wrote several of the trickier modules myself, including the MESI FSM's
race handling and the sequential-consistency checker. I read through and
understood everything else before it shipped.

One thing that came out of building this: the TSO litmus tests originally
assumed message passing needs a fence, which is a common misconception. A
FIFO store buffer preserves store-to-store order, so message passing is
actually fine unfenced under real x86-TSO. TSO only reorders store-to-load.
Fixed that, and the suite now covers both cases. More in
[docs/DESIGN.md](docs/DESIGN.md).

Happy to talk through any part of this in more depth, the MESI FSM, the SC
checker, the TSO store buffer, whatever's useful.
