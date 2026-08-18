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
 * JFFS2 File reader. This is designed for bootloaders.
 *
 * This code does an ok job. There are cases where a file deep in a directory
 * tree with common path parts may fill the directory cache. If you keep the
 * files the boot loader needs to find in a specific directory and not common
 * names the cache will not fill.
 */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include <machine/endian.h>
#include <string.h>
#include <sys/stat.h>

#include <driver/flash/flash.h>
#include <driver/zlib/tzlib.h>

#include "jffs2-boot.h"
#include "jffs2.h"

#if !defined(JFFS2_TRACE)
#define JFFS2_TRACE 1
#endif

#define JFFS2_TRACE_ON  JFFS2_TRACE
#define JFFS2_TRACE_OFF 0

#define trace_nodes                   JFFS2_TRACE_OFF
#define trace_nodes_clearmarker       JFFS2_TRACE_OFF
#define trace_nodes_blank             JFFS2_TRACE_OFF
#define trace_flash_read              JFFS2_TRACE_OFF
#define trace_buffer_flash_read_cache JFFS2_TRACE_OFF
#define trace_buffer_fill             JFFS2_TRACE_OFF
#define trace_buffer_read             JFFS2_TRACE_OFF
#define trace_find_node               JFFS2_TRACE_OFF
#define trace_remove_found            JFFS2_TRACE_OFF
#define trace_dir_name                JFFS2_TRACE_OFF
#define trace_parent_of               JFFS2_TRACE_OFF
#define trace_process_dir_off         JFFS2_TRACE_OFF
#define trace_process_dir             JFFS2_TRACE_OFF
#define trace_process_found           JFFS2_TRACE_OFF
#define trace_path_found              JFFS2_TRACE_OFF
#define trace_find_path               JFFS2_TRACE_OFF
#define trace_inode_copy              JFFS2_TRACE_OFF
#define trace_inode_copy_inodes       JFFS2_TRACE_OFF
#define trace_inode_copy_inodes_dump  JFFS2_TRACE_OFF
#define trace_inode_copy_inodes_zlib  JFFS2_TRACE_OFF
#define trace_inode_copy_inodes_data  JFFS2_TRACE_OFF
#define trace_inode_copy_stats        JFFS2_TRACE_OFF
#define trace_bad_hdr_crc             JFFS2_TRACE_OFF
#define trace_bad_dir_crc             JFFS2_TRACE_OFF
#define trace_bad_inode_crc           JFFS2_TRACE_OFF
#define trace_boot_read               JFFS2_TRACE_OFF

#if JFFS2_TRACE
#if !defined(jffs2_print_decl)
#define jffs2_print printf
#else
int jffs2_print_decl(const char*, ...) __attribute__((format(printf, 1, 2)));
#define jffs2_print jffs2_print_decl
#endif
#else
#define jffs2_print(...)                                                       \
  while (0) {                                                                  \
  }
#endif

#define target_endian BYTE_ORDER

static inline uint32_t __bswap_32(uint32_t i) {
  const uint8_t* p = (const uint8_t*)&i;
  return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

static inline uint16_t __bswap_16(uint16_t i) {
  const uint8_t* p = (const uint8_t*)&i;
  return (p[0] << 8) | p[1];
}

#undef cpu_to_je16
#undef cpu_to_je32
#undef cpu_to_jemode
#undef je16_to_cpu
#undef je32_to_cpu
#undef jemode_to_cpu

#define t16(x)                                                                 \
  ({                                                                           \
    uint16_t __b = (x);                                                        \
    (target_endian == BYTE_ORDER) ? __b : __bswap_16(__b);                     \
  })
#define t32(x)                                                                 \
  ({                                                                           \
    uint32_t __b = (x);                                                        \
    (target_endian == BYTE_ORDER) ? __b : __bswap_32(__b);                     \
  })

#define cpu_to_je16(x)   ((jint16_t){t16(x)})
#define cpu_to_je32(x)   ((jint32_t){t32(x)})
#define cpu_to_jemode(x) ((jmode_t){t32(x)})

#define je16_to_cpu(x)   (t16((x).v16))
#define je32_to_cpu(x)   (t32((x).v32))
#define jemode_to_cpu(x) (t32((x).m))

#define le16_to_cpu(x) (BYTE_ORDER == LITTLE_ENDIAN ? (x) : __bswap_16(x))
#define le32_to_cpu(x) (BYTE_ORDER == LITTLE_ENDIAN ? (x) : __bswap_32(x))
#define cpu_to_le16(x) (BYTE_ORDER == LITTLE_ENDIAN ? (x) : __bswap_16(x))
#define cpu_to_le32(x) (BYTE_ORDER == LITTLE_ENDIAN ? (x) : __bswap_32(x))

/*
 * File types
 */
#define DT_UNKNOWN 0
#define DT_FIFO    1
#define DT_CHR     2
#define DT_DIR     4
#define DT_BLK     6
#define DT_REG     8
#define DT_LNK     10
#define DT_SOCK    12
#define DT_WHT     14

#define DIRENT_INO(dirent)  ((dirent) != NULL ? je32_to_cpu((dirent)->ino) : 0)
#define DIRENT_PINO(dirent) ((dirent) != NULL ? je32_to_cpu((dirent)->pino) : 0)
#define MASK_N_DIV(x, n)    ((x) & ~((n) - 1))
#define MASK_N_MOD(x, n)    ((x) & ((n) - 1))
#define PAD_n(x, n)                                                            \
  (MASK_N_MOD(x, n) == 0 ? (x) : MASK_N_DIV((x) + ((n) - 1), n))
#define PAD_4(x)   PAD_n(x, 4)
#define PAD_512(x) PAD_n(x, 512)
#define MOD_512(x) MASK_N_MOD(x, 512)

#define JFFS2_EMPTY_SCAN_SIZE (256)

static void jffs2_dump_memory(const char* message, uint32_t base,
                              const void* buffer, size_t size) {
#if JFFS2_TRACE
  const uint8_t* p = buffer;
  size_t c;
  jffs2_print("%s: %zu\n", message, size);
  for (c = 0; c < size; ++c) {
    if ((c % 16) == 0) {
      if (c)
        jffs2_print("\n");
      jffs2_print(" %08zx ", base + c);
    }
    jffs2_print("%02x", p[c]);
    if ((c != (size - 1)) && ((c % 16) == 7))
      jffs2_print("-");
    else
      jffs2_print(" ");
  }
  jffs2_print("\n");
#endif
}

static void jffs2_buffer_reset(jffs2_buffer* buffer) {
  buffer->node_count = 0;
  buffer->offset = 0;
  buffer->out = 0;
  buffer->level = 0;
}

static void jffs2_buffer_init(jffs2_buffer* buffer, uint32_t base,
                              uint32_t size, uint32_t erase_sector_size,
                              uint8_t* cache, bool cache_crc_blocks) {
  buffer->base = base;
  buffer->size = size;
  buffer->erase_sector_size = erase_sector_size;
  if (cache) {
    const size_t bmap_size = JFFS2_BUFFER_CACHE_BITMAP_SIZE(buffer->size);
    buffer->cache_bitmap = (uint32_t*)(cache + 0);
    if (cache_crc_blocks) {
      const size_t cmap_size = JFFS2_BUFFER_CACHE_CRCMAP_SIZE(buffer->size);
      buffer->cache_crcmap = (uint32_t*)(cache + bmap_size);
      buffer->cache = cache + bmap_size + cmap_size;
    } else {
      buffer->cache = cache + bmap_size;
    }
  }
  jffs2_buffer_reset(buffer);
}

static inline size_t jffs2_buffer_size(jffs2_buffer* buffer) {
  return sizeof(buffer->buffer);
}

static inline size_t jffs2_buffer_data_remaining(jffs2_buffer* buffer) {
  if (buffer->out > buffer->level)
    jffs2_print("data-remaining: overflow\n");
  return buffer->level - buffer->out;
}

static inline uint32_t jffs2_buffer_offset(jffs2_buffer* buffer) {
  return buffer->offset + buffer->out;
}

static inline bool jffs2_buffer_erase_sector_boundary(jffs2_buffer* buffer) {
  return (jffs2_buffer_offset(buffer) & (buffer->erase_sector_size - 1)) == 0;
}

static inline void* jffs2_buffer_data(jffs2_buffer* buffer) {
  return buffer->buffer + buffer->out;
}

static inline jffs2_error
jffs2_buffer_flash_data_available(jffs2_buffer* buffer) {
  return jffs2_buffer_offset(buffer) < buffer->size;
}

static inline void jffs2_buffer_set_offset(jffs2_buffer* buffer,
                                           uint32_t offset) {
  if ((offset > buffer->offset) &&
      (offset < (buffer->offset + buffer->level))) {
    buffer->out = offset - buffer->offset;
  } else {
    buffer->offset = offset;
    buffer->out = 0;
    buffer->level = 0;
  }

  if (offset >= buffer->size)
    buffer->offset = buffer->size;
}

static void jffs2_buffer_skip(jffs2_buffer* buffer, size_t size) {
  buffer->out += size;
  if (buffer->out > buffer->level)
    buffer->out = buffer->level;
}

static jffs2_error jffs2_buffer_flash_read(jffs2_buffer* buffer,
                                           uint32_t address, void* buf,
                                           size_t length) {
  flash_error fe;

  if (trace_flash_read)
    jffs2_print("buffer_flash_read: address=%08x length=%zu\n", address,
                length);

  if (buffer->cache) {
    size_t pages;
    uint32_t page;
    uint32_t epage;
    uint32_t boff;
    uint32_t bit;
    uint32_t poff;
    size_t p;

    /*
     * Compute the page and bit off set in the cache bitmap.
     */
    page = address / JFFS2_CACHE_PAGE_SIZE;
    epage = (address + length - 1) / JFFS2_CACHE_PAGE_SIZE;
    pages = epage - page + 1;
    poff = page * JFFS2_CACHE_PAGE_SIZE;
    boff = page / 32;
    bit = page & (32 - 1);

#if 0
    bool trace_buffer_flash_read_cache = poff == 0x02d00000;

    if (trace_buffer_flash_read_cache)
    {
      static bool here;
      if (!here)
      {
        jffs2_print("At the address\n");
        here = true;
      }
    }
#else
#if !defined(trace_buffer_flash_read_cache)
#define trace_buffer_flash_read_cache 1
#endif
#endif

    if (trace_buffer_flash_read_cache)
      jffs2_print("buffer_flash_read: cache: page=%u epage=%u pages=%zu\n",
                  page, epage, pages);

    for (p = 0; p < pages; ++p) {
      if (trace_buffer_flash_read_cache)
        jffs2_print("buffer_flash_read: cache: page=%u bm=%u/%u poff=%08x\n",
                    page, boff, bit, poff);

      if ((buffer->cache_crcmap != NULL) &&
          (buffer->cache_bitmap[boff] & (1 << bit)) != 0) {
        uint32_t crc;

        crc = jffs2_crc32(0, buffer->cache + poff, JFFS2_CACHE_PAGE_SIZE);

        if (crc != buffer->cache_crcmap[page])
          buffer->cache_bitmap[boff] &= ~(1 << bit);
      }

      if ((buffer->cache_bitmap[boff] & (1 << bit)) == 0) {
        if (trace_flash_read)
          jffs2_print("buffer_flash_read: flash read: o=0x%08x s=%u (cache)\n",
                      poff, JFFS2_CACHE_PAGE_SIZE);
        fe = flash_read(buffer->base + poff, buffer->cache + poff,
                        JFFS2_CACHE_PAGE_SIZE);
        if (fe != FLASH_NO_ERROR)
          return JFFS2_FLASH_READ_ERROR;
        buffer->cache_bitmap[boff] |= 1 << bit;
        if (buffer->cache_crcmap != NULL)
          buffer->cache_crcmap[page] =
              jffs2_crc32(0, buffer->cache + poff, JFFS2_CACHE_PAGE_SIZE);
        ++buffer->cache_miss;
      } else {
        ++buffer->cache_hit;
      }

      ++bit;
      if (bit == 32) {
        ++boff;
        bit = 0;
      }

      ++page;
      poff += JFFS2_CACHE_PAGE_SIZE;
    }

    if (trace_buffer_flash_read_cache)
      jffs2_print("buffer_flash_read: copy: buf=%p addr=%08u length=%zu\n", buf,
                  address, length);
    if (length > JFFS2_INODE_BUF_SIZE)
      jffs2_print("buffer_flash_read: length too big: %zu\n", length);

    memcpy(buf, buffer->cache + address, length);
  } else {
    if (trace_flash_read)
      jffs2_print("buffer_flash_read: flash read: o=0x%08x s=%zu\n", address,
                  length);

    fe = flash_read(buffer->base + address, buf, length);
    if (fe != FLASH_NO_ERROR)
      return JFFS2_FLASH_READ_ERROR;
  }

  return JFFS2_NO_ERROR;
}

static jffs2_error jffs2_buffer_fill(jffs2_buffer* buffer, size_t need) {
  size_t remaining;

  if (trace_buffer_fill)
    jffs2_print("buffer_fill: need=%zu\n", need);

  if (need > jffs2_buffer_size(buffer))
    return JFFS2_FILL_TOO_BIG;

  remaining = jffs2_buffer_data_remaining(buffer);

  if (remaining < need) {
    size_t size;
    jffs2_error je;

    if (trace_buffer_fill)
      jffs2_print("buffer_fill: more: off=0x%08x o=%u l=%u r=%zu n=%zu\n",
                  buffer->offset, buffer->out, buffer->level, remaining, need);

    /*
     * If some data has been read out of the buffer and there is still some
     * remaining move it to the bottom of the buffer.
     */
    if (buffer->out) {
      if (remaining) {
        if (trace_buffer_fill)
          jffs2_print("buffer_fill: compact\n");
        if (remaining > jffs2_buffer_size(buffer))
          jffs2_print("buffer_fill: remaining too big: %08zx\n", remaining);
        memmove(buffer->buffer, buffer->buffer + buffer->out, remaining);
      }
      buffer->offset += buffer->out;
      buffer->level = remaining;
      buffer->out = 0;
    }

    /*
     * Read in the part we are missing. Pad the size.
     */
    size = PAD_512(need - buffer->level);

    /*
     * Compacting the buffer may result in the size exceeding the the actual
     * buffer size. That is the level is not zero and we are needing a full
     * buffer size. Trucate the size to not overrun the buffer.
     */
    if ((size + buffer->level) > jffs2_buffer_size(buffer))
      size = jffs2_buffer_size(buffer) - buffer->level;

    /*
     * Do not read past the end of the available flash.
     */
    if ((buffer->offset + buffer->level + size) > buffer->size)
      size = buffer->size - (buffer->offset + buffer->level);

    if (size == 0)
      return JFFS2_FLASH_READ_PAST_END;

    je = jffs2_buffer_flash_read(buffer, buffer->offset + buffer->level,
                                 buffer->buffer + buffer->level, size);
    if (je != JFFS2_NO_ERROR)
      return je;

    buffer->level += size;
  }

  return JFFS2_NO_ERROR;
}

static jffs2_error jffs2_buffer_read(jffs2_buffer* buffer, void* output,
                                     size_t size) {
  uint8_t* p = output;

  if (trace_buffer_read)
    jffs2_print("buffer_read: entry: offset=0x%08x size=%zu\n",
                jffs2_buffer_offset(buffer), size);

  while (size) {
    size_t remaining;
    size_t copy;
    jffs2_error je;

    /*
     * Do not attempt to fill past the buffer size. It is an error to try.
     */
    copy = size < jffs2_buffer_size(buffer) ? size : jffs2_buffer_size(buffer);

    je = jffs2_buffer_fill(buffer, copy);
    if (je != JFFS2_NO_ERROR)
      return je;

    remaining = jffs2_buffer_data_remaining(buffer);
    copy = size < remaining ? size : remaining;

    if (copy > JFFS2_INODE_BUF_SIZE)
      jffs2_print("buffer_read: copy too big: %zu\n", copy);

    memcpy(p, jffs2_buffer_data(buffer), copy);

    jffs2_buffer_skip(buffer, copy);

    p += copy;
    size -= copy;
  }

  if (trace_buffer_read)
    jffs2_print("buffer_read: exit: offset=%08x out=%u level=%u\n",
                jffs2_buffer_offset(buffer), buffer->out, buffer->level);

  return JFFS2_NO_ERROR;
}

static jffs2_error jffs2_buffer_find_node(jffs2_buffer* buffer, uint32_t type) {
  uint32_t fill_size = sizeof(struct jffs2_unknown_node);
  jffs2_error je;

  if (trace_find_node)
    jffs2_print("buffer_find_node: offset=0x%08x type=%04x\n",
                jffs2_buffer_offset(buffer), type);

  je = jffs2_buffer_fill(buffer, fill_size);
  if (je != JFFS2_NO_ERROR)
    return je;

  while (jffs2_buffer_flash_data_available(buffer)) {
    while (jffs2_buffer_data_remaining(buffer) >=
           sizeof(struct jffs2_unknown_node)) {
      struct jffs2_unknown_node* node;
      uint32_t len = 4;

      node = jffs2_buffer_data(buffer);

      if (je16_to_cpu(node->magic) == JFFS2_MAGIC_BITMASK) {
        uint32_t noffset;
        uint32_t ncrc;
        uint16_t ntype;
        uint32_t nlen;
        uint32_t crc;
        jffs2_error je;

        fill_size = sizeof(*node);

        ++buffer->node_count;

        noffset = jffs2_buffer_offset(buffer);
        ntype = je16_to_cpu(node->nodetype);
        ncrc = je32_to_cpu(node->hdr_crc);
        nlen = je32_to_cpu(node->totlen);

        crc = jffs2_crc32(0, node, sizeof(*node) - 4);

        if (trace_nodes)
          jffs2_print("%5d: %08x: type=%04x totlen=%4d icrc=%08x crc=%08x %c\n",
                      buffer->node_count, noffset, ntype, nlen, ncrc, crc,
                      ncrc == crc ? ' ' : 'X');

        if (crc == ncrc) {
          if (ntype == JFFS2_NODETYPE_CLEANMARKER) {
            uint32_t* blank;
            size_t b;

            jffs2_buffer_skip(buffer, sizeof(*node));
            je = jffs2_buffer_fill(buffer, JFFS2_EMPTY_SCAN_SIZE);
            if (je != JFFS2_NO_ERROR)
              return je;

            blank = jffs2_buffer_data(buffer);
            for (b = 0; b < (JFFS2_EMPTY_SCAN_SIZE / sizeof(uint32_t)); ++b) {
              if (*blank != 0xffffffffL)
                break;
              ++blank;
            }

            if (b == (JFFS2_EMPTY_SCAN_SIZE / sizeof(uint32_t))) {
              if (trace_nodes_clearmarker)
                jffs2_print("find_node: cleanmarker @ 0x%08x, skipping %u\n",
                            noffset, buffer->erase_sector_size);
              jffs2_buffer_set_offset(buffer,
                                      noffset + buffer->erase_sector_size);
              break;
            }
          } else if (type == ntype) {
            if (trace_find_node)
              jffs2_print("buffer_find_node: found\n");
            return JFFS2_NO_ERROR;
          } else {
            len = PAD_4(nlen);
          }
        } else {
          if (trace_bad_hdr_crc) {
            jffs2_print(
                "find_node: bad hdr crc @ 0x%08x image=0x%08x calc=0x%08x\n",
                noffset, ncrc, crc);
            jffs2_dump_memory("bad hdr crc", noffset, node, sizeof(*node));
          }
          fill_size = jffs2_buffer_size(buffer);
        }
      } else if ((*((uint32_t*)node) == 0xffffffffL) &&
                 jffs2_buffer_erase_sector_boundary(buffer)) {
        uint32_t* blank;
        size_t b;

        je = jffs2_buffer_fill(buffer, JFFS2_EMPTY_SCAN_SIZE - sizeof(*node));
        if (je != JFFS2_NO_ERROR)
          return je;

        blank = jffs2_buffer_data(buffer);
        for (b = 0; b < (JFFS2_EMPTY_SCAN_SIZE / sizeof(uint32_t)); ++b) {
          if (*blank != 0xffffffffL)
            break;
          ++blank;
        }

        if (*blank == 0xffffffffL) {
          if (trace_nodes_blank)
            jffs2_print("find_node: blank section @ 0x%08x, skipping %u\n",
                        jffs2_buffer_offset(buffer), buffer->erase_sector_size);
          jffs2_buffer_set_offset(buffer, jffs2_buffer_offset(buffer) +
                                              buffer->erase_sector_size);
          break;
        }
      } else {
        fill_size = jffs2_buffer_size(buffer);
      }

      if (len)
        jffs2_buffer_skip(buffer, len);
    }

    je = jffs2_buffer_fill(buffer, fill_size);
    if (je != JFFS2_NO_ERROR)
      return je;
  }

  return JFFS2_FLASH_READ_PAST_END;
}

static void jffs2_inode_print(const char* msg, struct jffs2_raw_inode* inode) {
#if JFFS2_TRACE
  if (inode) {
    jffs2_print(
        "%s: len=%u ino=%u ver=%u offset=%08x csize=%u dsize=%u compr=%d\n",
        msg, je32_to_cpu(inode->totlen), je32_to_cpu(inode->ino),
        je32_to_cpu(inode->version), je32_to_cpu(inode->offset),
        je32_to_cpu(inode->csize), je32_to_cpu(inode->dsize),
        (int)inode->compr);
  }
#endif
}

static void jffs2_dir_push_head(jffs2_dir** head, jffs2_dir* node) {
  node->next = *head;
  *head = node;
}

static jffs2_dir* jffs2_dir_pop_head(jffs2_dir** head) {
  jffs2_dir* node = *head;
  if (node) {
    *head = node->next;
    node->next = NULL;
  }
  return node;
}

static void jffs2_dir_append(jffs2_dir** head, jffs2_dir* node) {
  node->next = NULL;
  if (!*head) {
    *head = node;
  } else {
    jffs2_dir* curr = *head;
    while (curr->next)
      curr = curr->next;
    curr->next = node;
  }
}

static void jffs2_dir_remove(jffs2_dir** head, jffs2_dir* node) {
  jffs2_dir* curr = *head;
  jffs2_dir** prev = head;
  while (curr) {
    if (curr == node) {
      *prev = node->next;
      node->next = NULL;
      break;
    }
    prev = &curr->next;
    curr = curr->next;
  }
}

static int jffs2_dir_count(jffs2_dir* head) {
  int count = 0;
  while (head) {
    ++count;
    head = head->next;
  }
  return count;
}

static size_t jffs2_dir_length(const char* path) {
  const char* p = path;
  while ((*p != '/') && (*p != '\0'))
    ++p;
  return p - path;
}

static void jffs2_dir_print(const char* msg, jffs2_dir* dir) {
#if JFFS2_TRACE
  if (dir) {
    size_t len = jffs2_dir_length(dir->name);
    size_t i;
    jffs2_print("%s: '", msg);
    for (i = 0; i < len; ++i)
      jffs2_print("%c", dir->name[i]);
    jffs2_print("' (%zu) ino=%u pino=%u version=%u\n", len, dir->ino, dir->pino,
                dir->version);
  }
#endif
}

static jffs2_error jffs2_dir_set_path(jffs2_path* jpath, const char* path) {
  int p;
  int n;

  memset(jpath, 0, sizeof(*jpath));

  jpath->path = path;

  p = 0;
  n = 0;
  while (path[p] != '\0') {
    if (path[p] == '/')
      ++p;
    if (path[p] != '\0') {
      if (n == JFFS2_MAX_PATH_DEPTH)
        return JFFS2_PATH_TOO_DEEP;

      jpath->parts[n] = path + p;
      ++n;

      p += jffs2_dir_length(path + p);
    }
  }

  return JFFS2_NO_ERROR;
}

static void jffs2_dir_cache_init(jffs2_dir_cache* cache) {
  size_t n;
  memset(cache, 0, sizeof(*cache));
  cache->found = NULL;
  cache->free = NULL;
  cache->min_free = JFFS2_DIR_CACHE_NODES;
  for (n = 0; n < JFFS2_DIR_CACHE_NODES; ++n)
    jffs2_dir_append(&cache->free, &cache->nodes[n]);
}

static jffs2_dir* jffs2_dir_cache_alloc(jffs2_dir_cache* cache, size_t offset,
                                        const char* name,
                                        struct jffs2_raw_dirent* dir) {
  jffs2_dir* cdir = jffs2_dir_pop_head(&cache->free);
  uint32_t count;

  if (cdir == NULL)
    return NULL;

  count = jffs2_dir_count(cache->free);
  if (count < cache->min_free)
    cache->min_free = count;

  cdir->offset = offset;
  cdir->name = name;
  cdir->ino = je32_to_cpu(dir->ino);
  cdir->pino = je32_to_cpu(dir->pino);
  cdir->version = je32_to_cpu(dir->version);
  cdir->next = NULL;

  return cdir;
}

static jffs2_error jffs2_dir_read_type(jffs2_buffer* buffer, uint32_t offset,
                                       uint8_t* type) {
  struct jffs2_raw_dirent dir;
  jffs2_error je;

  jffs2_buffer_set_offset(buffer, offset);

  je = jffs2_buffer_read(buffer, &dir, sizeof(dir));
  if (je != JFFS2_NO_ERROR)
    return je;

  *type = dir.type;

  return JFFS2_NO_ERROR;
}

static bool jffs2_match_name(const char* n1, const char* n2) {
  /*
   * Match until a path delimiter or end of string. We cannot pointer match
   * because a path part may be repeated in the path string and the directory
   * node in the cache may have been created referencing a different path of
   * path which is the same.
   */
  while ((*n1 != '\0') && (*n1 != '/') && (*n2 != '\0') && (*n2 != '/')) {
    if (*n1 != *n2)
      return false;
    ++n1;
    ++n2;
  }
  /*
   * If either string end if valid delimiters it is a match.
   */
  if (((*n1 == '\0') || (*n1 == '/')) && ((*n2 == '\0') || (*n2 == '/')))
    return true;
  return false;
}

static void jffs2_control_init(jffs2_control* control, uint32_t base,
                               uint32_t size, uint32_t erase_sector_size,
                               uint8_t* buffer_cache, bool cache_crc_blocks) {
  memset(control, 0, sizeof(*control));
  jffs2_buffer_init(&control->buffer, base, size, erase_sector_size,
                    buffer_cache, cache_crc_blocks);
}

static void jffs2_remove_found_of_parent(jffs2_control* control,
                                         uint32_t pino) {
  jffs2_dir* dir;
  bool changed = true;

  while (changed) {
    changed = false;
    dir = control->cache.dir.found;
    while (dir) {
      if (trace_remove_found)
        jffs2_print("remove_found: checking: parent=%u ino=%u pino=%u\n", pino,
                    dir->ino, dir->pino);

      if (dir->pino == pino) {
        if (trace_remove_found)
          jffs2_print("remove_found: remove: parent=%u ino=%u pino=%u\n", pino,
                      dir->ino, dir->pino);

        uint32_t ino = dir->ino;
        if (trace_parent_of)
          jffs2_dir_print("parent_of", dir);
        changed = true;
        jffs2_dir_remove(&control->cache.dir.found, dir);
        jffs2_dir_push_head(&control->cache.dir.free, dir);
        jffs2_remove_found_of_parent(control, ino);
        break;
      }
      dir = dir->next;
    }
  }
}

static jffs2_error jffs2_process_found(jffs2_control* control) {
  const char* part;
  int parent_part = -1;
  uint32_t pino = 1;
  jffs2_dir* dir;
  bool updated = true;
  int p;

  /*
   * Find the end node of the path found discovered so far.
   */
  for (p = 0; p < JFFS2_MAX_PATH_DEPTH; ++p) {
    if (control->path.nodes[p] == NULL)
      break;
    parent_part = p;
    pino = control->path.nodes[p]->ino;
  }

  if (trace_process_found)
    jffs2_print("process_found: parent_part=%d pino=%u\n", parent_part, pino);

  /*
   * Range check. We should not be here if all the nodes in the path have been
   * found.
   */
  if (p >= JFFS2_MAX_PATH_DEPTH)
    return JFFS2_PATH_ALREADY_FOUND;

  /*
   * Search the found list looking for a part that matches the next part of the
   * path.
   */
  while (updated && control->cache.dir.found) {
    updated = false;
    part = control->path.parts[parent_part + 1];
    dir = control->cache.dir.found;
    while (dir) {
      if (trace_process_found) {
        jffs2_print("process_found: checking: pino=%u", pino);
        jffs2_dir_print("", dir);
      }

      /*
       * If the directory's parent inode number matches the parent part's inode
       * number we can process the directory entry. If the directory's name
       * matches the part we have found the directory entry and we can remove
       * any found directory entries with that parent inode number. If the name
       * does not match and parent inode numbers match then that node cannot be
       * the correct one and any nodes that reside under it that may match a
       * part in the path can also be removed.
       */

      if (dir->pino == pino) {
        if (trace_process_found)
          jffs2_dir_print("process_found: matching", dir);

        jffs2_dir_remove(&control->cache.dir.found, dir);

        if (jffs2_match_name(dir->name, part)) {
          jffs2_remove_found_of_parent(control, pino);

          /*
           * Must increment before using.
           */
          ++parent_part;
          control->path.nodes[parent_part] = dir;
          pino = dir->ino;

          if (trace_process_found) {
            jffs2_print("process_found: found: parent_part=%d", parent_part);
            jffs2_dir_print(" ", dir);
          }
        } else {
          jffs2_remove_found_of_parent(control, dir->ino);
          jffs2_dir_push_head(&control->cache.dir.free, dir);
        }

        updated = true;
        break;
      }
      dir = dir->next;
    }
  }

  return JFFS2_NO_ERROR;
}

static jffs2_error jffs2_process_dir(jffs2_control* control, size_t offset,
                                     struct jffs2_raw_dirent* rdir) {
  jffs2_error je;
  int parts;
  int p;

  if (trace_process_dir_off)
    jffs2_print("process_dir: offset=0x%08x\n", (uint32_t)offset);

  for (parts = 0; parts < JFFS2_MAX_PATH_DEPTH; ++parts) {
    if (control->path.nodes[parts] == NULL)
      break;
  }

  if (parts >= JFFS2_MAX_PATH_DEPTH)
    return JFFS2_PATH_ALREADY_FOUND;

  /*
   * The rdir entry has the first character in it. We need to collect the
   * remaining characters in the name from the buffer. The length is valid
   * because the node's crc has been validated.
   */
  je = jffs2_buffer_read(&control->buffer, control->cache.scratch, rdir->nsize);
  if (je != JFFS2_NO_ERROR)
    return je;
  control->cache.scratch[rdir->nsize] = '\0';

  if (trace_dir_name)
    jffs2_print("process_dir: %3d: %s (%u/%u)\n", rdir->nsize,
                control->cache.scratch, je32_to_cpu(rdir->ino),
                je32_to_cpu(rdir->version));

  for (p = parts; p < JFFS2_MAX_PATH_DEPTH; ++p) {

    if (control->path.parts[p] == NULL)
      break;

    if (jffs2_match_name(control->path.parts[p], control->cache.scratch)) {
      jffs2_dir* dir;
      jffs2_dir* node;

      /*
       * Allocate a cached directory entry.
       */
      dir = jffs2_dir_cache_alloc(&control->cache.dir, offset,
                                  control->path.parts[p], rdir);
      if (dir == NULL)
        return JFFS2_DIR_CACHE_FULL;

      if (trace_process_dir)
        jffs2_dir_print("process_dir: alloc", dir);

      /*
       * If the version of the new directory entry is greater than any nodes
       * with the same ino in the path or cache replace it. If the verson is
       * older drop this entry. If the ino does not match any existing nodes
       * add it to the cache's found list.
       */

      for (p = 0; p < JFFS2_MAX_PATH_DEPTH; ++p) {
        node = control->path.nodes[p];
        if (node == NULL)
          break;

        if (dir->ino == node->ino) {
          if (dir->version > node->version) {
            control->path.nodes[p] = dir;
            jffs2_dir_push_head(&control->cache.dir.free, node);
          } else {
            jffs2_dir_push_head(&control->cache.dir.free, dir);
          }
          dir = NULL;
          break;
        }
      }

      if (dir) {
        node = control->cache.dir.found;
        while (node) {
          if (dir->ino == node->ino) {
            if (dir->version > node->version) {
              jffs2_dir_remove(&control->cache.dir.found, node);
              jffs2_dir_push_head(&control->cache.dir.free, node);
            } else {
              jffs2_dir_push_head(&control->cache.dir.free, dir);
              dir = NULL;
            }
            node = NULL;
          } else {
            node = node->next;
          }
        }

        if (dir) {
          if (trace_process_dir)
            jffs2_dir_print("process_dir: found", dir);
          jffs2_dir_append(&control->cache.dir.found, dir);
        }
      }

      return jffs2_process_found(control);
    }
  }

  return JFFS2_NO_ERROR;
}

static bool jffs2_path_found(jffs2_control* control) {
  int part = -1;
  while (part < JFFS2_MAX_PATH_DEPTH) {
    if (control->path.parts[part + 1] == NULL)
      break;
    ++part;
  }
  if (trace_path_found)
    jffs2_print("path_found: part=%d node=%s\n", part,
                control->path.nodes[part] == NULL ? "null" : "present");
  return ((part >= 0) && (control->path.nodes[part] == NULL)) ? false : true;
}

static jffs2_dir* jffs2_path_dir_node(jffs2_control* control) {
  int part = JFFS2_MAX_PATH_DEPTH - 1;
  while (part > 0) {
    if (control->path.nodes[part] != NULL)
      break;
    --part;
  }
  return control->path.nodes[part];
}

static jffs2_error jffs2_find_path(jffs2_control* control, const char* path) {
  jffs2_error je;

  /*
   * Reset the cache and set the path.
   */
  jffs2_buffer_reset(&control->buffer);
  jffs2_dir_cache_init(&control->cache.dir);

  je = jffs2_dir_set_path(&control->path, path);
  if (je != JFFS2_NO_ERROR)
    return je;

  /*
   * Scan the whole image.
   */
  while (jffs2_buffer_flash_data_available(&control->buffer)) {
    struct jffs2_raw_dirent dir;
    uint32_t offset;
    uint32_t crc;
    uint32_t len;
    jffs2_error je;

    /*
     * Find the next directory node. Buffer left pointing to the start of the
     * node.
     */
    je = jffs2_buffer_find_node(&control->buffer, JFFS2_NODETYPE_DIRENT);
    if (je != JFFS2_NO_ERROR) {
      if (je == JFFS2_FLASH_READ_PAST_END)
        break;
      return je;
    }

    offset = jffs2_buffer_offset(&control->buffer);

    je = jffs2_buffer_read(&control->buffer, &dir, sizeof(dir));
    if (je != JFFS2_NO_ERROR)
      return je;

    len = PAD_4(je32_to_cpu(dir.totlen));

    crc = jffs2_crc32(0, &dir, sizeof(dir) - 8);

    if (crc == je32_to_cpu(dir.node_crc)) {
      je = jffs2_process_dir(control, offset, &dir);

      jffs2_buffer_set_offset(&control->buffer, offset + len);

      if (je != JFFS2_NO_ERROR)
        return je;

      if (jffs2_path_found(control))
        break;
    } else {
      if (trace_bad_dir_crc) {
        jffs2_dump_memory("dir", offset, &dir, sizeof(dir));
        jffs2_print("raw_dirent: bad dirent crc @ 0x%08x\n", offset);
      }
    }

    jffs2_buffer_set_offset(&control->buffer, offset + len);
  }

  if (trace_find_path) {
    jffs2_print("find_path: ");
    jffs2_print_path(control);
    jffs2_print(": nodes-used=%zu%% (%zu/%zu)\n",
                ((JFFS2_DIR_CACHE_NODES - control->cache.dir.min_free) * 100) /
                    JFFS2_DIR_CACHE_NODES,
                JFFS2_DIR_CACHE_NODES - control->cache.dir.min_free,
                JFFS2_DIR_CACHE_NODES);
  }

  return jffs2_path_found(control) ? JFFS2_NO_ERROR : JFFS2_NOT_FOUND;
}

static jffs2_error jffs2_inode_copy(jffs2_control* control, uint32_t ino,
                                    uint8_t* buffer, size_t* size) {
  uint32_t inode_count = 0;
  uint32_t isize_max = 0;
  uint32_t csize_total = 0;
  uint32_t dsize_total = 0;

  if (trace_inode_copy)
    jffs2_print("inodes_copy: ino=%u buffer=%p size=%zu\n", ino, buffer, *size);

  jffs2_buffer_reset(&control->buffer);

  /*
   * Find the inode.
   */
  while (jffs2_buffer_flash_data_available(&control->buffer)) {
    struct jffs2_raw_inode inode;
    uint32_t offset;
    uint32_t crc;
    uint32_t len;
    jffs2_error je;

    /*
     * Find the next inode. Buffer left pointing to the start of the node.
     */
    je = jffs2_buffer_find_node(&control->buffer, JFFS2_NODETYPE_INODE);
    if (je != JFFS2_NO_ERROR) {
      if (je == JFFS2_FLASH_READ_PAST_END)
        break;
      return je;
    }

    /*
     * Remember the offset so we can set the position to the offset plus the
     * aligned (padded) total len in the header. The total length is valid
     * because the header CRC is valid.
     */
    offset = jffs2_buffer_offset(&control->buffer);

    /*
     * Get a local copy. This avoids any buffering boundary issues at the cost
     * of a little stack space being used.
     */
    je = jffs2_buffer_read(&control->buffer, &inode, sizeof(inode));
    if (je != JFFS2_NO_ERROR)
      return je;

    len = PAD_4(je32_to_cpu(inode.totlen));

    crc = jffs2_crc32(0, &inode, sizeof(inode) - 8);

    if (crc == je32_to_cpu(inode.node_crc)) {
      const uint32_t nino = je32_to_cpu(inode.ino);
      if (nino == ino) {
        const uint32_t isize = je32_to_cpu(inode.isize);
        const uint32_t icsize = je32_to_cpu(inode.csize);
        uint32_t idsize = je32_to_cpu(inode.dsize);
        const uint32_t ioffset = je32_to_cpu(inode.offset);
        const uint32_t doffset = jffs2_buffer_offset(&control->buffer);
        uint32_t bsize;
        uLongf dsize;
        int ze;

        ++inode_count;
        csize_total += icsize;
        dsize_total += idsize;

        if (trace_inode_copy_inodes) {
          jffs2_inode_print("copy_inodes", &inode);
          if (trace_inode_copy_inodes_dump)
            jffs2_dump_memory("inode", offset, &inode, sizeof(inode));
        }

        if (isize_max < isize)
          isize_max = isize;

        /*
         * The user may have requested only part of a file so only return the
         * part they requested. If the data is outside the buffer requested
         * there is no need to process the inode.
         */

        if (ioffset < *size) {
          if ((ioffset + idsize) > *size)
            idsize = *size - ioffset;

          dsize = idsize;

          switch (inode.compr) {
          case JFFS2_COMPR_ZLIB:
          case JFFS2_COMPR_NONE:
            bsize = inode.compr == JFFS2_COMPR_ZLIB ? icsize : idsize;
            if (bsize > sizeof(control->cache.scratch)) {
              jffs2_print("inode: bad inode bsize @ 0x%08x : bsize:%u\n",
                          offset, bsize);
              jffs2_dump_memory("inode", offset, &inode, sizeof(inode));
              jffs2_dump_memory("data", doffset, control->cache.scratch, bsize);
              return JFFS2_INODE_DATA_TOO_BIG;
            }
            je = jffs2_buffer_read(&control->buffer, control->cache.scratch,
                                   bsize);
            if (je != JFFS2_NO_ERROR)
              return je;
            crc = jffs2_crc32(0, control->cache.scratch, bsize);
            if (crc != je32_to_cpu(inode.data_crc)) {
              if (trace_bad_inode_crc) {
                jffs2_print("inode: bad inode data crc @ 0x%08x\n", offset);
                jffs2_dump_memory("inode", offset, &inode, sizeof(inode));
                jffs2_dump_memory("data", doffset, control->cache.scratch,
                                  bsize);
                return JFFS2_INVALID_CRC;
              }
            }
            break;
          case JFFS2_COMPR_ZERO:
          default:
            break;
          }

          switch (inode.compr) {
          case JFFS2_COMPR_ZLIB:
            if (idsize > sizeof(control->cache.scratch))
              return JFFS2_INODE_DATA_TOO_BIG;
            if (trace_inode_copy_inodes_zlib)
              jffs2_dump_memory("inode zlib", doffset, control->cache.scratch,
                                icsize);
            ze = uncompress((buffer + ioffset), &dsize,
                            (uint8_t*)control->cache.scratch, icsize);
            if (trace_inode_copy_inodes_data)
              jffs2_dump_memory("inode data", (uintptr_t)(buffer + ioffset),
                                buffer + ioffset, dsize);
            if ((ze != Z_OK) && (ze != Z_BUF_ERROR))
              return JFFS2_ZLIB_ERROR;
            if (dsize != idsize)
              return JFFS2_ZLIB_BAD_SIZE;
            break;
          case JFFS2_COMPR_NONE:
            if (idsize > sizeof(control->cache.scratch))
              return JFFS2_INODE_DATA_TOO_BIG;
            memcpy(buffer + ioffset, control->cache.scratch, idsize);
            break;
          case JFFS2_COMPR_ZERO:
            if (idsize > sizeof(control->cache.scratch))
              return JFFS2_INODE_DATA_TOO_BIG;
            memset(buffer + ioffset, 0, idsize);
            break;
          default:
            return JFFS2_INVALID_COMPR;
            break;
          }
        }
      }
    } else {
      if (trace_bad_inode_crc) {
        jffs2_print("inode: bad inode crc @ 0x%08x\n", offset);
        jffs2_dump_memory("inode", offset, &inode, sizeof(inode));
        return JFFS2_INVALID_CRC;
      }
    }

    jffs2_buffer_set_offset(&control->buffer, offset + len);
  }

  if (*size > isize_max)
    *size = isize_max;

  if (trace_inode_copy_stats)
    jffs2_print("find_inodes: inodes=%u isize=%u csize=%u compr=%u%%\n",
                inode_count, isize_max, csize_total,
                100 - ((csize_total * 100) / dsize_total));

  return JFFS2_NO_ERROR;
}

jffs2_error jffs2_boot_read(jffs2_control* control, uint32_t flash_base,
                            uint32_t flash_size,
                            uint32_t flash_erase_sector_size,
                            uint8_t* buffer_cache, bool cache_crc_blocks,
                            const char* file, void* dest, size_t* size) {
  jffs2_dir* dir;
  uint8_t dt = 0;
  jffs2_error je;

  if (trace_boot_read)
    jffs2_print("boot_read: file=%s dest=%p size=%zu flash-size=%u\n", file,
                dest, *size, flash_size);

  jffs2_control_init(control, flash_base, flash_size, flash_erase_sector_size,
                     buffer_cache, cache_crc_blocks);

  je = jffs2_find_path(control, file);
  if (je != JFFS2_NO_ERROR)
    return je;

  dir = jffs2_path_dir_node(control);
  if (dir == NULL)
    return JFFS2_BAD_PATH_NODES;

  if (trace_boot_read) {
    jffs2_print("boot_read: path: ");
    jffs2_print_path(control);
    jffs2_print(": ino=%u offset=%08x\n", dir->ino, dir->offset);
  }

  je = jffs2_dir_read_type(&control->buffer, dir->offset, &dt);
  if (je != JFFS2_NO_ERROR)
    return je;

  if (dt != DT_REG)
    return JFFS2_NOT_A_FILE;

  je = jffs2_inode_copy(control, dir->ino, dest, size);
  if (je != JFFS2_NO_ERROR)
    return je;

  if (trace_boot_read)
    jffs2_print("boot_read: cache: hit:%u miss:%u\n", control->buffer.cache_hit,
                control->buffer.cache_miss);

  return JFFS2_NO_ERROR;
}

void jffs2_print_path(jffs2_control* control) {
  int p;
  for (p = 0; p < JFFS2_MAX_PATH_DEPTH; ++p) {
    jffs2_dir* dir = control->path.nodes[p];
    int c;
    if (dir == NULL)
      break;
    jffs2_print("/");
    c = 0;
    while ((dir->name[c] != '/') && (dir->name[c] != '\0')) {
      jffs2_print("%c", dir->name[c]);
      ++c;
    }
  }
}
