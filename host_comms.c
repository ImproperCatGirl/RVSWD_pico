/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

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
 
 /* This example demonstrate HID Generic raw Input & Output.
  * It will receive data from Host (In endpoint) and echo back (Out endpoint).
  * HID Report descriptor use vendor for usage page (using template TUD_HID_REPORT_DESC_GENERIC_INOUT)
  *
  * There are 2 ways to test the sketch
  * 1. Using nodejs
  * - Install nodejs and npm to your PC
  *
  * - Install excellent node-hid (https://github.com/node-hid/node-hid) by
  *   $ npm install node-hid
  *
  * - Run provided hid test script
  *   $ node hid_test.js
  *
  * 2. Using python
  * - Install `hid` package (https://pypi.org/project/hid/) by
  *   $ pip install hid
  *
  * - hid package replies on hidapi (https://github.com/libusb/hidapi) for backend,
  *   which already available in Linux. However on windows, you may need to download its dlls from their release page and
  *   copy it over to folder where python is installed.
  *
  * - Run provided hid test script to send and receive data to this device.
  *   $ python3 hid_test.py
  */
 
 //--------------------------------------------------------------------+
 // MACRO CONSTANT TYPEDEF PROTYPES
 //--------------------------------------------------------------------+
 
 /* Blink pattern
  * - 250 ms  : device not mounted
  * - 1000 ms : device mounted
  * - 2500 ms : device is suspended
  */
 enum  {
   BLINK_NOT_MOUNTED = 250,
   BLINK_MOUNTED = 1000,
   BLINK_SUSPENDED = 2500,
 };
 
 static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;
 
extern QueueHandle_t cmd_queue;

extern QueueHandle_t result_queue;

extern rvswd_handle_t wch_handle_pio;

extern TaskHandle_t xHandleUSBWoker;

uint8_t cmd_len;



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
 
 bool parse_next_command(const uint8_t **current_ptr, const uint8_t *end, rvswd_op_t *op) 
 {
    const uint8_t *current = *current_ptr;
    if (current + 1 > end) return false;  // Need at least opcode
    op->opcode = *current++;
    
    switch (op->opcode) 
    {
        case RVSWD_READ:
            if (current + 2 > end) return false;  // serial + addr
            op->serial = *current++;
            op->params.read.addr = *current++;
            break;
        case RVSWD_WRITE:
            if (current + 6 > end) return false;  // serial + addr + 4B data
            op->serial = *current++;
            op->params.write.addr = *current++;
            memcpy(&op->params.write.data_to_target, current, 4);
            current += 4;
            break;
        case RVSWD_RESET:
            if (current + 1 > end) return false;  // serial
            op->serial = *current++;
            break;
        default:
            printf("Unknown opcode %02X, dropping\n", op->opcode);
            return false;
    }
    
    *current_ptr = current;
    return true;
}

 // Invoked when a bulk OUT transfer is received from the host
void tud_vendor_rx_cb(uint8_t itf, uint8_t const* buffer, uint16_t bufsize)
{

    if(bufsize == 0)
    {
      printf("ZLP detected\n");
      return;
    }
    /*printf("vendor rx buffer:\n");
    for(int i = 0; i < bufsize; i ++)
    {
        printf("%02X ", buffer[i]);
        if(i % 16 == 0)
        {
            printf("\n");
        }
    }
    printf("\n");
*/
    if (rx_len + bufsize > RX_BUF_SIZE) {
      printf("RX buffer overflow!\n");
      return;
    }

    memcpy(rx_buf + rx_len, buffer, bufsize);
    rx_len += bufsize;

    size_t offset = 0;
    bool found_magic = false;
    while (offset + 5 <= rx_len) { // header (2) + cmd_len (1) + total_bytes (2)
        if (rx_buf[offset] != 0xE5 || rx_buf[offset + 1] != 0x8F) {
            // misaligned, skip until next header candidate
            offset++;
            continue;
        }
        found_magic = true;
        cmd_len = rx_buf[offset + 2];
        uint16_t total_bytes = rx_buf[offset + 3] | (rx_buf[offset + 4] << 8);
        //printf("total bytes = %d\n", total_bytes);

        if (offset + total_bytes > rx_len) {
            // Wait for full logical transfer
            break;
        }
        //printf("cmd_len = %d, total_bytes = %d\n", cmd_len, total_bytes);
        // Full transfer available
        const uint8_t *current = rx_buf + offset + 5;
        const uint8_t *end = rx_buf + offset + total_bytes;

        for (int i = 0; i < cmd_len; i++) {
            rvswd_op_t op = {0};
            if (!parse_next_command(&current, end, &op)) {
                printf("Parse error at command %d, dropping\n", i + 1);
                break;
            }
            xQueueSend(cmd_queue, &op, portMAX_DELAY);
        }

        offset += total_bytes; // advance past this logical transfer
    }
    if(found_magic)
    {
      //printf("magic found!, offset = %d\n", offset);
    }
    
    // Shift leftover bytes to the start of buffer
    if (offset > 0) {
        memmove(rx_buf, rx_buf + offset, rx_len - offset);
        rx_len -= offset;
    }

    tud_vendor_n_read_flush(itf);
}


void USB_worker(void *params)
{
    static uint16_t usb_buf_len = 0;

    rvswd_op_result_t res = {0};
    while (1)
    {
        // Only flush the buffer if notified that commands completed
        if(ulTaskNotifyTake(pdTRUE, 0) || usb_buf_len >= TX_BUF_SIZE)
        {
            /*printf("flushing, data:\n");
            for(int i = 0; i < usb_buf_len; i++)
            {
              printf("%02X ", tx_buf[i]);
              if(i % 16 == 0)
              {
                printf("\n");
              }
            }
            printf("\n");*/
            if(usb_buf_len > 0)
            {
                int timeout = 0;
                while (tud_vendor_write_available() < usb_buf_len)
                {
                    if(timeout > 10)
                        printf("USB writing timed out!\n");
                    vTaskDelay(pdMS_TO_TICKS(1));
                    timeout++;
                }
                tud_vendor_write(tx_buf, usb_buf_len);
                usb_buf_len = 0; // reset after flush
            }
            tud_vendor_write_flush();
        }

        // Always read from the result queue and buffer
        xQueueReceive(result_queue, &res, portMAX_DELAY);

        uint8_t buf_tmp[10] = {0};
        uint8_t buf_tmp_len = 0;

        if(res.opcode == RVSWD_WRITE)
        {
            buf_tmp_len = 2;
            buf_tmp[0] = res.serial;
            buf_tmp[1] = res.status;
        }
        else if(res.opcode == RVSWD_READ)
        {
            buf_tmp_len = 6;
            buf_tmp[0] = res.serial;
            buf_tmp[1] = res.status;
            memcpy(buf_tmp + 2, &res.data_from_target, sizeof(res.data_from_target));
        }

        // Append to the USB buffer
        if(usb_buf_len + buf_tmp_len <= TX_BUF_SIZE)
        {
            memcpy(tx_buf + usb_buf_len, buf_tmp, buf_tmp_len);
            usb_buf_len += buf_tmp_len;
        }
        else
        {
            printf("USB buffer overflow! buffer is %d byte long!\n", usb_buf_len + buf_tmp_len);
            usb_buf_len = 0;
        }
    }
}
