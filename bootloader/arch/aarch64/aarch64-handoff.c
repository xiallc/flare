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

#include <stdio.h>

#include <board-handoff.h>
#include <cache.h>

#include "mmu/aarch64-mmu.h"

#include <driver/wdog/wdog.h>

extern void aarch64_handoff(uint32_t address);

void board_handoff_exit(uint32_t address) {
  board_handoff_disable();
  cache_flush_invalidate();
  cache_disable();
  aarch64_el2_mmu_disable();
  aarch64_handoff(address);
  while (true)
    ;
}

void board_handoff_exit_no_mmu_reset(uint32_t address) {
  board_handoff_disable();
  cache_flush_invalidate();
  cache_disable();
  aarch64_handoff(address);
  while (true)
    ;
}

void board_handoff_jtag_exit(void) {
  board_handoff_disable();
  cache_flush_invalidate();
  cache_disable();
  while (true)
    asm volatile("wfe");
}
