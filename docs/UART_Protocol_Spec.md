# VisionCore UART Protocol Specification

**Version:** 1.0  
**Host Software:** C using libserialport  
**FPGA UART Module:** UART_Echo.v  

## 1. Purpose

This document defines the UART communication between the VisionCore host tool and the FPGA. The goal is to make sure the C host program and FPGA UART logic use the same communication settings, commands, responses, and error rules.

The ESP32-S3 is currently used as a test device for some host-tool features. The FPGA implementation currently supports the PING/PONG connection test.

## 2. UART Configuration

The host program and FPGA use the following UART settings:

- Baud rate: 115200
- Data bits: 8
- Parity: None
- Stop bits: 1
- Flow control: None
- FPGA clock: 100 MHz

This UART format is commonly written as **115200 8N1**.

## 3. Message Format

UART commands are sent as raw ASCII bytes.

The current protocol does not add a newline character, null terminator, or message-length byte to the transmitted command.

### PING Command

The host sends:

`PING`

This command contains exactly four ASCII bytes:

| Character | Hex Value |
|---|---|
| P | 0x50 |
| I | 0x49 |
| N | 0x4E |
| G | 0x47 |

Total transmitted size: **4 bytes**

### PONG Response

When the FPGA receives a valid PING command, it responds with:

`PONG`

The response also contains exactly four ASCII bytes:

| Character | Hex Value |
|---|---|
| P | 0x50 |
| O | 0x4F |
| N | 0x4E |
| G | 0x47 |

Total response size: **4 bytes**

Communication example:

Host -> FPGA: `PING`  
FPGA -> Host: `PONG`

## 4. FPGA Command Handling

The FPGA receives the PING command one byte at a time.

The expected sequence is:

`P` -> `I` -> `N` -> `G`

After all four bytes are received correctly, the FPGA sends the four-byte PONG response.

If an incorrect byte is received during the sequence, the FPGA stops the current command match and waits for a new valid command sequence.

## 5. Host Port Detection

The host tool automatically checks available serial ports to find the correct device.

For each available port, the host:

1. Attempts to open the port.
2. Configures it for 115200 8N1.
3. Sends the four-byte PING command.
4. Waits up to 500 ms for a PONG response.
5. Selects the port if the response is exactly PONG.

The host skips ports that are busy, silent, incorrectly configured, or return an invalid response.

## 6. Timing

The host tool currently uses the following timeout values:

| Operation | Timeout |
|---|---|
| Port detection | 500 ms |
| Normal write | 1000 ms |
| Normal read | 2000 ms |

During normal communication, the host waits for a response before continuing with the command.

## 7. Error Handling

The host reports an error if:

- The COM port cannot be opened.
- The UART settings cannot be configured.
- The complete command cannot be transmitted.
- No response is received before the timeout.
- Fewer than four response bytes are received.
- The received response is not exactly PONG.
- A serial read or write operation fails.

The current host tool does not automatically resend a failed PING command.

## 8. Current Commands

| Command | Target | Expected Response | Purpose |
|---|---|---|---|
| `PING` | FPGA / ESP32 | `PONG` | Verify UART communication |
| `MAC?` | ESP32 only | MAC address | Request ESP32 Wi-Fi MAC address |

The current FPGA UART responder supports **PING/PONG only**.

The MAC address command is used only with the ESP32 test device and is not currently implemented in the FPGA.

## 9. Future Commands

Additional commands may later be added for functions such as:

- Camera control
- Image capture
- SDRAM access
- Device status
- FPGA register access

Each new command must define the exact transmitted bytes, expected response, timeout behavior, and FPGA action before it is added to the protocol.

## 10. Version 1 Scope

The Version 1 UART protocol is mainly used to confirm reliable communication between the VisionCore host tool and the FPGA.

The current FPGA-side test is successful when:

Host sends: `PING`  
FPGA receives: `PING`  
FPGA sends: `PONG`  
Host receives: `PONG`

This provides a basic communication test before more advanced VisionCore commands are added.