// Direct-mapped tag/state/data array for one L1. One payload word per line,
// three combinational read ports (CPU / snoop / debug), one sync write port.
import mesi_pkg::*;

module cache_array (
    input  logic                    clk,
    input  logic                    rst_n,

    // CPU lookup (combinational)
    input  logic [ADDR_WIDTH-1:0]   rd_addr,
    output logic                    rd_hit,
    output mesi_state_t             rd_state,
    output logic [DATA_WIDTH-1:0]   rd_data,

    // snoop lookup (combinational)
    input  logic [ADDR_WIDTH-1:0]   snp_addr,
    output logic                    snp_hit,
    output mesi_state_t             snp_state,
    output logic [DATA_WIDTH-1:0]   snp_data,

    // debug lookup (combinational)
    input  logic [ADDR_WIDTH-1:0]   dbg_addr,
    output mesi_state_t             dbg_state,
    output logic [DATA_WIDTH-1:0]   dbg_data,

    // synchronous update
    input  logic                    wr_en,
    input  logic [ADDR_WIDTH-1:0]   wr_addr,
    input  mesi_state_t             wr_state,
    input  logic                    wr_data_en,
    input  logic [DATA_WIDTH-1:0]   wr_data
);
    logic [TAG_BITS-1:0]   tag_arr   [CACHE_SETS];
    mesi_state_t           state_arr [CACHE_SETS];
    logic [DATA_WIDTH-1:0] data_arr  [CACHE_SETS];

    // Generic combinational lookup.
    function automatic mesi_state_t lk_state(logic [ADDR_WIDTH-1:0] a);
        logic [INDEX_BITS-1:0] idx = addr_index(a);
        if (tag_arr[idx] == addr_tag(a)) return state_arr[idx];
        else return MESI_I;
    endfunction

    wire [INDEX_BITS-1:0] rd_idx  = addr_index(rd_addr);
    wire [INDEX_BITS-1:0] snp_idx = addr_index(snp_addr);
    wire [INDEX_BITS-1:0] dbg_idx = addr_index(dbg_addr);

    assign rd_state  = lk_state(rd_addr);
    assign rd_hit    = (rd_state != MESI_I);
    assign rd_data   = data_arr[rd_idx];

    assign snp_state = lk_state(snp_addr);
    assign snp_hit   = (snp_state != MESI_I);
    assign snp_data  = data_arr[snp_idx];

    assign dbg_state = lk_state(dbg_addr);
    assign dbg_data  = data_arr[dbg_idx];

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            for (int i = 0; i < CACHE_SETS; i++) state_arr[i] <= MESI_I;
        end else if (wr_en) begin
            tag_arr[addr_index(wr_addr)]   <= addr_tag(wr_addr);
            state_arr[addr_index(wr_addr)] <= wr_state;
            if (wr_data_en) data_arr[addr_index(wr_addr)] <= wr_data;
        end
    end
endmodule
