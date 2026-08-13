/*
 * Copyright 2024 Contemporary Software
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 */

/*
 * Xilinx Zynq Handoff.
 */

#include <stdio.h>

#include <board-handoff.h>
#include <cache.h>

#include <driver/slcr/board-slcr.h>
#include <driver/wdog/wdog.h>


#define ZYNQ_ARM_SWITCH_REGISTERS         uint32_t arm_switch_reg
#define ZYNQ_ARM_SWITCH_TO_ARM            ".align 2\nbx pc\n.arm\n"
#define ZYNQ_ARM_SWITCH_BACK              "add %[arm_switch_reg], pc, #1\nbx %[arm_switch_reg]\n.thumb\n"
#define ZYNQ_ARM_SWITCH_OUTPUT            [arm_switch_reg] "=&r" (arm_switch_reg)
#define ZYNQ_ARM_SWITCH_ADDITIONAL_OUTPUT ZYNQ_ARM_SWITCH_OUTPUT

#undef ASM_ARM_MODE
#define ASM_ARM_MODE ".align 2\nbx pc\n.arm\n"

static inline void
zynq_clear_caches(void)
{
  ZYNQ_ARM_SWITCH_REGISTERS;
  uint32_t sbz = 0;
  asm volatile(ZYNQ_ARM_SWITCH_TO_ARM
               "mcr p15, 0, %[sbz], cr7, cr5, 0\n"  /* Invalidate Instruction cache */
               "mcr p15, 0, %[sbz], cr7, cr5, 6\n"  /* Invalidate branch predictor array */
               "dsb\n"
               "isb\n"
               ZYNQ_ARM_SWITCH_BACK
               : ZYNQ_ARM_SWITCH_ADDITIONAL_OUTPUT
               : [sbz] "r" (sbz)
               : "memory");
}

static inline void
zynq_reset_mmu(void)
{
  ZYNQ_ARM_SWITCH_REGISTERS;
  uint32_t sbz = 0;
  asm volatile(ZYNQ_ARM_SWITCH_TO_ARM
               "mcr	p15, 0, %[sbz], cr1, cr0, 0\n" /* disable the ICache and MMU */
               "isb\n"                             /* make sure it completes */
               ZYNQ_ARM_SWITCH_BACK
               : ZYNQ_ARM_SWITCH_ADDITIONAL_OUTPUT
               : [sbz] "r" (sbz)
               : "memory");
}

static inline void
zynq_dispatch(uint32_t address)
{
  ZYNQ_ARM_SWITCH_REGISTERS;
  asm volatile(ZYNQ_ARM_SWITCH_TO_ARM
               "mov lr, %[address]\n"
               "bx  lr\n"
               ZYNQ_ARM_SWITCH_BACK
               : ZYNQ_ARM_SWITCH_ADDITIONAL_OUTPUT
               : [address] "r" (address)
               : "lr", "memory");
}

void
board_handoff_exit(uint32_t address)
{
  printf("Flare handing off to 0x%08x\n", address);
  board_handoff_disable();
  board_slcr_lock();
  zynq_clear_caches();
  cache_disable();
  zynq_reset_mmu();
  zynq_dispatch(address);
  wdog_control(true);
  while (true);
}

void
board_handoff_exit_no_mmu_reset(uint32_t address)
{
  printf("Flare handing off to 0x%08x\n", address);
  board_handoff_disable();
  board_slcr_lock();
  zynq_clear_caches();
  cache_disable();
  zynq_dispatch(address);
  wdog_control(true);
  while (true);
}

void
board_handoff_jtag_exit(void)
{
  board_handoff_disable();
  board_slcr_lock();
  zynq_clear_caches();
  cache_disable();
  zynq_reset_mmu();
  while (true)
    asm volatile("wfe");
}
