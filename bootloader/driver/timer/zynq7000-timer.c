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
 * Zynq Timer.
 */

#include <stdio.h>

#include "board-timer.h"

#include <driver/io/board-io.h>

#define ZYNQ_TIMER                    (0xf8f00200)
#define ZYNQ_TMR_COUNTER_LOWER_OFFSET (0x00)
#define ZYNQ_TMR_COUNTER_UPPER_OFFSET (0x04)

#define CPU_CLK_FREQ_HZ (750000000UL)
#define TIMER_CLK_DIV   (CPU_CLK_FREQ_HZ / (2 * 1000000ULL))

void board_timer_get(uint64_t* time) {
  uint32_t u = board_reg_read(ZYNQ_TIMER + ZYNQ_TMR_COUNTER_UPPER_OFFSET);
  uint32_t l = board_reg_read(ZYNQ_TIMER + ZYNQ_TMR_COUNTER_LOWER_OFFSET);
  if (board_reg_read(ZYNQ_TIMER + ZYNQ_TMR_COUNTER_UPPER_OFFSET) != u)
    l = board_reg_read(ZYNQ_TIMER + ZYNQ_TMR_COUNTER_LOWER_OFFSET);
  *time = ((((uint64_t)u) << 32) | (uint64_t)l) / TIMER_CLK_DIV;
}

void board_timer_reset(void) {}
