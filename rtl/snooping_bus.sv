// The snooping bus: arbitration, broadcast, snoop aggregation, writeback.
// One transaction per cycle. The winner's {txn, addr} goes to every cache;
// snoop responses fold into `shared`, and the owner is supplied either a dirty
// cache's data (intervention, also written back to memory) or memory's.
import mesi_pkg::*;

module snooping_bus (
    input  logic                  clk,
    input  logic                  rst_n,

    // per-core requests
    input  logic                  req_valid   [N_CORES],
    input  bus_txn_t              req_txn     [N_CORES],
    input  logic [ADDR_WIDTH-1:0] req_addr    [N_CORES],

    // per-core snoop responses
    input  logic                  snoop_had_copy [N_CORES],
    input  logic                  snoop_dirty    [N_CORES],
    input  logic [DATA_WIDTH-1:0] snoop_data     [N_CORES],

    // memory
    output logic [ADDR_WIDTH-1:0] mem_rd_addr,
    input  logic [DATA_WIDTH-1:0] mem_rd_data,
    output logic                  mem_wr_en,
    output logic [ADDR_WIDTH-1:0] mem_wr_addr,
    output logic [DATA_WIDTH-1:0] mem_wr_data,

    // broadcast to caches
    output logic                  grant      [N_CORES],
    output logic                  bus_active,
    output bus_txn_t              bus_txn,
    output logic [ADDR_WIDTH-1:0] bus_addr,
    output logic [$clog2(N_CORES)-1:0] bus_owner,
    output logic                  shared,
    output logic [DATA_WIDTH-1:0] supply_data,

    // perf taps
    output bus_txn_t              perf_txn,    // active transaction this cycle
    output logic                  perf_wb      // a writeback occurred this cycle
);
    logic [N_CORES-1:0] rv, grant_oh;
    logic               any_grant;
    logic [$clog2(N_CORES)-1:0] winner;

    always_comb for (int i = 0; i < N_CORES; i++) rv[i] = req_valid[i];

    bus_interface u_arb (
        .clk(clk), .rst_n(rst_n),
        .req_valid(rv), .grant_oh(grant_oh),
        .any_grant(any_grant), .winner(winner)
    );

    assign bus_active = any_grant;
    assign bus_owner  = winner;
    assign bus_txn    = any_grant ? req_txn[winner]  : BUS_IDLE;
    assign bus_addr   = any_grant ? req_addr[winner] : '0;

    always_comb for (int i = 0; i < N_CORES; i++) grant[i] = grant_oh[i];

    // Aggregate snoop responses (caches already gate their response with !grant).
    logic                  agg_shared, agg_dirty;
    logic [DATA_WIDTH-1:0] dirty_data;
    always_comb begin
        agg_shared = 1'b0;
        agg_dirty  = 1'b0;
        dirty_data = '0;
        for (int i = 0; i < N_CORES; i++) begin
            if (snoop_had_copy[i]) agg_shared = 1'b1;
            if (snoop_dirty[i]) begin
                agg_dirty  = 1'b1;
                dirty_data = snoop_data[i]; // exactly one owner can be dirty
            end
        end
    end

    assign shared      = agg_shared;
    assign mem_rd_addr = bus_addr;
    assign supply_data = agg_dirty ? dirty_data : mem_rd_data;

    // Writeback dirty intervention to memory on BusRd / BusRdX.
    wire is_fetch = bus_active && (bus_txn == BUS_RD || bus_txn == BUS_RDX);
    assign mem_wr_en   = is_fetch && agg_dirty;
    assign mem_wr_addr = bus_addr;
    assign mem_wr_data = dirty_data;

    assign perf_txn = bus_txn;
    assign perf_wb  = mem_wr_en;
endmodule
