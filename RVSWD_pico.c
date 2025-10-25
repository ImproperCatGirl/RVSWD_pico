#include <hardware/gpio.h>
#include <hardware/structs/io_bank0.h>
#include <hardware/timer.h>
#include <pico/time.h>
#include <stdio.h>
#include "class/vendor/vendor_device.h"
#include "common/tusb_types.h"
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "portmacro.h"
#include "projdefs.h"
#include "time.h"

#include <pico/time.h>
#include "blink.pio.h"
#include "rvswd.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "tusb.h"
#include "tusb_config.h"


#define CH32_REG_DEBUG_DATA0        0x04  // Data register 0, can be used for temporary storage of data
#define CH32_REG_DEBUG_DATA1        0x05  // Data register 1, can be used for temporary storage of data
#define CH32_REG_DEBUG_DMCONTROL    0x10  // Debug module control register
#define CH32_REG_DEBUG_DMSTATUS     0x11  // Debug module status register
#define CH32_REG_DEBUG_HARTINFO     0x12  // Microprocessor status register
#define CH32_REG_DEBUG_ABSTRACTCS   0x16  // Abstract command status register
#define CH32_REG_DEBUG_COMMAND      0x17  // Astract command register
#define CH32_REG_DEBUG_ABSTRACTAUTO 0x18  // Abstract command auto-executtion
#define CH32_REG_DEBUG_PROGBUF0     0x20  // Instruction cache register 0
#define CH32_REG_DEBUG_PROGBUF1     0x21  // Instruction cache register 1
#define CH32_REG_DEBUG_PROGBUF2     0x22  // Instruction cache register 2
#define CH32_REG_DEBUG_PROGBUF3     0x23  // Instruction cache register 3
#define CH32_REG_DEBUG_PROGBUF4     0x24  // Instruction cache register 4
#define CH32_REG_DEBUG_PROGBUF5     0x25  // Instruction cache register 5
#define CH32_REG_DEBUG_PROGBUF6     0x26  // Instruction cache register 6
#define CH32_REG_DEBUG_PROGBUF7     0x27  // Instruction cache register 7
#define CH32_REG_DEBUG_HALTSUM0     0x40  // Halt status register
#define CH32_REG_DEBUG_CPBR         0x7C  // Capability register
#define CH32_REG_DEBUG_CFGR         0x7D  // Configuration register
#define CH32_REG_DEBUG_SHDWCFGR     0x7E  // Shadow configuration register

#define CH32_REGS_CSR 0x0000  // Offsets for accessing CSRs.
#define CH32_REGS_GPR 0x1000  // Offsets for accessing general-purpose (x)registers.

#define CH32_CFGR_KEY   0x5aa50000
#define CH32_CFGR_OUTEN (1 << 10)

#define CH32_CODE_BEGIN 0x08000000  // The start of CH32 CODE Flash region
#define CH32_CODE_END   0x08004000  // the end of the CH32 CODE Flash region

#define CH32V20X_FLASH_STATR 0x4002200C  // Flash status register
#define CH32V20X_FLASH_CTLR  0x40022010  // Flash configuration register
#define CH32_FLASH_ADDR      0x40022014  // Flash address register

static uint8_t const ch32v20x_readmem[] = {0x88, 0x41, 0x02, 0x90};
static uint8_t const ch32v20x_writemem[] = {0x88, 0xc1, 0x02, 0x90};

rvswd_handle_t wch_handle_pio;

QueueHandle_t cmd_queue;

QueueHandle_t result_queue;

#define PULL_HELPER_PIN 14

#define LOGIC_ANALYZER_HELPER_PIN 10

void vApplicationMallocFailedHook(void) {
    printf("malloc failed!\n");
    for(;;); // Halt or handle error
}

void vApplicationStackOverflowHook( TaskHandle_t xTask,
    char * pcTaskName )
{
    printf("%s Stack Overflow\n", pcTaskName);
    while(1);
}


void RVSWD_Task(void *params)
{
    uint32_t data0_val = 0;
    while(1)
    {
        if(ulTaskNotifyTake(pdTRUE, 0)) // force flush the queue
        {
            tud_vendor_flush();
        }
        rvswd_op_t cmd = {0};
        rvswd_op_result_t res = {0};
        xQueueReceive(cmd_queue, &cmd, portMAX_DELAY);
        res.opcode = cmd.opcode;
        res.serial = cmd.serial;
        if(cmd.opcode == RVSWD_WRITE)
        {
            res.status = rvswd_pio_write(&wch_handle_pio, cmd.addr, cmd.data_to_target);
        }
        if(cmd.opcode == RVSWD_READ)
        {
            res.status = rvswd_pio_read(&wch_handle_pio, cmd.addr, &res.data_from_target);
        }

        uint8_t buf_tmp[10] = {0};
        uint8_t buf_tmp_len = 0;
        if(res.opcode == RVSWD_WRITE)
        {
            buf_tmp_len = 2;
            buf_tmp[0] = res.serial;
            buf_tmp[1] = res.status;
             // 1 byte status + 1 byte serial
        }
        if(res.opcode == RVSWD_READ)
        {
            buf_tmp_len = 6;
            buf_tmp[0] = res.serial;
            buf_tmp[1] = res.status;
            memcpy(buf_tmp + 2, &res.data_from_target, sizeof(res.data_from_target));
             // 1 byte status + 1 byte serial + 4 byte data
        }

        while(tud_vendor_write_available() < buf_tmp_len); // wait for FIFO flushing
        tud_vendor_write(buf_tmp, buf_tmp_len);
    }
}

void USB_Task(void *params)
{
     // init device stack on configured roothub port
    tusb_rhport_init_t dev_init = {
        .role = TUSB_ROLE_DEVICE,
        .speed = TUSB_SPEED_AUTO
    };
    tusb_init(BOARD_TUD_RHPORT, &dev_init);

    while(1)
    {
        tud_task();
    }
    vTaskDelete(NULL);
}

int main()
{
    stdio_init_all();
    printf("fuck WCH\n");
    timer_hw->dbgpause = 0;
    rvswd_handle_t wch_handle_type0 = {.swclk = 16, .swdio = 15};

    gpio_init(PULL_HELPER_PIN);
    gpio_set_function(PULL_HELPER_PIN, GPIO_FUNC_SIO);
    gpio_set_dir(PULL_HELPER_PIN, GPIO_OUT);
    gpio_put(PULL_HELPER_PIN, 1);

    gpio_init(LOGIC_ANALYZER_HELPER_PIN);
    gpio_set_function(LOGIC_ANALYZER_HELPER_PIN, GPIO_FUNC_SIO);
    gpio_set_dir(LOGIC_ANALYZER_HELPER_PIN, GPIO_OUT);
    gpio_put(LOGIC_ANALYZER_HELPER_PIN, 0);

    printf("PIO mode test\n");
    wch_handle_pio.swclk = 16;
    wch_handle_pio.swdio = 15;
    wch_handle_pio.logic_helper_pin = LOGIC_ANALYZER_HELPER_PIN;
    rvswd_pio_init(&wch_handle_pio);
    rvswd_pio_reset(&wch_handle_pio);

    rvswd_pio_write(&wch_handle_pio, CH32_REG_DEBUG_DMCONTROL, 0x80000001);  // Make the debug module work properly
    rvswd_pio_write(&wch_handle_pio, CH32_REG_DEBUG_DMCONTROL, 0x80000001);  // Initiate a halt request
    rvswd_pio_write(&wch_handle_pio, CH32_REG_DEBUG_DMCONTROL, 0x00000001);  // Clear the halt request
    rvswd_pio_write(&wch_handle_pio, CH32_REG_DEBUG_DMCONTROL, 0x00000003);  // Initiate a core reset request

    #if defined(portSUPPORT_SMP) && (portSUPPORT_SMP == 1)
    #warning "FreeRTOS is configured for SMP mode."
#else
    #error "FreeRTOS is NOT configured for SMP mode."
#endif

    TaskHandle_t xHandleRVSWD = NULL;
    

    xTaskCreate( RVSWD_Task, "RVSWD_Task", 1024, NULL, 1, &xHandleRVSWD );
    vTaskCoreAffinitySet( xHandleRVSWD, ( 1U << 0 ) );  // Affinity mask for core 0

    xTaskCreate(USB_Task, "USB_Task", 4096, NULL, 1, NULL);

    cmd_queue = xQueueCreate(256, sizeof(rvswd_op_t));
    result_queue = xQueueCreate(256, sizeof(rvswd_op_result_t));

    vTaskStartScheduler();

    while(1);
}
