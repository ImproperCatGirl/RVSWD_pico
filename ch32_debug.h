#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "rvswd.h"

#define CH32_REG_DEBUG_DATA0        0x04
#define CH32_REG_DEBUG_DATA1        0x05
#define CH32_REG_DEBUG_DMCONTROL    0x10
#define CH32_REG_DEBUG_DMSTATUS     0x11
#define CH32_REG_DEBUG_HARTINFO     0x12
#define CH32_REG_DEBUG_ABSTRACTCS   0x16
#define CH32_REG_DEBUG_COMMAND      0x17
#define CH32_REG_DEBUG_ABSTRACTAUTO 0x18
#define CH32_REG_DEBUG_PROGBUF0     0x20
#define CH32_REG_DEBUG_PROGBUF1     0x21
#define CH32_REG_DEBUG_PROGBUF2     0x22
#define CH32_REG_DEBUG_PROGBUF3     0x23
#define CH32_REG_DEBUG_PROGBUF4     0x24
#define CH32_REG_DEBUG_PROGBUF5     0x25
#define CH32_REG_DEBUG_PROGBUF6     0x26
#define CH32_REG_DEBUG_PROGBUF7     0x27
#define CH32_REG_DEBUG_HALTSUM0     0x40
#define CH32_REG_DEBUG_CPBR         0x7C
#define CH32_REG_DEBUG_CFGR         0x7D
#define CH32_REG_DEBUG_SHDWCFGR     0x7E

#define CH32_REGS_CSR 0x0000
#define CH32_REGS_GPR 0x1000

#define CH32_CFGR_KEY   0x5aa50000
#define CH32_CFGR_OUTEN (1 << 10)

#define CH32_CODE_BEGIN 0x08000000
#define CH32_CODE_END   0x08004000

#define CH32V20X_FLASH_STATR 0x4002200C
#define CH32V20X_FLASH_CTLR  0x40022010
#define CH32_FLASH_ADDR      0x40022014

rvswd_result_t ch32v20x_halt_microprocessor(rvswd_handle_t *handle);
rvswd_result_t ch32v20x_resume_microprocessor(rvswd_handle_t *handle);
rvswd_result_t ch32v20x_reset_microprocessor_and_run(rvswd_handle_t *handle);

bool ch32v20x_write_cpu_reg(rvswd_handle_t *handle, uint16_t regno, uint32_t value);
bool ch32v20x_read_cpu_reg(rvswd_handle_t *handle, uint16_t regno, uint32_t *value_out);
