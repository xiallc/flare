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

#include <stdbool.h>

#include <sleep.h>

#include <driver/io/board-io.h>
#include <driver/sdhci/sdhci.h>
#include <driver/sdhci/sdhci_hw.h>
#include <driver/timer/board-timer.h>

#define SDHCI_DEBUG_NONE  0
#define SDHCI_DEBUG_ENA   1
#define SDHCI_DEBUG_TRACE 2
#define SDHCI_DEBUG_VERB  3

static const int sdhci_debug = SDHCI_DEBUG_NONE;

struct sd_controller {
  int ctlr;
  bool initialised;
  uint8_t card_version;
  uint32_t card_id[4];
  uint32_t card_csd[4];
  uint8_t ext_card_csd[512];
  uint16_t card_rca;
};

static struct sd_controller sdhcis[SDHCI_CTLR_MAX + 1] = {0};

#define SDHCI_DEBUG(...)                                                       \
  do {                                                                         \
    if (sdhci_debug) {                                                         \
      printf(__VA_ARGS__);                                                     \
    }                                                                          \
  } while (0)

#define SDHCI_TRACE_DEBUG(...)                                                 \
  do {                                                                         \
    if (sdhci_debug >= SDHCI_DEBUG_TRACE) {                                    \
      printf(__VA_ARGS__);                                                     \
    }                                                                          \
  } while (0)

#define SDHCI_VERB_DEBUG(...)                                                  \
  do {                                                                         \
    if (sdhci_debug >= SDHCI_DEBUG_VERB) {                                     \
      printf(__VA_ARGS__);                                                     \
    }                                                                          \
  } while (0)

uint32_t sdhci_reg_read(int ctlr, uint32_t offset) {
  uint32_t val = board_reg_read(sdhci_address(ctlr) + offset);
  SDHCI_VERB_DEBUG("sdhci (%d): read32: 0x%08x = %08x\n", ctlr,
                   sdhci_address(ctlr) + offset, val);
  return val;
}

uint16_t sdhci_reg_read_16(int ctlr, uint32_t offset) {
  uint16_t val = board_reg_read_16(sdhci_address(ctlr) + offset);
  SDHCI_VERB_DEBUG("sdhci (%d): read16: 0x%08x = %04x\n", ctlr,
                   sdhci_address(ctlr) + offset, val);
  return val;
}

uint8_t sdhci_reg_read_8(int ctlr, uint32_t offset) {
  uint8_t val = board_reg_read_8(sdhci_address(ctlr) + offset);
  SDHCI_VERB_DEBUG("sdhci (%d): read8: 0x%08x = %02x\n", ctlr,
                   sdhci_address(ctlr) + offset, val);
  return val;
}

void sdhci_reg_write(int ctlr, uint32_t offset, uint32_t val) {
  SDHCI_VERB_DEBUG("sdhci (%d): write32: 0x%08x = %08x\n", ctlr,
                   sdhci_address(ctlr) + offset, val);
  board_reg_write(sdhci_address(ctlr) + offset, val);
}

void sdhci_reg_write_16(int ctlr, uint32_t offset, uint16_t val) {
  SDHCI_VERB_DEBUG("sdhci (%d): write16: 0x%08x = %04x\n", ctlr,
                   sdhci_address(ctlr) + offset, val);
  board_reg_write_16(sdhci_address(ctlr) + offset, val);
}

void sdhci_reg_write_8(int ctlr, uint32_t offset, uint8_t val) {
  SDHCI_VERB_DEBUG("sdhci (%d): write8: 0x%08x = %02x\n", ctlr,
                   sdhci_address(ctlr) + offset, val);
  board_reg_write_8(sdhci_address(ctlr) + offset, val);
}

static sdhci_error initialise(int ctlr);

static inline void check_initialised(int ctlr) {
  if (!sdhcis[ctlr].initialised) {
    initialise(ctlr);
  }
}

static sdhci_error set_clock(int ctlr, bool slow) {
  SDHCI_TRACE_DEBUG("sdhci (%d): set_clock(slow = %c)\n", ctlr,
                    slow ? 'T' : 'F');
  uint16_t clk_val;

  sdhci_reg_write_16(ctlr, SDHCI_CLOCK_CONTROL, 0);

  clk_val = sdhci_reg_read_16(ctlr, SDHCI_CLOCK_CONTROL);
  clk_val |= SDHCI_CLOCK_INT_EN;
  if (slow) {
    /* Set clock to 50Mhz / 128 = 400Khz */
    uint16_t divisor_power = 7;
    clk_val |= (1 << (divisor_power - 1)) << SDHCI_DIVIDER_SHIFT;
  } else {
    /* Set clock to 50Mhz / 2 = 25Mhz */
    uint16_t divisor_power = 1;
    clk_val |= (1 << (divisor_power - 1)) << SDHCI_DIVIDER_SHIFT;
  }
  sdhci_reg_write_16(ctlr, SDHCI_CLOCK_CONTROL, clk_val);

  /* Poll for clock stabilisation complete */
  uint32_t poll_mask = SDHCI_CLOCK_INT_STABLE;
  uint32_t timeout = 150000;
  clk_val = sdhci_reg_read_16(ctlr, SDHCI_CLOCK_CONTROL) & poll_mask;
  while (timeout != 0) {
    clk_val = sdhci_reg_read_16(ctlr, SDHCI_CLOCK_CONTROL) & poll_mask;
    if (clk_val != 0) {
      break;
    }
    timeout--;
    usleep(1);
  }
  if (timeout == 0) {
    return SDHCI_CLK_ERROR;
  }

  clk_val = sdhci_reg_read_16(ctlr, SDHCI_CLOCK_CONTROL);
  clk_val |= SDHCI_CLOCK_CARD_EN;
  sdhci_reg_write_16(ctlr, SDHCI_CLOCK_CONTROL, clk_val);

  usleep(50000);
  return SDHCI_NO_ERROR;
}

static sdhci_error reset_config(int ctlr) {
  SDHCI_TRACE_DEBUG("sdhci (%d): reset_config()\n", ctlr);
  disable_bus_power(ctlr);

  sdhci_reg_write_8(ctlr, SDHCI_SOFTWARE_RESET, SDHCI_RESET_ALL);

  uint32_t timeout = 100000;
  uint8_t cval;
  while (timeout != 0) {
    cval = sdhci_reg_read_8(ctlr, SDHCI_SOFTWARE_RESET) & SDHCI_RESET_ALL;
    if (cval == 0) {
      break;
    }
    timeout--;
    usleep(1);
  }

  if (timeout == 0) {
    SDHCI_TRACE_DEBUG("sdhci (%d): reset timed out\n", ctlr);
    return SDHCI_RESET_FAILED;
  }

  enable_bus_power(ctlr);
  return SDHCI_NO_ERROR;
}

static void config_power(int ctlr) {
  SDHCI_TRACE_DEBUG("sdhci (%d): config_power()\n", ctlr);
  uint8_t pl = 0;
  uint32_t caps = sdhci_reg_read(ctlr, SDHCI_CAPABILITIES);

  if (caps & SDHCI_CAN_VDD_330) {
    pl = SDHCI_POWER_330;
  } else if (caps & SDHCI_CAN_VDD_300) {
    pl = SDHCI_POWER_300;
  } else if (caps & SDHCI_CAN_VDD_180) {
    pl = SDHCI_POWER_180;
  }

  sdhci_reg_write_8(ctlr, SDHCI_POWER_CONTROL, pl | SDHCI_POWER_ON);
}

static inline bool check_idle(int ctlr) {
  uint32_t status = sdhci_reg_read(ctlr, SDHCI_PRESENT_STATE);
  return !(status & SDHCI_CMD_INHIBIT);
}

static uint32_t flag_generator(int ctlr, uint32_t cmd) {
  uint32_t flags = 0;

  if (cmd != CMD16) {
    flags |= SDHCI_TRNS_READ;
  }

  if (cmd == CMD0) {
    /* R None */
    flags |= SDHCI_CMD_RESP_NONE << 16;
  } else if (cmd == CMD2 || cmd == CMD9) {
    /* R2 */
    flags |= SDHCI_CMD_RESP_LONG << 16;
    flags |= SDHCI_CMD_CRC << 16;
  } else if (cmd == ACMD41 || cmd == CMD1) {
    /* R3 */
    flags |= SDHCI_CMD_RESP_SHORT << 16;
  } else if (cmd == CMD3 || cmd == CMD7) {
    /* R6 and R1b */
    flags |= SDHCI_CMD_RESP_SHORT_BUSY << 16;
    flags |= SDHCI_CMD_CRC << 16;
    flags |= SDHCI_CMD_INDEX << 16;
  } else {
    /* R1 */
    flags |= SDHCI_CMD_RESP_SHORT << 16;
    flags |= SDHCI_CMD_CRC << 16;
    flags |= SDHCI_CMD_INDEX << 16;
  }

  if (cmd == CMD17 || cmd == CMD18 ||
      (cmd == CMD8 && sdhcis[ctlr].card_version == SDHCI_CARD_TYPE_EMMC)) {
    flags |= SDHCI_CMD_DATA << 16;
  }

  if (cmd == CMD18) {
    flags |= SDHCI_TRNS_BLK_CNT_EN;
    flags |= SDHCI_TRNS_MULTI;
    flags |= SDHCI_TRNS_ACMD12;
  }

  return flags;
}

static sdhci_error cmd_transfer(int ctlr, uint32_t cmd, uint32_t arg,
                                uint16_t blk_cnt, uint32_t* res) {
  SDHCI_TRACE_DEBUG(
      "sdhci (%d): cmd_transfer(cmd=%c%d, arg=0x%08x, blk_cnt=%d, res=%p)\n",
      ctlr, cmd & 0x80 ? 'A' : 'C', cmd & 0x3F, arg, blk_cnt, res);
  if (!check_idle(ctlr)) {
    SDHCI_TRACE_DEBUG("sdhci (%d): cmd_transfer: SDHCI_BUSY\n", ctlr);
    return SDHCI_BUSY;
  }

  uint32_t flags = flag_generator(ctlr, cmd);

  sdhci_reg_write_16(ctlr, SDHCI_BLOCK_COUNT, blk_cnt);
  sdhci_reg_write_8(ctlr, SDHCI_TIMEOUT_CONTROL, 0xE);
  sdhci_reg_write(ctlr, SDHCI_ARGUMENT, arg);

  sdhci_reg_write(ctlr, SDHCI_INT_STATUS,
                  SDHCI_INT_NORMAL_MASK | SDHCI_INT_ERROR_MASK);

  cmd &= 0x3F;
  sdhci_reg_write(ctlr, SDHCI_TRANSFER_MODE, (cmd << 24) | flags);

  /* Poll for transfer complete */
  uint32_t poll_mask = SDHCI_INT_ERROR | SDHCI_INT_RESPONSE;
  uint64_t end_time;
  uint64_t curr_time;
  board_timer_get(&curr_time);
  end_time = curr_time + 100000;
  uint32_t cval;
  while (curr_time < end_time) {
    check_idle(ctlr);
    cval = sdhci_reg_read(ctlr, SDHCI_INT_STATUS) & poll_mask;
    if (cval != 0) {
      break;
    }
    board_timer_get(&curr_time);
  }
  if (cval & SDHCI_INT_ERROR || (curr_time >= end_time)) {
    /* Write to clear and return error */
    SDHCI_TRACE_DEBUG(
        "sdhci (%d): cmd_transfer: SDHCI_TRANSFER_FAILED: status: 0x%x\n", ctlr,
        sdhci_reg_read(ctlr, SDHCI_INT_STATUS));
    sdhci_reg_write(ctlr, SDHCI_INT_STATUS, SDHCI_INT_ERROR_MASK);
    return SDHCI_TRANSFER_FAILED;
  }

  /* Write to clear */
  sdhci_reg_write_16(ctlr, SDHCI_INT_STATUS, SDHCI_INT_RESPONSE);

  if (res != NULL) {
    *res = sdhci_reg_read(ctlr, SDHCI_RESPONSE0);
    SDHCI_TRACE_DEBUG("sdhci (%d): cmd_transfer: SDHCI_NO_ERROR: 0x%08x\n",
                      ctlr, *res);
  } else {
    SDHCI_TRACE_DEBUG("sdhci (%d): cmd_transfer: SDHCI_NO_ERROR\n", ctlr);
  }

  return SDHCI_NO_ERROR;
}

static uint32_t read_fifo(int ctlr, char* buffer) {
  SDHCI_TRACE_DEBUG("sdhci (%d): read_fifo(buffer = 0x%p)\n", ctlr, buffer);
  uint32_t* buff = (uint32_t*)buffer;
  sdhci_reg_write(ctlr, SDHCI_INT_STATUS, SDHCI_INT_DATA_AVAIL);
  uint32_t total_reads = SDHCI_BLK_SIZE / sizeof(uint32_t);
  for (uint32_t i = 0; i < total_reads; i++) {
    buff[i] = sdhci_reg_read(ctlr, SDHCI_BUFFER);
  };
  return SDHCI_BLK_SIZE;
}

static sdhci_error read_data_transfer(int ctlr, char* buffer) {
  SDHCI_TRACE_DEBUG("sdhci (%d): read_data_transfer(buffer = %p)\n", ctlr,
                    buffer);
  uint32_t status;
  uint64_t end_time;
  uint64_t curr_time;
  board_timer_get(&curr_time);
  end_time = curr_time + 5000000;
  while (curr_time < end_time) {
    status = sdhci_reg_read(ctlr, SDHCI_INT_STATUS);
    if ((status & SDHCI_INT_DATA_AVAIL) != 0) {
      buffer += read_fifo(ctlr, buffer);
    }
    if ((status & (SDHCI_INT_ERROR | SDHCI_INT_DATA_END)) != 0) {
      break;
    }
    board_timer_get(&curr_time);
  }
  if ((status & SDHCI_INT_ERROR) || (curr_time >= end_time)) {
    /* Write to clear and return error */
    SDHCI_TRACE_DEBUG(
        "sdhci (%d): read_data_transfer: SDHCI_TRANSFER_FAILED: 0x%x\n", ctlr,
        sdhci_reg_read(ctlr, SDHCI_INT_STATUS));
    sdhci_reg_write(ctlr, SDHCI_INT_STATUS, SDHCI_INT_ERROR_MASK);
    return SDHCI_TRANSFER_FAILED;
  }
  SDHCI_TRACE_DEBUG("sdhci (%d): read_data_transfer: SDHCI_NO_ERROR\n", ctlr);
  return SDHCI_NO_ERROR;
}

static sdhci_error initialise_sd(int ctlr) {
  SDHCI_DEBUG("sdhci (%d): initialise_sd()\n", ctlr);
  sdhci_error err;
  uint32_t res = 0;

  /* Get card id */
  err = cmd_transfer(ctlr, CMD2, 0, 0, &sdhcis[ctlr].card_id[0]);
  if (err != SDHCI_NO_ERROR && err != SDHCI_RESPONSE_TIMEOUT) {
    return err;
  }
  sdhcis[ctlr].card_id[1] = sdhci_reg_read(ctlr, SDHCI_RESPONSE1);
  sdhcis[ctlr].card_id[2] = sdhci_reg_read(ctlr, SDHCI_RESPONSE2);
  sdhcis[ctlr].card_id[3] = sdhci_reg_read(ctlr, SDHCI_RESPONSE3);

  /* Get card address */
  err = cmd_transfer(ctlr, CMD3, 0, 0, &res);
  if (err != SDHCI_NO_ERROR && err != SDHCI_RESPONSE_TIMEOUT) {
    return err;
  }
  sdhcis[ctlr].card_rca = (res >> 16) & 0xFFFF;

  /*
   * Get card CSD.
   * CSD is shifted 8 bits to the left because of SD host ctlr
   */
  err = cmd_transfer(ctlr, CMD9, (sdhcis[ctlr].card_rca << 16), 0,
                     &sdhcis[ctlr].card_csd[0]);
  if (err != SDHCI_NO_ERROR && err != SDHCI_RESPONSE_TIMEOUT) {
    return err;
  }
  sdhcis[ctlr].card_csd[1] = sdhci_reg_read(ctlr, SDHCI_RESPONSE1);
  sdhcis[ctlr].card_csd[2] = sdhci_reg_read(ctlr, SDHCI_RESPONSE2);
  sdhcis[ctlr].card_csd[3] = sdhci_reg_read(ctlr, SDHCI_RESPONSE3);

  uint32_t csd_struct =
      ((sdhcis[ctlr].card_csd[3] & CSD_STRUCT_MASK) >> CSD_STRUCT_SHIFT);
  if (csd_struct == 0) {
    uint32_t blk_len = (sdhcis[ctlr].card_csd[2] & READ_BLK_LEN_MASK) >> 8;
    uint32_t mult_shift =
        ((sdhcis[ctlr].card_csd[1] & C_SIZE_MULT_MASK) >> 7) + 2;
    uint32_t mult = 1 << mult_shift;
    uint32_t size_mb = (sdhcis[ctlr].card_csd[1] & C_SIZE_LOWER_MASK) >> 22;
    size_mb |= (sdhcis[ctlr].card_csd[2] & C_SIZE_UPPER_MASK) << 10;
    size_mb = (size_mb + 1) * mult;
    size_mb = size_mb * blk_len;
    size_mb = size_mb / (1024 * 1024);
    printf("SDSD card found (%08x MiB)\n", size_mb);
  } else if (csd_struct == 1) {
    uint32_t size_gb =
        ((sdhcis[ctlr].card_csd[1] & CSD_V2_C_SIZE_MASK) >> 8) + 1;
    size_gb = size_gb / (2 * 1024);
    printf("SDHC/SDXC card found (%d GiB)\n", size_gb);
  } else {
    printf("Card not found\n");
    return SDHCI_CARD_NOT_SUPPORTED;
  }

  /* Set clock speed to 25Mhz */
  set_clock(ctlr, false);

  /* Moving card to transfer mode */
  err = cmd_transfer(ctlr, CMD7, (sdhcis[ctlr].card_rca << 16), 0, &res);
  if (err != SDHCI_NO_ERROR && err != SDHCI_RESPONSE_TIMEOUT) {
    return err;
  }

  /* Set block size to 512 */
  sdhci_reg_write_16(ctlr, SDHCI_BLOCK_SIZE_REG, SDHCI_BLK_SIZE);
  err = cmd_transfer(ctlr, CMD16, SDHCI_BLK_SIZE, 0, &res);
  if (err != SDHCI_NO_ERROR) {
    return err;
  }

  sdhcis[ctlr].initialised = true;
  SDHCI_TRACE_DEBUG("sdhci (%d): initialise: success\n", ctlr);
  return SDHCI_NO_ERROR;
}

static sdhci_error initialise_emmc(int ctlr) {
  SDHCI_DEBUG("sdhci (%d): initialise_emmc()\n", ctlr);
  sdhci_error err;
  uint32_t res;
  uint32_t arg;

  err = reset_config(ctlr);
  if (err != SDHCI_NO_ERROR) {
    return err;
  }

  config_power(ctlr);

  bool card_present =
      sdhci_reg_read(ctlr, SDHCI_PRESENT_STATE) & SDHCI_CARD_PRESENT;
  if (!card_present) {
    return SDHCI_CARD_NOT_PRESENT;
  }

  /* Set clock to 400Khz */
  err = set_clock(ctlr, true);
  if (err != SDHCI_NO_ERROR) {
    return err;
  }

  /* Enable status bits */
  sdhci_reg_write(ctlr, SDHCI_INT_ENABLE,
                  SDHCI_INT_NORMAL_MASK | SDHCI_INT_ERROR_MASK);

  uint32_t status = 0;
  status = sdhci_reg_read(ctlr, SDHCI_INT_STATUS);
  SDHCI_DEBUG("Status: 0x%x\n", status);

  /* SW Reset the SD Card */
  err = cmd_transfer(ctlr, CMD0, 0, 0, NULL);
  if (err != SDHCI_NO_ERROR) {
    return err;
  }

  res = 0;
  while (!(res & SDHCI_OCR_READY)) {
    arg = SDHCI_OCR_SECTOR_MODE | SDHCI_OCR_2_7_3_6_V | SDHCI_OCR_1_7_1_95_V;
    err = cmd_transfer(ctlr, CMD1, arg, 0, &res);
    if (err != SDHCI_NO_ERROR) {
      return err;
    }
  }

  /* Get card id */
  err = cmd_transfer(ctlr, CMD2, 0, 0, &sdhcis[ctlr].card_id[0]);
  if (err != SDHCI_NO_ERROR && err != SDHCI_RESPONSE_TIMEOUT) {
    return err;
  }
  sdhcis[ctlr].card_id[1] = sdhci_reg_read(ctlr, SDHCI_RESPONSE1);
  sdhcis[ctlr].card_id[2] = sdhci_reg_read(ctlr, SDHCI_RESPONSE2);
  sdhcis[ctlr].card_id[3] = sdhci_reg_read(ctlr, SDHCI_RESPONSE3);

  /* Set card address */
  sdhcis[ctlr].card_rca = 0x1;
  arg = sdhcis[ctlr].card_rca << 16;
  err = cmd_transfer(ctlr, CMD3, arg, 0, &res);
  if (err != SDHCI_NO_ERROR && err != SDHCI_RESPONSE_TIMEOUT) {
    return err;
  }

  /*
   * Get card CSD.
   * CSD is shifted 8 bits to the left because of SD host ctlr
   */
  err = cmd_transfer(ctlr, CMD9, arg, 0, &sdhcis[ctlr].card_csd[0]);
  if (err != SDHCI_NO_ERROR && err != SDHCI_RESPONSE_TIMEOUT) {
    return err;
  }
  sdhcis[ctlr].card_csd[1] = sdhci_reg_read(ctlr, SDHCI_RESPONSE1);
  sdhcis[ctlr].card_csd[2] = sdhci_reg_read(ctlr, SDHCI_RESPONSE2);
  sdhcis[ctlr].card_csd[3] = sdhci_reg_read(ctlr, SDHCI_RESPONSE3);

  /* Set clock speed to 25Mhz */
  set_clock(ctlr, false);

  /* Moving card to transfer mode */
  err = cmd_transfer(ctlr, CMD7, (sdhcis[ctlr].card_rca << 16), 0, &res);
  if (err != SDHCI_NO_ERROR && err != SDHCI_RESPONSE_TIMEOUT) {
    return err;
  }

  /* Set block size to 512 */
  sdhci_reg_write_16(ctlr, SDHCI_BLOCK_SIZE_REG, SDHCI_BLK_SIZE);
  err = cmd_transfer(ctlr, CMD16, SDHCI_BLK_SIZE, 0, &res);
  if (err != SDHCI_NO_ERROR) {
    return err;
  }

  uint32_t csd_struct =
      ((sdhcis[ctlr].card_csd[3] & CSD_STRUCT_MASK) >> CSD_STRUCT_SHIFT);
  if (csd_struct == 0) {
    uint32_t blk_len = (sdhcis[ctlr].card_csd[2] & READ_BLK_LEN_MASK) >> 8;
    uint32_t mult_shift =
        ((sdhcis[ctlr].card_csd[1] & C_SIZE_MULT_MASK) >> 7) + 2;
    uint32_t mult = 1 << mult_shift;
    uint32_t size_mb = (sdhcis[ctlr].card_csd[1] & C_SIZE_LOWER_MASK) >> 22;
    size_mb |= (sdhcis[ctlr].card_csd[2] & C_SIZE_UPPER_MASK) << 10;
    size_mb = (size_mb + 1) * mult;
    size_mb = size_mb * blk_len;
    size_mb = size_mb / (1024 * 1024);
    printf("EMMC card found (%08x MiB)\n", size_mb);
  } else if (csd_struct == 1) {
    uint32_t size_gb =
        ((sdhcis[ctlr].card_csd[1] & CSD_V2_C_SIZE_MASK) >> 8) + 1;
    size_gb = size_gb / (2 * 1024);
    printf("EMMC card found (%d GiB)\n", size_gb);
  } else if (csd_struct == 2 || csd_struct == 3) {
    /* Read Extended CSD for size */
    err = cmd_transfer(ctlr, CMD8, 0, 0, &res);
    if (err != SDHCI_NO_ERROR && err != SDHCI_RESPONSE_TIMEOUT) {
      return err;
    }
    read_data_transfer(ctlr, (char*)sdhcis[ctlr].ext_card_csd);

    uint32_t sec_count = sdhcis[ctlr].ext_card_csd[215] << 24 |
                         sdhcis[ctlr].ext_card_csd[214] << 16 |
                         sdhcis[ctlr].ext_card_csd[213] << 8 |
                         sdhcis[ctlr].ext_card_csd[212];
    uint32_t size_gb = sec_count / 0x200000;
    uint32_t remainder = sec_count % 0x200000;
    remainder = remainder * 100;
    remainder = remainder / 0x200000;
    printf("EMMC card found (%d.%02d GiB)\n", size_gb, remainder);
  } else {
    printf("CSD version not supported: %d: Card not attached\n", csd_struct);
    return SDHCI_CARD_NOT_SUPPORTED;
  }

  sdhcis[ctlr].initialised = true;
  SDHCI_TRACE_DEBUG("sdhci (%d): initialise: success\n", ctlr);
  return SDHCI_NO_ERROR;
}

static sdhci_error initialise(int ctlr) {
  SDHCI_DEBUG("sdhci (%d): initialise()\n", ctlr);
  sdhci_error err;
  uint32_t res;

  if (sdhcis[ctlr].initialised) {
    return SDHCI_NO_ERROR;
  }

  bool card_present =
      sdhci_reg_read(ctlr, SDHCI_PRESENT_STATE) & SDHCI_CARD_PIN;
  if (!card_present) {
    return SDHCI_CARD_NOT_PRESENT;
  }

  card_present = sdhci_reg_read(ctlr, SDHCI_PRESENT_STATE) & SDHCI_CARD_PRESENT;
  uint64_t end_time;
  uint64_t curr_time;
  board_timer_get(&curr_time);
  end_time = curr_time + 10000000;
  while (!card_present && curr_time < end_time) {
    card_present =
        sdhci_reg_read(ctlr, SDHCI_PRESENT_STATE) & SDHCI_CARD_PRESENT;
    board_timer_get(&curr_time);
  }
  if (!card_present) {
    return SDHCI_CARD_NOT_PRESENT;
  }

  err = reset_config(ctlr);
  if (err != SDHCI_NO_ERROR) {
    return err;
  }

  config_power(ctlr);

  /* Set clock to 400Khz */
  err = set_clock(ctlr, true);
  if (err != SDHCI_NO_ERROR) {
    return err;
  }

  /* Enable status bits */
  sdhci_reg_write(ctlr, SDHCI_INT_ENABLE,
                  SDHCI_INT_NORMAL_MASK | SDHCI_INT_ERROR_MASK);

  /* SW Reset the SD Card */
  err = cmd_transfer(ctlr, CMD0, 0, 0, NULL);
  if (err != SDHCI_NO_ERROR) {
    return err;
  }

  /* Get card interface */
  err = cmd_transfer(ctlr, CMD8, 0x1AA, 0, &res);
  if (err != SDHCI_NO_ERROR && err != SDHCI_RESPONSE_TIMEOUT) {
    sdhcis[ctlr].card_version = SDHCI_CARD_TYPE_SDSC | SDHCI_CARD_TYPE_EMMC;
  } else if (res != 0x1AA) {
    sdhcis[ctlr].card_version = SDHCI_CARD_TYPE_SDSC;
  } else {
    sdhcis[ctlr].card_version = SDHCI_CARD_TYPE_SDHXUC;
  }

  /* Start initialisation process. ACMD41 */
  res = 0;
  while (!(res & SDHCI_OCR_READY)) {
    err = cmd_transfer(ctlr, CMD55, 0, 0, NULL);
    if (err != SDHCI_NO_ERROR) {
      if (sdhcis[ctlr].card_version | SDHCI_CARD_TYPE_EMMC) {
        sdhcis[ctlr].card_version = SDHCI_CARD_TYPE_EMMC;
        break;
      } else {
        return err;
      }
    }

    uint32_t arg = SDHCI_ACMD41_HCS | SDHCI_ACMD41_3V3 | (0x1FFU << 15U);
    err = cmd_transfer(ctlr, ACMD41, arg, 0, &res);
    if (err != SDHCI_NO_ERROR) {
      return err;
    }
  }

  if (sdhcis[ctlr].card_version == SDHCI_CARD_TYPE_EMMC) {
    return initialise_emmc(ctlr);
  }
  return initialise_sd(ctlr);
}

sdhci_error sdhci_open(int ctlr) {
  return initialise(ctlr);
}

sdhci_error sdhci_close(int ctlr) {
  sdhcis[ctlr].initialised = false;
  return SDHCI_NO_ERROR;
}

bool sdhci_initialised(int ctlr) {
  return sdhcis[ctlr].initialised;
}

sdhci_error sdhci_read(int ctlr, uint32_t sector, uint32_t count,
                       char* buffer) {
  SDHCI_DEBUG("sdhci (%d): read(sector = 0x%08x, count = %d, buffer = %p)\n",
              ctlr, sector, count, buffer);
  sdhci_error err;
  uint32_t res;
  if (sdhcis[ctlr].initialised == false) {
    sdhci_open(ctlr);
  }
  if (count == 0) {
    SDHCI_DEBUG("sdhci (%d): read() = %d\n", ctlr, SDHCI_NO_ERROR);
    return SDHCI_NO_ERROR;
  } else if (count == 1) {
    err = cmd_transfer(ctlr, CMD17, sector, 0, &res);
    if (err != SDHCI_NO_ERROR) {
      SDHCI_DEBUG("sdhci (%d): read() = %d\n", ctlr, err);
      return err;
    }
  } else {
    err = cmd_transfer(ctlr, CMD18, sector, count, &res);
    if (err != SDHCI_NO_ERROR) {
      SDHCI_DEBUG("sdhci (%d): read() = %d\n", ctlr, err);
      return err;
    }
  }
  err = read_data_transfer(ctlr, buffer);
  if (err != SDHCI_NO_ERROR) {
    SDHCI_DEBUG("sdhci (%d): read() = %d\n", ctlr, err);
    return err;
  }
  SDHCI_DEBUG("sdhci (%d): read() = %d\n", ctlr, SDHCI_NO_ERROR);
  return SDHCI_NO_ERROR;
}
