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
 * Xilinx Zynq Cache.
 *
 * Based on the RTEMS version in arm-cp15.h:
 *     Copyright (c) 2013 Hesham AL-Matary
 *     Copyright (c) 2009-2017 embedded brains GmbH.  All rights reserved.
 *
 */

#include <stdint.h>

#include <cache.h>

#include <driver/io/board-io.h>

#include <arch/arm/xil_errata.h>

/*
 * L2 cache controller registers.
 */
#define L2CC_BASE              (0xf8f02000)
#define L2CC_CNTRL             (0x100)
#define L2CC_AUX_CNTRL         (0x104)
#define L2CC_TAG_RAM_CNTRL     (0x108)
#define L2CC_DATA_RAM_CNTRL    (0x10C)
#define L2CC_IAR               (0x220)
#define L2CC_ISR               (0x21c)
#define L2CC_CACHE_SYNC        (0x730)
#define L2CC_CACHE_INVLD_WAY   (0x77c)
#define L2CC_DUMMY_CACHE_SYNC  (0x740)
#define L2CC_CACHE_INV_CLN_WAY (0x7fc)
#define L2CC_DEBUG_CTRL        (0xf40)

#define L2CC_AUX_REG_ZERO_MASK     (0xfff1ffff)
#define L2CC_AUX_REG_DEFAULT_MASK  (0x72360000)
#define L2CC_TAG_RAM_DEFAULT_MASK  (0x00000111)
#define L2CC_DATA_RAM_DEFAULT_MASK (0x00000121)

/*
 * ARM and thumb mode controls.
 */
#define ARM_SWITCH_REGISTERS uint32_t arm_switch_reg
#define ARM_SWITCH_TO_ARM    ".align 2\nbx pc\n.arm\n"
#define ARM_SWITCH_BACK                                                        \
  "add %[arm_switch_reg], pc, #1\nbx %[arm_switch_reg]\n.thumb\n"
#define ARM_SWITCH_OUTPUT            [arm_switch_reg] "=&r"(arm_switch_reg)
#define ARM_SWITCH_ADDITIONAL_OUTPUT , ARM_SWITCH_OUTPUT

static inline void cache_dsb(void) {
  __asm__ volatile(" dsb" : : : "memory");
}

static inline void cache_isb(void) {
  __asm__ volatile(" isb" : : : "memory");
}

static inline uint32_t cache_l1_get_control(void) {
  ARM_SWITCH_REGISTERS;
  uint32_t val;

  __asm__ volatile(ARM_SWITCH_TO_ARM
                   "mrc p15, 0, %[val], c1, c0, 0\n" ARM_SWITCH_BACK
                   : [val] "=&r"(val)ARM_SWITCH_ADDITIONAL_OUTPUT);

  return val;
}

static inline void cache_l1_set_control(uint32_t val) {
  ARM_SWITCH_REGISTERS;

  __asm__ volatile(ARM_SWITCH_TO_ARM "mcr p15, 0, %[val], c1, c0, 0\n"
                                     "nop\n"
                                     "nop\n" ARM_SWITCH_BACK
                   : ARM_SWITCH_OUTPUT
                   : [val] "r"(val)
                   : "memory");
}

static inline uint32_t cache_l1_get_cache_level_id(void) {
  ARM_SWITCH_REGISTERS;
  uint32_t val;

  __asm__ volatile(ARM_SWITCH_TO_ARM
                   "mrc p15, 1, %[val], c0, c0, 1\n" ARM_SWITCH_BACK
                   : [val] "=&r"(val)ARM_SWITCH_ADDITIONAL_OUTPUT);

  return val;
}

static inline void cache_l1_set_cache_size_selection(uint32_t val) {
  ARM_SWITCH_REGISTERS;

  __asm__ volatile(ARM_SWITCH_TO_ARM
                   "mcr p15, 2, %[val], c0, c0, 0\n" ARM_SWITCH_BACK
                   : ARM_SWITCH_OUTPUT
                   : [val] "r"(val)
                   : "memory");
}

static inline uint32_t cache_l1_get_cache_size_id(void) {
  ARM_SWITCH_REGISTERS;
  uint32_t val;

  __asm__ volatile(ARM_SWITCH_TO_ARM
                   "mrc p15, 1, %[val], c0, c0, 0\n" ARM_SWITCH_BACK
                   : [val] "=&r"(val)ARM_SWITCH_ADDITIONAL_OUTPUT);

  return val;
}

static inline uint32_t
cache_l1_get_cache_size_id_for_level(uint32_t level_and_inst_dat) {
  cache_l1_set_cache_size_selection(level_and_inst_dat);
  cache_isb();
  return cache_l1_get_cache_size_id();
}

static inline void
cache_l1_data_cache_clean_line_by_set_and_way(uint32_t set_and_way) {
  ARM_SWITCH_REGISTERS;

  __asm__ volatile(ARM_SWITCH_TO_ARM
                   "mcr p15, 0, %[set_and_way], c7, c10, 2\n" ARM_SWITCH_BACK
                   : ARM_SWITCH_OUTPUT
                   : [set_and_way] "r"(set_and_way)
                   : "memory");
}

static inline void cache_l1_dcache_clean_level(uint32_t level) {
  uint32_t ccsidr;
  uint32_t line_power;
  uint32_t associativity;
  uint32_t way;
  uint32_t way_shift;
  ccsidr = cache_l1_get_cache_size_id_for_level(level << 1);
  line_power = (ccsidr & 0x7) + 4;
  associativity = ((ccsidr >> 3) & 0x3ff) + 1;
  way_shift = __builtin_clz(associativity - 1);
  for (way = 0; way < associativity; ++way) {
    uint32_t num_sets = ((ccsidr >> 13) & 0x7fff) + 1;
    uint32_t set;
    for (set = 0; set < num_sets; ++set) {
      uint32_t set_way =
          (way << way_shift) | (set << line_power) | (level << 1);
      cache_l1_data_cache_clean_line_by_set_and_way(set_way);
    }
  }
}

static inline void cache_l1_dcache_clean_all_levels(void) {
  if ((cache_l1_get_control() & (1 << 2)) != 0) {
    uint32_t clidr = cache_l1_get_cache_level_id();
    uint32_t loc = (clidr >> 24) & 0x7;
    uint32_t level = 0;
    for (level = 0; level < loc; ++level) {
      uint32_t ctype = (clidr >> (3 * level)) & 0x7;
      /* Check if this level has a data cache or unified cache */
      if (((ctype & (0x6)) == 2) || (ctype == 4)) {
        cache_l1_dcache_clean_level(level);
      }
    }
    cache_dsb();
  }
}

/* In DDI0301H_arm1176jzfs_r0p7_trm
 * 'MCR p15, 0, <Rd>, c7, c14, 0' means
 * Clean and Invalidate Entire Data Cache
 */
static inline void cache_l1_dcache_clean_invalidate(void) {
  ARM_SWITCH_REGISTERS;
  uint32_t sbz = 0;

  __asm__ volatile(ARM_SWITCH_TO_ARM
                   "mcr p15, 0, %[sbz], c7, c14, 0\n" ARM_SWITCH_BACK
                   : ARM_SWITCH_OUTPUT
                   : [sbz] "r"(sbz)
                   : "memory");
}

void cache_l1_dcache_invalidate(void) {
  ARM_SWITCH_REGISTERS;
  uint32_t sbz = 0;

  __asm__ volatile(ARM_SWITCH_TO_ARM
                   "mcr p15, 0, %[sbz], c7, c6, 0\n" ARM_SWITCH_BACK
                   : ARM_SWITCH_OUTPUT
                   : [sbz] "r"(sbz)
                   : "memory");
}

/* ICIALLUIS, Instruction Cache Invalidate All to PoU, Inner Shareable */
static inline void cache_l1_icache_invalidate_all_inner_shareable(void) {
  ARM_SWITCH_REGISTERS;
  uint32_t sbz = 0;

  __asm__ volatile(ARM_SWITCH_TO_ARM
                   "mcr p15, 0, %[sbz], c7, c1, 0\n" ARM_SWITCH_BACK
                   : ARM_SWITCH_OUTPUT
                   : [sbz] "r"(sbz)
                   : "memory");
}

static inline void cache_l1_icache_invalidate(void) {
  if ((cache_l1_get_control() & (1 << 12)) != 0) {
    cache_l1_set_cache_size_selection(1);
    cache_l1_icache_invalidate_all_inner_shareable();
  }
}

static inline void cache_l1_icache_enable(void) {
  uint32_t ctrl = cache_l1_get_control();
  if ((ctrl & (1 << 12)) == 0) {
    cache_l1_icache_invalidate_all_inner_shareable();
    cache_l1_set_control(ctrl | (1 << 12));
  }
}

static inline void cache_l1_icache_disable(void) {
  uint32_t ctrl = cache_l1_get_control();
  if ((ctrl & (1 << 12)) != 0) {
    cache_dsb();
    cache_l1_icache_invalidate_all_inner_shareable();
    cache_l1_set_control(ctrl & ~(1 << 12));
  }
}

static inline void cache_l1_dcache_enable(void) {
  uint32_t ctrl = cache_l1_get_control();
  if ((ctrl & (1 << 2)) == 0) {
    cache_l1_dcache_invalidate();
    cache_l1_set_control(ctrl | (1 << 2));
  }
}

static inline void cache_l1_dcache_disable(void) {
  uint32_t ctrl = cache_l1_get_control();
  if ((ctrl & (1 << 2)) != 0) {
    cache_l1_dcache_clean_all_levels();
    cache_l1_set_control(ctrl & ~(1 << 2));
  }
}

static inline void cache_l2_sync(void) {
  while (board_reg_read(L2CC_BASE + L2CC_CACHE_SYNC) != 0)
    ;
#ifdef CONFIG_PL310_ERRATA_753970
  board_reg_write(L2CC_BASE + L2CC_DUMMY_CACHE_SYNC, 0x0);
#else
  board_reg_write(XPS_L2CC_BASE + L2CC_CACHE_SYNC, 0x0);
#endif
  while (board_reg_read(L2CC_BASE + L2CC_CACHE_SYNC) != 0)
    ;
  cache_dsb();
}

static inline void cache_l2_flush(void) {
  if ((board_reg_read(L2CC_BASE + L2CC_CNTRL) & 0x01) != 0) {
#if defined(CONFIG_PL310_ERRATA_588369) || defined(CONFIG_PL310_ERRATA_727915)
    board_reg_write(L2CC_BASE + L2CC_DEBUG_CTRL, 0x3);
#endif
    board_reg_write(L2CC_BASE + L2CC_CACHE_INV_CLN_WAY, 0x0000ffff);
    cache_l2_sync();
#if defined(CONFIG_PL310_ERRATA_588369) || defined(CONFIG_PL310_ERRATA_727915)
    board_reg_write(L2CC_BASE + L2CC_DEBUG_CTRL, 0x0);
#endif
    cache_dsb();
  }
}

static inline void cache_l2_invalidate(void) {
  board_reg_write(L2CC_BASE + L2CC_CACHE_INVLD_WAY, 0x0000ffff);
  cache_l2_sync();
  cache_dsb();
}

static inline void cache_l2_enable(void) {
  if ((board_reg_read(L2CC_BASE + L2CC_CNTRL) & 0x01) == 0) {
    uint32_t r = board_reg_read(L2CC_BASE + L2CC_AUX_CNTRL);
    r &= L2CC_AUX_REG_ZERO_MASK;
    r |= L2CC_AUX_REG_DEFAULT_MASK;
    board_reg_write(L2CC_BASE + L2CC_AUX_CNTRL, r);
    board_reg_write(L2CC_BASE + L2CC_TAG_RAM_CNTRL, L2CC_TAG_RAM_DEFAULT_MASK);
    board_reg_write(L2CC_BASE + L2CC_DATA_RAM_CNTRL,
                    L2CC_DATA_RAM_DEFAULT_MASK);
    board_reg_write(L2CC_BASE + L2CC_IAR, board_reg_read(L2CC_BASE + L2CC_ISR));
    board_reg_write(L2CC_BASE + L2CC_CNTRL,
                    board_reg_read(L2CC_BASE + L2CC_CNTRL) | 1);
    cache_l2_sync();
    cache_dsb();
  }
}

static inline void cache_l2_disable(void) {
  if ((board_reg_read(L2CC_BASE + L2CC_CNTRL) & 0x1) != 0) {
    cache_l2_flush();
    board_reg_write(L2CC_BASE + L2CC_CNTRL,
                    board_reg_read(L2CC_BASE + L2CC_CNTRL) & ~1);
    cache_dsb();
  }
}

void cache_flush(void) {
  cache_l1_dcache_clean_all_levels();
  cache_l2_flush();
}

void cache_invalidate_icache(void) {
  cache_l2_invalidate();
  cache_l1_icache_invalidate();
}

void cache_invalidate(void) {
  cache_invalidate_icache();
}
void cache_flush_invalidate(void) {
  cache_flush();
  cache_invalidate();
}

void cache_disable_icache(void) {
  cache_l2_disable();
  cache_l1_icache_disable();
}

void cache_disable_dcache(void) {
  cache_l2_disable();
  cache_l1_dcache_disable();
}

void cache_disable(void) {
  cache_flush();
  cache_disable_dcache();
  cache_disable_icache();
}

void cache_enable_icache(void) {
  cache_l1_icache_enable();
  cache_l2_enable();
}

void cache_enable_dcache(void) {
  cache_l1_dcache_enable();
  cache_l2_enable();
}

void cache_enable(void) {
  cache_enable_dcache();
  cache_enable_icache();
}
