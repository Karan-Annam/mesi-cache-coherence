// Shared parameters and types.
`ifndef MESI_PKG_SV
`define MESI_PKG_SV
package mesi_pkg;

    parameter int N_CORES    = 4;            // number of L1 caches
    parameter int CACHE_SETS = 64;           // sets per (direct-mapped) cache
    parameter int LINE_BYTES = 64;           // bytes per cache line
    parameter int ADDR_WIDTH = 32;
    parameter int DATA_WIDTH  = 32;

    parameter int OFFSET_BITS = $clog2(LINE_BYTES);          // 6
    parameter int INDEX_BITS  = $clog2(CACHE_SETS);          // 6
    parameter int TAG_BITS    = ADDR_WIDTH - INDEX_BITS - OFFSET_BITS; // 20
    parameter int LINE_ID_BITS = ADDR_WIDTH - OFFSET_BITS;   // tag+index

    // MESI state. Encoding matches the C++ model (M=11,E=10,S=01,I=00).
    typedef enum logic [1:0] {
        MESI_I = 2'b00,
        MESI_S = 2'b01,
        MESI_E = 2'b10,
        MESI_M = 2'b11
    } mesi_state_t;

    // Bus transaction type.
    typedef enum logic [2:0] {
        BUS_IDLE = 3'h0,
        BUS_RD   = 3'h1,   // BusRd
        BUS_RDX  = 3'h2,   // BusRdX
        BUS_UPGR = 3'h3,   // BusUpgr
        BUS_WB   = 3'h4    // writeback
    } bus_txn_t;

    // Processor request.
    typedef enum logic [1:0] {
        PR_NONE = 2'b00,
        PR_RD   = 2'b01,
        PR_WR   = 2'b10
    } pr_req_t;

    // Address field extractors.
    function automatic logic [INDEX_BITS-1:0] addr_index(logic [ADDR_WIDTH-1:0] a);
        return a[OFFSET_BITS +: INDEX_BITS];
    endfunction
    function automatic logic [TAG_BITS-1:0] addr_tag(logic [ADDR_WIDTH-1:0] a);
        return a[OFFSET_BITS+INDEX_BITS +: TAG_BITS];
    endfunction
    function automatic logic [LINE_ID_BITS-1:0] addr_line(logic [ADDR_WIDTH-1:0] a);
        return a[ADDR_WIDTH-1:OFFSET_BITS];
    endfunction

endpackage
`endif
