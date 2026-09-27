#include <stdio.h>
#include <string.h>
#include <libserialport.h>

int main(void)
{
    const char *port_name = "COM3";
    const int baud_rate = 115200;
    const int messageSize = 4;

    struct sp_port *port = NULL;

    printf("BOARD 3 UART Test\n");
    printf("-----------------\n");
    printf("Port: %s\n", port_name);
    printf("Baud: %d\n\n", baud_rate);

    // Find COM3
    if (sp_get_port_by_name(port_name, &port) != SP_OK)
    {
        printf("ERROR: Could not find %s\n", port_name);
        return 1;
    }

    // Open COM3 for sending and receiving
    if (sp_open(port, SP_MODE_READ_WRITE) != SP_OK)
    {
        printf("ERROR: Could not open %s\n", port_name);
        sp_free_port(port);
        return 1;
    }

    printf("Port opened successfully.\n");

    // Configure UART
    sp_set_baudrate(port, baud_rate);
    sp_set_bits(port, 8);
    sp_set_parity(port, SP_PARITY_NONE);
    sp_set_stopbits(port, 1);
    sp_set_flowcontrol(port, SP_FLOWCONTROL_NONE);

    // Message to send
    const char *message = "PING";

    printf("TX: PING\n");

    // Send message
    int bytes_written = sp_blocking_write(
        port,
        message,
        messageSize,
        1000
    );

    if (bytes_written < 0)
    {
        printf("ERROR: Failed to send data.\n");

        sp_close(port);
        sp_free_port(port);

        return 1;
    }

    // Buffer for received data
    char buffer[5];

    int bytes_read = sp_blocking_read(
        port,
        buffer,
        4,
        2000
    );

    if (bytes_read == 4)
    {
        buffer[4] = '\0';

        if (strcmp(buffer, "PONG") == 0)
        {
            printf("RX: PONG\n");
            printf("Communication successful.\n");
        }
        else
        {
            printf("ERROR: Invalid response: %s\n", buffer);
        }
    }
    else if (bytes_read == 0)
    {
        printf("ERROR: Timeout - no response received.\n");
    }
    else if (bytes_read > 0)
    {
        printf("ERROR: Incomplete response. Received %d of 4 bytes.\n",
            bytes_read);
    }
    else
    {
        printf("ERROR: Failed to read from UART.\n");
    }

    // Clean up
    sp_close(port);
    sp_free_port(port);

    printf("\nPort closed.\n");

    return 0;
}

