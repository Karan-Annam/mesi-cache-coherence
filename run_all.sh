#!/usr/bin/env bash
# Build and run the full test suite (C++ model + RTL). Exit 0 = everything passed.
set -u

# MSYS2 needs both its Unix utilities and UCRT64 toolchain. Leave Linux/macOS
# environments untouched and use the normal Verilator driver there.
if [ -d /c/msys64/ucrt64/bin ]; then
    export PATH="/c/msys64/ucrt64/bin:/usr/bin:$PATH"
    export VERILATOR_ROOT="${VERILATOR_ROOT:-C:/msys64/ucrt64/share/verilator}"
    VERILATOR_BIN="${VERILATOR_BIN:-verilator_bin.exe}"
else
    VERILATOR_BIN="${VERILATOR_BIN:-verilator}"
fi
ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"
mkdir -p build
# Keep compiler temporaries inside the repository. This also avoids relying on
# a writable global MSYS2 /tmp when the project runs in a restricted shell.
mkdir -p build/tmp
export TMPDIR="$ROOT/build/tmp"
export TMP="$TMPDIR"
export TEMP="$TMPDIR"

CXX=g++
CXXFLAGS="-std=c++17 -O2 -Wall -Wextra -pthread -I model -I sim -I primitives"

fail=0

echo "============================================================"
echo " STAGE 1 — C++ behavioral model + higher-layer test suite"
echo "============================================================"
MODEL_SRCS="model/cache_controller.cpp model/snooping_bus.cpp model/sc_checker.cpp"
TEST_SRCS=(sim/tests/*.cpp)
if $CXX $CXXFLAGS $MODEL_SRCS "${TEST_SRCS[@]}" -o build/model_tests; then
    if ./build/model_tests; then
        echo "[stage1] PASS"
    else
        echo "[stage1] FAIL (tests returned non-zero)"; fail=1
    fi
else
    echo "[stage1] FAIL (compilation error)"; fail=1
fi

echo
echo "============================================================"
echo " STAGE 2 — C++ behavioral model demo runner (model_main)"
echo "============================================================"
if $CXX $CXXFLAGS $MODEL_SRCS model/model_main.cpp -o build/model_sim; then
    if ./build/model_sim; then
        echo "[stage2] PASS"
    else
        echo "[stage2] FAIL (model_sim returned non-zero)"; fail=1
    fi
else
    echo "[stage2] FAIL (compilation error)"; fail=1
fi

echo
echo "============================================================"
echo " STAGE 3 — Verilator RTL build + testbench"
echo "============================================================"
if [ -f rtl/mesi_top.sv ] && [ -f sim/tb_top.cpp ]; then
    rm -rf obj_dir
    RTL="rtl/mesi_pkg.sv rtl/cache_array.sv rtl/mesi_fsm.sv rtl/bus_interface.sv \
         rtl/l1_cache.sv rtl/snooping_bus.sv rtl/memory_controller.sv \
         rtl/store_buffer.sv rtl/perf_counters.sv rtl/cpu_stub.sv rtl/mesi_top.sv \
         sim/mesi_tb_top.sv"
    if "$VERILATOR_BIN" --cc --exe --build -j 0 \
        -Wall -Wno-UNUSED -Wno-DECLFILENAME -Wno-IMPORTSTAR \
        --CFLAGS "-std=c++17 -I../sim" \
        --top-module mesi_tb_top --Mdir obj_dir -o mesi_sim \
        $RTL sim/tb_top.cpp sim/cpu_driver.cpp > build/verilator.log 2>&1; then
        if ./obj_dir/mesi_sim; then
            echo "[stage3] PASS"
        else
            echo "[stage3] FAIL (RTL testbench returned non-zero)"; fail=1
        fi
    else
        echo "[stage3] FAIL (Verilator build error — see build/verilator.log)"
        tail -30 build/verilator.log
        fail=1
    fi
else
    echo "[stage3] SKIP (RTL not present yet)"
fi

echo
echo "============================================================"
echo " STAGE 4 — Python analysis scripts"
echo "============================================================"
PY="$(command -v python || command -v python3)"
if [ -n "$PY" ]; then
    for s in plot_false_sharing plot_lock_scalability plot_tso_litmus; do
        if "$PY" analysis/$s.py; then echo "[$s] ok"; else echo "[$s] FAIL"; fail=1; fi
    done
else
    echo "[stage4] SKIP (no python found)"
fi

echo
echo "============================================================"
if [ "$fail" -eq 0 ]; then
    echo " RESULT: ALL STAGES PASSED"
else
    echo " RESULT: FAILURES PRESENT"
fi
echo "============================================================"
exit $fail
