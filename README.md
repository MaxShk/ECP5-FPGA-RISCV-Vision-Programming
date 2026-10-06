# Vision Core
<img width="2130" height="1356" alt="image" src="https://github.com/user-attachments/assets/d009a748-f6ff-46f8-b307-a08c38795514" />


An open FPGA development board for building and testing real-time video pipelines: camera in, external SDRAM as a frame buffer, HDMI-compatible video out, all on one board.

Status: hardware in design (schematic done, PCB layout in progress). Items marked **TBD** are decisions or specs the team still has to confirm.

## The problem

Prototyping a real-time vision pipeline on an FPGA needs three things working together: a camera input, a frame buffer in external memory, and a video output. A student or researcher who wants to try an idea (a filter, an edge detector, a custom scaler) usually has to assemble these from separate parts: a dev board, a camera adapter, an HDMI breakout and a memory board. Each combination brings its own pin mapping, voltage-level and signal-integrity problems, and the project stalls on wiring before any vision work starts.

Boards that integrate all of this are often closed, expensive, or tied to a vendor toolchain, which makes them hard to modify, teach with or reproduce.

## The solution

Vision Core puts the whole path on one open board:


PC host tool over UART
          ^
          |
camera --> FPGA --> SDRAM frame buffer --> FPGA --> HDMI-compatible output 

            
- A Lattice ECP5 FPGA that works with open-source toolchains, so the full design can be inspected and reproduced.
- External SDRAM sized and routed to run at 100 MHz as the frame buffer.
- Camera input, video output, USB, an ADC, LVDS-capable GPIO pairs and an ESP32 module on the same board.
- A host tool for talking to the board from a PC.
- Schematic, layout, gateware and host software all in one open repository.

Why not an existing board? **TBD:** before we publish this, add a short comparison table against two or three existing ECP5 and vendor boards (camera input, SDRAM, video output, price, openness). Only list claims we have checked against each board's documentation.

## Goals and success criteria

Each goal is a test someone else can run. A goal is done only when the test passes on real hardware.

### Must have

| ID | Goal | How we verify it |
| --- | --- | --- |
| M1 | Power rails come up correctly | 1.1 V, 2.5 V and 3.3 V rails measured within 5% of nominal at idle and under load, with no brown-outs or excessive heating over a 30 minute test design |
| M2 | FPGA can be programmed | Bitstream loads over JTAG and boots from flash after a power cycle |
| M3 | PC can talk to the board | The host tool finds the board's COM port by itself and gets PONG in response to PING |
| M4 | SDRAM works at speed | Full-array write and read-back pattern test at 100 MHz with zero errors for 10 minutes |
| M5 | Video output works | A 720p60 test pattern shown on a standard HDMI monitor |

### Should have 

| ID | Goal | How we verify it |
| --- | --- | --- |
| S1 | Live camera to screen | Camera frames captured into SDRAM and shown on the monitor in real time at 30 fps or better (resolution and camera module **TBD**) |
| S2 | GPIO pairs work as designed | Loopback test on every differential pair; bank 7 verified in both 2.5 V (LVDS) and 3.3 V (LVCMOS) modes |
| S3 | ADC captured by the host | A known input signal sampled by the FPGA and read back through the host tool (ADC specs **TBD**) |
| S4 | USB enumerates | The board shows up on a PC (USB role and class **TBD**) |
| S5 | Reproducible | A teammate who did not design the board can build the gateware and run the host tool from the repository instructions alone |


### Non-goals

- Not a commercial product: no FCC/CE certification and no production volume planning.
- Not aiming at resolutions above 1080p or at high-speed transceivers such as PCIe.
- Not a general Linux platform.

### Constraints

- Fabrication and assembly budget of about $300 for 2 assembled boards and 3 bare boards. Component costs are tracked separately.
- Designed in KiCad. The PCB is 4 layers, with the stackup under design review.

## Hardware overview

| Block | Details |
| --- | --- |
| FPGA | Lattice LFE5U-85F-6BG381C (ECP5, 381-ball BGA, 0.8 mm pitch) |
| Memory | SDR SDRAM at a 100 MHz clock target (part number **TBD**) |
| Video out | GPDI (HDMI-compatible) differential pairs |
| Camera in | Camera interface (module and format **TBD**) |
| USB | Differential pair to the FPGA (role **TBD**) |
| ADC | Analog input captured by the FPGA (part and resolution **TBD**) |
| Wireless | ESP32 module (**TBD**: confirm role) |
| GPIO | Header with differential pairs; bank 7 I/O voltage selectable between 2.5 V and 3.3 V |
| Other | LEDs, audio output |
| Power | 1.1 V core, 2.5 V auxiliary, 3.3 V I/O |

## Host tool 

A command-line program for Windows, written in C. It:

- finds the correct serial device on its own and connects over UART
- checks the connection with PING/PONG
- requests the ESP32 MAC address and shows device information
- sends custom commands and shows the connection settings
- reports timeouts, invalid responses and failed connections

More advanced features come after the FPGA-side UART logic is complete. The UART protocol is written down in (**TBD**).


## Roadmap

- [x] Schematic captured
- [ ] PCB layout finished and design review done
- [ ] Boards ordered
- [x] UART responder working in simulation and on a development board
- [x] SDRAM controller passing simulation 
- [ ] Host tool tested against the real FPGA responder
- [ ] Bring-up of the first board (M1 to M5)
- [ ] Camera to screen demo (S1)

## Team

- Cris: hardware (schematic, PCB layout, bring-up)
- Jason: gateware (UART responder, SDRAM controller)
- Max: host software (command-line tool)

## License

**TBD.** license for hardware ? (for example CERN-OHL) and one for software before the repository goes public? or MIT license works too
