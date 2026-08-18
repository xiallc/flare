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
 * FSBL factory configruation.
 */

#include <stdio.h>

#include <boot-buffer.h>
#include <datasafe.h>
#include <factory-data.h>

#include <driver/crc/crc.h>
#include <driver/flash/flash.h>

void factory_config_load(void) {
  char* buffer;
  CRC32 crc = 0;
  flash_error fe;
  flare_factory_data* data;

  if (flare_get_read_bufferSize() < sizeof(flare_factory_data)) {
    printf("error: factory table size too large\n");
    return;
  }

  buffer = flare_get_read_buffer();

  /*
   * Assumes the file has been mounted and so the flash driver initialised.
   */
  fe = flash_read(flash_device_size() - flash_device_sector_erase_size(),
                  buffer, sizeof(flare_factory_data));
  if (fe != FLASH_NO_ERROR) {
    printf("error: factory table read: %d\n", fe);
    return;
  }

  data = (flare_factory_data*)buffer;

  crc32_update(&crc, (uint8_t*)(&data->format), FACTORY_CRC_LENGTH);

  if (data->crc32 == crc) {
    flare_datasafe_factory_data_set(
        &data->mac_address_0[0], &data->mac_address_1[0],
        &data->mac_address_2[0], &data->mac_address_3[0],
        &data->assembly_serial_number[0], &data->part_number[0],
        &data->revision[0], &data->modstrike[0], &data->board_serial_number[0],
        &data->app_data[0], &data->boot_cmd[0]);
  } else {
    flare_datasafe_factory_data_clear();
    printf("error: invalid factory data checksum, %08x != %08x\n", data->crc32,
           crc);
  }
}
