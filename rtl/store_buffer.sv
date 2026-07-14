// TSO store buffer: FIFO drain + load bypass + MFENCE. Stores park here
// (invisible to other cores) and drain to L1 in order; the owning CPU's loads
// check the buffer first, which is why a core sees its own stores early.
// RTL counterpart of model/tso.hpp in FIFO mode.
import mesi_pkg::*;

module store_buffer #(
    parameter int DEPTH  = 8,
    parameter int ADDR_W = 32,
    parameter int DATA_W = 32
) (
    input  logic              clk,
    input  logic              rst_n,
    // from CPU
    input  logic              st_valid,
    input  logic [ADDR_W-1:0] st_addr,
    input  logic [DATA_W-1:0] st_data,
    output logic              sb_full,
    // to L1 (drain)
    output logic              drain_valid,
    output logic [ADDR_W-1:0] drain_addr,
    output logic [DATA_W-1:0] drain_data,
    input  logic              drain_ready,
    // bypass read
    input  logic [ADDR_W-1:0] bypass_addr,
    output logic              bypass_hit,
    output logic [DATA_W-1:0] bypass_data,
    // MFENCE
    input  logic              fence_req,
    output logic              fence_ack,
    // status
    output logic [$clog2(DEPTH+1)-1:0] count
);
    localparam int PW   = $clog2(DEPTH);       // pointer width (DEPTH is power of 2)
    localparam int CNTW = $clog2(DEPTH+1);     // count width

    logic [ADDR_W-1:0] addr_q [DEPTH];
    logic [DATA_W-1:0] data_q [DEPTH];
    logic [PW-1:0]     head, tail;
    logic [CNTW-1:0]   cnt;

    assign count   = cnt;
    // A fence blocks younger stores until all older entries have drained.
    assign sb_full = (cnt == CNTW'(DEPTH)) || fence_req;
    assign drain_valid = (cnt != 0);
    assign drain_addr  = addr_q[head];
    assign drain_data  = data_q[head];
    assign fence_ack   = fence_req && (cnt == 0);

    // Bypass: newest matching entry wins. Scan from tail-1 back to head.
    always_comb begin
        int phys;
        bypass_hit  = 1'b0;
        bypass_data = '0;
        for (int k = 0; k < DEPTH; k++) begin
            // logical position from newest to oldest (pointers wrap mod DEPTH)
            phys = (int'(tail) - 1 - k + DEPTH) % DEPTH;
            if (CNTW'(k) < cnt && !bypass_hit && addr_q[phys] == bypass_addr) begin
                bypass_hit  = 1'b1;
                bypass_data = data_q[phys];
            end
        end
    end

    wire do_push = st_valid && !sb_full;
    wire do_pop  = drain_valid && drain_ready;

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            head <= '0; tail <= '0; cnt <= '0;
        end else begin
            if (do_push) begin
                addr_q[tail] <= st_addr;
                data_q[tail] <= st_data;
                tail <= tail + PW'(1); // wraps mod DEPTH (power of two)
            end
            if (do_pop) head <= head + PW'(1);
            case ({do_push, do_pop})
                2'b10: cnt <= cnt + CNTW'(1);
                2'b01: cnt <= cnt - CNTW'(1);
                default: ; // 00 or 11: unchanged
            endcase
        end
    end
endmodule
