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

#if !defined(ZYNQ_H)
#define ZYNQ_H

#include <stdint.h>

#include <driver/io/board-io.h>

#include "zynq7000-flash-map.h"

#define ARCHITECTURE "Zynq"

/*
 * PS reset control register define
 */
#define PS_RST_MASK 0x00000001 /**< PS software reset */

/*
 * SLCR BOOT Mode Register defines
 */
#define BOOT_MODES_MASK 0x00000007 /**< FLASH types */

/*
 * Boot Modes
 */
#define JTAG_MODE       0x00000000 /**< JTAG Boot Mode */
#define QSPI_MODE       0x00000001 /**< QSPI Boot Mode */
#define NOR_FLASH_MODE  0x00000002 /**< NOR Boot Mode */
#define NAND_FLASH_MODE 0x00000004 /**< NAND Boot Mode */
#define SD_MODE         0x00000005 /**< SD Boot Mode */
#define MMC_MODE        0x00000006 /**< MMC Boot Device */
#define NO_MODE_MODE    0x00000007 /**< Invalid mode. */

#define RESET_REASON_SRST 0x00000020 /**< Reason for reset is SRST */
#define RESET_REASON_SWDT 0x00000001 /**< Reason for reset is SWDT */

/*
 * SLCR Registers
 */
#define PS_RST_CTRL_REG   (0xf8000200)
#define FPGA_RESET_REG    (0xf8000240)
#define RESET_REASON_CLR  (0xf8000254)
#define REBOOT_STATUS_REG (0xf8000258)
#define RESET_REASON_REG  (0xf8000258)
#define BOOT_MODE_REG     (0xf800025c)
#define PS_LVL_SHFTR_EN   (0xf8000900)

#endif
