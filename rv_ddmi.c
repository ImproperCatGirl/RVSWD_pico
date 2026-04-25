
#include <hardware/timer.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "class/vendor/vendor_device.h"
#include "portmacro.h"
#include "projdefs.h"
#include "rvswd.h"
#include "tusb.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "misc.h"

enum  {
    BLINK_NOT_MOUNTED = 250,
    BLINK_MOUNTED = 1000,
    BLINK_SUSPENDED = 2500,
  };
  
  static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;
  
 
 
 
 
 #define RX_BUF_SIZE 1024
 #define TX_BUF_SIZE 1024
 static uint8_t rx_buf[RX_BUF_SIZE];
 static uint8_t tx_buf[TX_BUF_SIZE];
 
 static size_t rx_len = 0;
 
  //--------------------------------------------------------------------+
  // Device callbacks
  //--------------------------------------------------------------------+
  
  // Invoked when device is mounted
  void tud_mount_cb(void)
  {
    blink_interval_ms = BLINK_MOUNTED;
  }
  
  // Invoked when device is unmounted
  void tud_umount_cb(void)
  {
    blink_interval_ms = BLINK_NOT_MOUNTED;
  }
  
  // Invoked when usb bus is suspended
  // remote_wakeup_en : if host allow us  to perform remote wakeup
  // Within 7ms, device must draw an average of current less than 2.5 mA from bus
  void tud_suspend_cb(bool remote_wakeup_en)
  {
    (void) remote_wakeup_en;
    //blink_interval_ms = BLINK_SUSPENDED;
  }
  
  // Invoked when usb bus is resumed
  void tud_resume_cb(void)
  {
    blink_interval_ms = tud_mounted() ? BLINK_MOUNTED : BLINK_NOT_MOUNTED;
  }
  
 
 extern TaskHandle_t xHandleDDMI;
 
 void tud_vendor_rx_cb(uint8_t itf, uint8_t const* buffer, uint16_t bufsize)
  {
   xTaskNotifyGive(xHandleDDMI);
   taskYIELD();
 }
 
extern rvswd_handle_t wch_handle_pio;
void ddmi_process_with_chain();

void ddmi_worker_task(void *pvParameters) 
{
    while (1) 
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        ddmi_process_with_chain();
    }
}

// Internal tracking for the reset state machine
static enum {
    IDLE,
    RESET_SEQUENCE_TRIGGERED,
    WAITING_FOR_OPENOCD_ACK
} reset_state = IDLE;

// RISC-V Debug Module Register Addresses
#define RISCV_REG_DMCONTROL    0x10
#define RISCV_REG_DMSTATUS     0x11
#define RISCV_REG_ABSTRACTAUTO 0x18

// DMCONTROL Bit Masks
#define DM_DMACTIVE            (1 << 0)
#define DM_NDMRESET            (1 << 1)
#define DM_ACKHAVERESET        (1 << 28)
#define DM_HALTREQ             (1 << 31)
#define DM_HARTSEL_MASK        0x03FF0000

// DMSTATUS Bit Masks
#define DS_ALLHALTED           (1 << 9)
#define DS_ANYHALTED           (1 << 8)
#define DS_ALLHAVERESET        (1 << 19)
#define DS_ANYHAVERESET        (1 << 18)
// State tracking for the Reset Lifecycle
static bool in_reset_polling_phase = false;
static bool in_reset_halt = false;

static uint32_t pool_idx = 0;
static uint32_t response_pool[100];
static uint8_t  total_responses_queued = 0;
#define USB_FS_MPS 64
#define DDMI_ITF 0
#define RESP_SIZE 4
void ddmi_process_with_chain() {
    uint8_t pkt[64];
    uint32_t len = tud_vendor_n_read(DDMI_ITF, pkt, 64);
    if (len < 2) return;

    uint8_t countdown = pkt[0];
    uint8_t op_count  = pkt[1];
    uint8_t *payload  = pkt + 2;

    if(op_count == 255 && pkt[2] == 'R')
    {
        rvswd_pio_reset(&wch_handle_pio);

        rvswd_pio_write(&wch_handle_pio, RISCV_REG_DMCONTROL, 0x80000001);  // Make the debug module work properly
        rvswd_pio_write(&wch_handle_pio, RISCV_REG_DMCONTROL, 0x80000001);  // Initiate a halt request
        rvswd_pio_write(&wch_handle_pio, RISCV_REG_DMCONTROL, 0x00000001);  // Clear the halt request
        rvswd_pio_write(&wch_handle_pio, RISCV_REG_DMCONTROL, 0x00000003);  // Initiate a core reset request
        ch32v20x_halt_microprocessor(&wch_handle_pio);
        ch32v20x_resume_microprocessor(&wch_handle_pio);
        return;
    }   

    for (uint8_t i = 0; i < op_count; i++) {
        uint8_t *cmd = payload + (i * 9);
        
        uint32_t addr = cmd[1] | (cmd[2] << 8) | (cmd[3] << 16) | (cmd[4] << 24);
        uint32_t data = cmd[5] | (cmd[6] << 8) | (cmd[7] << 16) | (cmd[8] << 24);
    
        if (cmd[0] == 'r') {
            uint32_t val;
            int stat = rvswd_pio_read(&wch_handle_pio, addr, &val);
            if (addr == RISCV_REG_DMSTATUS) {
                if (in_reset_polling_phase) {
                    /* * OpenOCD is in the while(1) loop in deassert_reset.
                     * We must force ALLHAVERESET so it proceeds to ACK.
                     * We also force HALTED bits because OpenOCD expects 
                     * the hart to be halted post-reset (reset_halt=1).
                     */
                     printf("OpenOCD is in the while(1) loop in deassert_reset.\n");
                    val |= (DS_ALLHAVERESET | DS_ANYHAVERESET |
                            DS_ALLHALTED    | DS_ANYHALTED);

                }
            }
            // Abstractauto masking (keep this for stability)
            if (addr == RISCV_REG_ABSTRACTAUTO) val = 0;
            response_pool[total_responses_queued++] = val;
            //printf("%08X read result = %08X, stat = %d\n", addr, val, stat);
    
        } else 
        { // WRITE
            // --- WORKAROUND: DMCONTROL (0x10) Hartsel Masking ---
            if (addr == RISCV_REG_DMCONTROL) {
                // OpenOCD tries to probe 1024 harts because WCH implements 
                // all hartsel bits. Force all hartsel bits to 0 (Hart 0).
                data &= ~DM_HARTSEL_MASK; 
            }

            if(addr == RISCV_REG_DMCONTROL)
            {
                if (data & DM_NDMRESET) {
                    printf("RESETTING!\n");
                    in_reset_polling_phase = true;
                    if(data & DM_HALTREQ)
                    {
                        printf("Halt pending!\n");
                        in_reset_halt = true;
                    }
                }
                // Detect "ACKHAVERESET" (The end of deassert_reset)
                if (data & DM_ACKHAVERESET) {
                    printf("ACK-ing\n");
                    in_reset_polling_phase = false;
                    in_reset_halt = false;
                }
            }
            int stat = rvswd_pio_write(&wch_handle_pio, addr, data);
            if(in_reset_halt)
            {
                rvswd_write(&wch_handle_pio, RISCV_REG_DMCONTROL, DM_HALTREQ | DM_DMACTIVE);  // Initiate a halt request
                vTaskDelay(pdMS_TO_TICKS(10)); // For some reaosn the halting of the chip is really slow. 
            }
            
            //printf("%08X write %08X stat = %d\n", addr, data, stat);
            response_pool[total_responses_queued++] = 0x00000000;
        }
    }

    // --- PHASE 2: BURST RESPONSES ---
    // Only when the chain is complete do we saturate the USB IN pipe
    if (countdown == 0) {
        uint8_t *resp_ptr = (uint8_t *)response_pool;
        uint32_t bytes_remaining = total_responses_queued * RESP_SIZE;

        while (bytes_remaining > 0) {
            // Check how much space is in the TinyUSB TX FIFO
            uint32_t fifo_space = tud_vendor_n_write_available(DDMI_ITF);
            
            if (fifo_space >= USB_FS_MPS || fifo_space >= bytes_remaining) {
                uint32_t chunk = (bytes_remaining >= USB_FS_MPS) ? USB_FS_MPS : bytes_remaining;
                
                tud_vendor_n_write(DDMI_ITF,resp_ptr, chunk);
                
                // If we just sent a full 64-byte block, flush it immediately 
                // to ensure the Host sees a "Full Packet"
                if (chunk == USB_FS_MPS) {
                    tud_vendor_n_write_flush(DDMI_ITF);
                }
                
                resp_ptr += chunk;
                bytes_remaining -= chunk;
            } else {
                // FIFO full, give the USB stack a moment to breathe
                tud_vendor_n_write_flush(DDMI_ITF);
                break; 
            }
        }
        
        // Final flush for the "tail" fragment (if total length % 64 != 0)
        tud_vendor_n_write_flush(DDMI_ITF);
        total_responses_queued = 0;
    }
}