// rvswd_wch.c
#include "pico/stdlib.h"
#include "hardware/gpio.h"
/*
#define SWDIO_PIN 14  // GPIO0 for SWDIO
#define SWCLK_PIN 16  // GPIO1 for SWCLK
#define BIT_DELAY_US 1  // ~500 kHz

typedef enum {
    RVSWD_OK = 0,
    RVSWD_FAIL = 1
} rvswd_result_t;

void rvswd_init(void) {
    gpio_init(SWDIO_PIN);
    gpio_set_function(SWDIO_PIN, GPIO_FUNC_SIO);
    gpio_set_pulls(SWDIO_PIN, true, false);
    gpio_set_dir(SWDIO_PIN, GPIO_IN);

    gpio_init(SWCLK_PIN);
    gpio_set_function(SWCLK_PIN, GPIO_FUNC_SIO);
    gpio_set_dir(SWCLK_PIN, GPIO_OUT);
    gpio_put(SWCLK_PIN, 1);
}

rvswd_result_t rvswd_start(void) {
    gpio_set_dir(SWDIO_PIN, GPIO_IN);
    gpio_put(SWCLK_PIN, 1);
    sleep_us(2);

    gpio_put(SWDIO_PIN, 0);
    gpio_set_dir(SWDIO_PIN, GPIO_OUT);
    gpio_put(SWCLK_PIN, 1);
    sleep_us(1);

    gpio_put(SWCLK_PIN, 0);
    sleep_us(1);
    return RVSWD_OK;
}

rvswd_result_t rvswd_stop(void) {
    gpio_put(SWDIO_PIN, 0);
    gpio_set_dir(SWDIO_PIN, GPIO_OUT);
    sleep_us(1);

    gpio_put(SWCLK_PIN, 1);
    sleep_us(2);

    gpio_set_dir(SWDIO_PIN, GPIO_IN);
    sleep_us(1);
    return RVSWD_OK;
}

rvswd_result_t rvswd_reset(void) {
    gpio_set_dir(SWDIO_PIN, GPIO_IN);
    sleep_us(1);

    for (uint8_t i = 0; i < 100; i++) {
        gpio_put(SWCLK_PIN, 0);
        sleep_us(1);
        gpio_put(SWCLK_PIN, 1);
        sleep_us(1);
    }
    return rvswd_stop();
}

void rvswd_write_bit(bool value) {
    if (value) {
        gpio_set_dir(SWDIO_PIN, GPIO_IN);
    } else {
        gpio_put(SWDIO_PIN, 0);
        gpio_set_dir(SWDIO_PIN, GPIO_OUT);
    }
    gpio_put(SWCLK_PIN, 0);
    sleep_us(BIT_DELAY_US);
    gpio_put(SWCLK_PIN, 1);
    sleep_us(BIT_DELAY_US);
}

bool rvswd_read_bit(void) {
    gpio_set_dir(SWDIO_PIN, GPIO_IN);
    gpio_put(SWCLK_PIN, 0);
    sleep_us(BIT_DELAY_US);
    gpio_put(SWCLK_PIN, 1);
    sleep_us(BIT_DELAY_US);
    return gpio_get(SWDIO_PIN);
}

rvswd_result_t rvswd_write(uint8_t reg, uint32_t value) {
    rvswd_start();

    // Address-host (7 bits)
    bool parity = true;  // Even parity (WCH starts at 1)
    for (uint8_t i = 0; i < 7; i++) {
        bool bit = (reg >> (6 - i)) & 1;
        rvswd_write_bit(bit);
        if (bit) parity = !parity;
    }

    // Operation (2 bits): write = 10
    rvswd_write_bit(true);
    if (true) parity = !parity;
    rvswd_write_bit(false);

    // Parity-host (1 bit)
    rvswd_write_bit(parity);

    // Data-host (32 bits)
    parity = true;  // Even parity
    for (uint8_t i = 0; i < 32; i++) {
        bool bit = (value >> (31 - i)) & 1;
        rvswd_write_bit(bit);
        if (bit) parity = !parity;
    }

    // Address-target (7 bits, dummy for write)
    for (uint8_t i = 0; i < 7; i++) {
        rvswd_write_bit(false);  // Dummy 0s
    }

    // Data-target (32 bits, dummy)
    for (uint8_t i = 0; i < 32; i++) {
        rvswd_write_bit(false);
    }

    // Status (2 bits, dummy write)
    rvswd_write_bit(false);
    rvswd_write_bit(false);

    // Parity-target (1 bit, dummy)
    rvswd_write_bit(false);

    rvswd_stop();
    return RVSWD_OK;
}

rvswd_result_t rvswd_read(uint8_t reg, uint32_t *value, uint8_t *status) {
    bool parity = true;  // Even parity

    rvswd_start();

    // Address-host (7 bits)
    for (uint8_t i = 0; i < 7; i++) {
        bool bit = (reg >> (6 - i)) & 1;
        rvswd_write_bit(bit);
        if (bit) parity = !parity;
    }

    // Operation (1 bit): read = 0
    rvswd_write_bit(false);

    // Parity-host (2 bits)
    rvswd_write_bit(parity);
    rvswd_write_bit(false);  // Second bit (WCH uses 2)

    // Padding-host (4 bits)
    for (uint8_t i = 0; i < 4; i++) {
        rvswd_write_bit(false);
    }

    // Data-target (32 bits)
    *value = 0;
    parity = true;  // Even parity
    for (uint8_t i = 0; i < 32; i++) {
        bool bit = rvswd_read_bit();
        if (bit) *value |= (1u << (31 - i));
        if (bit) parity = !parity;
    }

    // Parity-target (2 bits)
    bool parity_read1 = rvswd_read_bit();
    bool parity_read2 = rvswd_read_bit();
    if (parity != parity_read1 || parity_read2) return RVSWD_FAIL;

    // Status (2 bits)
    *status = 0;
    if (rvswd_read_bit()) *status |= 2;
    if (rvswd_read_bit()) *status |= 1;

    // Padding-target (2 bits, ignored)
    rvswd_read_bit();
    rvswd_read_bit();

    rvswd_stop();
    return (*status == 0x3) ? RVSWD_OK : RVSWD_FAIL;  // Expect busy (0x3) for test
}

// Test function for OpenOCD startup packet
rvswd_result_t rvswd_test_openocd(void) {
    uint32_t value;
    uint8_t status;
    rvswd_result_t res = rvswd_read(0x11, &value, &status);
    if (res == RVSWD_OK && value == 0xffffffff && status == 0x3) {
        return RVSWD_OK;  // Matches expected: dmstatus read, all 1s, busy
    }
    return RVSWD_FAIL;
}*/