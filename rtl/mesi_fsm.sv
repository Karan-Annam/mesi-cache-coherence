// Combinational MESI next-state logic. Two halves: the processor side decides
// what bus transaction a local PrRd/PrWr requires and where we land after it,
// the snoop side reacts to other caches' transactions (and flags writebacks).
import mesi_pkg::*;

module mesi_fsm (
    input  mesi_state_t cur_state,
    input  pr_req_t     pr_req,      // local pending processor request
    input  logic        shared_in,   // some other cache holds the line (for I->S/E)
    input  bus_txn_t    snoop_txn,   // transaction observed from another cache

    // processor side
    output bus_txn_t    req_txn,      // bus txn we must issue (BUS_IDLE = none)
    output logic        needs_bus,    // req_txn != BUS_IDLE
    output mesi_state_t pr_next_state,// our state after the op completes

    // snoop side
    output mesi_state_t snoop_next_state,
    output logic        snoop_wb      // we must write back (were Modified)
);

    // ---- processor side: which bus transaction is required ----
    always_comb begin
        req_txn = BUS_IDLE;
        unique case (pr_req)
            PR_RD: if (cur_state == MESI_I) req_txn = BUS_RD;
            PR_WR: unique case (cur_state)
                       MESI_I: req_txn = BUS_RDX;
                       MESI_S: req_txn = BUS_UPGR;
                       default: req_txn = BUS_IDLE; // E (silent), M (hit)
                   endcase
            default: req_txn = BUS_IDLE;
        endcase
    end
    assign needs_bus = (req_txn != BUS_IDLE);

    // ---- processor side: resulting state ----
    always_comb begin
        pr_next_state = cur_state;
        unique case (pr_req)
            PR_RD:
                if (cur_state == MESI_I)
                    pr_next_state = shared_in ? MESI_S : MESI_E;
            PR_WR:
                pr_next_state = MESI_M; // M/E/S/I all end in M on a write
            default: pr_next_state = cur_state;
        endcase
    end

    // ---- snoop side ----
    always_comb begin
        snoop_next_state = cur_state;
        snoop_wb = 1'b0;
        unique case (snoop_txn)
            BUS_RD: begin
                if (cur_state == MESI_M) begin
                    snoop_next_state = MESI_S; snoop_wb = 1'b1;
                end else if (cur_state == MESI_E) begin
                    snoop_next_state = MESI_S;
                end // S stays S, I stays I
            end
            BUS_RDX: begin
                if (cur_state == MESI_M) snoop_wb = 1'b1;
                if (cur_state != MESI_I) snoop_next_state = MESI_I;
            end
            BUS_UPGR: begin
                if (cur_state != MESI_I) snoop_next_state = MESI_I;
            end
            default: ; // BUS_IDLE / BUS_WB: no snoop effect
        endcase
    end

endmodule
