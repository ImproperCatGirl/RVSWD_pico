# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")


set(CMAKE_BUILD_TYPE RelWithDebInfo)

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(RVSWD_pico C CXX ASM)


# Optional: If you want to completely skip bundled USB features (e.g., stdio over USB)
set(PICO_STDIO_USB 0)  # Disables pico-sdk's USB serial; use TinyUSB's CDC if needed
set(PICO_TINYUSB_PATH "")          # Disable Pico SDK’s TinyUSB
set(PICO_TINYUSB_HOST_ENABLED 0)
set(PICO_TINYUSB_DEVICE_ENABLED 0)


set(PICO_SDK_TINYUSB_FREERTOS 1)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()



# Pull in FreeRTOS
include(FreeRTOS_Kernel_import.cmake)


# Path to the submodule we added
set(TINYUSB_PATH ${CMAKE_CURRENT_SOURCE_DIR}/tinyusb)

# 1. Create the library target
add_library(tinyusb_upstream STATIC)

# 2. Add all the necessary TinyUSB source files
# This includes the core stack and the RP2040-specific port
file(GLOB_RECURSE TINYUSB_SOURCES
    "${TINYUSB_PATH}/src/*.c"
    "${TINYUSB_PATH}/hw/mcu/raspberry_pi/rp2040/rp2040_usb.c"
)
target_sources(tinyusb_upstream PRIVATE ${TINYUSB_SOURCES})

# 3. Add the necessary include directories
target_include_directories(tinyusb_upstream
    PUBLIC
        # This allows your main app to #include "tusb.h"
        "${TINYUSB_PATH}/src" 
        "${CMAKE_CURRENT_LIST_DIR}"
        "${PICO_SDK_PATH}/src/rp2_common/pico_fix/"
    PRIVATE
        # Include paths needed by the library itself
        "${TINYUSB_PATH}/hw"
)

# 4. Add required compile definitions
target_compile_definitions(tinyusb_upstream
    PUBLIC
        # This tells TinyUSB which MCU port to use
        CFG_TUSB_MCU=OPT_MCU_RP2040
        #TUD_OPT_RP2040_USB_DEVICE_ENUMERATION_FIX=1
)


target_link_libraries(tinyusb_upstream
    PRIVATE
        FreeRTOS-Kernel
)


# Add executable. Default name is the project name, version 0.1

add_executable(RVSWD_pico RVSWD_pico.c  RVSWD_bitbang_CPU.c RVSWD_pio.c
usb_desc.c  rv_ddmi.c cdc_uart.c SWIO.c)

pico_set_program_name(RVSWD_pico "RVSWD_pico")
pico_set_program_version(RVSWD_pico "0.1")

# Generate PIO header
pico_generate_pio_header(RVSWD_pico ${CMAKE_CURRENT_LIST_DIR}/blink.pio
    ${CMAKE_CURRENT_LIST_DIR}/RVSWD_full.pio ${CMAKE_CURRENT_LIST_DIR}/SWIO.pio 
)




# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(RVSWD_pico 1)
pico_enable_stdio_usb(RVSWD_pico 0)

# Add the standard library to the build
target_link_libraries(RVSWD_pico
        pico_stdlib
        FreeRTOS-Kernel FreeRTOS-Kernel-Heap4 hardware_spi hardware_gpio hardware_dma)


# Add any user requested libraries
target_link_libraries(RVSWD_pico 
        hardware_pio
        hardware_gpio
        tinyusb_upstream
        )

pico_add_extra_outputs(RVSWD_pico)

