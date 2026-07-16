// Portions of this project are derived from
// Copyright (c) 2025 Nicolai Electronics

#include <stdio.h>

#include <hardware/gpio.h>
#include <hardware/timer.h>
#include <pico/stdlib.h>
#include <pico/time.h>

#include "FreeRTOS.h"
#include "task.h"
#include "tusb.h"
#include "tusb_config.h"

#include "SWIO.h"
#include "board_config.h"
#include "cdc_uart.h"
#include "ch32_debug.h"
#include "host_comms.h"
#include "rvswd.h"

rvswd_handle_t wch_handle_pio;

TaskHandle_t xHandleTinyUSB = NULL;
TaskHandle_t xHandleDDMI = NULL;

extern TaskHandle_t uart_taskhandle;
extern protocol_state state;

void vApplicationMallocFailedHook(void) {
    printf("malloc failed\n");
    for (;;);
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    (void)xTask;
    printf("%s stack overflow\n", pcTaskName);
    while (1);
}

void USB_Task(void *params) {
    (void)params;

    tusb_rhport_init_t dev_init = {
        .role = TUSB_ROLE_DEVICE,
        .speed = TUSB_SPEED_AUTO
    };
    tusb_init(BOARD_TUD_RHPORT, &dev_init);

    while (1) {
        tud_task();
    }
}

static void init_logic_helper_pin(void) {
    gpio_init(LOGIC_ANALYZER_HELPER_PIN);
    gpio_set_function(LOGIC_ANALYZER_HELPER_PIN, GPIO_FUNC_SIO);
    gpio_set_dir(LOGIC_ANALYZER_HELPER_PIN, GPIO_OUT);
    gpio_put(LOGIC_ANALYZER_HELPER_PIN, 0);
}

static void init_rvswd_probe(void) {
    wch_handle_pio.swclk = RVSWD_CLK_PIN;
    wch_handle_pio.swdio = RVSWD_DIO_PIN;
    wch_handle_pio.logic_helper_pin = LOGIC_ANALYZER_HELPER_PIN;

    rvswd_pio_init(&wch_handle_pio);
    rvswd_pio_reset(&wch_handle_pio);
}

static void request_target_core_reset(void) {
    rvswd_pio_write(&wch_handle_pio, CH32_REG_DEBUG_DMCONTROL, 0x80000001);
    rvswd_pio_write(&wch_handle_pio, CH32_REG_DEBUG_DMCONTROL, 0x80000001);
    rvswd_pio_write(&wch_handle_pio, CH32_REG_DEBUG_DMCONTROL, 0x00000001);
    rvswd_pio_write(&wch_handle_pio, CH32_REG_DEBUG_DMCONTROL, 0x00000003);
}

static void detect_target_protocol(void) {
    SWIO_reset(SWIO_DATA_PIN);

    uint32_t test_data = get_data(CH32_REG_DEBUG_DMSTATUS);
    if (test_data != 0xFFFFFFFF && test_data != 0x0) {
        state = TYPE_SWIO;
    } else {
        state = TYPE_RVSWD;
    }
}

static void start_tasks(void) {
    xTaskCreate(USB_Task, "USB", 4096, NULL, 3, &xHandleTinyUSB);
    xTaskCreate(ddmi_worker_task, "DDMI", 512, NULL, 4, &xHandleDDMI);
    xTaskCreate(cdc_thread, "UART", configMINIMAL_STACK_SIZE, NULL, 2, &uart_taskhandle);
    vTaskStartScheduler();
}

int main(void) {
    stdio_init_all();
    timer_hw->dbgpause = 0;

    init_logic_helper_pin();
    cdc_uart_init();
    init_rvswd_probe();
    request_target_core_reset();

#if !defined(portSUPPORT_SMP) || (portSUPPORT_SMP != 1)
#error "FreeRTOS must be configured for SMP mode."
#endif

#if CFG_TUSB_OS == OPT_OS_PICO
#warning "TinyUSB is configured for Pico OS; RVSWD_pico expects FreeRTOS."
#endif

    busy_wait_ms(500);
    detect_target_protocol();
    ch32v20x_halt_microprocessor(&wch_handle_pio);
    ch32v20x_resume_microprocessor(&wch_handle_pio);
    start_tasks();

    while (1);
}
