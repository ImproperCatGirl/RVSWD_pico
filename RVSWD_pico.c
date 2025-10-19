#include <hardware/gpio.h>
#include <hardware/structs/io_bank0.h>
#include <hardware/timer.h>
#include <pico/time.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "time.h"

#include <pico/time.h>
#include "blink.pio.h"
#include "rvswd.h"


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


#define PULL_HELPER_PIN 14

#define LOGIC_ANALYZER_HELPER_PIN 10

void blink_pin_forever(PIO pio, uint sm, uint offset, uint pin, uint freq) {
    blink_program_init(pio, sm, offset, pin);
    pio_sm_set_enabled(pio, sm, true);

    printf("Blinking pin %d at %d Hz\n", pin, freq);

    // PIO counter program takes 3 more cycles in total than we pass as
    // input (wait for n + 1; mov; jmp)
    pio->txf[sm] = (125000000 / (2 * freq)) - 3;
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


    uint32_t data0_val = 0;
    rvswd_init(&wch_handle_type0);
    wch_handle_type0.logic_helper_pin = LOGIC_ANALYZER_HELPER_PIN;
    rvswd_reset(&wch_handle_type0);

    rvswd_write(&wch_handle_type0, CH32_REG_DEBUG_DMCONTROL, 0x80000001);  // Make the debug module work properly
    rvswd_write(&wch_handle_type0, CH32_REG_DEBUG_DMCONTROL, 0x80000001);  // Initiate a halt request
    rvswd_write(&wch_handle_type0, CH32_REG_DEBUG_DMCONTROL, 0x00000001);  // Clear the halt request
    rvswd_write(&wch_handle_type0, CH32_REG_DEBUG_DMCONTROL, 0x00000003);  // Initiate a core reset request

    uint8_t timeout = 50;
    while (1) {
        uint32_t value;
        rvswd_read(&wch_handle_type0, CH32_REG_DEBUG_DMSTATUS, &value);
        printf("DMSTATUS = %08X\n", value);
        if (((value >> 18) & 0b11) == 0b11) {  // Check that processor has been reset
            printf("SUCCESS!\n");
            break;
        }
        if (timeout == 0) {
            printf("Failed to reset microprocessor");
            return RVSWD_FAIL;
        }
        timeout--;
        sleep_ms(10);
        //vTaskDelay(pdMS_TO_TICKS(10));
    }
    sleep_ms(100);
    for(int i = 0; i < 2; i++)
    {
        uint32_t value = 0xAA;
        rvswd_read(&wch_handle_type0, CH32_REG_DEBUG_DMSTATUS, &value);
        printf("DMSTATUS = %04X\n", value);

        rvswd_read(&wch_handle_type0, CH32_REG_DEBUG_HARTINFO, &value);
        printf("HARTINFO = %04X\n", value);

        rvswd_read(&wch_handle_type0, CH32_REG_DEBUG_DATA0, &value);
        printf("DATA0 = %04X\n", value);

        sleep_ms(10);
        rvswd_write(&wch_handle_type0, CH32_REG_DEBUG_DATA0, data0_val);

        data0_val++;
        sleep_ms(500);
    }
    printf("PIO mode test\n");
    rvswd_handle_t wch_handle_pio = {.swclk = 16, .swdio = 15 , .logic_helper_pin = LOGIC_ANALYZER_HELPER_PIN};
    rvswd_pio_init(&wch_handle_pio);
    /*rvswd_pio_reset(&wch_handle_pio);

    rvswd_pio_write(&wch_handle_pio, CH32_REG_DEBUG_DMCONTROL, 0x80000001);  // Make the debug module work properly
    rvswd_pio_write(&wch_handle_pio, CH32_REG_DEBUG_DMCONTROL, 0x80000001);  // Initiate a halt request
    rvswd_pio_write(&wch_handle_pio, CH32_REG_DEBUG_DMCONTROL, 0x00000001);  // Clear the halt request
    rvswd_pio_write(&wch_handle_pio, CH32_REG_DEBUG_DMCONTROL, 0x00000003);  // Initiate a core reset request*/
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

        sleep_ms(10);
        rvswd_pio_write(&wch_handle_pio, CH32_REG_DEBUG_DATA0, data0_val);

        data0_val++;

        sleep_ms(1000);
    }
    // PIO Blinking example
    /*PIO pio = pio0;
    uint offset = pio_add_program(pio, &blink_program);
    printf("Loaded program at %d\n", offset);
    
    #ifdef PICO_DEFAULT_LED_PIN
    blink_pin_forever(pio, 0, offset, PICO_DEFAULT_LED_PIN, 3);
    #else
    blink_pin_forever(pio, 0, offset, 6, 3);
    #endif
    // For more pio examples see https://github.com/raspberrypi/pico-examples/tree/master/pio

    while (true) {
        printf("Hello, world!\n");
        sleep_ms(1000);
    }*/
}
