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
 * Set up the MMU table.
 */

#include <stdint.h>
#include <string.h>

#define MMU_LAYOUT_ENTRIES (12)
struct {
  uint32_t count;
  uint32_t attr;
} mmuLayout[MMU_LAYOUT_ENTRIES] = {
    /* 0x00000000 - 0x3fffffff (DDR Cacheable)
       S=b1 TEX=b101 AP=b11, Domain=b1111, C=b0, B=b1 */
    {0x0400, 0x15de6},
    /* 0x40000000 - 0x7fffffff (FPGA slave0)
       S=b0 TEX=b000 AP=b11, Domain=b0, C=b0, B=b1 */
    {0x0400, 0x00c02},
    /* 0x80000000 - 0xbfffffff (FPGA slave1)
       S=b0 TEX=b000 AP=b11, Domain=b0, C=b0, B=b1 */
    {0x0400, 0x00c02},
    /* 0xc0000000 - 0xdfffffff (unassigned/reserved).
       Generates a translation fault if accessed
       S=b0 TEX=b000 AP=b00, Domain=b0, C=b0, B=b0 */
    {0x0200, 0x00000},
    /* 0xe0000000 - 0xe1ffffff (Memory mapped devices)
       UART/USB/IIC/SPI/CAN/GEM/GPIO/QSPI/SD/NAND
       S=b0 TEX=b000 AP=b11, Domain=b0, C=b0, B=b1 */
    {0x0020, 0x00c06},
    /* 0xe2000000 - 0xe3ffffff (NOR)
       S=b0 TEX=b000 AP=b11, Domain=b0, C=b0, B=b1 */
    {0x0020, 0x00c06},
    /* 0xe4000000 - 0xe5ffffff (SRAM)
       S=b0 TEX=b000 AP=b11, Domain=b0, C=b1, B=b1 */
    {0x0020, 0x00c0e},
    /* 0xe6000000 - 0xf7ffffff (unassigned/reserved).
       Generates a translation fault if accessed
       S=b0 TEX=b000 AP=b00, Domain=b0, C=b0, B=b0 */
    {0x0120, 0x00000},
    /* 0xf8000000 - 0xf8ffffff (AMBA APB Peripherals)
       S=b0 TEX=b000 AP=b11, Domain=b0, C=b0, B=b1 */
    {0x0010, 0x00c06},
    /* 0xf9000000 - 0xfbffffff (unassigned/reserved).
       Generates a translation fault if accessed
       S=b0 TEX=b000 AP=b00, Domain=b0, C=b0, B=b0 */
    {0x0030, 0x00000},
    /* 0xfc000000 - 0xffefffff (Linear QSPI - XIP)
       S=b0 TEX=b000 AP=b11, Domain=b0, C=b1, B=b1 */
    {0x003f, 0x00c0a},
    /* 256K OCM when mapped to high address space
       inner-cacheable
       S=b0 TEX=b100 AP=b11, Domain=b0, C=b1, B=b1 */
    {0x0001, 0x04c0e}};

void _setupMMU(void) {
  uint32_t* table = (uint32_t*)0xFFFE0000UL;
  uint32_t e;
  uint32_t le;
  uint32_t c;
  uint32_t sect;

  memset(table, 0, 16 * 1024UL);

  for (e = 0, le = 0, sect = 0; le < MMU_LAYOUT_ENTRIES; ++le) {
    for (c = 0; c < mmuLayout[le].count; ++c, ++e, sect += 0x100000) {
      table[e] = sect + mmuLayout[le].attr;
    }
  }
}
