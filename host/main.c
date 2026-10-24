#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libserialport.h>

#define BAUD_RATE 115200
#define WRITE_TIMEOUT_MS 1000
#define READ_TIMEOUT_MS 2000
#define DETECTION_TIMEOUT_MS 500

/*
 * Supported host-tool targets.
 */
typedef enum
{
    TARGET_ESP32,
    TARGET_FPGA
} TargetType;


/*
 * Convert the command-line target argument into a TargetType.
 *
 * Expected use:
 *   main.exe --target esp32
 *   main.exe --target fpga
 *
 * Returns 1 if the target is valid.
 * Returns 0 if the command-line arguments are invalid.
 */
int parse_target(int argc, char *argv[], TargetType *target)
{
    if (argc != 3)
    {
        return 0;
    }

    if (strcmp(argv[1], "--target") != 0)
    {
        return 0;
    }

    if (strcmp(argv[2], "esp32") == 0)
    {
        *target = TARGET_ESP32;
        return 1;
    }

    if (strcmp(argv[2], "fpga") == 0)
    {
        *target = TARGET_FPGA;
        return 1;
    }

    return 0;
}


/*
 * Return a readable target name for terminal output.
 */
const char *target_name(TargetType target)
{
    if (target == TARGET_ESP32)
    {
        return "ESP32-S3";
    }

    return "VisionCore FPGA";
}


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

    /*
     * Keep track of the scan results so a final summary can be shown.
     */
    int ports_checked = 0;
    int busy_ports = 0;
    int silent_ports = 0;
    int partial_reply_ports = 0;
    int wrong_reply_ports = 0;
    int other_errors = 0;

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

        ports_checked++;
        printf("Checking %s... ", port_name);

        /*
         * Try to open the port.
         * If another program is using it, skip it.
         */
        if (sp_open(test_port, SP_MODE_READ_WRITE) != SP_OK)
        {
            printf("BUSY or could not open. Skipped.\n");
            busy_ports++;
            continue;
        }

        /*
         * Configure the port for 115200 8N1.
         */
        if (!configure_port(test_port))
        {
            printf("CONFIGURATION ERROR. Skipped.\n");
            other_errors++;
            sp_close(test_port);
            continue;
        }

        /*
         * Remove old data before testing the port.
         */
        sp_flush(test_port, SP_BUF_BOTH);

        const char *message = "PING";

        /*
         * Send PING with the shorter detection timeout.
         */
        int bytes_written = sp_blocking_write(
            test_port,
            message,
            4,
            DETECTION_TIMEOUT_MS
        );

        if (bytes_written < 0)
        {
            printf("WRITE ERROR. Skipped.\n");
            other_errors++;
            sp_close(test_port);
            continue;
        }

        if (bytes_written != 4)
        {
            printf(
                "INCOMPLETE WRITE (%d of 4 bytes). Skipped.\n",
                bytes_written
            );
            other_errors++;
            sp_close(test_port);
            continue;
        }

        char response[5];

        /*
         * Wait up to 500 ms for PONG.
         */
        int bytes_read = sp_blocking_read(
            test_port,
            response,
            4,
            DETECTION_TIMEOUT_MS
        );

        if (bytes_read == 0)
        {
            printf("SILENT - no response. Skipped.\n");
            silent_ports++;
            sp_close(test_port);
            continue;
        }

        if (bytes_read < 0)
        {
            printf("READ ERROR. Skipped.\n");
            other_errors++;
            sp_close(test_port);
            continue;
        }

        if (bytes_read != 4)
        {
            printf(
                "INCOMPLETE RESPONSE (%d of 4 bytes). Skipped.\n",
                bytes_read
            );
            partial_reply_ports++;
            sp_close(test_port);
            continue;
        }

        response[4] = '\0';

        if (strcmp(response, "PONG") == 0)
        {
            printf("FOUND - received PONG.\n");

            snprintf(
                detected_port,
                size,
                "%s",
                port_name
            );

            printf("\nPort Scan Summary\n");
            printf("-----------------\n");
            printf("Ports checked:       %d\n", ports_checked);
            printf("Busy/unavailable:    %d\n", busy_ports);
            printf("Silent/no reply:     %d\n", silent_ports);
            printf("Partial replies:     %d\n", partial_reply_ports);
            printf("Wrong replies:       %d\n", wrong_reply_ports);
            printf("Other errors:        %d\n", other_errors);
            printf("Compatible device:   %s\n\n", detected_port);

            sp_close(test_port);
            sp_free_port_list(port_list);

            return 1;
        }

        printf(
            "INVALID RESPONSE \"%s\". Skipped.\n",
            response
        );
        wrong_reply_ports++;

        sp_close(test_port);
    }

    sp_free_port_list(port_list);

    printf("\nNo compatible device found.\n");
    printf("\nPort Scan Summary\n");
    printf("-----------------\n");
    printf("Ports checked:       %d\n", ports_checked);
    printf("Busy/unavailable:    %d\n", busy_ports);
    printf("Silent/no reply:     %d\n", silent_ports);
    printf("Partial replies:     %d\n", partial_reply_ports);
    printf("Wrong replies:       %d\n", wrong_reply_ports);
    printf("Other errors:        %d\n", other_errors);
    printf("Compatible device:   None\n\n");

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
 *
 * This command is ESP32-only.
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
void show_device_information(struct sp_port *port, TargetType target)
{
    const char *name = sp_get_port_name(port);
    const char *description = sp_get_port_description(port);

    printf("\nDevice Information\n");
    printf("------------------\n");
    printf("Target: %s\n", target_name(target));
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
void show_connection_settings(const char *port_name, TargetType target)
{
    printf("\nConnection Settings\n");
    printf("-------------------\n");
    printf("Target: %s\n", target_name(target));
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
void show_menu(TargetType target)
{
    printf("\nVisionCore Host Tool\n");
    printf("--------------------\n");
    printf("Target: %s\n\n", target_name(target));

    printf("1) Grab MAC Address");

    if (target == TARGET_FPGA)
    {
        printf(" (ESP32 only)");
    }

    printf("\n");

    printf("2) Check Connection\n");
    printf("3) Get Device Information\n");
    printf("4) Send Custom Command\n");
    printf("5) Show Connection Settings\n");
    printf("6) Exit\n");
    printf("\nSelect an option: ");
}


int main(int argc, char *argv[])
{
    TargetType target;

    /*
     * Parse the required target flag.
     */
    if (!parse_target(argc, argv, &target))
    {
        printf("Usage:\n");
        printf("  %s --target esp32\n", argv[0]);
        printf("  %s --target fpga\n", argv[0]);
        return 1;
    }

    printf("Selected target: %s\n\n", target_name(target));

    char detected_port[64];

    /*
     * Automatically find a compatible device.
     *
     * Both the ESP32 test firmware and FPGA UART responder
     * use PING/PONG for the basic connection test.
     */
    if (!find_board_port(detected_port, sizeof(detected_port)))
    {
        printf("ERROR: Compatible device could not be found.\n");
        return 1;
    }

    const char *port_name = detected_port;
    struct sp_port *port = NULL;

    /*
     * Get the detected serial port.
     */
    if (sp_get_port_by_name(port_name, &port) != SP_OK)
    {
        printf("ERROR: Could not find %s\n", port_name);
        return 1;
    }

    /*
     * Open the port for sending and receiving.
     */
    if (sp_open(port, SP_MODE_READ_WRITE) != SP_OK)
    {
        printf("ERROR: Could not open %s\n", port_name);
        sp_free_port(port);
        return 1;
    }

    /*
     * Configure UART.
     */
    if (!configure_port(port))
    {
        printf("ERROR: Could not configure %s\n", port_name);

        sp_close(port);
        sp_free_port(port);

        return 1;
    }

    sp_flush(port, SP_BUF_BOTH);

    printf("Connected to %s at %d baud.\n", port_name, BAUD_RATE);
    printf("Target: %s\n", target_name(target));

    /*
     * Main host-tool menu loop.
     */
    while (1)
    {
        char input[32];
        int choice;

        show_menu(target);

        if (fgets(input, sizeof(input), stdin) == NULL)
        {
            printf("ERROR: Could not read menu option.\n");
            break;
        }

        choice = atoi(input);

        switch (choice)
        {
            case 1:
                /*
                 * MAC? is supported by the ESP32 test target only.
                 */
                if (target == TARGET_ESP32)
                {
                    grab_mac_address(port);
                }
                else
                {
                    printf(
                        "\nMAC address is only supported by the ESP32 target.\n"
                    );
                }
                break;

            case 2:
                printf("\nChecking connection...\n");
                check_connection(port);
                break;

            case 3:
                show_device_information(port, target);
                break;

            case 4:
                send_custom_command(port);
                break;

            case 5:
                show_connection_settings(port_name, target);
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
