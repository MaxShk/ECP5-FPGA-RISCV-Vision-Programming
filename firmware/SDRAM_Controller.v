`timescale 1ns / 1ps

module SDRAM_Controller(
    input clk,
    input rst,
    input request_valid,         
    input is_write,                                    
    input [12:0] A_IN,          
    input [12:0] C_IN,          
    input [1:0]  BA_IN,         
    input [15:0] din_cpu,       
    output [15:0] dout_cpu,     
    output SDRAM_CLK,
    output SDRAM_CKE,
    output wire [12:0] SDRAM_A,
    output wire [1:0]  SDRAM_BA,
    output wire        WE,
    output wire        CAS,
    output wire        RAS,
    output wire        CS,
    inout [15:0]  SDRAM_DQ      
);

    
    reg [4:0]  STATE;
    reg [15:0] COUNTER;
    reg [2:0]  burst_counter;
    reg        r_is_write_latched;

    
    reg [9:0]  refresh_timer;
    reg        refresh_pending;

    localparam [4:0] BOOT_DELAY            = 5'd0;
    localparam [4:0] PRECHARGE             = 5'd1;
    localparam [4:0] WAIT_TRP              = 5'd2;
    localparam [4:0] REFRESH_1             = 5'd3;
    localparam [4:0] WAIT_TRFC_1           = 5'd4;
    localparam [4:0] REFRESH_2             = 5'd5;
    localparam [4:0] WAIT_TRFC_2           = 5'd6;
    localparam [4:0] LOAD_MODE_REGISTER    = 5'd7;
    localparam [4:0] WAIT_TMRD             = 5'd8;
    localparam [4:0] IDLE                  = 5'd9;
    localparam [4:0] ACTIVE_STATE          = 5'd10;
    localparam [4:0] WAIT_TRCD             = 5'd11;
    localparam [4:0] RW                    = 5'd12;
    localparam [4:0] WAIT_TWR              = 5'd13; 
    localparam [4:0] WAIT_TRP_POST         = 5'd14;
    localparam [4:0] REFRESH_PERIODIC      = 5'd15;
    localparam [4:0] WAIT_TRFC_PER         = 5'd16;

    localparam [3:0] NOP    = 4'b0111;
    localparam [3:0] PRE    = 4'b0010;
    localparam [3:0] REF    = 4'b0001;
    localparam [3:0] LMR    = 4'b0000;
    localparam [3:0] ACTIVE = 4'b0011;
    localparam [3:0] READ   = 4'b0101;
    localparam [3:0] WRITE  = 4'b0100;
    
    reg [3:0]  r_cmd;
    reg [12:0] r_addr;
    reg [1:0]  r_ba;

    assign SDRAM_CKE = 1'b1;
    assign SDRAM_CLK = clk;
    assign {CS, RAS, CAS, WE} = r_cmd;

    always @(posedge clk) begin
        if (!rst) begin
            COUNTER            <= 0;
            burst_counter      <= 0;
            r_is_write_latched <= 0;
            refresh_timer      <= 0;
            refresh_pending    <= 0;
            STATE              <= BOOT_DELAY;
        end
        else begin
            if (STATE != BOOT_DELAY) begin
                if (refresh_timer >= 10'd781) begin
                    refresh_timer   <= 0;
                    refresh_pending <= 1'b1;
                end else begin
                    refresh_timer <= refresh_timer + 1;
                end
            end

            case(STATE) 
                BOOT_DELAY: begin
                    COUNTER <= COUNTER + 1;
                    if (COUNTER == 10000) begin
                        STATE   <= PRECHARGE;
                        COUNTER <= 0;            
                    end
                end
                PRECHARGE: begin
                    STATE <= WAIT_TRP;
                end
                WAIT_TRP: begin
                    COUNTER <= COUNTER + 1;
                    if (COUNTER == 2) begin
                        COUNTER <= 0;
                        STATE   <= REFRESH_1;
                    end
                end
                REFRESH_1: begin
                   STATE <= WAIT_TRFC_1;
                end
                WAIT_TRFC_1: begin
                   COUNTER <= COUNTER + 1;
                   if (COUNTER == 6) begin
                       COUNTER <= 0;
                       STATE   <= REFRESH_2;
                   end
                end
                REFRESH_2: begin
                    STATE <= WAIT_TRFC_2;
                end
                WAIT_TRFC_2: begin
                    COUNTER <= COUNTER + 1;
                    if (COUNTER == 6) begin
                        COUNTER <= 0;
                        STATE   <= LOAD_MODE_REGISTER;
                    end 
                end
                LOAD_MODE_REGISTER: begin
                   STATE <= WAIT_TMRD;
                end 
                WAIT_TMRD: begin
                    COUNTER <= COUNTER + 1;
                    if (COUNTER == 2) begin
                        COUNTER <= 0;
                        STATE   <= IDLE;
                    end
                end
                IDLE: begin
                    if (refresh_pending) begin
                        refresh_pending <= 1'b0;
                        STATE           <= REFRESH_PERIODIC;
                    end else if (request_valid) begin
                        r_is_write_latched <= is_write;
                        STATE              <= ACTIVE_STATE;
                    end
                end
                REFRESH_PERIODIC: begin
                    STATE <= WAIT_TRFC_PER;
                end
                WAIT_TRFC_PER: begin
                    COUNTER <= COUNTER + 1;
                    if (COUNTER == 6) begin
                        COUNTER <= 0;
                        STATE   <= IDLE;
                    end
                end
                ACTIVE_STATE: begin
                    STATE <= WAIT_TRCD;
                    COUNTER <= 0; 
                end
                WAIT_TRCD: begin
                    COUNTER <= COUNTER + 1;
                    if (COUNTER == 2) begin
                        COUNTER       <= 0;
                        burst_counter <= 0;
                        STATE         <= RW;
                    end            
                end
                RW: begin
                    burst_counter <= burst_counter + 1;
                    if (burst_counter == 7) begin
                        burst_counter <= 0;
                        COUNTER       <= 0;
                        STATE         <= r_is_write_latched ? WAIT_TWR : WAIT_TRP_POST;
                    end
                end
                WAIT_TWR: begin
                    COUNTER <= COUNTER + 1;
                    if (COUNTER == 2) begin
                        COUNTER <= 0;
                        STATE   <= WAIT_TRP_POST;
                    end
                end
                WAIT_TRP_POST: begin
                    COUNTER <= COUNTER + 1;
                    if (COUNTER == 2) begin
                        COUNTER <= 0;
                        STATE   <= IDLE;
                    end
                end
            endcase
        end
    end

    
    wire write_output_active = (STATE == RW) && r_is_write_latched;
    assign SDRAM_DQ = write_output_active ? din_cpu : 16'hz;

    
    reg [15:0] r_dout;
    always @(posedge clk) begin
        if (!rst) begin
            r_dout <= 16'd0;
        end else if ((STATE == RW) && !r_is_write_latched && (burst_counter == 3'd2)) begin
            r_dout <= SDRAM_DQ; 
        end
    end
    assign dout_cpu = r_dout;

    
    always @(*) begin
        r_cmd  = NOP;
        r_addr = 13'd0;
        r_ba   = 2'b00;
        
        case(STATE)
            PRECHARGE: begin
                r_cmd  = PRE;
                r_addr = 13'b001_0000_0000_00; 
            end
            REFRESH_1, REFRESH_2, REFRESH_PERIODIC: begin
                r_cmd  = REF;
                r_addr = 13'd0;
            end
            LOAD_MODE_REGISTER: begin
                r_cmd  = LMR;
                r_addr = 13'b000000_010_0_011; 
            end
            ACTIVE_STATE, WAIT_TRCD: begin
                r_cmd  = (STATE == ACTIVE_STATE) ? ACTIVE : NOP;
                r_addr = A_IN;
                r_ba   = BA_IN;
            end
            RW: begin
                if (burst_counter == 3'd0) begin
                    r_cmd = r_is_write_latched ? WRITE : READ;
                end else begin
                    r_cmd = NOP;
                end
                r_addr = {4'b0000, C_IN[8:0]}; 
                r_ba   = BA_IN;
            end
            WAIT_TWR: begin
                r_cmd  = NOP;
                r_addr = 13'd0;
                r_ba   = BA_IN;
            end
            WAIT_TRP_POST: begin
                r_cmd  = PRE;
                r_addr = 13'b001_0000_0000_00; 
                r_ba   = BA_IN;
            end
            default: begin
                r_cmd  = NOP;
                r_addr = 13'd0;
                r_ba   = 2'b00;
            end
        endcase
    end

    
    assign SDRAM_A  = r_addr;
    assign SDRAM_BA = r_ba;

endmodule