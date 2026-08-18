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
 * Low level hardware access to the Board
 */

#if !defined(_BOARD_IO_H_)
#define _BOARD_IO_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*
 * Write to a device memory location.
 */
static inline void board_reg_write(uintptr_t address, uint32_t value) {
  volatile uint32_t* ap = (uint32_t*)address;
  *ap = value;
}

/*
 * Read from a device memory location.
 */
static inline uint32_t board_reg_read(uintptr_t address) {
  volatile uint32_t* ap = (uint32_t*)address;
  return *ap;
}

/*
 * Write to a device memory location.
 */
static inline void board_reg_write_16(uintptr_t address, uint16_t value) {
  volatile uint16_t* ap = (uint16_t*)address;
  *ap = value;
}

/*
 * Read from a device memory location.
 */
static inline uint16_t board_reg_read_16(uintptr_t address) {
  volatile uint16_t* ap = (uint16_t*)address;
  return *ap;
}

/*
 * Write to a device memory location.
 */
static inline void board_reg_write_8(uintptr_t address, uint8_t value) {
  volatile uint8_t* ap = (uint8_t*)address;
  *ap = value;
}

/*
 * Read from a device memory location.
 */
static inline uint8_t board_reg_read_8(uintptr_t address) {
  volatile uint8_t* ap = (uint8_t*)address;
  return *ap;
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
