// Main-memory backing store, one word per line. Indexed by the low line-id
// bits since tests only touch a sparse slice of the address space.
import mesi_pkg::*;

module memory_controller #(
    parameter int MEM_LINES = 1024
) (
    input  logic                  clk,
    input  logic                  rst_n,
    input  logic [ADDR_WIDTH-1:0] rd_addr,
    output logic [DATA_WIDTH-1:0] rd_data,
    input  logic                  wr_en,
    input  logic [ADDR_WIDTH-1:0] wr_addr,
    input  logic [DATA_WIDTH-1:0] wr_data
);
    localparam int IDX_BITS = $clog2(MEM_LINES);
    logic [DATA_WIDTH-1:0] mem [MEM_LINES];

    function automatic logic [IDX_BITS-1:0] midx(logic [ADDR_WIDTH-1:0] a);
        return a[OFFSET_BITS +: IDX_BITS];
    endfunction

    assign rd_data = mem[midx(rd_addr)];

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            for (int i = 0; i < MEM_LINES; i++) mem[i] <= '0;
        end else if (wr_en) begin
            mem[midx(wr_addr)] <= wr_data;
        end
    end
endmodule
