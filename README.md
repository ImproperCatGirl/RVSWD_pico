# RVSWD_pico

RVSWD_pico is Raspberry Pi Pico firmware for a PIO-accelerated debug probe for
WCH QingKe RISC-V microcontrollers. It exposes a TinyUSB vendor interface used
by the DDMI transport in a matching OpenOCD fork, plus a CDC UART bridge.

The current firmware targets QingKe V4 devices. QingKe V3 support is expected
to need small protocol changes and is not validated here yet.

## Hardware

Default probe pins:

| Pico GPIO | Signal |
| --- | --- |
| GP7 | RVSWD clock |
| GP8 | RVSWD data |
| GP4 | UART TX |
| GP5 | UART RX |
| GP10 | logic analyzer helper |
| GP14 | SWIO pull helper |
| GP15 | SWIO data |

For long jumper wires, place about 22 ohms in series with the clock and data
lines near the Pico to reduce ringing.

## Build

Prerequisites:

- Raspberry Pi Pico SDK 2.x, available through `PICO_SDK_PATH`
- FreeRTOS-Kernel with the RP2040 port, available through `FREERTOS_KERNEL_PATH`
- Arm embedded GCC toolchain compatible with the Pico SDK
- TinyUSB submodule checked out with `git submodule update --init`

Build out of tree:

```sh
cmake -S . -B build
cmake --build build
```

Flash the generated `build/RVSWD_pico.uf2` to the Pico in BOOTSEL mode.

## Host Setup

The vendor interface uses VID `0xcafe` and PID `0x4008`.

Linux udev rule:

```udev
SUBSYSTEM=="usb", ATTR{idVendor}=="cafe", ATTR{idProduct}=="4008", MODE="0666", GROUP="plugdev"
```

On Windows, use Zadig to replace the default driver for `MCU Debug Probe` with
WinUSB so OpenOCD can access the vendor interface.

## Firmware Architecture

Startup lives in `RVSWD_pico.c`. It initializes stdio, the board pins, the CDC
UART bridge, the RVSWD PIO engine, performs SWIO/RVSWD target detection, and
starts the FreeRTOS tasks.

The DDMI USB command path lives in `rv_ddmi.c`. TinyUSB notifies the DDMI worker
when vendor data arrives. The worker reads chained register operations, dispatches
them to either RVSWD or SWIO, and writes packed 32-bit responses back through the
vendor endpoint.

The RVSWD transport lives in `RVSWD_pio.c` and `RVSWD_full.pio`. The `.pio` file
is the source of truth; generated `*.pio.h` files are build artifacts.

The SWIO transport lives in `SWIO.c` and `SWIO.pio`. It uses PIO1 and the helper
pull pin for WCH single-wire debug probing.

USB descriptors and TinyUSB configuration live in `usb_desc.c`, `usb_desc.h`,
and `tusb_config.h`. CDC UART bridge behavior is in `cdc_uart.c`.

## Acknowledgements

The RVSWD protocol code was built with reference to and adapted from
[aappleby/picorvd](https://github.com/aappleby/picorvd) and
[Nicolai-Electronics/esp32-component-rvswd](https://github.com/Nicolai-Electronics/esp32-component-rvswd).
See [ACKNOWLEDGEMENTS.md](ACKNOWLEDGEMENTS.md) for details.

## Repository Notes

Generated firmware, CMake files, object files, static archives, and generated PIO
headers are intentionally ignored. Keep source changes in the C files, headers,
`.pio` files, CMake project files, and documentation.

TinyUSB is tracked as a git submodule. The Pico SDK and FreeRTOS kernel are
external dependencies resolved by their import CMake files and environment paths.
