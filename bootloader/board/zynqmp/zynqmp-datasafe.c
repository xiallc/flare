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

/**
 * Data safe hardware specific.
 */

#include <board.h>
#include <hw-datasafe.h>

#include <driver/io/board-io.h>

#define ZYNQMP_BOOTMODE_REGISTER 0xFF5E0200
#define ZYNQMP_RESET_REASON      0xFF5E0220

#define ZYNQMP_RR_INT_POR (1 << 0)
#define ZYNQMP_RR_EXT_POR (1 << 1)
#define ZYNQMP_RR_PMU_SYS (1 << 2)
#define ZYNQMP_RR_PS_ONLY (1 << 3)
#define ZYNQMP_RR_SRST    (1 << 4)
#define ZYNQMP_RR_SOFT    (1 << 5)
#define ZYNQMP_RR_DEBUG   (1 << 6)

#define ZYNQMP_BM_MASK    (0xF)
#define ZYNQMP_BM_JTAG    (0x0)
#define ZYNQMP_BM_QSPI_24 (0x1)
#define ZYNQMP_BM_QSPI_32 (0x2)
#define ZYNQMP_BM_SD0     (0x3)
#define ZYNQMP_BM_NAND    (0x4)
#define ZYNQMP_BM_SD1     (0x5)
#define ZYNQMP_BM_EMMC    (0x6)
#define ZYNQMP_BM_USB0    (0x7)
#define ZYNQMP_BM_PJTAG0  (0x8)
#define ZYNQMP_BM_PJTAG1  (0x9)
#define ZYNQMP_BM_SD1_LS  (0xE)

int board_bootmode() {
  uint32_t bootmode = board_reg_read(ZYNQMP_BOOTMODE_REGISTER) & ZYNQMP_BM_MASK;

  if (bootmode == ZYNQMP_BM_JTAG) {
    return FLARE_DS_BOOTMODE_JTAG;
  } else if (bootmode == ZYNQMP_BM_QSPI_24 || bootmode == ZYNQMP_BM_QSPI_32) {
    return FLARE_DS_BOOTMODE_QSPI;
  } else if (bootmode == ZYNQMP_BM_SD0 || bootmode == ZYNQMP_BM_SD1) {
    return FLARE_DS_BOOTMODE_SD_CARD;
  } else {
    return FLARE_DS_BOOTMODE_ERROR;
  }
}

void flare_datasafe_hw_init(flare_datasafe* ds) {
  uint32_t rs = board_reg_read(ZYNQMP_RESET_REASON);

  ds->bootmode &= ~FLARE_DS_BOOTMODE_HW_MASK;
  ds->bootmode |= board_bootmode();

  ds->reset &= ~FLARE_DS_RESET_MASK;
  if (rs & ZYNQMP_RR_INT_POR || rs & ZYNQMP_RR_EXT_POR) {
    ds->reset |= FLARE_DS_RESET_POR;
  } else if (rs & ZYNQMP_RR_SRST) {
    ds->reset |= FLARE_DS_RESET_EXT;
  } else if (rs & ZYNQMP_RR_DEBUG) {
    ds->reset |= FLARE_DS_RESET_DBG;
  } else if (rs & ZYNQMP_RR_SOFT || rs & ZYNQMP_RR_PMU_SYS ||
             rs & ZYNQMP_RR_PS_ONLY) {
    ds->reset |= FLARE_DS_RESET_SWR;
  } else {
    ds->reset |= FLARE_DS_RESET_ERR;
  }
}
