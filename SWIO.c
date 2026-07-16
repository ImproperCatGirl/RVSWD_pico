
#include "SWIO.pio.h"

#include <hardware/gpio.h>
#include <hardware/pio.h>
#include <hardware/structs/pio.h>
#include <hardware/timer.h>
#include <stdio.h>

#include "board_config.h"

//#define DUMP_COMMANDS

__attribute__((noinline)) void busy_wait(int count) {
  volatile int c = count;
  while (c) c = c - 1;
}

// WCH-specific debug interface config registers
static const int WCH_DM_CPBR     = 0x7C;
static const int WCH_DM_CFGR     = 0x7D;
static const int WCH_DM_SHDWCFGR = 0x7E;
static const int WCH_DM_PART     = 0x7F; // not in doc but appears to be part info


//------------------------------------------------------------------------------
uint sm  = 0;

uint32_t get_data(uint32_t addr) {
    pio_sm_put_blocking(pio1, sm, ((~addr) << 1) | 1);
    uint32_t data = pio_sm_get_blocking(pio1, 0);
    //printf("get_dbg %08x 0x%08x\n", addr, data);
    return data;
  }
  
  //------------------------------------------------------------------------------
  
  void put_data(uint32_t addr, uint32_t data) {
  #ifdef DUMP_COMMANDS
    printf("set_dbg %15s 0x%08x\n", addr_to_regname(addr), data);
  #endif
    pio_sm_put_blocking(pio1, sm , ((~addr) << 1) | 0);
    pio_sm_put_blocking(pio1, sm, ~data);
  }
void SWIO_reset(int pin) {
    pin = SWIO_DATA_PIN;
  // Configure GPIO
  gpio_set_drive_strength(pin, GPIO_DRIVE_STRENGTH_2MA);
  gpio_set_slew_rate     (pin, GPIO_SLEW_RATE_SLOW);
  gpio_set_function      (pin, GPIO_FUNC_PIO1);


  gpio_init(SWIO_PULL_PIN);
  gpio_set_function(SWIO_PULL_PIN, GPIO_FUNC_SIO);
  gpio_set_dir(SWIO_PULL_PIN, GPIO_OUT);

  gpio_set_drive_strength(SWIO_PULL_PIN, GPIO_DRIVE_STRENGTH_12MA);
  gpio_set_slew_rate     (SWIO_PULL_PIN, GPIO_SLEW_RATE_FAST);
  gpio_put(SWIO_PULL_PIN, 1);

  // Reset PIO module
  pio1->ctrl = 0b000100010001;
  sm = pio_claim_unused_sm(pio1, true);

  pio_sm_set_enabled(pio1, sm, false);

  // Upload PIO program
  pio_clear_instruction_memory(pio1);
  uint pio_offset = pio_add_program(pio1, &singlewire_program);

  // Configure PIO module
  pio_sm_config c = pio_get_default_sm_config();
  sm_config_set_wrap        (&c, pio_offset + singlewire_wrap_target, pio_offset + singlewire_wrap);
  sm_config_set_sideset     (&c, 1, /*optional*/ false, /*pindirs*/ true);
  sm_config_set_out_pins    (&c, pin, 1);
  sm_config_set_in_pins     (&c, pin);
  sm_config_set_set_pins    (&c, pin, 1);
  sm_config_set_sideset_pins(&c, pin);
  sm_config_set_out_shift   (&c, /*shift_right*/ false, /*autopull*/ false, /*pull_threshold*/ 32);
  sm_config_set_in_shift    (&c, /*shift_right*/ false, /*autopush*/ true,  /*push_threshold*/ 32);

  // 125 mhz / 12 = 96 nanoseconds per tick, close enough to 100 ns.
  sm_config_set_clkdiv      (&c, 12);

  pio_sm_init       (pio1, sm, pio_offset, &c);
  pio_sm_set_pins   (pio1, sm, 0);
  pio_sm_set_enabled(pio1, sm, true);

  // Grab pin and send an 8 usec low pulse to reset debug module
  // If we use the sdk functions to do this we get jitter :/
  sio_hw->gpio_clr    = (1 << pin);
  sio_hw->gpio_oe_set = (1 << pin);
  io_bank0_hw->io[pin].ctrl = GPIO_FUNC_SIO << IO_BANK0_GPIO15_CTRL_FUNCSEL_LSB;
  busy_wait(100); // ~8 usec
  sio_hw->gpio_oe_clr = (1 << pin);
  io_bank0_hw->io[pin].ctrl = GPIO_FUNC_PIO1 << IO_BANK0_GPIO15_CTRL_FUNCSEL_LSB;

  // Enable debug output pin on target
  put_data(WCH_DM_SHDWCFGR, 0x5AA50400);
  put_data(WCH_DM_CFGR,     0x5AA50400);

  // Reset debug module on target
  put_data(0x10, 0x00000000);
  put_data(0x10, 0x00000001);
}

void SWIO_re_open()
{
    // Enable debug output pin on target
    put_data(WCH_DM_SHDWCFGR, 0x5AA50400);
    put_data(WCH_DM_CFGR,     0x5AA50400);
  
    // Reset debug module on target
    put_data(0x10, 0x00000000);
    put_data(0x10, 0x00000001);

}
