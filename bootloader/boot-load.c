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
 * Boot Load.
 */

#include <stdio.h>
#include <string.h>

#include <boot-load.h>
#include <flare-boot.h>
#include <flash-map.h>
#include <uboot.h>

#include <fs/boot-filesystem.h>

#include <driver/crc/crc.h>
#include <driver/zlib/tzlib.h>

#define MASK_N_DIV(x, n) ((x) & ~((n) - 1))
#define MASK_N_MOD(x, n) ((x) & ((n) - 1))
#define PAD_n(x, n)                                                            \
  (MASK_N_MOD(x, n) == 0 ? (x) : MASK_N_DIV((x) + ((n) - 1), n))
#define PAD_4(x) PAD_n(x, 4)

static inline uintptr_t swap_end_32(uint32_t val) {
  return (((0xFF000000 & val) >> 24) | ((0x00FF0000 & val) >> 8) |
          ((0x0000FF00 & val) << 8) | ((0x000000FF & val) << 24));
}

bool load_uboot_image(uint8_t* image, size_t size, uint32_t* entry_point) {
  uint8_t* loadTo;
  uint8_t compression;
  char name[UBOOT_NAME_LEN + 1] = {0};
  uint32_t magic_num;

  loadTo = (uint8_t*)swap_end_32(*(uint32_t*)(image + UBOOT_LOAD_ADDR_OFF));
  size = (size_t)swap_end_32(*(uint32_t*)(image + UBOOT_DATA_SIZE_OFF));
  *entry_point = swap_end_32(*(uint32_t*)(image + UBOOT_ENTRY_PT_OFF));
  compression = *(image + UBOOT_COMP_OFF);
  magic_num = swap_end_32(*(uint32_t*)(image + UBOOT_MAGIC_NUM_OFF));

  memcpy(name, image + UBOOT_IMAGE_NAME_OFF, UBOOT_NAME_LEN);

  if (magic_num != UBOOT_MAGIC_NUMBER) {
    printf("Bad magic number\nExpected: %08x\nFound: %08x\n",
           UBOOT_MAGIC_NUMBER, magic_num);
    return false;
  }

  if (compression != UBOOT_COMPRESSION_NONE &&
      compression != UBOOT_COMPRESSION_GZIP) {
    printf("Invalid compression format (%d)\n", compression);
    return false;
  }

  printf("       Loading: U-Boot Image: %s\n", name);
  printf("            To: %p\n", loadTo);
  printf("          Size: 0x%08zx\n", size);
  printf("    Compressed: %d\n", compression);
  printf("   Entry point: 0x%08x\n", *entry_point);

  image = image + UBOOT_DATA_OFF;

  if (compression == UBOOT_COMPRESSION_GZIP) {
    size_t offset;
    uint32_t dsize;
    int ze;

    /*
     * We only support a gzip format file:
     *
     *    http://www.gzip.org/zlib/rfc-gzip.html
     *
     * Check the header.
     */

    if ((image[0] != 0x1f) || (image[1] != 0x8b) || (image[2] != 0x08)) {
      printf("error: invalid %s header\n", name);
      return false;
    }

    offset = 10;

    /* FLG.FEXTRA */
    if ((image[3] & (1 << 2)) != 0)
      offset += ((image[offset] << 8) | image[offset + 1]) + 2;
    /* FLG.FNAME */
    if ((image[3] & (1 << 3)) != 0) {
      while (image[offset] != 0) {
        if (offset >= size) {
          printf("error: invalid %s header: fname\n", name);
          return false;
        }
        ++offset;
      }
      ++offset;
    }
    /* FLG.FCOMMENT */
    if ((image[3] & (1 << 4)) != 0) {
      while (image[offset] != 0) {
        if (offset >= size) {
          printf("error: invalid %s header: fcomment\n", name);
          return false;
        }
        ++offset;
      }
      ++offset;
    }
    /* FLG.FHCRC */
    if ((image[3] & (1 << 1)) != 0)
      offset += 2;

    dsize = FLARE_EXECUTABLE_SIZE - PAD_4(size);

    ze = raw_uncompress(loadTo + PAD_4(size), &dsize, image + offset,
                        size - offset - 8);
    if (ze != Z_OK) {
      printf("error: %s uncompress failure: %d\n", name, ze);
      return false;
    }

    memmove(loadTo, loadTo + PAD_4(size), dsize);
  } else {
    memmove(loadTo, (const void*)image, size);
  }

  return true;
}

bool load_exe(const boot_script* const script, uint32_t* entry_point) {
  bool csum_valid = boot_script_checksum_valid(script);
  const char* const error = "\b: error:";
  size_t i;
  CRC32 crc;
  int rc;
  uint32_t length = FLARE_EXECUTABLE_SIZE;
  uint8_t checksum[CRC_CHECKSUM_SIZE];

  printf("  Executable: %s", script->path);
  if (script->path[strlen(script->path) - 1] != '/') {
    printf("/");
  }
  printf("%s ", script->executable);

  rc = flare_chdir(script->fs, script->path);
  if (rc != 0) {
    printf("%s chdir: %d\n", error, rc);
    return false;
  }

  rc = flare_read_file(script->fs, script->executable,
                       (char*)FLARE_IMAGE_STAGE_ADDR, &length);
  if (rc != 0) {
    printf("%s read: %d\n", error, rc);
    return false;
  }

  crc32_clear(&crc);
  crc32_update(&crc, (const void*)FLARE_IMAGE_STAGE_ADDR, length);
  crc32_str(&crc, checksum);

  printf("%s(CRC32: ", csum_valid ? "" : "[NOT CHECKED] ");
  for (i = 0; i < CRC_CHECKSUM_SIZE; ++i)
    printf("%c", checksum[i]);
  printf(")\n");

  if (csum_valid) {
    for (i = 0; i < CRC_CHECKSUM_SIZE; ++i) {
      if (script->checksum[i] != checksum[i]) {
        printf("error: invalid checksum\n");
        return false;
      }
    }
  }

  return load_uboot_image((uint8_t*)FLARE_IMAGE_STAGE_ADDR, length,
                          entry_point);
}
