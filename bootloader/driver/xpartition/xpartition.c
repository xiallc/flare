/*
 * Copyright 2026 Contemporary Software
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

#include <sleep.h>
#include <stdio.h>
#include <string.h>

#include <board.h>
#include <datasafe.h>

#include <fs/fatfs-filesystem.h>

#include <driver/fatfs/ff.h>
#include <driver/fatfs/sdwrapper.h>
#include <driver/flash/flash.h>
#include <driver/sdhci/sdhci.h>

#include "xpartition.h"

#define XPART_PARTS_MAX 8

#define XPART_NAME_LEN 12

#define XPART_DEBUG_NONE  0
#define XPART_DEBUG_TRACE 1

#define XPART_DEBUG_MODE XPART_DEBUG_NONE

struct boot_header {
  uint32_t dummy[8];
  uint32_t width_hdr;
  uint32_t signature;
  uint32_t key_src;
  uint32_t padding[27];
  uint32_t img_hdr_off;
  uint32_t part_hdr_off;
};

struct image_header_table {
  uint32_t version;
  uint32_t part_count;
  uint32_t part_hdr_addr;
  uint32_t reserved_0;
  uint32_t auth_cert_off;
  uint32_t part_present_dev;
  uint32_t reserved_1[9];
  uint32_t csum;
};

struct image_header {
  uint32_t next_off;
  uint32_t part_hdr_off;
  uint32_t reserved;
  uint32_t part_count;
  uint32_t name[XPART_NAME_LEN];
};

struct part_header {
  uint32_t enc_len;
  uint32_t unenc_len;
  uint32_t total_len;
  uint32_t next_part_off;
  uint32_t dest_exe_addr_lo;
  uint32_t dest_exe_addr_hi;
  uint32_t dest_load_addr_lo;
  uint32_t dest_load_addr_hi;
  uint32_t data_off;
  uint32_t attr;
  uint32_t sec_count;
  uint32_t csum_off;
  uint32_t image_hdr_off;
  uint32_t auth_cert_off;
  uint32_t id;
  uint32_t hdr_csum;
};

struct image {
  struct image_header_table hdr;
  struct image_header parts[XPART_PARTS_MAX];
};

static int open_boot_bin() {
  int status;
  int bootmode = board_bootmode();

  if (bootmode == FLARE_DS_BOOTMODE_QSPI) {
    const char* label;
    status = flash_open(&label);
    if (status != 0) {
      return status;
    }
  } else if (bootmode == FLARE_DS_BOOTMODE_SD_CARD || true) {
    status = sdhci_open(SDHCI_CTLR_SD);
    if (status != 0) {
      return status;
    }

    fatfs_set_sdhci_ctlr(SDHCI_CTLR_SD);
    status = fatfs_filesystem_mount();
    if (status != 0) {
      return status;
    }
  } else {
    return 1;
  }

  return 0;
}

static int read_boot_bin(uint32_t offset, void* buf, size_t len) {
  int status;
  int bootmode = board_bootmode();

  if (bootmode == FLARE_DS_BOOTMODE_QSPI) {
    status = flash_read(offset, &buf, len);
    if (status != 0) {
      return status;
    }
  } else if (bootmode == FLARE_DS_BOOTMODE_SD_CARD) {
    FIL file;
    uint32_t read_len;

    status = f_open(&file, "/BOOT.BIN", FA_READ);
    if (status != FR_OK) {
      return status;
    }

    status = f_lseek(&file, offset);
    if (status != FR_OK) {
      return status;
    }

    status = f_read(&file, buf, len, &read_len);
    if (status != FR_OK) {
      return status;
    }
    if (read_len != len) {
      return 1;
    }

    status = f_close(&file);
    if (status != FR_OK) {
      return status;
    }
  } else {
    printf("Unsupported boot mode, can't load BOOT.BIN partitions\n");
    return 1;
  }

  return 0;
}

void xpart_load_parts() {
  int status = 0;
  int partition;
  int partition_count;
  struct boot_header boot_hdr;
  struct image img;
  char part_name[XPART_NAME_LEN * sizeof(uint32_t)] = {0};

  status = open_boot_bin();
  if (status != 0) {
    printf("Unable to open BOOT.BIN, can't load partitions\n");
    return;
  }

  status = read_boot_bin(0, &boot_hdr, sizeof(struct boot_header));
  if (status != 0) {
    printf("Unable to read BOOT.BIN, can't load partitions\n");
    return;
  }

  if (boot_hdr.width_hdr != XLNX_HDR_WIDTH_DETECT ||
      boot_hdr.signature != XLNX_HDR_SIGNATURE) {
    printf("Unable to read BOOT.BIN, bad values\n");
    return;
  }

  if (boot_hdr.key_src != XLNX_HDR_KEY_SRC_UNENC) {
    printf("Unable to read BOOT.BIN, encryption not supported\n");
    return;
  }

  status = read_boot_bin(boot_hdr.img_hdr_off, &img, sizeof(struct image));
  if (status != 0) {
    printf("Unable to read BOOT.BIN, can't load partitions\n");
    return;
  }

  partition_count = img.hdr.part_count;

  if (partition_count < 2) {
    printf("No valid BOOT.BIN partitions found\n");
    return;
  }

  /* Start partitions at 1, partition 0 is flare itself. */
  for (partition = 1; partition < partition_count; partition++) {
    struct part_header part_hdr;
    struct part_header sec_hdr;

    status = read_boot_bin(img.parts[partition].part_hdr_off * sizeof(uint32_t),
                           &part_hdr, sizeof(struct part_header));
    if (status != 0) {
      printf("Unable to read BOOT.BIN, can't load partitions\n");
      return;
    }
    memcpy(&sec_hdr, &part_hdr, sizeof(struct part_header));

    if (((part_hdr.attr & XLNX_PART_HDR_PART_OWNER_MASK) !=
         XLNX_PART_HDR_PART_OWNER_FSBL) ||
        ((part_hdr.attr & XLNX_PART_HDR_DEST_CPU_MASK) !=
         XLNX_PART_HDR_DEST_CPU_A53_0) ||
        (part_hdr.attr & XLNX_PART_HDR_ENCRYPTED_MASK)) {
      continue;
    }

    if (XPART_DEBUG_MODE) {
      for (int i = 0; i < XPART_NAME_LEN; i++) {
        part_name[(i * 4) + 0] = (img.parts[partition].name[i] >> 24) & 0xFF;
        part_name[(i * 4) + 1] = (img.parts[partition].name[i] >> 16) & 0xFF;
        part_name[(i * 4) + 2] = (img.parts[partition].name[i] >> 8) & 0xFF;
        part_name[(i * 4) + 3] = (img.parts[partition].name[i] >> 0) & 0xFF;
      }

      printf("Loading partition (%s)\n", part_name);
    }

    for (uint32_t sec = 0; sec < part_hdr.sec_count; sec++) {
      if (XPART_DEBUG_MODE) {
        printf("  Section at 0x%08x (%ld bytes)\n", sec_hdr.dest_load_addr_lo,
               sec_hdr.total_len * sizeof(uint32_t));
      }

      status = read_boot_bin(sec_hdr.data_off * sizeof(uint32_t),
                             (void*)(uint64_t)sec_hdr.dest_load_addr_lo,
                             sec_hdr.total_len * sizeof(uint32_t));
      if (status != 0) {
        printf("Unable to read BOOT.BIN, can't load section\n");
        return;
      }

      status = read_boot_bin(sec_hdr.next_part_off * sizeof(uint32_t), &sec_hdr,
                             sizeof(struct part_header));
      if (status != 0) {
        printf("Unable to read BOOT.BIN, can't load section header\n");
        return;
      }
    }
  }
}
