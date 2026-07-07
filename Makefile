# MESI cache coherence system.
# On MSYS2 use `bash run_all.sh` (fixes PATH ordering + calls verilator_bin.exe
# directly) — see docs/BUILDING.md.

VERILATOR     ?= verilator_bin.exe
VERILATOR_ROOT ?= C:/msys64/ucrt64/share/verilator
export VERILATOR_ROOT

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pthread -I model -I sim -I primitives

MODEL_SRCS = model/cache_controller.cpp model/snooping_bus.cpp model/sc_checker.cpp
TEST_SRCS  = $(wildcard sim/tests/*.cpp)

RTL = rtl/mesi_pkg.sv        \
      rtl/cache_array.sv     \
      rtl/mesi_fsm.sv        \
      rtl/bus_interface.sv   \
      rtl/l1_cache.sv        \
      rtl/snooping_bus.sv    \
      rtl/memory_controller.sv \
      rtl/store_buffer.sv    \
      rtl/perf_counters.sv   \
      rtl/cpu_stub.sv        \
      rtl/mesi_top.sv        \
      sim/mesi_tb_top.sv

VFLAGS = --cc --exe --build -j 0 \
         -Wall -Wno-UNUSED -Wno-DECLFILENAME -Wno-IMPORTSTAR \
         -CFLAGS "-std=c++17 -I../sim" \
         --top-module mesi_tb_top --Mdir obj_dir -o mesi_sim

.PHONY: all test model sim model-tests analysis lint clean

# Build + run everything (recommended).
all:
	bash run_all.sh

# C++ behavioral model + higher-layer tests.
model-tests:
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(MODEL_SRCS) $(TEST_SRCS) -o build/model_tests
	./build/model_tests

# Demo runner (emits analysis CSVs).
model:
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(MODEL_SRCS) model/model_main.cpp -o build/model_sim
	./build/model_sim

# Verilator RTL build + testbench.
sim:
	$(VERILATOR) $(VFLAGS) $(RTL) sim/tb_top.cpp sim/cpu_driver.cpp
	./obj_dir/mesi_sim

lint:
	$(VERILATOR) --lint-only -Wall -Wno-UNUSED -Wno-DECLFILENAME -Wno-IMPORTSTAR \
	    --top-module mesi_tb_top $(RTL)

analysis:
	python analysis/plot_false_sharing.py
	python analysis/plot_lock_scalability.py
	python analysis/plot_tso_litmus.py

clean:
	rm -rf obj_dir build analysis/*.png
