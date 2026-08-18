/*
 * Copyright 2025 Contemporary Software
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

#include "sdhci.h"
#include "sdhci_hw.h"
#include "zynqmp-sdhci.h"

uint32_t sdhci_address(int ctlr) {
  if (ctlr == SDHCI_CTLR_EMMC) {
    return SDHCI0_REG_BASE;
  } else if (ctlr == SDHCI_CTLR_SD) {
    return SDHCI1_REG_BASE;
  }
  printf("sdhci (%d): controller not supported\n", ctlr);
  return 0;
}

void disable_bus_power(int) {}

void enable_bus_power(int) {}
