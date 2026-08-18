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

#include <driver/io/board-io.h>

#include "ipi.h"
#include "pm.h"
#include "pm_api_version.h"
#include "pm_defs.h"

static char* err_to_str(uint32_t err) {
  char* err_str = NULL;

  switch (err) {
  case 0:
    err_str = "NONE";
    break;
  case XST_PM_INTERNAL:
    err_str = "PM INTERNAL";
    break;
  case XST_PM_CONFLICT:
    err_str = "PM CONFLICT";
    break;
  case XST_PM_NO_ACCESS:
    err_str = "PM NO ACCESS";
    break;
  case XST_PM_INVALID_NODE:
    err_str = "PM INVALID NODE";
    break;
  case XST_PM_DOUBLE_REQ:
    err_str = "PM DOUBLE REQUEST";
    break;
  case XST_PM_ABORT_SUSPEND:
    err_str = "PM ABORT SUSPEND";
    break;
  case XST_PM_TIMEOUT:
    err_str = "PM TIMEOUT";
    break;
  case XST_PM_NODE_USED:
    err_str = "PM NODE_USED";
    break;
  default:
    err_str = "UNKNOWN";
    break;
  }

  return err_str;
}

void pm_set_configuration() {
  uint32_t buf[IPI_PAYLOAD_ARG_CNT];
  uint32_t reg;
  uint32_t api_major = 0;
  uint32_t api_minor = 0;
  int status;

  reg = board_reg_read(PMU_GLOBAL_GLOBAL_CNTRL);
  if ((reg & PMU_GLOBAL_GLOBAL_CNTRL_FW_IS_PRESENT_MASK) == 0) {
    printf("    PMUFW: Not present\n");
    return;
  }

  buf[0] = PM_GET_API_VERSION;
  for (int i = 1; i < IPI_PAYLOAD_ARG_CNT; i++) {
    buf[i] = 0;
  }

  status = pm_ipi_send(buf);
  if (status) {
    printf("       PMUFW: transfer failed\n");
  }
  status = pm_ipi_read(buf);
  if (status) {
    printf("       PMUFW: transfer failed\n");
  }
  if (buf[0]) {
    printf("       PMUFW: API version read failed: %s\n", err_to_str(buf[0]));
    return;
  }

  api_major = (buf[1] & PM_VERSION_MAJOR_MASK) >> PM_VERSION_MAJOR_SHIFT;
  api_minor = (buf[1] & PM_VERSION_MINOR_MASK) >> PM_VERSION_MINOR_SHIFT;

  printf("       PMUFW: present, API version: %d.%d\n", api_major, api_minor);

  buf[0] = PM_SET_CONFIGURATION;
  buf[1] = (uint32_t)((uint64_t)XPm_ConfigObject);
  for (int i = 2; i < IPI_PAYLOAD_ARG_CNT; i++) {
    buf[i] = 0;
  }

  status = pm_ipi_send(buf);
  if (status) {
    printf("       PMUFW: transfer failed\n");
  }
  status = pm_ipi_read(buf);
  if (status) {
    printf("       PMUFW: transfer failed\n");
  }

  if (buf[0]) {
    printf("       PMUFW: config load failed: %s\n", err_to_str(buf[0]));
    return;
  }
  printf("       PMUFW: config loaded\n");

  /* Print Chip ID */
  buf[0] = PM_GET_CHIPID;
  for (int i = 1; i < IPI_PAYLOAD_ARG_CNT; i++) {
    buf[i] = 0;
  }

  status = pm_ipi_send(buf);
  if (status) {
    printf("       PMUFW: transfer failed\n");
  }
  status = pm_ipi_read(buf);
  if (status) {
    printf("       PMUFW: transfer failed\n");
  }
  if (buf[0]) {
    printf("       Chip ID read failed: %s\n", err_to_str(buf[0]));
    return;
  }

  printf("     Chip ID: 0x%08x\n    Chip Ver: 0x%08x\n", buf[1], buf[2]);
}
