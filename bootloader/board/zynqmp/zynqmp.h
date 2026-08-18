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
 * Xilinx ZynqMP.
 */

#if !defined(ZYNQMP_H)
#define ZYNQMP_H

#include <stdint.h>

#include "zynqmp-flash-map.h"

#define ARCHITECTURE "ZynqMP"

/*
 * PS reset control register define
 */
#define PS_RST_MASK 0x00000001 /**< PS software reset */

/*
 * SLCR BOOT Mode Register defines
 */
#define BOOT_MODES_MASK 0x0000000F /**< FLASH types */

/*
 * Boot Modes
 */
#define JTAG_MODE 0x00000000 /**< JTAG Boot Mode */
#define QSPI_MODE 0x00000002 /**< QSPI32 Boot Mode */
#define SD_MODE   0x00000005 /**< SD Boot Mode */
#define MMC_MODE  0x00000006 /**< eMMC 4.51 1.8v Boot Device */

/*
 * Register Addresses
 */

#define BOOT_MODE_REG    (0xff5e0200)
#define RESET_REASON_REG (0xff5e0220)

#endif /* ZYNQMP_H */
