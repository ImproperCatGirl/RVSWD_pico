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

 #include "common/tusb_types.h"
#include "tusb.h"
 #include "usb_desc.h"
 
 //--------------------------------------------------------------------+
 // Device Descriptors
 //--------------------------------------------------------------------+
 tusb_desc_device_t const desc_device_ =
 {
     .bLength            = sizeof(tusb_desc_device_t),
     .bDescriptorType    = TUSB_DESC_DEVICE,
     .bcdUSB             = 0x0200, // USB 2.0
     .bDeviceClass       = TUSB_CLASS_VENDOR_SPECIFIC,
     .bDeviceSubClass    = 0x00,
     .bDeviceProtocol    = 0x00,
     .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
 
     .idVendor           = 0xCAFE, // Custom VID
     .idProduct          = 0x4002, // Custom PID
     .bcdDevice          = 0x0100,
 
     .iManufacturer      = 0x01,
     .iProduct           = 0x02,
     .iSerialNumber      = 0x03,
 
     .bNumConfigurations = 0x01
 };
 tusb_desc_device_t const desc_device = {
  .bLength            = sizeof(tusb_desc_device_t),
  .bDescriptorType    = TUSB_DESC_DEVICE,
  .bcdUSB             = 0x0200,         // USB 2.1 (Needed for BOS/WinUSB)
  .bDeviceClass       = 0xEF,           // Miscellaneous
  .bDeviceSubClass    = 0x02,           // Common Class
  //.bDeviceProtocol    = 0x01,           // Interface Association Descriptor
  .bDeviceProtocol    = MISC_PROTOCOL_IAD,
  .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
  
  .idVendor           = 0xcafe,         // Your VID
  .idProduct          = 0x4002,         // Your PID
  .bcdDevice          = 0x0100,
  
  .iManufacturer      = 0x01,
  .iProduct           = 0x02,           // String "CMSIS-DAP" is usually here
  .iSerialNumber      = 0x03,
  .bNumConfigurations = 0x01
};

 
 // Invoked when received GET DEVICE DESCRIPTOR
 // Application return pointer to descriptor
 uint8_t const * tud_descriptor_device_cb(void)
 {
   return (uint8_t const *) &desc_device;
 }
 
 //--------------------------------------------------------------------+
 // Configuration Descriptor
 //--------------------------------------------------------------------+
 enum
 {
   ITF_NUM_VENDOR,
   ITF_NUM_TOTAL
 };
 
 #define CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_VENDOR_DESC_LEN)
 
 uint8_t const desc_configuration[] =
 {
   // Config number, interface count, string index, total length, attribute, power in mA
   TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_SELF_POWERED, 100),
 
   // Interface number, string index, EP Out & IN address, EP size
   TUD_VENDOR_DESCRIPTOR(ITF_NUM_VENDOR, 0, EPNUM_OUT, EPNUM_IN, 64)
 };
 
 // Invoked when received GET CONFIGURATION DESCRIPTOR
 // Application return pointer to descriptor
 // Descriptor contents must exist long enough for transfer to complete
 uint8_t const * tud_descriptor_configuration_cb(uint8_t index)
 {
   (void) index; // for multiple configurations
   return desc_configuration;
 }
 
 //--------------------------------------------------------------------+
 // String Descriptors
 //--------------------------------------------------------------------+
 
 // array of pointer to string descriptors
 char const* string_desc_arr [] =
 {
   (char[]) { 0x09, 0x04 }, // 0: is supported language is English (0x0409)
   "TinyUSB",              // 1: Manufacturer
   "MCU Debug Probe",      // 2: Product
   "123456",               // 3: Serials, should use chip ID
 };
 
 static uint16_t _desc_str[32];
 
 // Invoked when received GET STRING DESCRIPTOR request
 // Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
 uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid)
 {
   (void) langid;
 
   uint8_t chr_count;
 
   if ( index == 0)
   {
     memcpy(&_desc_str[1], string_desc_arr[0], 2);
     chr_count = 1;
   }else
   {
     // Note: the 0xEE index string is a Microsoft OS 1.0 Descriptors.
     // https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/microsoft-defined-usb-descriptors
 
     if ( !(index < sizeof(string_desc_arr)/sizeof(string_desc_arr[0])) ) return NULL;
 
     const char* str = string_desc_arr[index];
 
     // Cap at max char
     chr_count = (uint8_t) strlen(str);
     if ( chr_count > 31 ) chr_count = 31;
 
     // Convert ASCII string to UTF-16
     for(uint8_t i=0; i<chr_count; i++)
     {
       _desc_str[1+i] = str[i];
     }
   }
 
   // first byte is length (in bytes), second byte is string type
   _desc_str[0] = (uint16_t) ((TUSB_DESC_STRING << 8 ) | (2*chr_count + 2));
 
   return _desc_str;
 }
 