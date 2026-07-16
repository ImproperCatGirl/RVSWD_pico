#include "ch32_debug.h"

#include <inttypes.h>
#include <stdio.h>

#include <pico/time.h>

#include "FreeRTOS.h"
#include "debug_log.h"
#include "task.h"

rvswd_result_t ch32v20x_halt_microprocessor(rvswd_handle_t *handle) {
    rvswd_pio_write(handle, CH32_REG_DEBUG_DMCONTROL, 0x80000001);
    rvswd_pio_write(handle, CH32_REG_DEBUG_DMCONTROL, 0x80000001);

    int timeout = 1000;
    RVSWD_LOG("halt timeout start = %d\n", timeout);
    while (1) {
        uint32_t value;
        int ret = rvswd_pio_read(handle, CH32_REG_DEBUG_DMSTATUS, &value);
        if (ret) {
            RVSWD_LOG("DMSTATUS read failed at timeout = %d\n", timeout);
        }
        if (((value >> 8) & 0b11) == 0b11) {
            break;
        }
        if (timeout == 0) {
            printf("Failed to halt microprocessor, DMSTATUS=%" PRIx32 "\n", value);
            return RVSWD_FAIL;
        }
        timeout--;
    }
    RVSWD_LOG("halt timeout remaining = %d\n", timeout);

    rvswd_pio_write(handle, CH32_REG_DEBUG_DMCONTROL, 0x00000001);
    RVSWD_LOG("Microprocessor halted\n");
    return RVSWD_OK;
}

rvswd_result_t ch32v20x_resume_microprocessor(rvswd_handle_t *handle) {
    rvswd_pio_write(handle, CH32_REG_DEBUG_DMCONTROL, 0x80000001);
    rvswd_pio_write(handle, CH32_REG_DEBUG_DMCONTROL, 0x80000001);
    rvswd_pio_write(handle, CH32_REG_DEBUG_DMCONTROL, 0x00000001);
    rvswd_pio_write(handle, CH32_REG_DEBUG_DMCONTROL, 0x40000001);

    int timeout = 1000;
    while (1) {
        uint32_t value;
        rvswd_pio_read(handle, CH32_REG_DEBUG_DMSTATUS, &value);
        if ((((value >> 10) & 0b11) == 0b11)) {
            RVSWD_LOG("DMSTATUS after resume = %08" PRIX32 "\n", value);
            break;
        }
        if (timeout == 0) {
            printf("Failed to resume microprocessor, DMSTATUS=%" PRIx32 "\n", value);
            return RVSWD_FAIL;
        }
        timeout--;
        busy_wait_ms(10);
    }
    return RVSWD_OK;
}

rvswd_result_t ch32v20x_reset_microprocessor_and_run(rvswd_handle_t *handle) {
    rvswd_pio_write(handle, CH32_REG_DEBUG_DMCONTROL, 0x80000001);
    rvswd_pio_write(handle, CH32_REG_DEBUG_DMCONTROL, 0x80000001);
    rvswd_pio_write(handle, CH32_REG_DEBUG_DMCONTROL, 0x00000001);
    rvswd_pio_write(handle, CH32_REG_DEBUG_DMCONTROL, 0x00000003);

    uint8_t timeout = 5;
    while (1) {
        uint32_t value;
        rvswd_pio_read(handle, CH32_REG_DEBUG_DMSTATUS, &value);
        if (((value >> 18) & 0b11) == 0b11) {
            break;
        }
        if (timeout == 0) {
            printf("Failed to reset microprocessor\n");
            return RVSWD_FAIL;
        }
        timeout--;
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    rvswd_pio_write(handle, CH32_REG_DEBUG_DMCONTROL, 0x00000001);
    busy_wait_ms(10);
    rvswd_pio_write(handle, CH32_REG_DEBUG_DMCONTROL, 0x10000001);
    busy_wait_ms(10);
    rvswd_pio_write(handle, CH32_REG_DEBUG_DMCONTROL, 0x00000001);

    return RVSWD_OK;
}

bool ch32v20x_write_cpu_reg(rvswd_handle_t *handle, uint16_t regno, uint32_t value) {
    uint32_t command = regno
                       | (1 << 16)
                       | (1 << 17)
                       | (2 << 20)
                       | (0 << 24);

    rvswd_write(handle, CH32_REG_DEBUG_DATA0, value);
    rvswd_write(handle, CH32_REG_DEBUG_COMMAND, command);
    return true;
}

bool ch32v20x_read_cpu_reg(rvswd_handle_t *handle, uint16_t regno, uint32_t *value_out) {
    uint32_t command = regno
                       | (0 << 16)
                       | (1 << 17)
                       | (2 << 20)
                       | (0 << 24);

    rvswd_write(handle, CH32_REG_DEBUG_COMMAND, command);
    rvswd_read(handle, CH32_REG_DEBUG_DATA0, value_out);
    return true;
}
