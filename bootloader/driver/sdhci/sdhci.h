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

#ifndef DRIVERS_SDHCI_SDHCI_H_
#define DRIVERS_SDHCI_SDHCI_H_

#include <stdbool.h>
#include <stdint.h>

#define SDHCI_BLK_SIZE 512

#define SDHCI_CARD_TYPE_EMMC   0x01
#define SDHCI_CARD_TYPE_SDSC   0x02
#define SDHCI_CARD_TYPE_SDHXUC 0x04

#define SDHCI_CTLR_MAX 1

#define SDHCI_CTLR_SD   0
#define SDHCI_CTLR_EMMC 1

typedef enum
{
    SDHCI_NO_ERROR = 0,
    SDHCI_CARD_NOT_PRESENT,
    SDHCI_CARD_NOT_STABLE,
    SDHCI_CARD_NOT_SUPPORTED,
    SDHCI_RESET_FAILED,
    SDHCI_BUSY,
    SDHCI_TRANSFER_FAILED,
    SDHCI_CLK_ERROR,
    SDHCI_RESPONSE_TIMEOUT,
} sdhci_error;

sdhci_error sdhci_open(int controller);
sdhci_error sdhci_close(int controller);

bool sdhci_initialised(int controller);

sdhci_error sdhci_read(int controller, uint32_t sector, uint32_t count, char* buffer);

#endif /* DRIVERS_SDHCI_SDHCI_H_ */
