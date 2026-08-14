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
 * Flare FSBL JFFS2 Factory Boot support.
 */

#include <stdio.h>
#include <string.h>

#include <board-handoff.h>
#include <board.h>
#include <boot-factory-config.h>
#include <boot-load.h>
#include <cache.h>
#include <datasafe.h>
#include <factory-boot.h>
#include <flare-boot.h>
#include <flash-map.h>
#include <reset.h>
#include <uboot.h>

#include <driver/crc/crc.h>
#include <driver/flash/flash.h>
#include <driver/wdog/wdog.h>
#include <driver/zlib/tzlib.h>

/*
 * The factory boot base and the number of file slots.
 */
#define FACTORY_BOOT_BASE       FLARE_FLASH_EXE_BASE

 /*
  * Three files:
  *  1: bitfile     * load
  *  2: executable  * load
  *  3: file image  - ignore
  */
#define FLASH_SLOT_BASE      FLARE_FLASH_EXE_BASE
#define FLASH_SLOT_SIZE      FLARE_FLASH_EXE_SIZE
#define EXECUTABLE_LOAD_BASE FLARE_EXECUTABLE_BASE
#define EXECUTABLE_LOAD_SIZE FLARE_EXECUTABLE_SIZE

#define MASK_N_DIV(x, n) ((x) & ~((n) - 1))
#define MASK_N_MOD(x, n) ((x) & ((n) - 1))
#define PAD_n(x, n)      (MASK_N_MOD(x, n) == 0 ? (x) : MASK_N_DIV((x) + ((n) - 1), n))
#define PAD_4(x)         PAD_n(x, 4)

uint8_t factory_header[IMAGE_HEADER_TOTAL(FLASH_SLOT_FILES)];

uint32_t
factory_load_image_get32(uint8_t* table, uint32_t index, uint32_t offset)
{
    return *((uint32_t*) IMAGE_HEADER_RECORD(table, index, offset));
}

uint8_t
factory_load_image_get8(uint8_t* table, uint32_t index, uint32_t offset)
{
    return *((uint8_t*) IMAGE_HEADER_RECORD(table, index, offset));
}

void
platform_factory_booter(uint8_t* header, size_t header_size)
{
    uint32_t        flash_offset;
    uint32_t        entry_point;
    size_t          size;
    flash_error     fe;
    CRC32           crc;
    int             i;
    uint8_t         checksum[CRC_CHECKSUM_SIZE];
    char            name[UBOOT_NAME_LEN + 1] = {0};
    const uint32_t* header_crc = (const uint32_t*)IMAGE_HEADER_RECORD(header, 0, IMAGE_HEADER_CRC);

    flare_datasafe_set_factory_boot();

    size = factory_load_image_get32(header, 0, IMAGE_HEADER_SIZE);
    if (size > EXECUTABLE_LOAD_SIZE)
    {
        printf("error: factory image too big: 0x%08zx\n", size);
        return;
    }

    flash_offset = FLASH_SLOT_BASE + header_size;

    size = factory_load_image_get32(header, 0, IMAGE_HEADER_SIZE);
    if (size > EXECUTABLE_LOAD_SIZE)
    {
        printf("error: factory image too big\n");
        return;
    }

    fe = flash_read(flash_offset, (uint8_t*)FLARE_IMAGE_STAGE_ADDR, size);
    if (fe != FLASH_NO_ERROR)
    {
        printf("error: load factory image: %d\n", fe);
        return;
    }

    crc32_clear(&crc);
    crc32_update(&crc, (const void*)FLARE_IMAGE_STAGE_ADDR, size);
    crc32_str(&crc, checksum);

    printf("         CRC32: ");
    for (i = 0; i < CRC_CHECKSUM_SIZE; ++i) {
        printf("%c", checksum[i]);
    }
    printf("\n");

    if (*header_crc != crc) {
        printf("error: invalid checksum 0x%08x 0x%08x\n", *header_crc, crc);
        return;
    }

    if(!load_uboot_image((uint8_t*) FLARE_IMAGE_STAGE_ADDR, size, &entry_point)) {
        return;
    }

    memcpy(name, (uint8_t*)FLARE_IMAGE_STAGE_ADDR + UBOOT_IMAGE_NAME_OFF, UBOOT_NAME_LEN);
    flare_datasafe_set_boot("", (const char*)name);

    flare_datasafe_clear_factory_boot_request();
    wdog_control(true);
    cache_disable();
    board_handoff_exit(entry_point);
}

void
factory_boot()
{
    uint8_t*          header = factory_header;
    size_t            header_size = sizeof(factory_header);
    flash_error       fe;
    bool              ok = true;
    CRC32             crc;
    const uint32_t*   header_crc;
    const char*       label;

    fe = flash_open(&label);
    if (fe != FLASH_NO_ERROR)
    {
        printf("error: opening flash: %d\n", fe);
        return;
    }
    printf("         Flash: %s\n", label);
    factory_config_load();

    fe = flash_read(FACTORY_BOOT_BASE, header, header_size);
    if (fe != FLASH_NO_ERROR)
    {
        printf("error: reading factory header: %d\n", fe);
        return;
    }

    crc32_clear(&crc);
    crc32_update(&crc, (const void*)header + IMAGE_HEADER_HEADER_REC,
        header_size - IMAGE_HEADER_HEADER_CRC_SIZE);

    header_crc = (const uint32_t*)(&header[IMAGE_HEADER_HEADER_CRC]);
    if (*header_crc != crc)
    {
        printf("error: invalid header checksum: 0x%08x != 0x%08x\n", *header_crc, crc);
        ok = false;
    }

    if (ok) {
        platform_factory_booter(header, header_size);
    }

    printf("Reset ... ");
    reset();
}
