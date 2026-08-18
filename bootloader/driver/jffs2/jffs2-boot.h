/*
 * Copyright (c) 2014, Chris Johns <chrisj@rtems.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * JFFS2 Boot functions.
 */

#if !defined(_JFFS2_BOOT_H_)
#define _JFFS2_BOOT_H_

#include <stdbool.h>
#include <stdint.h>

#define JFFS2_MAX_PATH_DEPTH   (32)
#define JFFS2_CONTROL_BUF_SIZE (2 * 1024)
#define JFFS2_DIR_CACHE_SIZE   (2 * 1024)
#define JFFS2_DIR_CACHE_NODES  (JFFS2_DIR_CACHE_SIZE / sizeof(jffs2_dir))
#define JFFS2_INODE_BUF_SIZE   (4 * 1024)
#define JFFS2_DIR_MAX_NAME_LEN (254)
#define JFFS2_CACHE_PAGE_SIZE  (2048)

/*
 * A cache of the flags, crcs and image can be provided. This holds the
 * contents of the flash read into memory so far. If you are reading a number
 * of files the cache improves performance.
 *
 * This macro computes the amount of memory you need for the cache.
 */
#define JFFS2_BUFFER_CACHE_PAGES(size) (((size) * 1UL) / JFFS2_CACHE_PAGE_SIZE)
#define JFFS2_BUFFER_CACHE_BITMAP_SIZE(size)                                   \
  (JFFS2_BUFFER_CACHE_PAGES(size) / 8UL)
#define JFFS2_BUFFER_CACHE_CRCMAP_SIZE(size)                                   \
  (JFFS2_BUFFER_CACHE_PAGES(size) * sizeof(uint32_t))
#define JFFS2_BUFFER_CACHE_SIZE(size, crc)                                     \
  ((size) + JFFS2_BUFFER_CACHE_BITMAP_SIZE(size) +                             \
   (crc ? JFFS2_BUFFER_CACHE_CRCMAP_SIZE(size) : 0))

struct jffs2_dir_;
typedef struct jffs2_dir_ jffs2_dir;

struct jffs2_dir_ {
  uint32_t offset;
  const char* name;
  uint32_t ino;
  uint32_t pino;
  uint32_t version;
  jffs2_dir* next;
};

typedef struct {
  jffs2_dir* found;
  jffs2_dir* free;
  jffs2_dir nodes[JFFS2_DIR_CACHE_NODES];
  uint32_t min_free;
} jffs2_dir_cache;

typedef struct {
  const char* path;
  const char* parts[JFFS2_MAX_PATH_DEPTH];
  jffs2_dir* nodes[JFFS2_MAX_PATH_DEPTH];
} jffs2_path;

typedef struct {
  int node_count;
  uint32_t base;
  uint32_t size;
  uint32_t erase_sector_size;
  uint32_t offset;
  uint32_t out;
  uint32_t level;
  uint8_t buffer[JFFS2_CONTROL_BUF_SIZE];
  uint32_t* cache_bitmap;
  uint32_t* cache_crcmap;
  uint8_t* cache;
  uint32_t cache_hit;
  uint32_t cache_miss;
} jffs2_buffer;

typedef struct {
  jffs2_dir_cache dir;
  char scratch[JFFS2_INODE_BUF_SIZE];
} jffs2_cache;

typedef struct {
  jffs2_path path;
  jffs2_cache cache;
  jffs2_buffer buffer;
} jffs2_control;

typedef enum {
  JFFS2_NO_ERROR = 0,
  JFFS2_NOT_FOUND,
  JFFS2_NOT_A_FILE,
  JFFS2_INVALID_CRC,
  JFFS2_PATH_TOO_DEEP,
  JFFS2_DIR_CACHE_FULL,
  JFFS2_INODE_CACHE_FULL,
  JFFS2_PATH_ALREADY_FOUND,
  JFFS2_BAD_PATH_NODES,
  JFFS2_TOO_LONG,
  JFFS2_INODE_DATA_TOO_BIG,
  JFFS2_INVALID_COMPR,
  JFFS2_ZLIB_ERROR,
  JFFS2_ZLIB_BAD_SIZE,
  JFFS2_FILL_TOO_BIG,
  JFFS2_FLASH_READ_ERROR,
  JFFS2_FLASH_READ_PAST_END
} jffs2_error;

jffs2_error jffs2_boot_read(jffs2_control* control, uint32_t flash_base,
                            uint32_t flash_size,
                            uint32_t flash_erase_sector_size,
                            uint8_t* buffer_cache, bool cache_crc_blocks,
                            const char* file, void* dest, size_t* size);

void jffs2_print_path(jffs2_control* control);

uint32_t jffs2_crc32(uint32_t val, const void* ss, int len);

#endif
