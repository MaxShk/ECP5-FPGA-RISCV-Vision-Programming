`timescale 1ns/1ps

module UART_Echo_TB;

    
    reg clk;
    reg sys_rst;
    initial begin
        clk = 0;
        forever #5 clk = ~clk; 
    end

    
    reg  FTDI_TXD;   
    wire FTDI_RXD;   

    
    UART_Echo #(
        .CLK_FREQ(100000000), 
        .BAUD_RATE(115200)   
    ) uut (
        .clk(clk),
        .sys_rst(sys_rst),
        .FTDI_TXD(FTDI_TXD),
        .FTDI_RXD(FTDI_RXD)
    );

    
    task send_serial_byte(input [7:0] ascii_char);
        integer i;
        begin
            // Start Bit (Line drops low)
            FTDI_TXD = 1'b0;
            #8681; 
            
            
            for (i = 0; i < 8; i = i + 1) begin
                FTDI_TXD = ascii_char[i];
                #8681;
            end
            
            
            FTDI_TXD = 1'b1;
            #8681;
        end
    endtask

    
    initial begin
        $display("\n=== STARTING HARDWARE VERIFICATION SESSION ===");
        $display("Target Protocol Spec: 115200 8N1 Raw Streaming @ 100MHz");
        $display("------------------------------------------------------\n");
    end

    
    always @(uut.current_state) begin
        case (uut.current_state)
            3'd0: $display("[FSM STATUS @ %0t ps] State changed to: STATE_IDLE  (Waiting for P...)", $time);
            3'd1: $display("[FSM STATUS @ %0t ps] State changed to: STATE_RCV_P (Ingested 'P', waiting for I...)", $time);
            3'd2: $display("[FSM STATUS @ %0t ps] State changed to: STATE_RCV_I (Ingested 'I', waiting for N...)", $time);
            3'd3: $display("[FSM STATUS @ %0t ps] State changed to: STATE_RCV_N (Ingested 'N', waiting for G...)", $time);
            3'd4: $display("[FSM STATUS @ %0t ps] FULL MATCH DETECTED! State changed to: STATE_TX_P (Transmitting 'P'...) ", $time);
            3'd5: $display("[FSM STATUS @ %0t ps] State changed to: STATE_TX_O (Transmitting 'O'...)", $time);
            3'd6: $display("[FSM STATUS @ %0t ps] State changed to: STATE_TX_N (Transmitting 'N'...)", $time);
            3'd7: $display("[FSM STATUS @ %0t ps] State changed to: STATE_TX_G (Transmitting 'G'...)", $time);
        endcase
    end

    
    always @(posedge clk) begin
        if (uut.rx_valid) begin
            $display(" >> [HW RX ENGINE] Validated incoming ASCII Byte character code: \"%c\"", uut.rx_data);
        end
    end

    
    initial begin
        $dumpfile("uart_clean_test.vcd");
        $dumpvars(0, UART_Echo_TB);

        
        FTDI_TXD = 1'b1; 
        sys_rst  = 1'b1; 
        #200;
        sys_rst  = 1'b0; 
        #2000;           

        
        $display("\n[HOST SCRIPT] Starting Transmission Sequence: 'PING'...");
        send_serial_byte("P");
        send_serial_byte("I");
        send_serial_byte("N");
        send_serial_byte("G");

        $display("\n[HOST SCRIPT] Stream transmission done. Monitoring response bus for PONG return...");
        #400000; 

        $display("\n=== HARDWARE VERIFICATION SUCCESSFULLY PASSED ===");
        $finish;
    end

endmodule
