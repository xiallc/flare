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
 * Versal Timer.
 */

#include <stdio.h>

#include "board-io.h"

#include "board-timer.h"

#define VERSAL_TIMER                    (0xf0990000)
#define VERSAL_TSG_COUNTER_LOWER_OFFSET (0x08)
#define VERSAL_TSG_COUNTER_UPPER_OFFSET (0x0C)
#define VERSAL_TSG_CTRL_OFFSET          (0x00)
#define VERSAL_TSG_BASE_FREQ_OFFSET     (0x20)

#define TSG_CLK_FREQ_HZ (0x02FAF080) /* 50MHz */
#define TIMER_CLK_DIV   (TSG_CLK_FREQ_HZ / (1000000ULL))

void board_timer_get(uint64_t* time) {
  uint32_t u = board_reg_read(VERSAL_TIMER + VERSAL_TSG_COUNTER_UPPER_OFFSET);
  uint32_t l = board_reg_read(VERSAL_TIMER + VERSAL_TSG_COUNTER_LOWER_OFFSET);
  if (board_reg_read(VERSAL_TIMER + VERSAL_TSG_COUNTER_UPPER_OFFSET) != u)
    l = board_reg_read(VERSAL_TIMER + VERSAL_TSG_COUNTER_LOWER_OFFSET);
  *time = ((((uint64_t)u) << 32) | (uint64_t)l) / TIMER_CLK_DIV;
}

void board_timer_reset(void) {
  /* Disable counter and debug halting */
  board_reg_write(VERSAL_TIMER + VERSAL_TSG_CTRL_OFFSET, 0x0);

  /* Set counter to 0 */
  board_reg_write(VERSAL_TIMER + VERSAL_TSG_COUNTER_LOWER_OFFSET, 0x0);
  board_reg_write(VERSAL_TIMER + VERSAL_TSG_COUNTER_UPPER_OFFSET, 0x0);

  /* Store the clock freqency */
  board_reg_write(VERSAL_TIMER + VERSAL_TSG_BASE_FREQ_OFFSET, TSG_CLK_FREQ_HZ);

  /* Enable counter */
  board_reg_write(VERSAL_TIMER + VERSAL_TSG_CTRL_OFFSET, 0x1);
}
