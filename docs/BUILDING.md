# Building and running

## Requirements

- Verilator 5.x (tested with 5.040)
- g++ with C++17 (tested with GCC 15, MSYS2 UCRT64)
- GNU Make, bash
- Python 3 for the analysis plots. matplotlib is optional — the scripts fall back
  to an ASCII summary if it isn't installed.

## Run everything

```bash
bash run_all.sh
```

Exit code 0 means all four stages passed:

1. C++ model + layers 3/4/5 test suite
2. model demo runner (writes the CSVs the analysis scripts read)
3. Verilator RTL build + testbench
4. analysis plots

Individual stages:

```bash
make model-tests   # C++ model + sync primitive + TSO tests
make model         # demo runner, regenerates analysis CSVs
make sim           # Verilator RTL testbench
make lint          # verilator --lint-only
make analysis      # render the plots
```

## Windows / MSYS2 notes

I develop this on Windows under MSYS2, which has two traps worth knowing about.
`run_all.sh` works around both automatically, but if you're running make targets
by hand:

1. **PATH ordering matters.** If `/mingw64/bin` comes before `/c/msys64/ucrt64/bin`
   on PATH, `cc1plus.exe` picks up incompatible gmp/mpfr DLLs from the mingw64 tree
   and dies with exit 1 and *no error message at all*. The symptom is bizarre:
   `g++ --version` works fine but every actual compile fails instantly with empty
   stderr. Fix: put `/c/msys64/ucrt64/bin` first.

2. **The `verilator` Perl wrapper can be broken** (`Can't locate Pod/Usage.pm`).
   The compiled driver works fine, so the build calls `verilator_bin.exe` directly
   (`VERILATOR ?= verilator_bin.exe` in the Makefile).

On Linux none of this applies — plain `verilator` + `g++` should just work.

Build artifacts land in `obj_dir/` and `build/`; both are gitignored.
