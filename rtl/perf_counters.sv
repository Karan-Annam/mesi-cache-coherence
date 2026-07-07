// perf_counters.sv — bus + per-core performance counters.
import mesi_pkg::*;

module perf_counters (
    input  logic        clk,
    input  logic        rst_n,
    // bus taps
    input  logic        bus_active,
    input  bus_txn_t    perf_txn,
    input  logic        perf_wb,
    // per-core taps
    input  logic        ev_hit   [N_CORES],
    input  logic        ev_miss  [N_CORES],
    input  logic        ev_inv   [N_CORES],
    input  logic        stall    [N_CORES],
    // outputs
    output logic [31:0] bus_rd_count,
    output logic [31:0] bus_rdx_count,
    output logic [31:0] bus_upgr_count,
    output logic [31:0] bus_wb_count,
    output logic [31:0] bus_busy_cycles,
    output logic [31:0] l1_hit   [N_CORES],
    output logic [31:0] l1_miss  [N_CORES],
    output logic [31:0] inv_count[N_CORES],
    output logic [31:0] stall_cycles[N_CORES]
);
    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            bus_rd_count <= 0; bus_rdx_count <= 0; bus_upgr_count <= 0;
            bus_wb_count <= 0; bus_busy_cycles <= 0;
            for (int i = 0; i < N_CORES; i++) begin
                l1_hit[i] <= 0; l1_miss[i] <= 0; inv_count[i] <= 0; stall_cycles[i] <= 0;
            end
        end else begin
            if (bus_active) begin
                bus_busy_cycles <= bus_busy_cycles + 1;
                unique case (perf_txn)
                    BUS_RD:   bus_rd_count   <= bus_rd_count + 1;
                    BUS_RDX:  bus_rdx_count  <= bus_rdx_count + 1;
                    BUS_UPGR: bus_upgr_count <= bus_upgr_count + 1;
                    default:  ;
                endcase
            end
            if (perf_wb) bus_wb_count <= bus_wb_count + 1;
            for (int i = 0; i < N_CORES; i++) begin
                if (ev_hit[i])  l1_hit[i]      <= l1_hit[i] + 1;
                if (ev_miss[i]) l1_miss[i]     <= l1_miss[i] + 1;
                if (ev_inv[i])  inv_count[i]   <= inv_count[i] + 1;
                if (stall[i])   stall_cycles[i]<= stall_cycles[i] + 1;
            end
        end
    end
endmodule
