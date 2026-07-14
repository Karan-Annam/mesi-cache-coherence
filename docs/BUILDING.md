# Building and Running

## Requirements

- C++17 compiler with pthread support
- GNU Make and Bash
- Verilator 5.x
- Python 3; matplotlib is optional because plot scripts have an ASCII fallback

## Full verification

```bash
bash run_all.sh
```

The runner returns nonzero if the C++ suite, measurement driver, Verilator
build/tests, or an analysis script fails.

Individual targets:

```bash
make model-tests   # C++ model, workload, primitive, and TSO tests
make model         # internal checks and regenerated analysis CSV files
make sim           # build and run the Verilator testbench
make lint          # Verilator lint-only pass
make analysis      # regenerate plots, or print ASCII data without matplotlib
```

Build artifacts are written to `build/` and `obj_dir/`.

## Windows/MSYS2

Run the commands from an MSYS2 shell or invoke its Bash explicitly. The runner
detects `/c/msys64/ucrt64/bin`, places the UCRT64 toolchain and `/usr/bin` first
on `PATH`, and selects `verilator_bin.exe`. This avoids mixing MinGW runtimes and
does not depend on the Perl `verilator` wrapper. Linux uses the normal
`verilator` executable.
