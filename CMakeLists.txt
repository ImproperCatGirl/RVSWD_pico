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


set(CMAKE_BUILD_TYPE Debug)

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(RVSWD_pico C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()


# Pull in FreeRTOS
include(FreeRTOS_Kernel_import.cmake)


# Add executable. Default name is the project name, version 0.1

add_executable(RVSWD_pico RVSWD_pico.c  RVSWD_bitbang_CPU.c RVSWD_bitbang_CPU_2.c RVSWD_pio.c)

pico_set_program_name(RVSWD_pico "RVSWD_pico")
pico_set_program_version(RVSWD_pico "0.1")

# Generate PIO header
pico_generate_pio_header(RVSWD_pico ${CMAKE_CURRENT_LIST_DIR}/blink.pio
    ${CMAKE_CURRENT_LIST_DIR}/RVSWD_full.pio
)

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(RVSWD_pico 1)
pico_enable_stdio_usb(RVSWD_pico 0)

# Add the standard library to the build
target_link_libraries(RVSWD_pico
        pico_stdlib
        FreeRTOS-Kernel FreeRTOS-Kernel-Heap4 hardware_spi hardware_gpio hardware_dma)

# Add the standard include files to the build
target_include_directories(RVSWD_pico PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

# Add any user requested libraries
target_link_libraries(RVSWD_pico 
        hardware_pio
        hardware_gpio
        )

pico_add_extra_outputs(RVSWD_pico)

