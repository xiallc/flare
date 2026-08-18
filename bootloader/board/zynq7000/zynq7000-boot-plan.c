/*
 * Copyright 2025 Contemporary Software
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

#include <stdio.h>

#include <board.h>
#include <datasafe.h>
#include <flare-boot.h>
#include <sleep.h>
#include <user-break.h>

#include <fs/fatfs-filesystem.h>

#include <driver/flash/flash.h>
#include <driver/sdhci/sdhci.h>
#include <driver/uart/console.h>

#define JTAG_BOOT_DEFAULT FLARE_DS_BOOTMODE_QSPI
#if JTAG_BOOT_DEFAULT == FLARE_DS_BOOTMODE_QSPI
#define JTAG_BOOT_PRIMARY   "QSPI"
#define JTAG_BOOT_SECONDARY "SD  "
#else
#define JTAG_BOOT_PRIMARY   "SD  "
#define JTAG_BOOT_SECONDARY "QSPI"
#endif

static int open_qspi() {
  const char* label;
  flash_error err = flash_open(&label);
  return err;
}

static int mount_qspi_fatfs() {
  return flare_filesystem_mount(FILESYSTEM_QSPI_JFFS2);
}

static int open_sd() {
  return sdhci_open(SDHCI_CTLR_SD);
}

static int mount_sd_fatfs() {
  return flare_filesystem_mount(FILESYSTEM_SD_FATFS);
}

static void qspi_boot(flare_boot_plan* bp) {
  bp->opens[0] = &open_qspi;
  bp->opens_name[0] = "QSPI";

  for (int i = 1; i < FLARE_STAGE_FUNC_MAX; i++) {
    bp->opens[i] = NULL;
    bp->opens_name[i] = NULL;
  }

  bp->mounts[0] = &mount_qspi_fatfs;
  bp->mounts_name[0] = "JFFS2";

  for (int i = 1; i < FLARE_STAGE_FUNC_MAX; i++) {
    bp->mounts[i] = NULL;
    bp->mounts_name[i] = NULL;
  }

  bp->boot_fs = FILESYSTEM_QSPI_JFFS2;
  bp->bs_name = "flare-0";
}

static void sdhci_boot(flare_boot_plan* bp) {
  bp->opens[0] = &open_sd;
  bp->opens_name[0] = "SD";

  for (int i = 1; i < FLARE_STAGE_FUNC_MAX; i++) {
    bp->opens[i] = NULL;
    bp->opens_name[i] = NULL;
  }

  bp->mounts[0] = &mount_sd_fatfs;
  bp->mounts_name[0] = "FATFS";

  for (int i = 1; i < FLARE_STAGE_FUNC_MAX; i++) {
    bp->mounts[i] = NULL;
    bp->mounts_name[i] = NULL;
  }

  bp->boot_fs = FILESYSTEM_SD_FATFS;
  bp->bs_name = "flare-0";
}

static void jtag_boot(flare_boot_plan* bp) {
  bool pressed;

  printf("   JTAG boot: booting from %s in %1d   (^c for %s)"
         "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b",
         JTAG_BOOT_PRIMARY, FLARE_BOOT_DELAY, JTAG_BOOT_SECONDARY);
  pressed = user_break(FLARE_BOOT_DELAY, '\x0');
  printf("    \b\b\b\b\b\b\b\b\b\b\b\b\b\b%s                       \n",
         pressed ? JTAG_BOOT_SECONDARY : JTAG_BOOT_PRIMARY);

  if ((pressed && JTAG_BOOT_DEFAULT != FLARE_DS_BOOTMODE_QSPI) ||
      (!pressed && JTAG_BOOT_DEFAULT == FLARE_DS_BOOTMODE_QSPI)) {
    flare_datasafe_set_bootmode(FLARE_DS_BOOTMODE_JTAG |
                                FLARE_DS_BOOTMODE_QSPI);
    qspi_boot(bp);
  } else {
    flare_datasafe_set_bootmode(FLARE_DS_BOOTMODE_JTAG |
                                FLARE_DS_BOOTMODE_SD_CARD);
    sdhci_boot(bp);
  }
}

void flare_get_boot_plan(flare_boot_plan* bp) {
  uint32_t bootmode = board_bootmode();

  if (bootmode == FLARE_DS_BOOTMODE_QSPI) {
    qspi_boot(bp);
  } else if (bootmode == FLARE_DS_BOOTMODE_SD_CARD) {
    sdhci_boot(bp);
  } else if (bootmode == FLARE_DS_BOOTMODE_JTAG) {
    jtag_boot(bp);
  }
}
