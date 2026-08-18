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

/**
 * Flash Layout
 */

#if !defined(_FLARE_ZYNQMP_FLASH_H_)
#define _FLARE_ZYNQMP_FLASH_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * Flash memory map.
 */
#define FLARE_FLASH_SIZE       flash_device_size()
#define FLARE_FLASH_BLOCK_SIZE flash_device_sector_erase_size()

#define FLARE_FLASH_BOOT_BASE    (0UL)
#define FLARE_FLASH_BOOT_SIZE    (16UL * 1024 * 1024)
#define FLARE_FLASH_FACTORY_BASE (FLARE_FLASH_SIZE - FLARE_FLASH_FACTORY_SIZE)
#define FLARE_FLASH_FACTORY_SIZE (FLARE_FLASH_BLOCK_SIZE)
#define FLARE_FLASH_FILESYSTEM_BASE                                            \
  (FLARE_FLASH_BOOT_BASE + FLARE_FLASH_BOOT_SIZE)
#define FLARE_FLASH_FILESYSTEM_SIZE                                            \
  (FLARE_FLASH_FACTORY_BASE - (FLARE_FLASH_BOOT_BASE + FLARE_FLASH_BOOT_SIZE))

/*
 * Factory flash partition.
 */
#define FLARE_FLASH_FSBL_BASE FLARE_FLASH_BOOT_BASE
#define FLARE_FLASH_FSBL_SIZE (3 * 1024 * 1024)
#define FLARE_FLASH_SSBL_BASE (FLARE_FLASH_FSBL_BASE + FLARE_FLASH_FSBL_SIZE)
#define FLARE_FLASH_SSBL_SIZE (0)
#define FLARE_FLASH_EXE_BASE  (FLARE_FLASH_SSBL_BASE + FLARE_FLASH_SSBL_SIZE)
#define FLARE_FLASH_EXE_SIZE  (FLARE_FLASH_BOOT_SIZE - FLARE_FLASH_EXE_BASE)

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
