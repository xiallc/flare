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

#ifndef _FLARE_DRIVERS_XPARTITION_XPARTITION_H_
#define _FLARE_DRIVERS_XPARTITION_XPARTITION_H_

#include <stdint.h>

#define XLNX_HDR_WIDTH_DETECT 0xAA995566
#define XLNX_HDR_SIGNATURE    0x584C4E58

#define XLNX_HDR_KEY_SRC_UNENC    0x0
#define XLNX_HDR_KEY_SRC_B_EFUSE  0xA5C3C5A5
#define XLNX_HDR_KEY_SRC_O_EFUSE  0xA5C3C5A7
#define XLNX_HDR_KEY_SRC_R_BBRAM  0x3A5C3C5A
#define XLNX_HDR_KEY_SRC_R_EFUSE  0xA5C3C5A3
#define XLNX_HDR_KEY_SRC_O_BHDR   0xA35C7CA5
#define XLNX_HDR_KEY_SRC_U_BHDR   0xA3A5C3C5
#define XLNX_HDR_KEY_SRC_B_BHDR   0xA35C7C53

#define XLNX_PART_HDR_ATTR_TRUST_MASK     0x00000001
  #define XLNX_PART_HDR_ATTR_SECURE         0x00000001
  #define XLNX_PART_HDR_ATTR_NON_SECURE     0x00000000
#define XLNX_PART_HDR_EXCEP_LVL_MASK      0x00000006
  #define XLNX_PART_HDR_EXCEP_LVL_EL0       (0U << 1)
  #define XLNX_PART_HDR_EXCEP_LVL_EL1       (1U << 1)
  #define XLNX_PART_HDR_EXCEP_LVL_EL2       (2U << 1)
  #define XLNX_PART_HDR_EXCEP_LVL_EL3       (3U << 1)
#define XLNX_PART_HDR_EXEC_STATE_MASK     0x00000008
  #define XLNX_PART_HDR_EXEC_STATE_A64      (0U << 3)
  #define XLNX_PART_HDR_EXEC_STATE_A32      (1U << 3)
#define XLNX_PART_HDR_ENCRYPTED_MASK      0x00000080
#define XLNX_PART_HDR_DEST_CPU_MASK       0x00000F00
  #define XLNX_PART_HDR_DEST_CPU_A53_0      (1U << 8)
  #define XLNX_PART_HDR_DEST_CPU_A53_1      (2U << 8)
  #define XLNX_PART_HDR_DEST_CPU_A53_2      (3U << 8)
  #define XLNX_PART_HDR_DEST_CPU_A53_3      (4U << 8)
  #define XLNX_PART_HDR_DEST_CPU_R5_0       (5U << 8)
  #define XLNX_PART_HDR_DEST_CPU_R5_1       (6U << 8)
#define XLNX_PART_HDR_PART_OWNER_MASK     0x00030000
  #define XLNX_PART_HDR_PART_OWNER_FSBL     (0U << 16)
  #define XLNX_PART_HDR_PART_OWNER_UBOOT    (0U << 16)

void xpart_load_parts();

#endif /* _FLARE_DRIVERS_XPARTITION_XPARTITION_H_ */
