// One L1: cache_array + two mesi_fsm instances + bus request/commit glue.
// Single outstanding request: hits that need no bus complete locally, anything
// else raises a bus request and stalls until granted.
//
// The array has one write port, so at most one of {owner commit, snoop
// downgrade/invalidate, local write-hit} can land per cycle. A local write is
// deferred if a snoop needs the port that cycle — see the write mux below.
import mesi_pkg::*;

module l1_cache #(
    parameter int CORE_ID = 0
) (
    input  logic                  clk,
    input  logic                  rst_n,

    // CPU side
    input  logic                  cpu_req,      // a pending op is present
    input  logic                  cpu_we,       // 1 = write, 0 = read
    input  logic [ADDR_WIDTH-1:0] cpu_addr,
    input  logic [DATA_WIDTH-1:0] cpu_wdata,
    output logic                  cpu_done,     // op completes this cycle
    output logic [DATA_WIDTH-1:0] cpu_rdata,
    output logic                  stall,

    // request to bus
    output logic                  req_valid,
    output bus_txn_t              req_txn,
    output logic [ADDR_WIDTH-1:0] req_addr,

    // bus broadcast / grant in
    input  logic                  grant,        // this cache owns the bus now
    input  logic                  bus_active,
    input  bus_txn_t              bus_txn,
    input  logic [ADDR_WIDTH-1:0] bus_addr,
    input  logic                  shared_in,    // others hold the line (owner read)
    input  logic [DATA_WIDTH-1:0] supply_data,  // data to load (owner read)

    // snoop response out
    output logic                  snoop_had_copy,
    output logic                  snoop_dirty,
    output logic [DATA_WIDTH-1:0] snoop_data,

    // debug
    input  logic [ADDR_WIDTH-1:0] dbg_addr,
    output mesi_state_t           dbg_state,
    output logic [DATA_WIDTH-1:0] dbg_data,

    // perf pulses
    output logic                  ev_hit,
    output logic                  ev_miss,
    output logic                  ev_inv
);
    // ---- array ----
    logic        cpu_hit;  mesi_state_t cpu_state;  logic [DATA_WIDTH-1:0] cpu_data;
    logic        snp_hit;  mesi_state_t snp_state;  logic [DATA_WIDTH-1:0] snp_rdata;

    logic                  wr_en, wr_data_en;
    logic [ADDR_WIDTH-1:0] wr_addr;
    mesi_state_t           wr_state;
    logic [DATA_WIDTH-1:0] wr_data;

    cache_array u_arr (
        .clk(clk), .rst_n(rst_n),
        .rd_addr(cpu_addr), .rd_hit(cpu_hit), .rd_state(cpu_state), .rd_data(cpu_data),
        .snp_addr(bus_addr), .snp_hit(snp_hit), .snp_state(snp_state), .snp_data(snp_rdata),
        .dbg_addr(dbg_addr), .dbg_state(dbg_state), .dbg_data(dbg_data),
        .wr_en(wr_en), .wr_addr(wr_addr), .wr_state(wr_state),
        .wr_data_en(wr_data_en), .wr_data(wr_data)
    );

    // ---- processor-side FSM ----
    pr_req_t     pr;
    bus_txn_t    proc_req_txn;
    logic        needs_bus;
    mesi_state_t pr_next;
    mesi_state_t proc_snoop_next_unused; // proc FSM has no snoop input
    logic        proc_snoop_wb_unused;
    assign pr = !cpu_req ? PR_NONE : (cpu_we ? PR_WR : PR_RD);

    mesi_fsm u_fsm_proc (
        .cur_state(cpu_state), .pr_req(pr), .shared_in(shared_in),
        .snoop_txn(BUS_IDLE),
        .req_txn(proc_req_txn), .needs_bus(needs_bus), .pr_next_state(pr_next),
        .snoop_next_state(proc_snoop_next_unused), .snoop_wb(proc_snoop_wb_unused)
    );

    // ---- snoop-side FSM ----
    bus_txn_t    snoop_txn_in;
    mesi_state_t snoop_next;
    logic        snoop_wb;
    bus_txn_t    snoop_req_txn_unused;   // snoop FSM issues no request
    logic        snoop_needs_bus_unused;
    mesi_state_t snoop_pr_next_unused;
    assign snoop_txn_in = (bus_active && !grant) ? bus_txn : BUS_IDLE;

    mesi_fsm u_fsm_snoop (
        .cur_state(snp_state), .pr_req(PR_NONE), .shared_in(1'b0),
        .snoop_txn(snoop_txn_in),
        .req_txn(snoop_req_txn_unused), .needs_bus(snoop_needs_bus_unused),
        .pr_next_state(snoop_pr_next_unused),
        .snoop_next_state(snoop_next), .snoop_wb(snoop_wb)
    );

    // ---- bus request ----
    assign req_valid = cpu_req && needs_bus;
    assign req_txn   = proc_req_txn;
    assign req_addr  = cpu_addr;

    // ---- snoop response (only meaningful when bus_active && !grant) ----
    wire snoop_active = bus_active && !grant && snp_hit;
    assign snoop_had_copy = snoop_active;
    assign snoop_dirty    = snoop_active && (snp_state == MESI_M);
    assign snoop_data     = snp_rdata;

    // Does the snoop change our state (needs the write port)?
    wire snoop_write = (bus_active && !grant) && (snoop_next != snp_state);

    // ---- completion ----
    wire local_read_done  = cpu_req && !needs_bus && !cpu_we;
    wire local_write_done = cpu_req && !needs_bus &&  cpu_we && !snoop_write;
    assign cpu_done = grant || local_read_done || local_write_done;
    assign stall    = cpu_req && !cpu_done;

    always_comb begin
        cpu_rdata = '0;
        if (grant && !cpu_we)        cpu_rdata = supply_data; // owner read miss
        else if (local_read_done)    cpu_rdata = cpu_data;    // read hit
    end

    // ---- single array write per cycle ----
    always_comb begin
        wr_en = 1'b0; wr_addr = cpu_addr; wr_state = cpu_state;
        wr_data_en = 1'b0; wr_data = cpu_wdata;
        if (grant) begin
            // owner commit
            wr_en = 1'b1; wr_addr = cpu_addr; wr_state = pr_next;
            wr_data_en = 1'b1;
            wr_data = cpu_we ? cpu_wdata : supply_data;
        end else if (snoop_write) begin
            // snoop downgrade / invalidate (keep data)
            wr_en = 1'b1; wr_addr = bus_addr; wr_state = snoop_next; wr_data_en = 1'b0;
        end else if (local_write_done) begin
            // local write-hit: E->M or M write
            wr_en = 1'b1; wr_addr = cpu_addr; wr_state = MESI_M;
            wr_data_en = 1'b1; wr_data = cpu_wdata;
        end
    end

    // ---- perf events (pulse on completion / invalidation) ----
    assign ev_hit  = cpu_done && (cpu_state != MESI_I);
    assign ev_miss = cpu_done && (cpu_state == MESI_I);
    assign ev_inv  = snoop_write && (snoop_next == MESI_I);
endmodule
