RVSWD_pico


RVSWD_pico is a high-performance, PIO-accelerated debug probe firmware for WCH QingKeV4 RISC-V microcontrollers.

Features:

  No WCH-LinkE required
  
  OpenOCD Support: Works with the ddmi transport in my OpenOCD fork.
  
  DMI Batching: Eliminates USB "ping-pong" latency on WCH-LinkE. dumps 64KB of SRAM in ~1.2s.

Pinout:

  GP7	SDI_CLK
  
  GP8 SDI_DIO
  
  Signal Integrity Note: If you are using "crappy" long DuPont wires, 
  it is highly recommended to place a 22Ω resistor in series,on both the CLK and DIO lines near the Pico to dampen reflections and ringing.
  
Driver Setup (Windows Only)

If you are on Windows, you must replace the default USB driver to allow OpenOCD to talk to the probe:

    Download and open Zadig.
    
    Select MCU Debug Probe from the device list.
    
    Select WinUSB as the driver.
    
    Click Replace Driver.

    
UDEV setup: "# RVSWD_pico - WCH RISC-V Debug Probe
SUBSYSTEM=="usb", ATTR{idVendor}=="cafe", ATTR{idProduct}=="4008", MODE="0666", GROUP="plugdev"" 

Note: this probe current only implmented QingKe V4's debug protocol, V3 requires some minoe changes, but I haven't got my V3 board yet.
