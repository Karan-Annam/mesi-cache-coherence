# Tooling

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

## Where I actually put in the work

The code and the docs in this repo, including this file, were built with AI
assistance (Claude Code) and then reviewed and edited by me, not typed out
from nothing by hand. I wrote the spec, drove the build, and made the calls
on scope and verification. I hand-wrote the modules where getting it wrong
would be easy to miss, in particular the MESI FSM's race handling (the
BusUpgr race is the one that trips people up) and the sequential-consistency
checker. Everything else I read closely enough to actually understand before
it shipped, not just skim past.

The clearest proof of that: the TSO litmus tests originally assumed message
passing needs a fence, which is a common claim people repeat without
checking it. A FIFO store buffer preserves store-to-store order, so message
passing is actually fine unfenced under real x86-TSO, TSO only reorders
store-to-load. I caught that mid-build and had it fixed properly: the suite
now proves MP passes unfenced under the FIFO store buffer, and only breaks
under a deliberately weakened RELAXED drain mode, where a fence repairs it.
Full writeup in [docs/DESIGN.md](docs/DESIGN.md).

Ask me about the MESI FSM, the SC checker, or the store buffer if you want to
go deeper, I can talk through any of it.
