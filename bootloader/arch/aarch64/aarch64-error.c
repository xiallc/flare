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
 * Xilinx Versal.
 */

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>

#include <board.h>
#include <cache.h>

#include <driver/uart/console.h>

#include "mmu/aarch64-mmu.h"

struct error_stack_frame {
  uint64_t padding[2];
  uint64_t Xn[31];
  uint64_t ELR;
  uint64_t ESR;
  uint64_t FAR;
  uint64_t stack[64];
};

static void error_lockdown(void) {
  printf("Bootloader failure. Resetting ...  \b \b \b \b");
  console_flush();
  while (true)
    ;
}

static void trace_error(int el, int es, const char* label) {
  int i;
  volatile struct error_stack_frame* frame = (struct error_stack_frame*)&frame;
  const char* el_str;
  const char* es_str;

  switch (el) {
  case 1:
    el_str = "EL1";
    break;
  case 2:
    el_str = "EL2";
    break;
  case 3:
    el_str = "EL3";
    break;
  default:
    el_str = "Unknown exception level";
    break;
  }

  switch (es) {
  case 0:
    es_str = "EL is using SP_EL0 stack";
    break;
  case 1:
    es_str = "EL is using SP_ELx stack";
    break;
  case 2:
    es_str = "From lower EL in AARCH64";
    break;
  case 3:
    es_str = "From lower EL in AARCH32";
    break;
  default:
    es_str = "Unknown error source";
    break;
  }

  printf("\nFATAL: %s %s (%s)\nStack Pointer: %016lx\n", el_str, label, es_str,
         (uint64_t)frame->stack);

  printf("ELR: %016lx\n", frame->ELR);
  printf("ESR: %016lx\n", frame->ESR);
  printf("FAR: %016lx\n", frame->FAR);

  for (i = 30; i > 0; i = i - 2) {
    printf("x%02d: 0x%016lx   x%02d: 0x%016lx\n", 30 - i, frame->Xn[i],
           30 - (i - 1), frame->Xn[i - 1]);
  }
  printf("x%02d: 0x%016lx\n", 30, frame->Xn[0]);

  printf("\n--------Stack--------");
  for (i = 0; i < 64; ++i) {
    if ((i & 0x3) == 0) {
      printf("\n ");
    }
    printf("%016lx ", frame->stack[i]);
  }
  printf("\n ");

  printf("- Stack from error handler -\n");
  printf(" %016lx %016lx\n", frame->padding[0], frame->padding[1]);
}

void SError(int el, int es) {
  trace_error(el, es, "SError");
  error_lockdown();
}

void SWInterrupt(int el, int es) {
  trace_error(el, es, "SWI");
  error_lockdown();
}

void IRQInterrupt(int el, int es) {
  trace_error(el, es, "IRQ");
  error_lockdown();
}

void FIQInterrupt(int el, int es) {
  trace_error(el, es, "FIQ");
  error_lockdown();
}

void board_hardware_setup(void) {}
