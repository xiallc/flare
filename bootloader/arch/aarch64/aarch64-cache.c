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
 * AARCH64 Cache.
 */

#include "aarch64-el.h"

#include "mmu/aarch64-cache.h"

void cache_flush(void) {
  rtems_cache_flush_entire_data();
}

void cache_invalidate(void) {
  rtems_cache_invalidate_entire_data();
  rtems_cache_invalidate_entire_instruction();
}

void cache_flush_invalidate(void) {
  cache_flush();
  cache_invalidate();
}

void cache_disable_icache(void) {
  cache_flush();
  rtems_cache_disable_instruction(get_current_EL());
}

void cache_disable_dcache(void) {
  cache_flush();
  rtems_cache_disable_data(get_current_EL());
}

void cache_disable(void) {
  cache_flush();
  cache_disable_dcache();
  cache_disable_icache();
}

void cache_enable_icache(void) {
  rtems_cache_enable_instruction(get_current_EL());
  rtems_cache_invalidate_entire_instruction();
}

void cache_enable_dcache(void) {
  rtems_cache_enable_data(get_current_EL());
  rtems_cache_invalidate_entire_data();
}

void cache_enable(void) {
  cache_enable_icache();
  cache_enable_dcache();
}
