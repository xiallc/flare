/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @file
 *
 * @ingroup RTEMSBSPsAArch64XilinxZynqMP
 *
 * @brief This source file contains the default MMU tables and setup.
 */

/*
 * Copyright (C) 2021 On-Line Applications Research Corporation (OAR)
 * Written by Kinsey Moore <kinsey.moore@oarcorp.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdint.h>

#include <cache.h>

#include <arch/aarch64/mmu/aarch64-mmu.h>
#include <arch/aarch64/mmu/aarch64-mmu-vmsav8-64.h>

void zynqmp_setup_el3_mmu_and_cache(void);
void zynqmp_setup_el2_mmu_and_cache(void);
void zynqmp_disable_el3_mmu_and_cache(void);
void zynqmp_disable_el2_mmu_and_cache(void);

static const aarch64_mmu_config_entry zynqmp_mmu_config_table[] = {
    {/* RAM Region 0 */
     .begin = 0x00000000U,
     .end = 0x80000000U,
     .flags = AARCH64_MMU_DATA_RW_CACHED},
    {/* APU GIC */
     .begin = 0xf9000000U,
     .end = 0xf9100000U,
     .flags = AARCH64_MMU_DEVICE},
    {/* FPD and LPD Slaves */
     .begin = 0xfd000000U,
     .end = 0xffc00000U,
     .flags = AARCH64_MMU_DEVICE},
    {/* PMU */
     .begin = 0xffd00000U,
     .end = 0xfffc0000U,
     .flags = AARCH64_MMU_DEVICE},
    {/* OCM */
     .begin = 0xfffc0000U,
     .end = 0x100000000U,
     .flags = AARCH64_MMU_DATA_RW_CACHED}};

void aarch64_setup_el3_mmu_and_cache(void) {
  zynqmp_setup_el3_mmu_and_cache();
}

void aarch64_setup_el2_mmu_and_cache(void) {
  zynqmp_setup_el2_mmu_and_cache();
}

void aarch64_disable_el3_mmu_and_cache(void) {
  zynqmp_disable_el3_mmu_and_cache();
}

void aarch64_disable_el2_mmu_and_cache(void) {
  zynqmp_disable_el2_mmu_and_cache();
}

void zynqmp_setup_el3_mmu_and_cache(void) {
  aarch64_el3_mmu_setup();

  aarch64_el3_mmu_setup_translation_table(
      &zynqmp_mmu_config_table[0], RTEMS_ARRAY_SIZE(zynqmp_mmu_config_table));

  aarch64_el3_mmu_enable();
}

void zynqmp_setup_el2_mmu_and_cache(void) {
  aarch64_el2_mmu_setup();

  aarch64_el2_mmu_setup_translation_table(
      &zynqmp_mmu_config_table[0], RTEMS_ARRAY_SIZE(zynqmp_mmu_config_table));

  aarch64_el2_mmu_enable();
}

void zynqmp_disable_el3_mmu_and_cache(void) {
  cache_flush_invalidate();
  cache_disable();
  aarch64_el3_mmu_disable();
}

void zynqmp_disable_el2_mmu_and_cache(void) {
  cache_flush_invalidate();
  cache_disable();
  aarch64_el2_mmu_disable();
}
