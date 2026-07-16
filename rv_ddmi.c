
#include <hardware/timer.h>
#include <stdbool.h>
#include <stdio.h>

#include "rvswd.h"
#include "tusb.h"

#include "FreeRTOS.h"
#include "task.h"

#include "debug_log.h"
#include "misc.h"


#include "SWIO.h"
  
 
 protocol_state state;
 
  //--------------------------------------------------------------------+
  // Device callbacks
  //--------------------------------------------------------------------+
  
  // Invoked when device is mounted
  void tud_mount_cb(void)
  {
  }
  
  // Invoked when device is unmounted
  void tud_umount_cb(void)
  {
  }
  
  // Invoked when usb bus is suspended
  // remote_wakeup_en : if host allow us  to perform remote wakeup
  // Within 7ms, device must draw an average of current less than 2.5 mA from bus
  void tud_suspend_cb(bool remote_wakeup_en)
  {
    (void) remote_wakeup_en;
  }
  
  // Invoked when usb bus is resumed
  void tud_resume_cb(void)
  {
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

// RISC-V Debug Module Register Addresses
#define RISCV_REG_DMCONTROL    0x10
#define RISCV_REG_DMSTATUS     0x11

// DMCONTROL Bit Masks
#define DM_NDMRESET            (1 << 1)
#define DM_ACKHAVERESET        (1 << 28)
#define DM_HALTREQ             (1 << 31)

// DMSTATUS Bit Masks
#define DS_ALLHALTED           (1 << 9)
#define DS_ANYHALTED           (1 << 8)
#define DS_ALLHAVERESET        (1 << 19)
#define DS_ANYHAVERESET        (1 << 18)
// State tracking for the Reset Lifecycle
static bool in_reset_polling_phase = false;
static bool in_reset_halt = false;

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
        if(state == TYPE_RVSWD)
        {
            rvswd_pio_reset(&wch_handle_pio);
            ch32v20x_halt_microprocessor(&wch_handle_pio);
            ch32v20x_resume_microprocessor(&wch_handle_pio);
        }
        if(state == TYPE_SWIO)
        {
            SWIO_re_open();
        }
        return;
    }   

    for (uint8_t i = 0; i < op_count; i++) {
        uint8_t *cmd = payload + (i * 9);
        
        uint32_t addr = cmd[1] | (cmd[2] << 8) | (cmd[3] << 16) | (cmd[4] << 24);
        uint32_t data = cmd[5] | (cmd[6] << 8) | (cmd[7] << 16) | (cmd[8] << 24);
    
        if (cmd[0] == 'r') {
            uint32_t val;
            if(state == TYPE_RVSWD)
            {
                int stat = rvswd_pio_read(&wch_handle_pio, addr, &val);
                int timeout = 5;
                while(stat != RVSWD_OK && timeout-- > 0)
                {
                    stat = rvswd_pio_read(&wch_handle_pio, addr, &val);
                }
            }
            if(state == TYPE_SWIO)
            {
                val = get_data(addr);
            }
            
            response_pool[total_responses_queued++] = val;
    
        } else 
        { // WRITE
            if(state == TYPE_RVSWD)
            {
                int stat = rvswd_pio_write(&wch_handle_pio, addr, data);
                int timeout = 5;
                while(stat != RVSWD_OK && timeout-- > 0)
                {
                    stat = rvswd_pio_write(&wch_handle_pio, addr, data);
                }
            }
            if(state == TYPE_SWIO)
            {
                put_data(addr,data);
            }
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
