
/**
 * Copyright (c) 2025 Nicolai Electronics
 *
 * SPDX-License-Identifier: MIT
 */

 #pragma once

 #include <hardware/pio.h>
#include <pico/types.h>
#include <stdint.h>
 typedef struct rvswd_handle {
     uint swdio;
     uint swclk;
    // PIO state (loaded once)
    PIO pio;
    uint sm;
    uint pio_offset; // Program offset
    uint logic_helper_pin;
 } rvswd_handle_t;
 
 typedef enum rvswd_opcode {
    RVSWD_WRITE,
    RVSWD_READ,
    RVSWD_RESET,
} rvswd_opcode_t;

typedef struct {
    uint8_t serial;
    rvswd_opcode_t opcode;
    union {
        struct {  // For READ
            uint8_t addr;
        } read;
        struct {  // For WRITE
            uint8_t addr;
            uint32_t data_to_target;
        } write;
        // For RESET: no extra fields
    } params;
} rvswd_op_t;


 typedef enum rvswd_result {
     RVSWD_OK = 0,
     RVSWD_FAIL = 1,
     RVSWD_INVALID_ARGS = 2,
     RVSWD_PARITY_ERROR = 3,
 } rvswd_result_t;

 typedef struct rvswd_op_result
 {
    rvswd_opcode_t opcode;
    rvswd_result_t status;
    uint32_t data_from_target;
    uint8_t serial;
 } rvswd_op_result_t;

 rvswd_result_t rvswd_start(rvswd_handle_t* handle);
 rvswd_result_t rvswd_stop(rvswd_handle_t* handle);

 rvswd_result_t rvswd_reset(rvswd_handle_t* handle);

rvswd_result_t rvswd_init(rvswd_handle_t* handle);
rvswd_result_t rvswd_reset(rvswd_handle_t* handle);
rvswd_result_t rvswd_write(rvswd_handle_t* handle, uint8_t reg, uint32_t value);
rvswd_result_t rvswd_read(rvswd_handle_t* handle, uint8_t reg, uint32_t* value);


rvswd_result_t rvswd_pio_start(rvswd_handle_t* handle);
rvswd_result_t rvswd_pio_stop(rvswd_handle_t* handle);


rvswd_result_t rvswd_pio_reset(rvswd_handle_t* handle);
rvswd_result_t rvswd_pio_init(rvswd_handle_t* handle);

rvswd_result_t rvswd_pio_write(rvswd_handle_t* handle, uint8_t reg, uint32_t value);

rvswd_result_t rvswd_pio_read(rvswd_handle_t* handle, uint8_t reg, uint32_t* value);