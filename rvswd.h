
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
    
    // Command offsets (cached)
    uint offset_write_bits;
    uint offset_read_bits;
    uint offset_start;
    uint offset_stop;
    uint offset_reset;
 } rvswd_handle_t;
 
 typedef enum rvswd_result {
     RVSWD_OK = 0,
     RVSWD_FAIL = 1,
     RVSWD_INVALID_ARGS = 2,
     RVSWD_PARITY_ERROR = 3,
 } rvswd_result_t;


 rvswd_result_t rvswd_start(rvswd_handle_t* handle);
 rvswd_result_t rvswd_stop(rvswd_handle_t* handle);

 rvswd_result_t rvswd_reset(rvswd_handle_t* handle);

rvswd_result_t rvswd_init(rvswd_handle_t* handle);
rvswd_result_t rvswd_reset(rvswd_handle_t* handle);
rvswd_result_t rvswd_write(rvswd_handle_t* handle, uint8_t reg, uint32_t value);
rvswd_result_t rvswd_read(rvswd_handle_t* handle, uint8_t reg, uint32_t* value);


rvswd_result_t rvswd_pio_start(rvswd_handle_t* handle);
rvswd_result_t rvswd_pio_stop(rvswd_handle_t* handle);


rvswd_result_t rvswd_pio_init(rvswd_handle_t* handle);

rvswd_result_t rvswd_pio_write(rvswd_handle_t* handle, uint8_t reg, uint32_t value);

rvswd_result_t rvswd_pio_read(rvswd_handle_t* handle, uint8_t reg, uint32_t* value);