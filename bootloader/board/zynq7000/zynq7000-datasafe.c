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

#define ZYNQ7000_REBOOT_STATUS_REGISTER 0xF8000258
#define ZYNQ7000_BOOTMODE_REGISTER      0xF800025C

#define ZYNQ7000_RS_POR       (1 << 22)
#define ZYNQ7000_RS_SRST_B    (1 << 21)
#define ZYNQ7000_RS_DBG_RST   (1 << 20)
#define ZYNQ7000_RS_SLC_RST   (1 << 19)
#define ZYNQ7000_RS_AWDT0_RST (1 << 18)
#define ZYNQ7000_RS_AWDT1_RST (1 << 17)
#define ZYNQ7000_RS_SWDT_RST  (1 << 16)

#define ZYNQ7000_BM_MASK (0x7)
#define ZYNQ7000_BM_JTAG (0x0)
#define ZYNQ7000_BM_NOR  (0x2)
#define ZYNQ7000_BM_NAND (0x4)
#define ZYNQ7000_BM_QSPI (0x1)
#define ZYNQ7000_BM_SD   (0x5)

int board_bootmode() {
  uint32_t bootmode =
      board_reg_read(ZYNQ7000_BOOTMODE_REGISTER) & ZYNQ7000_BM_MASK;
  if (bootmode == ZYNQ7000_BM_QSPI) {
    return FLARE_DS_BOOTMODE_QSPI;
  } else if (bootmode == ZYNQ7000_BM_SD) {
    return FLARE_DS_BOOTMODE_SD_CARD;
  } else if (bootmode == ZYNQ7000_BM_JTAG) {
    return FLARE_DS_BOOTMODE_JTAG;
  } else {
    return FLARE_DS_BOOTMODE_ERROR;
  }
}

void flare_datasafe_hw_init(flare_datasafe* ds) {
  uint32_t rs = board_reg_read(ZYNQ7000_REBOOT_STATUS_REGISTER);

  ds->bootmode &= ~FLARE_DS_BOOTMODE_HW_MASK;
  ds->bootmode |= board_bootmode();

  ds->reset &= ~FLARE_DS_RESET_MASK;
  if (rs & ZYNQ7000_RS_POR) {
    ds->reset |= FLARE_DS_RESET_POR;
  } else if (rs & ZYNQ7000_RS_SRST_B) {
    ds->reset |= FLARE_DS_RESET_EXT;
  } else if (rs & ZYNQ7000_RS_DBG_RST) {
    ds->reset |= FLARE_DS_RESET_DBG;
  } else if (rs & ZYNQ7000_RS_SLC_RST) {
    ds->reset |= FLARE_DS_RESET_SWR;
  } else if (rs & ZYNQ7000_RS_AWDT0_RST || rs & ZYNQ7000_RS_AWDT1_RST ||
             rs & ZYNQ7000_RS_SWDT_RST) {
    ds->reset |= FLARE_DS_RESET_WDT;
  } else {
    ds->reset |= FLARE_DS_RESET_ERR;
  }
}
