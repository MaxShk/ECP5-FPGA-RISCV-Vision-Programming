#include <stdio.h>
#include <string.h>
#include <libserialport.h>

int main(void)
{
    const char *port_name = "COM3";
    const int baud_rate = 115200;

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
    const char *message = "PING\n";

    printf("TX: PING\n");

    // Send message
    int bytes_written = sp_blocking_write(
        port,
        message,
        strlen(message),
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
    char buffer[64];

    int bytes_read = sp_blocking_read(
        port,
        buffer,
        sizeof(buffer) - 1,
        2000
    );

    if (bytes_read > 0)
    {
        // Convert received bytes into a normal C string
        buffer[bytes_read] = '\0';

        printf("RX: %s", buffer);
    }
    else if (bytes_read == 0)
    {
        printf("Timeout: no response received.\n");
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

