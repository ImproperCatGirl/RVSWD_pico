#include <hardware/gpio.h>
#include <hardware/structs/io_bank0.h>
#include <hardware/timer.h>
#include <pico/time.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "projdefs.h"
#include "time.h"

#include <pico/time.h>
#include "blink.pio.h"
#include "rvswd.h"

#include "FreeRTOS.h"
#include "task.h"


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
        //rvswd_pio_reset(&wch_handle_pio);
        uint32_t value = 0xAA;
        rvswd_pio_read(&wch_handle_pio, CH32_REG_DEBUG_DMSTATUS, &value);
        printf("DMSTATUS = %04X\n", value);

        rvswd_pio_read(&wch_handle_pio, CH32_REG_DEBUG_HARTINFO, &value);
        printf("HARTINFO = %04X\n", value);

        rvswd_pio_read(&wch_handle_pio, CH32_REG_DEBUG_DATA0, &value);
        printf("DATA0 = %04X\n", value);

        //sleep_ms(10);
        rvswd_pio_write(&wch_handle_pio, CH32_REG_DEBUG_DATA0, data0_val);

        data0_val++;
        vTaskDelay(pdMS_TO_TICKS(500));
        //sleep_ms(500);
    }
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


    vTaskStartScheduler();

    while(1);
}
