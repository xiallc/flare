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


.global _boot
.global __stack
.global __sbss_start
.global __sbss_end
.global __tbss_start
.global __tbss_end
.global __bss_start
.global __bss_end
.global __vector_table
.global _reset
.global aarch64_atf_return

/* Stack pointer and section locations for boot code */
.set system_stack,  __stack
.set sbss_start,    __sbss_start
.set sbss_end,      __sbss_end
.set tbss_start,    __tbss_start
.set tbss_end,      __tbss_end
.set bss_start,     __bss_start
.set bss_end,       __bss_end

.set vector_base,  __vector_table

.section .boot,"ax"
_boot:
  /* Set up the stack pointer */
  ldr  x0, =system_stack      /* system stack pointer */
  mov  sp, x0

  ldr x0, =vector_base
  msr VBAR_EL3, x0
  msr VBAR_EL2, x0
  msr VBAR_EL1, x0

  /* Initialise .bss, .tbss and .sbss */
  bl _xbss_init

  /* Disable MMU and cache */
  bl aarch64_disable_el3_mmu_and_cache

  /* Set up DDR and peripherals */
  bl psu_init

  /* Set up mmu and cache */
  bl aarch64_setup_el3_mmu_and_cache

  /* Print bootloader information */
  bl aarch64_print_flare_id

  /* Set PM configuration */
  bl pm_set_configuration

  /* Load BOOT.BIN partitions into memory */
  bl xpart_load_parts

  /* Hand off to ATF */
  b aarch64_atf_handoff

aarch64_atf_return:
  /* Set up the stack pointer */
  ldr  x0, =system_stack      /* system stack pointer */
  mov  sp, x0

  ldr x0, =vector_base
  msr VBAR_EL2, x0
  msr VBAR_EL1, x0

  /* Disable MMU and cache */
  bl aarch64_disable_el2_mmu_and_cache

  /* Set up mmu and cache */
  bl aarch64_setup_el2_mmu_and_cache

  /* Hand off to ATF */
  b main

/* Zero registers and jump to boot */
_reset:
  mov X0, XZR
  mov X1, XZR
  mov X2, XZR
  mov X3, XZR
  mov X4, XZR
  mov X5, XZR
  mov X6, XZR
  mov X7, XZR
  mov X8, XZR
  mov X9, XZR
  mov X10, XZR
  mov X11, XZR
  mov X12, XZR
  mov X13, XZR
  mov X14, XZR
  mov X15, XZR
  mov X16, XZR
  mov X17, XZR
  mov X18, XZR
  mov X19, XZR
  mov X20, XZR
  mov X21, XZR
  mov X22, XZR
  mov X23, XZR
  mov X24, XZR
  mov X25, XZR
  mov X26, XZR
  mov X27, XZR
  mov X28, XZR
  mov X29, XZR
  mov X30, XZR
  b _boot

_zero_loop:
  cmp   x0, x1
  b.ge  _zero_loop_ret
  stp   xzr, xzr, [x0], #16
  b     _zero_loop
_zero_loop_ret:
  ret

_xbss_init:
  mov   x3, x30
  ldr   x0, =sbss_start
  ldr   x1, =sbss_end
  bl    _zero_loop
  ldr   x0, =tbss_start
  ldr   x1, =tbss_end
  bl    _zero_loop
  ldr   x0, =bss_start
  ldr   x1, =bss_end
  bl    _zero_loop
  mov   x30, x3
  ret
