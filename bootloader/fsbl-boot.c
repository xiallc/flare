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
 * Flare FSBL Loader.
 */

#include <inttypes.h>
#include <stdio.h>

#include <board-handoff.h>
#include <board.h>
#include <boot-factory-config.h>
#include <boot-load.h>
#include <boot-script.h>
#include <cache.h>
#include <datasafe.h>
#include <factory-boot.h>
#include <flare-boot.h>
#include <flare-build-id.h>
#include <flash-map.h>
#include <fs/boot-filesystem.h>
#include <reset.h>
#include <sleep.h>

#include <driver/flash/flash.h>
#include <driver/io/board-io.h>
#include <driver/leds/leds.h>
#include <driver/power-switch/power-switch.h>
#include <driver/sdhci/sdhci.h>
#include <driver/slcr/board-slcr.h>
#include <driver/timer/board-timer.h>
#include <driver/uart/console.h>
#include <driver/wdog/wdog.h>

static void
flare_boot_board_requests(void)
{
    if (flare_datasafe_factory_boot_requested()) {
        printf("Factory boot requested\n");
        factory_boot();
    }
}

static void boot_failure() {
    led_failure();
    factory_boot();
    printf("Reset ..... ");
    console_flush();
    reset();
}

int main(void) {
    flare_boot_plan bp;
    const char* label;
    boot_script script;
    uint32_t entry_point = 0;
    bool status = false;

    board_hardware_setup();
    board_timer_reset();

    printf("\nFlare FSBL (Apache 2.0 Licensed)\n");
    printf("    Build ID: %s\n", flare_build_id());

    cache_enable();

    wdog_init();
    wdog_control(false);

    led_init();
    led_normal();

    flare_datasafe_init();

    flash_error err = flash_open(&label);
    if (err == FLASH_NO_ERROR) {
        printf("       Flash: %s\n", label);
    }
    factory_config_load();
    flare_boot_board_requests();

    flare_get_boot_plan(&bp);

    for (int i = 0; i < FLARE_STAGE_FUNC_MAX; i++) {
        if (bp.opens[i] == NULL) {
            break;
        }

        status = bp.opens[i]();
        if (status) {
            printf("Open failure: %s: %d\n", bp.opens_name[i], status);
            boot_failure();
        }
    }

    for (int i = 0; i < FLARE_STAGE_FUNC_MAX; i++) {
        if (bp.mounts[i] == NULL) {
            break;
        }

        status = bp.mounts[i]();
        if (status) {
            printf("Mount failure: %s: %d\n", bp.mounts_name[i], status);
            boot_failure();
        }
    }

    status = boot_script_load(bp.boot_fs, bp.bs_name, &script);
    if (status) {
        printf("Invalid boot script: %d\n", status);
        boot_failure();
    }

    status = load_exe(&script, &entry_point);
    if (status) {
        printf("Invalid executable: %d\n", status);
    }

    flare_datasafe_set_boot(script.path, script.executable);
    wdog_control(true);
    cache_flush_invalidate();
    console_flush();
    board_handoff_exit(entry_point);

    return 0;
}
