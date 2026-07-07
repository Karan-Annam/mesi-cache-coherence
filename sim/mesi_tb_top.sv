// Simulation top. Wraps mesi_top with packed ports (easier to poke from C++)
// and brings out standalone mesi_fsm / store_buffer / cpu_stub instances for
// unit testing.
import mesi_pkg::*;

module mesi_tb_top (
    input  logic clk,
    input  logic rst_n,

    // --- system CPU ports (packed) ---
    input  logic [N_CORES-1:0]              cpu_req,
    input  logic [N_CORES-1:0]              cpu_we,
    input  logic [N_CORES*ADDR_WIDTH-1:0]   cpu_addr,
    input  logic [N_CORES*DATA_WIDTH-1:0]   cpu_wdata,
    output logic [N_CORES-1:0]              cpu_done,
    output logic [N_CORES*DATA_WIDTH-1:0]   cpu_rdata,
    output logic [N_CORES-1:0]              cpu_stall,

    input  logic [ADDR_WIDTH-1:0]           dbg_addr,
    output logic [N_CORES*2-1:0]            dbg_state,
    output logic [N_CORES*DATA_WIDTH-1:0]   dbg_data,

    output logic [31:0] bus_rd_count,
    output logic [31:0] bus_rdx_count,
    output logic [31:0] bus_upgr_count,
    output logic [31:0] bus_wb_count,
    output logic [31:0] bus_busy_cycles,
    output logic [N_CORES*32-1:0] l1_hit,
    output logic [N_CORES*32-1:0] l1_miss,
    output logic [N_CORES*32-1:0] inv_count,
    output logic [N_CORES*32-1:0] stall_cycles,

    // --- mesi_fsm unit test (combinational) ---
    input  logic [1:0] fsm_cur_state,
    input  logic [1:0] fsm_pr_req,
    input  logic       fsm_shared_in,
    input  logic [2:0] fsm_snoop_txn,
    output logic [2:0] fsm_req_txn,
    output logic       fsm_needs_bus,
    output logic [1:0] fsm_pr_next,
    output logic [1:0] fsm_snoop_next,
    output logic       fsm_snoop_wb,

    // --- store_buffer unit test ---
    input  logic                  sb_st_valid,
    input  logic [ADDR_WIDTH-1:0] sb_st_addr,
    input  logic [DATA_WIDTH-1:0] sb_st_data,
    output logic                  sb_full,
    output logic                  sb_drain_valid,
    output logic [ADDR_WIDTH-1:0] sb_drain_addr,
    output logic [DATA_WIDTH-1:0] sb_drain_data,
    input  logic                  sb_drain_ready,
    input  logic [ADDR_WIDTH-1:0] sb_bypass_addr,
    output logic                  sb_bypass_hit,
    output logic [DATA_WIDTH-1:0] sb_bypass_data,
    input  logic                  sb_fence_req,
    output logic                  sb_fence_ack,
    output logic [31:0]           sb_count,

    // --- cpu_stub unit test ---
    input  logic                  stub_enable,
    input  logic [ADDR_WIDTH-1:0] stub_base,
    input  logic                  stub_done,
    output logic                  stub_req,
    output logic                  stub_we,
    output logic [ADDR_WIDTH-1:0] stub_addr,
    output logic [DATA_WIDTH-1:0] stub_wdata,
    output logic [31:0]           stub_issued
);
    // ---- unflatten packed CPU ports into unpacked arrays ----
    logic                  u_cpu_req   [N_CORES];
    logic                  u_cpu_we    [N_CORES];
    logic [ADDR_WIDTH-1:0] u_cpu_addr  [N_CORES];
    logic [DATA_WIDTH-1:0] u_cpu_wdata [N_CORES];
    logic                  u_cpu_done  [N_CORES];
    logic [DATA_WIDTH-1:0] u_cpu_rdata [N_CORES];
    logic                  u_cpu_stall [N_CORES];
    mesi_state_t           u_dbg_state [N_CORES];
    logic [DATA_WIDTH-1:0] u_dbg_data  [N_CORES];
    logic [31:0]           u_l1_hit    [N_CORES];
    logic [31:0]           u_l1_miss   [N_CORES];
    logic [31:0]           u_inv       [N_CORES];
    logic [31:0]           u_stall_cyc [N_CORES];

    genvar g;
    generate
        for (g = 0; g < N_CORES; g++) begin : gen_unpack
            assign u_cpu_req[g]   = cpu_req[g];
            assign u_cpu_we[g]    = cpu_we[g];
            assign u_cpu_addr[g]  = cpu_addr [g*ADDR_WIDTH +: ADDR_WIDTH];
            assign u_cpu_wdata[g] = cpu_wdata[g*DATA_WIDTH +: DATA_WIDTH];
            assign cpu_done[g]    = u_cpu_done[g];
            assign cpu_rdata[g*DATA_WIDTH +: DATA_WIDTH] = u_cpu_rdata[g];
            assign cpu_stall[g]   = u_cpu_stall[g];
            assign dbg_state[g*2 +: 2] = u_dbg_state[g];
            assign dbg_data[g*DATA_WIDTH +: DATA_WIDTH] = u_dbg_data[g];
            assign l1_hit  [g*32 +: 32] = u_l1_hit[g];
            assign l1_miss [g*32 +: 32] = u_l1_miss[g];
            assign inv_count[g*32 +: 32] = u_inv[g];
            assign stall_cycles[g*32 +: 32] = u_stall_cyc[g];
        end
    endgenerate

    mesi_top u_dut (
        .clk(clk), .rst_n(rst_n),
        .cpu_req(u_cpu_req), .cpu_we(u_cpu_we), .cpu_addr(u_cpu_addr),
        .cpu_wdata(u_cpu_wdata), .cpu_done(u_cpu_done), .cpu_rdata(u_cpu_rdata),
        .cpu_stall(u_cpu_stall),
        .dbg_addr(dbg_addr), .dbg_state(u_dbg_state), .dbg_data(u_dbg_data),
        .bus_rd_count(bus_rd_count), .bus_rdx_count(bus_rdx_count),
        .bus_upgr_count(bus_upgr_count), .bus_wb_count(bus_wb_count),
        .bus_busy_cycles(bus_busy_cycles),
        .l1_hit(u_l1_hit), .l1_miss(u_l1_miss), .inv_count(u_inv),
        .stall_cycles(u_stall_cyc)
    );

    // ---- standalone mesi_fsm for unit testing ----
    bus_txn_t fsm_req_txn_e, fsm_snoop_txn_e;
    mesi_state_t fsm_pr_next_e, fsm_snoop_next_e;
    assign fsm_snoop_txn_e = bus_txn_t'(fsm_snoop_txn);
    mesi_fsm u_fsm (
        .cur_state(mesi_state_t'(fsm_cur_state)),
        .pr_req(pr_req_t'(fsm_pr_req)),
        .shared_in(fsm_shared_in),
        .snoop_txn(fsm_snoop_txn_e),
        .req_txn(fsm_req_txn_e),
        .needs_bus(fsm_needs_bus),
        .pr_next_state(fsm_pr_next_e),
        .snoop_next_state(fsm_snoop_next_e),
        .snoop_wb(fsm_snoop_wb)
    );
    assign fsm_req_txn    = fsm_req_txn_e;
    assign fsm_pr_next    = fsm_pr_next_e;
    assign fsm_snoop_next = fsm_snoop_next_e;

    // ---- standalone store_buffer for unit testing ----
    logic [$clog2(8+1)-1:0] sb_count_small;
    store_buffer #(.DEPTH(8)) u_sb (
        .clk(clk), .rst_n(rst_n),
        .st_valid(sb_st_valid), .st_addr(sb_st_addr), .st_data(sb_st_data),
        .sb_full(sb_full),
        .drain_valid(sb_drain_valid), .drain_addr(sb_drain_addr),
        .drain_data(sb_drain_data), .drain_ready(sb_drain_ready),
        .bypass_addr(sb_bypass_addr), .bypass_hit(sb_bypass_hit),
        .bypass_data(sb_bypass_data),
        .fence_req(sb_fence_req), .fence_ack(sb_fence_ack),
        .count(sb_count_small)
    );
    assign sb_count = {{(32-$bits(sb_count_small)){1'b0}}, sb_count_small};

    // ---- standalone cpu_stub for unit testing (SEQUENTIAL mode) ----
    cpu_stub #(.CORE_ID(1), .MODE(3'd0)) u_stub (
        .clk(clk), .rst_n(rst_n), .enable(stub_enable), .base_addr(stub_base),
        .done(stub_done), .req(stub_req), .we(stub_we), .addr(stub_addr),
        .wdata(stub_wdata), .issued(stub_issued)
    );
endmodule
