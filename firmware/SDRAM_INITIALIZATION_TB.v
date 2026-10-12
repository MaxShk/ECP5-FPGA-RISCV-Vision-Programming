`timescale 1ns / 1ps

module SDRAM_INITIALIZATION_TB;

    // Inputs
    reg clk;
    reg rst;
    reg request_valid;
    reg is_write;
    reg [12:0] A_IN;
    reg [12:0] C_IN;
    reg [1:0]  BA_IN;
    reg [15:0] din_cpu;

    // Outputs
    wire [15:0] dout_cpu;
    wire SDRAM_CLK;
    wire SDRAM_CKE;
    wire [12:0] SDRAM_A;
    wire [1:0]  SDRAM_BA;
    wire WE;
    wire CAS;
    wire RAS;
    wire CS;
    
    // Bidirectional
    wire [15:0] SDRAM_DQ;

    // Instantiate the SDRAM Controller
    SDRAM_Controller uut (
        .clk(clk),
        .rst(rst),
        .request_valid(request_valid),
        .is_write(is_write),
        .A_IN(A_IN),
        .C_IN(C_IN),
        .BA_IN(BA_IN),
        .din_cpu(din_cpu),
        .dout_cpu(dout_cpu),
        .SDRAM_CLK(SDRAM_CLK),
        .SDRAM_CKE(SDRAM_CKE),
        .SDRAM_A(SDRAM_A),
        .SDRAM_BA(SDRAM_BA),
        .WE(WE),
        .CAS(CAS),
        .RAS(RAS),
        .CS(CS),
        .SDRAM_DQ(SDRAM_DQ)
    );

    mt48lc16m16a2 mem_model (
        .Dq(SDRAM_DQ),
        .Addr(SDRAM_A),
        .Ba(SDRAM_BA),
        .Clk(SDRAM_CLK),
        .Cke(SDRAM_CKE),
        .Cs_n(CS),
        .Ras_n(RAS),
        .Cas_n(CAS),
        .We_n(WE),
        .Dqm(2'b00)
    );


    always #5 clk = ~clk;

    initial begin
        $monitor("Time=%0t ns | STATE=%0d | COUNTER=%0d", $time, uut.STATE, uut.COUNTER);
    end

    initial begin
        clk = 0;
        rst = 0;          
        request_valid = 0;
        is_write = 0;
        A_IN = 0;
        C_IN = 0;
        BA_IN = 0;
        din_cpu = 0;

        // Dump waveforms for GTKWave
        $dumpfile("sim_sdram.vcd");
        $dumpvars(0, SDRAM_INITIALIZATION_TB);

       
        #100;
        rst = 1;


        #105000;

        @(posedge clk);
        BA_IN         = 2'b00;
        A_IN          = 13'h0020; 
        C_IN          = 13'h0050; 
        din_cpu       = 16'hCAFE;
        is_write      = 1'b1;
        request_valid = 1'b1;
        
        @(posedge clk);
        request_valid = 1'b0;

        @(posedge clk);
        wait(uut.STATE == 5'd9); // IDLE state
        #20;

        @(posedge clk);
        BA_IN         = 2'b01;
        A_IN          = 13'h0030; 
        C_IN          = 13'h0060; 
        din_cpu       = 16'hBEEF;
        is_write      = 1'b1;
        request_valid = 1'b1;
        
        @(posedge clk);
        request_valid = 1'b0;

        @(posedge clk);
        wait(uut.STATE == 5'd9); 
        #20;

        @(posedge clk);
        BA_IN         = 2'b00;
        A_IN          = 13'h0020; 
        C_IN          = 13'h0050; 
        din_cpu       = 16'h0000;
        is_write      = 1'b0;
        request_valid = 1'b1;

        @(posedge clk);
        request_valid = 1'b0;

        @(posedge clk);
        wait(uut.STATE == 5'd9); 
        #20;
        
        if (dout_cpu == 16'hCAFE) begin
            $display("SUCCESS: Bank 0 read data matches written pattern (0xCAFE)!");
        end else begin
            $display("ERROR: Bank 0 expected 0xCAFE, got 0x%h", dout_cpu);
        end

        @(posedge clk);
        BA_IN         = 2'b01;
        A_IN          = 13'h0030; 
        C_IN          = 13'h0060; 
        din_cpu       = 16'h0000;
        is_write      = 1'b0;
        request_valid = 1'b1;

        @(posedge clk);
        request_valid = 1'b0;

        @(posedge clk);
        wait(uut.STATE == 5'd9); // IDLE state
        #20;
        
        if (dout_cpu == 16'hBEEF) begin
            $display("SUCCESS: Bank 1 read data matches written pattern (0xBEEF)!");
        end else begin
            $display("ERROR: Bank 1 expected 0xBEEF, got 0x%h", dout_cpu);
        end

        #2000;
        $finish;
    end

endmodule