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

#include <sleep.h>
#include <stdio.h>

#include "ipi.h"

#include <driver/io/board-io.h>
#include <driver/timer/board-timer.h>

static uint32_t* get_buf_addr(uint32_t src, uint32_t dest, bool resp) {
  uint32_t* addr =
      (uint32_t*)(uint64_t)(FLARE_IPI_MSG_BASE + (src * IPI_BUF_OFF_GROUP) +
                            (dest * IPI_BUF_OFF_TARGET));

  if (resp) {
    addr = addr + (IPI_BUF_OFF_RESP / sizeof(uint32_t));
  }

  return addr;
}

static int poll_for_ack(uint32_t src, uint32_t dest_mask) {
  bool acked = false;
  uint64_t end_time;
  uint64_t curr_time;

  board_timer_get(&curr_time);
  end_time = curr_time + 1000000;
  acked = ((board_reg_read(IPI_OBS(src)) & dest_mask) == 0);

  while (!acked) {
    board_timer_get(&curr_time);
    if (curr_time > end_time) {
      return -1;
    }

    acked = ((board_reg_read(IPI_OBS(src)) & dest_mask) == 0);
  }

  return 0;
}

int pm_ipi_read(uint32_t response[IPI_PAYLOAD_ARG_CNT]) {
  int status = 0;
  uint32_t* msg_buf_addr;

  status = poll_for_ack(IPI_APU_0_OFF, IPI_MASK_PMU_0);
  if (status) {
    return status;
  }

  msg_buf_addr = get_buf_addr(IPI_INDEX_APU_0, IPI_INDEX_PMU_0, true);

  for (int i = 0; i < IPI_PAYLOAD_ARG_CNT; i++) {
    response[i] = board_reg_read((uintptr_t)&msg_buf_addr[i]);
  }

  return 0;
}

int pm_ipi_send(uint32_t payload[IPI_PAYLOAD_ARG_CNT]) {
  int status = 0;
  uint32_t* msg_buf_addr;

  status = poll_for_ack(IPI_APU_0_OFF, IPI_MASK_PMU_0);
  if (status) {
    return status;
  }

  msg_buf_addr = get_buf_addr(IPI_INDEX_APU_0, IPI_INDEX_PMU_0, false);

  for (int i = 0; i < IPI_PAYLOAD_ARG_CNT; i++) {
    board_reg_write((uintptr_t)&msg_buf_addr[i], payload[i]);
  }

  board_reg_write(IPI_TRIG(IPI_APU_0_OFF), IPI_MASK_PMU_0);

  status = poll_for_ack(IPI_PMU_0_OFF, IPI_MASK_PMU_0);
  if (status) {
    return status;
  }

  return 0;
}
