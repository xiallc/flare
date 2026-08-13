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

#include <driver/sdhci/sdhci.h>

#include "sdwrapper.h"
#include "ff.h"
#include "diskio.h"

static int sdhci_ctlr = 0;

void fatfs_set_sdhci_ctlr(int ctlr) {
    sdhci_ctlr = ctlr;
}

DSTATUS disk_status (
	BYTE pdrv		/* Physical drive nmuber to identify the drive */
)
{
    (void)pdrv;
    if (!sdhci_initialised(sdhci_ctlr)) {
        return STA_NOINIT;
    } else {
        return 0;
    }
}

DSTATUS disk_initialize (
	BYTE pdrv				/* Physical drive nmuber to identify the drive */
)
{
    (void)pdrv;
    sdhci_error err = sdhci_open(sdhci_ctlr);
    if (err == SDHCI_NO_ERROR) {
        return 0;
    } else if (err == SDHCI_CARD_NOT_PRESENT) {
        return STA_NODISK;
    } else {
        return STA_NOINIT;
    }
}

DRESULT disk_read (
	BYTE pdrv,		/* Physical drive nmuber to identify the drive */
	BYTE *buff,		/* Data buffer to store read data */
	LBA_t sector,	/* Start sector in LBA */
	UINT count		/* Number of sectors to read */
)
{
    (void)pdrv;
    sdhci_error err = sdhci_read(sdhci_ctlr, sector, count, (char*)buff);
    if (err != SDHCI_NO_ERROR) {
        return RES_ERROR;
    } else {
        return RES_OK;
    }
}
