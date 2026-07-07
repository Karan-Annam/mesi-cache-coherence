// Load/store stimulus generator. Holds each request until the cache reports
// done; MODE picks the address/RW pattern. Most tests drive the cache straight
// from C++ instead (cpu_driver.cpp), this is the in-RTL source.
import mesi_pkg::*;

module cpu_stub #(
    parameter int CORE_ID = 0,
    parameter logic [2:0] MODE = 3'd0  // 0=SEQUENTIAL 1=SHARED_RW 2=FALSE_SHARE
) (
    input  logic                  clk,
    input  logic                  rst_n,
    input  logic                  enable,
    input  logic [ADDR_WIDTH-1:0] base_addr,
    input  logic                  done,      // from cache: current op completed
    output logic                  req,
    output logic                  we,
    output logic [ADDR_WIDTH-1:0] addr,
    output logic [DATA_WIDTH-1:0] wdata,
    output logic [31:0]           issued     // number of ops issued so far
);
    logic [31:0] step;

    logic [ADDR_WIDTH-1:0] fs_off;
    assign fs_off = CORE_ID[0] ? ADDR_WIDTH'(4) : ADDR_WIDTH'(0);

    always_comb begin
        req   = enable;
        we    = 1'b0;
        addr  = base_addr;
        wdata = {CORE_ID[7:0], step[23:0]};
        unique case (MODE)
            3'd0: begin // SEQUENTIAL: walk addresses, alternating R/W
                addr = base_addr + (step << OFFSET_BITS);
                we   = step[0];
            end
            3'd1: begin // SHARED_RW: hammer the same line, write
                addr = base_addr;
                we   = 1'b1;
            end
            3'd2: begin // FALSE_SHARE: adjacent words in the same line
                addr = base_addr + fs_off;
                we   = 1'b1;
            end
            default: ;
        endcase
    end

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) step <= '0;
        else if (enable && done) step <= step + 1;
    end
    assign issued = step;
endmodule
