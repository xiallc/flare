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
 * Xilinx Zynq.
 */

#include <inttypes.h>
#include <stdio.h>

#include <board.h>
#include <cache.h>

#include <driver/slcr/board-slcr.h>
#include <driver/uart/console.h>
#include <driver/wdog/wdog.h>

#include "ps7_init.h"
#include "zynq7000.h"

static void error_lockdown(void) {
  printf("Bootloader failure. Reseting ...  \b \b \b \b");
  console_flush();
  board_slcr_lock();
  wdog_control(true);
  while (true)
    ;
}

static void trace_error(const char* label) {
  volatile uint32_t* stack;
  int i;
  stack = (volatile uint32_t*)&stack;
  printf("FATAL: %s: sp:%08" PRIx32, label, (uint32_t)stack);
  for (i = 0; i < 64; ++i) {
    if ((i & 0x7) == 0)
      printf("\n ");
    printf("%08" PRIx32 " ", stack[i]);
  }
  printf("\n ");
}

void UndefinedException(void);
void SWInterrupt(void);
void PrefetchAbortInterrupt(void);
void FIQInterrupt(void);
void IRQInterrupt(void);
void DataAbortInterrupt(void);

void UndefinedException(void) {
  trace_error("UNDEFINED\n");
  error_lockdown();
}

void SWInterrupt(void) {
  trace_error("SWI\n");
  error_lockdown();
}

void PrefetchAbortInterrupt(void) {
  trace_error("PREFETCH-ABORT\n");
  error_lockdown();
}

void DataAbortInterrupt(void) {
  trace_error("DATA-ABORT\n");
  error_lockdown();
}

void IRQInterrupt(void) {
  trace_error("IRQ\n");
  error_lockdown();
}

void FIQInterrupt(void) {
  trace_error("FIQ\n");
  error_lockdown();
}
void board_hardware_setup(void) {
  uint32_t status;

  /*
   * Initialization for MIO,PLL,CLK and DDR
   */
  board_slcr_unlock();
  status = ps7_init();
  board_slcr_lock();
  if (status != PS7_INIT_SUCCESS) {
    printf("error: PS7 init_fail : %s\n", getPS7MessageInfo(status));
    error_lockdown();
  }
}
