module UART_Echo #(
    parameter CLK_FREQ  = 100000000,
    parameter BAUD_RATE = 115200   
)(
    input  wire clk,
    input  wire sys_rst,
    input  wire FTDI_TXD,          
    output reg  FTDI_RXD           
);

    localparam BIT_PERIOD = CLK_FREQ / BAUD_RATE;

    reg [11:0] rx_clk_counter;
    reg [3:0]  rx_bit_counter;
    reg [7:0]  rx_shift_reg;
    reg        rx_busy;
    reg        rx_valid;
    reg [7:0]  rx_data;
    
    reg rx_sync_0, rx_sync_1;
    always @(posedge clk) begin
        rx_sync_0 <= FTDI_TXD;
        rx_sync_1 <= rx_sync_0;
    end

    always @(posedge clk or posedge sys_rst) begin
        if (sys_rst) begin
            rx_clk_counter <= 0;
            rx_bit_counter <= 0;
            rx_busy        <= 1'b0;
            rx_valid       <= 1'b0;
            rx_data        <= 8'h00;
        end else begin
            rx_valid <= 1'b0;
            if (!rx_busy) begin
                if (!rx_sync_1) begin 
                    rx_busy        <= 1'b1;
                    rx_clk_counter <= BIT_PERIOD / 2; 
                    rx_bit_counter <= 0;
                end
            end else begin
                if (rx_clk_counter == 0) begin
                    rx_clk_counter <= BIT_PERIOD - 1;
                    if (rx_bit_counter == 0) begin
                        if (!rx_sync_1) rx_bit_counter <= rx_bit_counter + 1;
                        else            rx_busy        <= 1'b0;
                    end else if (rx_bit_counter <= 8) begin
                        rx_shift_reg   <= {rx_sync_1, rx_shift_reg[7:1]};
                        rx_bit_counter <= rx_bit_counter + 1;
                    end else begin
                        rx_busy  <= 1'b0;
                        if (rx_sync_1) begin 
                            rx_valid <= 1'b1;
                            rx_data  <= rx_shift_reg;
                        end
                    end
                end else begin
                    rx_clk_counter <= rx_clk_counter - 1;
                end
            end
        end
    end


    reg [11:0] tx_clk_counter;
    reg [3:0]  tx_bit_counter;
    reg [9:0]  tx_shift_reg;
    reg        tx_busy;
    reg        tx_start;    
    reg [7:0]  tx_byte;

    always @(posedge clk or posedge sys_rst) begin
        if (sys_rst) begin
            tx_clk_counter <= 0;
            tx_bit_counter <= 0;
            tx_busy        <= 1'b0;
            FTDI_RXD       <= 1'b1; 
        end else begin
            if (tx_start && !tx_busy) begin
                tx_shift_reg   <= {1'b1, tx_byte, 1'b0}; 
                tx_busy        <= 1'b1;
                tx_clk_counter <= BIT_PERIOD - 1;
                tx_bit_counter <= 0;
            end else if (tx_busy) begin
                if (tx_clk_counter == 0) begin
                    tx_clk_counter <= BIT_PERIOD - 1;
                    FTDI_RXD       <= tx_shift_reg[0]; 
                    tx_shift_reg   <= {1'b1, tx_shift_reg[9:1]};
                    if (tx_bit_counter == 9) begin
                        tx_busy <= 1'b0;
                    end else begin
                        tx_bit_counter <= tx_bit_counter + 1;
                    end
                end else begin
                    tx_clk_counter <= tx_clk_counter - 1;
                end
            end
        end
    end


    localparam STATE_IDLE   = 3'd0,
               STATE_RCV_P  = 3'd1,
               STATE_RCV_I  = 3'd2,
               STATE_RCV_N  = 3'd3,
               STATE_TX_P   = 3'd4,
               STATE_TX_O   = 3'd5,
               STATE_TX_N   = 3'd6,
               STATE_TX_G   = 3'd7;

    reg [2:0] current_state, next_state;

    always @(posedge clk or posedge sys_rst) begin
        if (sys_rst) current_state <= STATE_IDLE;
        else         current_state <= next_state;
    end

    always @(*) begin
        next_state = current_state;
        case (current_state)
            STATE_IDLE: begin
                if (rx_valid && (rx_data == "P")) next_state = STATE_RCV_P;
            end
            STATE_RCV_P: begin
                if (rx_valid) begin
                    if (rx_data == "I")      next_state = STATE_RCV_I;
                    else if (rx_data == "P") next_state = STATE_RCV_P;
                    else                     next_state = STATE_IDLE;
                end
            end
            STATE_RCV_I: begin
                if (rx_valid) begin
                    if (rx_data == "N")      next_state = STATE_RCV_N;
                    else if (rx_data == "P") next_state = STATE_RCV_P;
                    else                     next_state = STATE_IDLE;
                end
            end
            STATE_RCV_N: begin
                if (rx_valid) begin
                    if (rx_data == "G")      next_state = STATE_TX_P; 
                    else if (rx_data == "P") next_state = STATE_RCV_P;
                    else                     next_state = STATE_IDLE;
                end
            end
            

            STATE_TX_P: if (!tx_busy && !tx_start) next_state = STATE_TX_O;
            STATE_TX_O: if (!tx_busy && !tx_start) next_state = STATE_TX_N;
            STATE_TX_N: if (!tx_busy && !tx_start) next_state = STATE_TX_G;
            STATE_TX_G: if (!tx_busy && !tx_start) next_state = STATE_IDLE;
        endcase
    end

    always @(*) begin
        tx_byte  = 8'h00;
        case (current_state)
            STATE_TX_P: tx_byte = "P";
            STATE_TX_O: tx_byte = "O";
            STATE_TX_N: tx_byte = "N";
            STATE_TX_G: tx_byte = "G";
        endcase
    end


    always @(posedge clk or posedge sys_rst) begin
        if (sys_rst) begin
            tx_start <= 1'b0;
        end else if (next_state != current_state && (next_state == STATE_TX_P || next_state == STATE_TX_O || next_state == STATE_TX_N || next_state == STATE_TX_G)) begin
            tx_start <= 1'b1;
        end else begin
            tx_start <= 1'b0;
        end
    end

endmodule
