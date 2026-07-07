// Round-robin bus arbiter. One winner per cycle; the priority pointer steps
// past each winner so nobody starves. Assumes N_CORES is a power of two.
import mesi_pkg::*;

module bus_interface (
    input  logic               clk,
    input  logic               rst_n,
    input  logic [N_CORES-1:0] req_valid,
    output logic [N_CORES-1:0] grant_oh,
    output logic               any_grant,
    output logic [$clog2(N_CORES)-1:0] winner
);
    localparam int PW = $clog2(N_CORES);
    logic [PW-1:0] ptr;

    always_comb begin
        logic [PW-1:0] idx;
        grant_oh  = '0;
        any_grant = 1'b0;
        winner    = '0;
        for (int k = 0; k < N_CORES; k++) begin
            idx = ptr + PW'(k); // wraps mod N_CORES (power of two)
            if (!any_grant && req_valid[idx]) begin
                any_grant     = 1'b1;
                grant_oh[idx] = 1'b1;
                winner        = idx;
            end
        end
    end

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) ptr <= '0;
        else if (any_grant) ptr <= winner + PW'(1);
    end
endmodule
