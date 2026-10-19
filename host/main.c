#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libserialport.h>

#define BAUD_RATE 115200
#define WRITE_TIMEOUT_MS 1000
#define READ_TIMEOUT_MS 2000
#define DETECTION_TIMEOUT_MS 500

/*
 * Configure a serial port for:
 * 115200 baud, 8 data bits, no parity,
 * 1 stop bit, and no flow control.
 */
int configure_port(struct sp_port *port)
{
    if (sp_set_baudrate(port, BAUD_RATE) != SP_OK ||
        sp_set_bits(port, 8) != SP_OK ||
        sp_set_parity(port, SP_PARITY_NONE) != SP_OK ||
        sp_set_stopbits(port, 1) != SP_OK ||
        sp_set_flowcontrol(port, SP_FLOWCONTROL_NONE) != SP_OK)
    {
        return 0;
    }

    return 1;
}


/*
 * Send a fixed number of bytes using the normal host-tool timeout.
 * Returns 1 if every byte was sent.
 */
int send_bytes(struct sp_port *port, const char *message, int length)
{
    int bytes_written = sp_blocking_write(
        port,
        message,
        length,
        WRITE_TIMEOUT_MS
    );

    return bytes_written == length;
}


/*
 * Check the connection using:
 *
 * Host sends:     PING
 * Device returns: PONG
 */
int check_connection(struct sp_port *port)
{
    const char *message = "PING";
    char response[5];

    sp_flush(port, SP_BUF_INPUT);

    if (!send_bytes(port, message, 4))
    {
        printf("ERROR: Could not send PING.\n");
        return 0;
    }

    int bytes_read = sp_blocking_read(
        port,
        response,
        4,
        READ_TIMEOUT_MS
    );

    if (bytes_read == 0)
    {
        printf("ERROR: Timed out waiting for PONG.\n");
        return 0;
    }

    if (bytes_read < 0)
    {
        printf("ERROR: Failed to read PONG response.\n");
        return 0;
    }

    if (bytes_read != 4)
    {
        printf(
            "ERROR: Incomplete PONG response. Received %d of 4 bytes.\n",
            bytes_read
        );
        return 0;
    }

    response[4] = '\0';

    if (strcmp(response, "PONG") != 0)
    {
        printf("ERROR: Invalid response: %s\n", response);
        return 0;
    }

    printf("TX: PING\n");
    printf("RX: PONG\n");
    printf("Connection is working.\n");

    return 1;
}


/*
 * Search through all available serial ports.
 *
 * During detection, each port gets a short 500 ms
 * read/write timeout so busy or silent ports do not
 * slow down the full scan.
 *
 * A device is considered compatible if it responds:
 *
 * Host sends:     PING
 * Device returns: PONG
 */
int find_board_port(char *detected_port, int size)
{
    struct sp_port **port_list;

    printf("Searching for compatible serial device...\n\n");

    if (sp_list_ports(&port_list) != SP_OK)
    {
        printf("ERROR: Could not list serial ports.\n");
        return 0;
    }

    for (int i = 0; port_list[i] != NULL; i++)
    {
        struct sp_port *test_port = port_list[i];
        const char *port_name = sp_get_port_name(test_port);

        printf("Checking %s... ", port_name);

        /* Try to open the port. */
        if (sp_open(test_port, SP_MODE_READ_WRITE) != SP_OK)
        {
            printf("BUSY or could not open. Skipped.\n");
            continue;
        }

        /* Configure the port for 115200 8N1. */
        if (!configure_port(test_port))
        {
            printf("CONFIGURATION ERROR. Skipped.\n");
            sp_close(test_port);
            continue;
        }

        /* Remove old data before testing the port. */
        sp_flush(test_port, SP_BUF_BOTH);

        const char *message = "PING";

        /* Send PING with the shorter detection timeout. */
        int bytes_written = sp_blocking_write(
            test_port,
            message,
            4,
            DETECTION_TIMEOUT_MS
        );

        if (bytes_written < 0)
        {
            printf("WRITE ERROR. Skipped.\n");
            sp_close(test_port);
            continue;
        }

        if (bytes_written != 4)
        {
            printf(
                "INCOMPLETE WRITE (%d of 4 bytes). Skipped.\n",
                bytes_written
            );
            sp_close(test_port);
            continue;
        }

        char response[5];

        /* Wait up to 500 ms for PONG. */
        int bytes_read = sp_blocking_read(
            test_port,
            response,
            4,
            DETECTION_TIMEOUT_MS
        );

        if (bytes_read == 0)
        {
            printf("SILENT - no response. Skipped.\n");
            sp_close(test_port);
            continue;
        }

        if (bytes_read < 0)
        {
            printf("READ ERROR. Skipped.\n");
            sp_close(test_port);
            continue;
        }

        if (bytes_read != 4)
        {
            printf(
                "INCOMPLETE RESPONSE (%d of 4 bytes). Skipped.\n",
                bytes_read
            );
            sp_close(test_port);
            continue;
        }

        response[4] = '\0';

        if (strcmp(response, "PONG") == 0)
        {
            printf("FOUND - received PONG.\n\n");

            snprintf(
                detected_port,
                size,
                "%s",
                port_name
            );

            sp_close(test_port);
            sp_free_port_list(port_list);

            return 1;
        }

        printf(
            "INVALID RESPONSE \"%s\". Skipped.\n",
            response
        );

        sp_close(test_port);
    }

    sp_free_port_list(port_list);

    printf("\nNo compatible device found.\n");

    return 0;
}


/*
 * Ask the ESP32 for its Wi-Fi MAC address.
 *
 * Command sent:  MAC?
 * Expected reply example:
 * AA:BB:CC:DD:EE:FF
 *
 * The response is 17 characters.
 */
void grab_mac_address(struct sp_port *port)
{
    const char *command = "MAC?";
    char mac_address[18];

    sp_flush(port, SP_BUF_INPUT);

    if (!send_bytes(port, command, 4))
    {
        printf("ERROR: Could not send MAC request.\n");
        return;
    }

    int bytes_read = sp_blocking_read(
        port,
        mac_address,
        17,
        READ_TIMEOUT_MS
    );

    if (bytes_read == 17)
    {
        mac_address[17] = '\0';
        printf("\nESP32 MAC Address: %s\n", mac_address);
    }
    else if (bytes_read == 0)
    {
        printf("\nERROR: Timed out waiting for MAC address.\n");
    }
    else if (bytes_read > 0)
    {
        mac_address[bytes_read] = '\0';

        printf(
            "\nERROR: Incomplete MAC response: %s\n",
            mac_address
        );
    }
    else
    {
        printf("\nERROR: Failed to read MAC address.\n");
    }
}


/*
 * Show information that the host can see
 * about the connected serial device.
 */
void show_device_information(struct sp_port *port)
{
    const char *name = sp_get_port_name(port);
    const char *description = sp_get_port_description(port);

    printf("\nDevice Information\n");
    printf("------------------\n");
    printf("Port: %s\n", name);

    if (description != NULL)
    {
        printf("Description: %s\n", description);
    }
    else
    {
        printf("Description: Not available\n");
    }
}


/*
 * Let the user type and send a command.
 * This option only sends the command.
 */
void send_custom_command(struct sp_port *port)
{
    char command[128];

    printf("\nEnter command: ");

    if (fgets(command, sizeof(command), stdin) == NULL)
    {
        printf("ERROR: Could not read command.\n");
        return;
    }

    command[strcspn(command, "\r\n")] = '\0';

    int length = (int)strlen(command);

    if (length == 0)
    {
        printf("No command entered.\n");
        return;
    }

    if (send_bytes(port, command, length))
    {
        printf("Sent: %s\n", command);
    }
    else
    {
        printf("ERROR: Could not send complete command.\n");
    }
}


/*
 * Display the current UART settings.
 */
void show_connection_settings(const char *port_name)
{
    printf("\nConnection Settings\n");
    printf("-------------------\n");
    printf("Port: %s\n", port_name);
    printf("Baud Rate: %d\n", BAUD_RATE);
    printf("Data Bits: 8\n");
    printf("Parity: None\n");
    printf("Stop Bits: 1\n");
    printf("Flow Control: None\n");
}


/*
 * Display the Version 1 host-tool menu.
 */
void show_menu(void)
{
    printf("\nVisionCore Host Tool\n");
    printf("--------------------\n");
    printf("1) Grab MAC Address\n");
    printf("2) Check Connection\n");
    printf("3) Get Device Information\n");
    printf("4) Send Custom Command\n");
    printf("5) Show Connection Settings\n");
    printf("6) Exit\n");
    printf("\nSelect an option: ");
}


int main(void)
{
    char detected_port[64];

    /* Automatically find a compatible device. */
    if (!find_board_port(detected_port, sizeof(detected_port)))
    {
        printf("ERROR: Compatible device could not be found.\n");
        return 1;
    }

    const char *port_name = detected_port;
    struct sp_port *port = NULL;

    /* Get the detected serial port. */
    if (sp_get_port_by_name(port_name, &port) != SP_OK)
    {
        printf("ERROR: Could not find %s\n", port_name);
        return 1;
    }

    /* Open the port for sending and receiving. */
    if (sp_open(port, SP_MODE_READ_WRITE) != SP_OK)
    {
        printf("ERROR: Could not open %s\n", port_name);
        sp_free_port(port);
        return 1;
    }

    /* Configure UART. */
    if (!configure_port(port))
    {
        printf("ERROR: Could not configure %s\n", port_name);

        sp_close(port);
        sp_free_port(port);

        return 1;
    }

    sp_flush(port, SP_BUF_BOTH);

    printf("Connected to %s at %d baud.\n", port_name, BAUD_RATE);

    /* Main host-tool menu loop. */
    while (1)
    {
        char input[32];
        int choice;

        show_menu();

        if (fgets(input, sizeof(input), stdin) == NULL)
        {
            printf("ERROR: Could not read menu option.\n");
            break;
        }

        choice = atoi(input);

        switch (choice)
        {
            case 1:
                grab_mac_address(port);
                break;

            case 2:
                printf("\nChecking connection...\n");
                check_connection(port);
                break;

            case 3:
                show_device_information(port);
                break;

            case 4:
                send_custom_command(port);
                break;

            case 5:
                show_connection_settings(port_name);
                break;

            case 6:
                printf("\nClosing host tool...\n");
                sp_close(port);
                sp_free_port(port);
                printf("Port closed.\n");
                return 0;

            default:
                printf("\nInvalid option. Enter a number from 1 to 6.\n");
                break;
        }
    }

    sp_close(port);
    sp_free_port(port);

    return 0;
}
