/*
 * Copyright 2026 Contemporary Software
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
#include <stdint.h>
#include <stdio.h>

#include <cache.h>

#include "aarch64-atf.h"

#include <driver/io/board-io.h>
#include <driver/uart/console.h>

extern void aarch64_handoff(uint32_t address);
extern void aarch64_atf_return(uint32_t address);

static struct xil_atf_handoff_params atf_params;

static uint32_t get_params_address() {
  uint64_t params_addr_64 = (uint64_t)&atf_params;
  uint32_t params_addr_32 = (uint32_t)(params_addr_64 & 0xFFFFFFFF);

  if (params_addr_64 > 0xFFFFFFFF) {
    printf("error: ATF handoff params outside of 32bit memory: 0x%lx",
           params_addr_64);
  }

  return params_addr_32;
}

void aarch64_atf_handoff() {
  atf_params.magic_value[0] = XILINX_ATF_MAGIC_VALUE_0;
  atf_params.magic_value[1] = XILINX_ATF_MAGIC_VALUE_1;
  atf_params.magic_value[2] = XILINX_ATF_MAGIC_VALUE_2;
  atf_params.magic_value[3] = XILINX_ATF_MAGIC_VALUE_3;

  atf_params.num_entries = FLARE_ATF_PARAMS_NUM_ENTRIES;

  atf_params.entries[0].entry_point = (uint64_t)&aarch64_atf_return;
  atf_params.entries[0].flags = FLARE_ATF_ENTRY_FLAGS;

  board_reg_write(PMU_GLOBAL_GLOBAL_GEN_STORAGE6, get_params_address());

  printf("Handoff to ARM Trusted Firmware (BL31) at 0x%x\n\n",
         ATF_DEFAULT_ENTRY);

  console_flush();

  cache_flush();

  aarch64_handoff(ATF_DEFAULT_ENTRY);
}
