// The full system: N L1 caches on a snooping bus, plus memory and counters.
// CPU ports come out at the top so the testbench can drive exact load/store
// scenarios and read back per-core state.
import mesi_pkg::*;

module mesi_top (
    input  logic                  clk,
    input  logic                  rst_n,

    // CPU ports (one per core)
    input  logic                  cpu_req   [N_CORES],
    input  logic                  cpu_we    [N_CORES],
    input  logic [ADDR_WIDTH-1:0] cpu_addr  [N_CORES],
    input  logic [DATA_WIDTH-1:0] cpu_wdata [N_CORES],
    output logic                  cpu_done  [N_CORES],
    output logic [DATA_WIDTH-1:0] cpu_rdata [N_CORES],
    output logic                  cpu_stall [N_CORES],

    // debug: inspect each core's state/data for one address
    input  logic [ADDR_WIDTH-1:0] dbg_addr,
    output mesi_state_t           dbg_state [N_CORES],
    output logic [DATA_WIDTH-1:0] dbg_data  [N_CORES],

    // performance counters
    output logic [31:0] bus_rd_count,
    output logic [31:0] bus_rdx_count,
    output logic [31:0] bus_upgr_count,
    output logic [31:0] bus_wb_count,
    output logic [31:0] bus_busy_cycles,
    output logic [31:0] l1_hit       [N_CORES],
    output logic [31:0] l1_miss      [N_CORES],
    output logic [31:0] inv_count    [N_CORES],
    output logic [31:0] stall_cycles [N_CORES]
);
    // inter-module nets
    logic                  req_valid [N_CORES];
    bus_txn_t              req_txn   [N_CORES];
    logic [ADDR_WIDTH-1:0] req_addr  [N_CORES];
    logic                  snoop_had_copy [N_CORES];
    logic                  snoop_dirty    [N_CORES];
    logic [DATA_WIDTH-1:0] snoop_data     [N_CORES];
    logic                  grant     [N_CORES];
    logic                  ev_hit    [N_CORES];
    logic                  ev_miss   [N_CORES];
    logic                  ev_inv    [N_CORES];

    logic                  bus_active;
    bus_txn_t              bus_txn;
    logic [ADDR_WIDTH-1:0] bus_addr;
    logic [$clog2(N_CORES)-1:0] bus_owner;
    logic                  shared;
    logic [DATA_WIDTH-1:0] supply_data;
    bus_txn_t              perf_txn;
    logic                  perf_wb;

    // memory
    logic [ADDR_WIDTH-1:0] mem_rd_addr, mem_wr_addr;
    logic [DATA_WIDTH-1:0] mem_rd_data, mem_wr_data;
    logic                  mem_wr_en;

    genvar g;
    generate
        for (g = 0; g < N_CORES; g++) begin : gen_core
            l1_cache #(.CORE_ID(g)) u_l1 (
                .clk(clk), .rst_n(rst_n),
                .cpu_req(cpu_req[g]), .cpu_we(cpu_we[g]),
                .cpu_addr(cpu_addr[g]), .cpu_wdata(cpu_wdata[g]),
                .cpu_done(cpu_done[g]), .cpu_rdata(cpu_rdata[g]), .stall(cpu_stall[g]),
                .req_valid(req_valid[g]), .req_txn(req_txn[g]), .req_addr(req_addr[g]),
                .grant(grant[g]), .bus_active(bus_active), .bus_txn(bus_txn),
                .bus_addr(bus_addr), .shared_in(shared), .supply_data(supply_data),
                .snoop_had_copy(snoop_had_copy[g]), .snoop_dirty(snoop_dirty[g]),
                .snoop_data(snoop_data[g]),
                .dbg_addr(dbg_addr), .dbg_state(dbg_state[g]), .dbg_data(dbg_data[g]),
                .ev_hit(ev_hit[g]), .ev_miss(ev_miss[g]), .ev_inv(ev_inv[g])
            );
        end
    endgenerate

    snooping_bus u_bus (
        .clk(clk), .rst_n(rst_n),
        .req_valid(req_valid), .req_txn(req_txn), .req_addr(req_addr),
        .snoop_had_copy(snoop_had_copy), .snoop_dirty(snoop_dirty), .snoop_data(snoop_data),
        .mem_rd_addr(mem_rd_addr), .mem_rd_data(mem_rd_data),
        .mem_wr_en(mem_wr_en), .mem_wr_addr(mem_wr_addr), .mem_wr_data(mem_wr_data),
        .grant(grant), .bus_active(bus_active), .bus_txn(bus_txn), .bus_addr(bus_addr),
        .bus_owner(bus_owner), .shared(shared), .supply_data(supply_data),
        .perf_txn(perf_txn), .perf_wb(perf_wb)
    );

    memory_controller #(.MEM_LINES(1024)) u_mem (
        .clk(clk), .rst_n(rst_n),
        .rd_addr(mem_rd_addr), .rd_data(mem_rd_data),
        .wr_en(mem_wr_en), .wr_addr(mem_wr_addr), .wr_data(mem_wr_data)
    );

    perf_counters u_perf (
        .clk(clk), .rst_n(rst_n),
        .bus_active(bus_active), .perf_txn(perf_txn), .perf_wb(perf_wb),
        .ev_hit(ev_hit), .ev_miss(ev_miss), .ev_inv(ev_inv), .stall(cpu_stall),
        .bus_rd_count(bus_rd_count), .bus_rdx_count(bus_rdx_count),
        .bus_upgr_count(bus_upgr_count), .bus_wb_count(bus_wb_count),
        .bus_busy_cycles(bus_busy_cycles),
        .l1_hit(l1_hit), .l1_miss(l1_miss), .inv_count(inv_count),
        .stall_cycles(stall_cycles)
    );

    // silence unused
    wire _unused = &{1'b0, bus_owner};
endmodule
