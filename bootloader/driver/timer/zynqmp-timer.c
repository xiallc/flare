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
 * ZynqMP Timer.
 */

#include <stdio.h>

#include <driver/io/board-io.h>

#include "board-timer.h"

#define ZYNQMP_TIMER                    (0xfe900000)
#define ZYNQMP_TSG_COUNTER_LOWER_OFFSET (0x08)
#define ZYNQMP_TSG_COUNTER_UPPER_OFFSET (0x0C)
#define ZYNQMP_TSG_CTRL_OFFSET          (0x00)
#define ZYNQMP_TSG_BASE_FREQ_OFFSET     (0x20)

#define TSG_CLK_FREQ_HZ (0x02FAF080 * 5) /* 250MHz */
#define TIMER_CLK_DIV   (TSG_CLK_FREQ_HZ / (1000000ULL))

static bool enabled = false;

void board_timer_get(uint64_t* time) {
  if (!enabled) {
    board_timer_reset();
  }
  uint32_t u = board_reg_read(ZYNQMP_TIMER + ZYNQMP_TSG_COUNTER_UPPER_OFFSET);
  uint32_t l = board_reg_read(ZYNQMP_TIMER + ZYNQMP_TSG_COUNTER_LOWER_OFFSET);
  *time = ((((uint64_t)u) << 32) | (uint64_t)l) / TIMER_CLK_DIV;
}

void board_timer_reset(void) {
  /* Disable counter and debug halting */
  board_reg_write(ZYNQMP_TIMER + ZYNQMP_TSG_CTRL_OFFSET, 0x0);

  /* Set counter to 0 */
  board_reg_write(ZYNQMP_TIMER + ZYNQMP_TSG_COUNTER_LOWER_OFFSET, 0x0);
  board_reg_write(ZYNQMP_TIMER + ZYNQMP_TSG_COUNTER_UPPER_OFFSET, 0x0);

  /* Store the clock frequency */
  board_reg_write(ZYNQMP_TIMER + ZYNQMP_TSG_BASE_FREQ_OFFSET, TSG_CLK_FREQ_HZ);

  /* Enable counter */
  board_reg_write(ZYNQMP_TIMER + ZYNQMP_TSG_CTRL_OFFSET, 0x1);
  enabled = true;
}
