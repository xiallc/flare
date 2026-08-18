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

/*
 * Flare FATFS wrapper.
 */

#include <stdbool.h>
#include <stdio.h>

#include <driver/fatfs/ff.h>

FATFS fs;

int fatfs_filesystem_mount() {
  FRESULT res;
  res = f_mount(&fs, "", 0);
  if (res != FR_OK) {
    return res;
  }
  return 0;
}

int fatfs_read_file(const char* name, void* const buffer, uint32_t* size) {
  FIL file;
  FRESULT fr;
  uint32_t len = *size;

  fr = f_open(&file, name, FA_READ);
  if (fr != FR_OK) {
    return fr;
  }

  fr = f_read(&file, buffer, len, size);
  if (fr != FR_OK) {
    return fr;
  }

  f_close(&file);
  return 0;
}

int fatfs_chdir(const char* path) {
  FRESULT fr;

  fr = f_chdir(path);
  if (fr != FR_OK) {
    return fr;
  }
  return 0;
}
