#include <stdio.h>
#include <string.h>
#include <libserialport.h>


/*
 * Search through available serial ports.
 *
 * A device is considered the correct BOARD 3 device if:
 *
 *      Host sends:     PING
 *      Device returns: PONG
 *
 * Returns 1 if the device is found.
 * Returns 0 if no matching device is found.
 */
int find_board_port(char *detected_port, int size)
{
    struct sp_port **port_list;

    printf("Searching for BOARD 3 device...\n\n");

    // Ask libserialport for a list of available serial ports
    if (sp_list_ports(&port_list) != SP_OK)
    {
        printf("ERROR: Could not list serial ports.\n");
        return 0;
    }

    // Go through each serial port
    for (int i = 0; port_list[i] != NULL; i++)
    {
        struct sp_port *test_port = port_list[i];

        const char *port_name = sp_get_port_name(test_port);

        printf("Checking %s...\n", port_name);


        // Try to open the port
        if (sp_open(test_port, SP_MODE_READ_WRITE) != SP_OK)
        {
            printf("Could not open %s. Skipping...\n\n", port_name);
            continue;
        }


        // Configure UART
        sp_set_baudrate(test_port, 115200);
        sp_set_bits(test_port, 8);
        sp_set_parity(test_port, SP_PARITY_NONE);
        sp_set_stopbits(test_port, 1);
        sp_set_flowcontrol(test_port, SP_FLOWCONTROL_NONE);


        // Clear any old data that may already be waiting
        sp_flush(test_port, SP_BUF_BOTH);


        // Send exactly 4 ASCII bytes: P I N G
        const char *message = "PING";

        int bytes_written = sp_blocking_write(
            test_port,
            message,
            4,
            1000
        );


        // Only continue if all 4 bytes were sent
        if (bytes_written == 4)
        {
            char buffer[5];

            // Wait for exactly 4 bytes back
            int bytes_read = sp_blocking_read(
                test_port,
                buffer,
                4,
                2000
            );


            if (bytes_read == 4)
            {
                // Add a null terminator so buffer becomes a C string
                buffer[4] = '\0';


                // Check whether the device responded with PONG
                if (strcmp(buffer, "PONG") == 0)
                {
                    printf(
                        "BOARD 3 compatible device found on %s!\n\n",
                        port_name
                    );


                    // Save the detected COM-port name
                    snprintf(
                        detected_port,
                        size,
                        "%s",
                        port_name
                    );


                    // Close the temporary connection
                    sp_close(test_port);

                    // Free the list of serial ports
                    sp_free_port_list(port_list);

                    return 1;
                }
            }
        }


        // This port was not our device
        sp_close(test_port);

        printf("No valid PONG response from %s.\n\n", port_name);
    }


    // Finished checking every port
    sp_free_port_list(port_list);

    return 0;
}



int main(void)
{
    const int baud_rate = 115200;
    const int messageSize = 4;

    char detected_port[64];


    /*
     * Automatically find the serial device.
     */
    if (!find_board_port(detected_port, sizeof(detected_port)))
    {
        printf("ERROR: BOARD 3 device could not be found.\n");
        return 1;
    }


    /*
     * detected_port could now contain something like:
     *
     * COM3
     * COM4
     * COM5
     *
     * depending on what Windows assigned.
     */
    const char *port_name = detected_port;

    struct sp_port *port = NULL;



    printf("BOARD 3 UART Test\n");
    printf("-----------------\n");

    printf("Port: %s\n", port_name);
    printf("Baud: %d\n\n", baud_rate);



    /*
     * Get the detected serial port.
     */
    if (sp_get_port_by_name(port_name, &port) != SP_OK)
    {
        printf(
            "ERROR: Could not find %s\n",
            port_name
        );

        return 1;
    }



    /*
     * Open the port for both sending and receiving.
     */
    if (sp_open(port, SP_MODE_READ_WRITE) != SP_OK)
    {
        printf(
            "ERROR: Could not open %s\n",
            port_name
        );

        sp_free_port(port);

        return 1;
    }


    printf("Port opened successfully.\n");



    /*
     * Configure UART:
     *
     * 115200 baud
     * 8 data bits
     * No parity
     * 1 stop bit
     * No flow control
     */
    sp_set_baudrate(port, baud_rate);
    sp_set_bits(port, 8);
    sp_set_parity(port, SP_PARITY_NONE);
    sp_set_stopbits(port, 1);
    sp_set_flowcontrol(port, SP_FLOWCONTROL_NONE);



    /*
     * Clear any data remaining from the auto-detection test.
     */
    sp_flush(port, SP_BUF_BOTH);



    /*
     * Message sent to the device.
     *
     * This is exactly 4 ASCII bytes:
     *
     * P I N G
     */
    const char *message = "PING";

    printf("TX: PING\n");



    /*
     * Send PING.
     *
     * port        = serial port
     * message     = data being sent
     * messageSize = 4 bytes
     * 1000        = 1 second timeout
     */
    int bytes_written = sp_blocking_write(
        port,
        message,
        messageSize,
        1000
    );



    /*
     * Make sure exactly 4 bytes were transmitted.
     */
    if (bytes_written != messageSize)
    {
        printf(
            "ERROR: Expected to send 4 bytes, but sent %d.\n",
            bytes_written
        );

        sp_close(port);
        sp_free_port(port);

        return 1;
    }



    /*
     * Create space for:
     *
     * P O N G \0
     *
     * 4 received bytes + null terminator
     */
    char buffer[5];



    /*
     * Wait for the device to send PONG.
     *
     * Wait for:
     *      4 bytes
     *
     * Timeout:
     *      2000 ms = 2 seconds
     */
    int bytes_read = sp_blocking_read(
        port,
        buffer,
        4,
        2000
    );



    /*
     * Check the received response.
     */
    if (bytes_read == 4)
    {
        // Convert received bytes into a C string
        buffer[4] = '\0';


        if (strcmp(buffer, "PONG") == 0)
        {
            printf("RX: PONG\n");
            printf("Communication successful.\n");
        }
        else
        {
            printf(
                "ERROR: Invalid response: %s\n",
                buffer
            );
        }
    }

    else if (bytes_read == 0)
    {
        printf(
            "ERROR: Timeout - no response received.\n"
        );
    }

    else if (bytes_read > 0)
    {
        printf(
            "ERROR: Incomplete response. Received %d of 4 bytes.\n",
            bytes_read
        );
    }

    else
    {
        printf(
            "ERROR: Failed to read from UART.\n"
        );
    }



    /*
     * Clean up.
     */
    sp_close(port);
    sp_free_port(port);


    printf("\nPort closed.\n");

    return 0;
}