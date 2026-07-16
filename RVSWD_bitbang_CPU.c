/**
 * RVSWD protocol implementation informed by:
 * - https://github.com/aappleby/picorvd
 * - https://github.com/Nicolai-Electronics/esp32-component-rvswd
 *
 * SPDX-License-Identifier: MIT
 */
 #include "rvswd.h"
 #include <inttypes.h>
#include <pico/time.h>
 #include <stdint.h>
 #include "hardware/gpio.h"

 #define BIT_DELAY_US 2
 
 rvswd_result_t rvswd_init(rvswd_handle_t* handle) {
     
    gpio_init(handle->swclk);
    gpio_set_function(handle->swclk, GPIO_FUNC_SIO);
    gpio_set_dir(handle->swclk, GPIO_OUT);
    

    gpio_set_function(handle->swdio, GPIO_FUNC_SIO);  // Software control
    gpio_set_pulls(handle->swdio, true, false);       // Internal pull-up (~50kΩ)
    gpio_set_dir(handle->swdio, GPIO_IN); //Hi-Z at init
    return RVSWD_OK;
 }
 
 rvswd_result_t rvswd_start(rvswd_handle_t* handle) {
     // Start with both lines high
     gpio_set_dir(handle->swdio, GPIO_IN);  // Release to pull-up
    gpio_put(handle->swclk, 1);
    busy_wait_us(2);
 
    // SWDIO low, SWCLK high
    gpio_put(handle->swdio, 0);
    gpio_set_dir(handle->swdio, GPIO_OUT);
    gpio_put(handle->swclk, 1);
    busy_wait_us(1);
 
     // SWCLK low
    gpio_put(handle->swclk, 0);
    busy_wait_us(1);
     return RVSWD_OK;
 }
 
 rvswd_result_t rvswd_stop(rvswd_handle_t* handle) {
     // SWDIO low
    gpio_put(handle->swdio, 0);
    gpio_set_dir(handle->swdio, GPIO_OUT);
    busy_wait_us(1);

    // SWCLK high
    gpio_put(handle->swclk, 1);
    busy_wait_us(2);

    // SWDIO high (release)
    gpio_set_dir(handle->swdio, GPIO_IN);
    busy_wait_us(1);
     return RVSWD_OK;
 }
 
 rvswd_result_t rvswd_reset(rvswd_handle_t* handle) {
     // SWDIO high
    gpio_set_dir(handle->swdio, GPIO_IN);
    busy_wait_us(1);

    // 100 clocks with SWDIO high
    for (uint8_t i = 0; i < 100; i++) {
        gpio_put(handle->swclk, 0);
        busy_wait_us(1);
        gpio_put(handle->swclk, 1);
        busy_wait_us(1);
    }
    return rvswd_stop(handle);
 }
 
 void rvswd_write_bit(rvswd_handle_t* handle, bool value) {
     // Set data
    if (value) {
        gpio_set_dir(handle->swdio, GPIO_IN);  // Release to pull-up
    } else {
        gpio_put(handle->swdio, 0);
        gpio_set_dir(handle->swdio, GPIO_OUT);  // Drive low
    }

    // Clock cycle: low -> high
    gpio_put(handle->swclk, 0);
    busy_wait_us(BIT_DELAY_US);
    gpio_put(handle->swclk, 1);  // Sample on rising edge
    busy_wait_us(BIT_DELAY_US);
 }
 
 bool rvswd_read_bit(rvswd_handle_t* handle) {
     // Release SWDIO (input, pull-up)
    gpio_set_dir(handle->swdio, GPIO_IN);

    // Clock cycle: low -> high
    gpio_put(handle->swclk, 0);
    busy_wait_us(BIT_DELAY_US);
    gpio_put(handle->swclk, 1);  // Target drives on low, sample on/after rising
    busy_wait_us(BIT_DELAY_US);
     return gpio_get(handle->swdio);
 }
 
 rvswd_result_t rvswd_write(rvswd_handle_t* handle, uint8_t reg, uint32_t value) {
     rvswd_start(handle);
 
     // ADDR HOST
     bool parity = false;  // This time it's odd parity?
     for (uint8_t position = 0; position < 7; position++) {
         bool bit = (reg >> (6 - position)) & 1;
         rvswd_write_bit(handle, bit);
         if (bit) parity = !parity;
     }
 
     // Operation: write
     rvswd_write_bit(handle, true);
     parity = !parity;
 
     // Parity bit (even)
     rvswd_write_bit(handle, parity);
 
     rvswd_write_bit(handle, 1);
     rvswd_write_bit(handle, 0);
     rvswd_write_bit(handle, 1);
     rvswd_write_bit(handle, 0);
     rvswd_write_bit(handle, 1);
 
     // Data
     parity = false;  // This time it's even parity?
     for (uint8_t position = 0; position < 32; position++) {
         bool bit = (value >> (31 - position)) & 1;
         rvswd_write_bit(handle, bit);
         if (bit) parity = !parity;
     }
 
     // Parity bit
     rvswd_write_bit(handle, parity);
 
     rvswd_write_bit(handle, 1);
     rvswd_write_bit(handle, 0);
     rvswd_write_bit(handle, 1);
     rvswd_write_bit(handle, 1);
     rvswd_write_bit(handle, 1);
 
     rvswd_stop(handle);
 
     return RVSWD_OK;
 }
 
 rvswd_result_t rvswd_read(rvswd_handle_t* handle, uint8_t reg, uint32_t* value) {
     bool parity;
     gpio_put(handle->logic_helper_pin, 1);
     rvswd_start(handle);
     gpio_put(handle->logic_helper_pin, 0);
 
     // ADDR HOST
     parity = false;
     for (uint8_t position = 0; position < 7; position++) {
         bool bit = (reg >> (6 - position)) & 1;
         rvswd_write_bit(handle, bit);
         if (bit) parity = !parity;
     }
 
     // Operation: read
     rvswd_write_bit(handle, false);
 
     // Parity bit (even)
     rvswd_write_bit(handle, parity);
 
     rvswd_write_bit(handle, 1);
     rvswd_write_bit(handle, 0);
     rvswd_write_bit(handle, 1);
     rvswd_write_bit(handle, 0);
     rvswd_write_bit(handle, 1);
 
     *value = 0;
 
     // Data
     parity = false;
     for (uint8_t position = 0; position < 32; position++) {
         bool bit = rvswd_read_bit(handle);
         if (bit) {
             *value |= 1 << (31 - position);
         }
         if (bit) parity = !parity;
     }
 
     // Parity bit
     bool parity_read = rvswd_read_bit(handle);
 
     rvswd_write_bit(handle, 1);
     rvswd_write_bit(handle, 0);
     rvswd_write_bit(handle, 1);
     rvswd_write_bit(handle, 1);
     rvswd_write_bit(handle, 1);
 
     gpio_put(handle->logic_helper_pin, 1);
     rvswd_stop(handle);
 
     gpio_put(handle->logic_helper_pin, 0);
     return (parity == parity_read) ? RVSWD_OK : RVSWD_FAIL;
 }
