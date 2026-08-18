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
 * Flare JFFS2 wrapper.
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <datasafe.h>
#include <flash-map.h>
#include <fs/boot-filesystem.h>

#include <driver/flash/flash.h>
#include <driver/jffs2/jffs2-boot.h>

/*
 * Cache header.
 */
#define FLARE_JFFS2_CACHE_HEADER_MARKER_1 (0)
#define FLARE_JFFS2_CACHE_HEADER_VERSION  (1)
#define FLARE_JFFS2_CACHE_HEADER_FLAGS    (2)
#define FLARE_JFFS2_CACHE_HEADER_MARKER_2 (3)
#define FLARE_JFFS2_CACHE_HEADER_CACHE    (4)
#define FLARE_JFFS2_CACHE_HEADER_SIZE     (4 * sizeof(uint32_t))

/*
 * Cache Marker.
 */
#define FLARE_JFFS2_CACHE_HEADER_MARKER (0x20010928)

/*
 * Cache Flags.
 */
#define FLARE_JFFS2_CACHE_FLAGS_MASK                                           \
  0x00000001 /* unused bits must be clear                                      \
              */
#define FLARE_JFFS2_CACHE_FLAGS_CRC (1 << 0)

/*
 * The location of the cache. The bitmap section of the cache must be cleared
 * only once.
 */
#define FLARE_JFFS2_CACHE_VERSION (2)
#define FLARE_JFFS2_USE_CACHE_CRC false
#define FLARE_JFFS2_CACHE_BASE    ((uint8_t*)(320UL * 1024UL * 1024UL))
#define FLARE_JFFS2_CACHE_SIZE                                                 \
  (FLARE_JFFS2_CACHE_HEADER_SIZE +                                             \
   JFFS2_BUFFER_CACHE_SIZE(FLARE_FLASH_FILESYSTEM_SIZE,                        \
                           FLARE_JFFS2_USE_CACHE_CRC))

static jffs2_control jffs2;
static char cwd[128];
static char scratch[256];

static void flare_Jffs2Cache_Setup(void) {
  uint32_t* cache = (uint32_t*)FLARE_JFFS2_CACHE_BASE;
  memset(FLARE_JFFS2_CACHE_BASE, 0, FLARE_JFFS2_CACHE_SIZE);
  cache[FLARE_JFFS2_CACHE_HEADER_MARKER_1] = FLARE_JFFS2_CACHE_HEADER_MARKER;
  cache[FLARE_JFFS2_CACHE_HEADER_VERSION] = FLARE_JFFS2_CACHE_VERSION;
  cache[FLARE_JFFS2_CACHE_HEADER_FLAGS] = 0;
  if (FLARE_JFFS2_USE_CACHE_CRC)
    cache[FLARE_JFFS2_CACHE_HEADER_FLAGS] |= FLARE_JFFS2_CACHE_FLAGS_CRC;
  cache[FLARE_JFFS2_CACHE_HEADER_MARKER_2] = FLARE_JFFS2_CACHE_HEADER_MARKER;
}

static uint8_t* flare_jffs2_cache_valid(void) {
  uint8_t* cache = NULL;
  uint32_t* header = (uint32_t*)FLARE_JFFS2_CACHE_BASE;
  if ((header[FLARE_JFFS2_CACHE_HEADER_MARKER_1] ==
       FLARE_JFFS2_CACHE_HEADER_MARKER) &&
      (header[FLARE_JFFS2_CACHE_HEADER_MARKER_2] ==
       FLARE_JFFS2_CACHE_HEADER_MARKER) &&
      (header[FLARE_JFFS2_CACHE_HEADER_VERSION] == FLARE_JFFS2_CACHE_VERSION) &&
      ((header[FLARE_JFFS2_CACHE_HEADER_FLAGS] &
        ~FLARE_JFFS2_CACHE_FLAGS_MASK) == 0)) {
    cache = (uint8_t*)&header[FLARE_JFFS2_CACHE_HEADER_CACHE];
  }
  return cache;
}

static bool flare_jffs2_cache_flag(uint32_t flag) {
  bool valid = false;
  if (flare_jffs2_cache_valid() != NULL) {
    uint32_t* cache = (uint32_t*)FLARE_JFFS2_CACHE_BASE;
    valid = (cache[FLARE_JFFS2_CACHE_HEADER_FLAGS] & flag) != 0 ? true : false;
  }
  return valid;
}

int jffs2_filesystem_mount(bool setup_cache) {
  cwd[0] = '\0';

  if (setup_cache)
    flare_Jffs2Cache_Setup();

  return 0;
}

int jffs2_read_file(const char* name, void* const buffer, uint32_t* size) {
  uint8_t* cache_base = NULL;
  bool cache_crc = false;
  jffs2_error je;
  size_t ssize = *size;
  size_t i = 0;

  while (cwd[i] != '\0') {
    scratch[i] = cwd[i];
    ++i;
  }

  if (*name != '/') {
    scratch[i] = '/';
    ++i;
  }

  while ((*name != '\0') && (i < (sizeof(scratch) - 1))) {
    scratch[i] = *name;
    ++name;
    ++i;
  }

  scratch[i] = '\0';

  cache_base = flare_jffs2_cache_valid();
  if (cache_base != NULL) {
    cache_crc = flare_jffs2_cache_flag(FLARE_JFFS2_CACHE_FLAGS_CRC);
  }

  je = jffs2_boot_read(&jffs2, FLARE_FLASH_FILESYSTEM_BASE,
                       FLARE_FLASH_FILESYSTEM_SIZE, FLARE_FLASH_BLOCK_SIZE,
                       cache_base, cache_crc, scratch, buffer, &ssize);
  *size = ssize;
  if (je != JFFS2_NO_ERROR)
    return je;

  return 0;
}

int jffs2_chdir(const char* path) {
  size_t i = 0;

  while (*path != '\0') {
    if (i >= (sizeof(cwd) - 1))
      return false;
    cwd[i] = *path;
    ++path;
    ++i;
  }

  cwd[i] = '\0';

  return 0;
}
