`timescale 1ns / 1ps

module SDRAM_INITIALIZATION_TB;
    reg clk;
    reg rst;
    
    wire SDRAM_CLK;
    wire SDRAM_CKE;
    wire [12:0] SDRAM_A;
    wire [1:0]  SDRAM_BA;
    wire WE;
    wire CAS;
    wire RAS;
    wire CS;

    
    SDRAM_Controller dut(
        .clk(clk),
        .rst(rst),
        .SDRAM_CLK(SDRAM_CLK),
        .SDRAM_CKE(SDRAM_CKE),
        .SDRAM_A(SDRAM_A),
        .SDRAM_BA(SDRAM_BA),
        .WE(WE),
        .CAS(CAS),
        .RAS(RAS),
        .CS(CS)
    );

    
    always #5 clk = ~clk;

    // Text Monitor for Terminal Output
    initial begin
        $display("-----------------------------------------------------------------------------");
        $display("   TIME   | RST | STATE |  CMD (CS_RAS_CAS_WE)  |  SDRAM_A (HEX) | SDRAM_BA ");
        $display("-----------------------------------------------------------------------------");
        $monitor("%8d ns |  %1b  |   %2d  |          4'b%1b%1b%1b%1b         |     13'h%3h    |    2'b%2b", 
                 $time, rst, dut.STATE, CS, RAS, CAS, WE, SDRAM_A, SDRAM_BA);
    end

    
    initial begin
        $dumpfile("sdram_init_waveform.vcd");
        $dumpvars(0, SDRAM_INITIALIZATION_TB);

        clk = 0;
        rst = 0; 
        #20;    
        
        rst = 1; 
        
        
        #105000; 
        
        $display("-----------------------------------------------------------------------------");
        $display("Simulation complete");
        $display("-----------------------------------------------------------------------------");
        $finish;
    end
    
endmodule
