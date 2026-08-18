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

#include <fs/boot-filesystem.h>
#include <fs/fatfs-filesystem.h>
#include <fs/jffs2-filesystem.h>

#include <driver/fatfs/sdwrapper.h>
#include <driver/sdhci/sdhci.h>

int flare_filesystem_mount(flare_fs fs) {
  if (fs == FILESYSTEM_QSPI_JFFS2) {
    return jffs2_filesystem_mount(true);
  } else if (fs == FILESYSTEM_SD_FATFS) {
    fatfs_set_sdhci_ctlr(SDHCI_CTLR_SD);
    return fatfs_filesystem_mount();
  } else if (fs == FILESYSTEM_EMMC_FATFS) {
    fatfs_set_sdhci_ctlr(SDHCI_CTLR_EMMC);
    return fatfs_filesystem_mount();
  }
  return 0;
}

int flare_read_file(flare_fs fs, const char* name, void* const buffer,
                    uint32_t* size) {
  if (fs == FILESYSTEM_QSPI_JFFS2) {
    return jffs2_read_file(name, buffer, size);
  } else if (fs == FILESYSTEM_SD_FATFS || fs == FILESYSTEM_EMMC_FATFS) {
    return fatfs_read_file(name, buffer, size);
  }
  return 0;
}

int flare_chdir(flare_fs fs, const char* path) {
  if (fs == FILESYSTEM_QSPI_JFFS2) {
    return jffs2_chdir(path);
  } else if (fs == FILESYSTEM_SD_FATFS || fs == FILESYSTEM_EMMC_FATFS) {
    return fatfs_chdir(path);
  }
  return 0;
}
