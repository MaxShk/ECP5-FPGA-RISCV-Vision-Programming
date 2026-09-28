`timescale 1ns / 1ps

module SDRAM_Controller(
    input clk,
    input rst,
    output SDRAM_CLK,
    output SDRAM_CKE,
    
    
    output wire [12:0] SDRAM_A,
    output wire [1:0]  SDRAM_BA,
    output wire        WE,
    output wire        CAS,
    output wire        RAS,
    output wire        CS
);

    
    reg [3:0]  STATE;
    reg [15:0] COUNTER;

    localparam [3:0] BOOT_DELAY           = 4'd0;
    localparam [3:0] PRECHARGE            = 4'd1;
    localparam [3:0] WAIT_TRP             = 4'd2;
    localparam [3:0] REFRESH_1            = 4'd3;
    localparam [3:0] WAIT_TRFC_1          = 4'd4;
    localparam [3:0] REFRESH_2            = 4'd5;
    localparam [3:0] WAIT_TRFC_2          = 4'd6;
    localparam [3:0] LOAD_MODE_REGISTER   = 4'd7;
    localparam [3:0] WAIT_TMRD            = 4'd8;
    localparam [3:0] IDLE                 = 4'd9;

    
    localparam [3:0] NOP = 4'b0111;
    localparam [3:0] PRE = 4'b0010;
    localparam [3:0] REF = 4'b0001;
    localparam [3:0] LMR = 4'b0000;

    
    reg [3:0]  r_cmd;
    reg [12:0] r_addr;

    
    assign SDRAM_CKE = 1'b1;
    assign SDRAM_CLK = clk;
    
    
    assign {CS, RAS, CAS, WE} = r_cmd;
    assign SDRAM_A            = r_addr;
    assign SDRAM_BA           = 2'b00; 

    
    always @(posedge clk) begin
        if (!rst) begin
            COUNTER <= 0;
            STATE   <= BOOT_DELAY;
        end
        else begin
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
                    STATE <= IDLE; 
                end
                default: STATE <= IDLE;
            endcase
        end
    end

    
    always @(*) begin
        
        r_cmd  = NOP;
        r_addr = 13'd0;
        
        case(STATE)
            BOOT_DELAY: begin
                r_cmd  = NOP;
                r_addr = 13'd0;
            end
            PRECHARGE: begin
                r_cmd  = PRE;
                r_addr = 13'b001_0000_0000_00; 
            end
            REFRESH_1, REFRESH_2: begin
                r_cmd  = REF;
                r_addr = 13'd0;
            end
            LOAD_MODE_REGISTER: begin
                r_cmd  = LMR;
                r_addr = 13'b000000_011_0_011; 
            end
            default: begin 
                r_cmd  = NOP;
                r_addr = 13'd0;
            end
        endcase
    end

endmodule
