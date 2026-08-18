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

#include <stdio.h>
#include <unistd.h>

#include <io/board-io.h>

#include "flash.h"
#include "versal-flash.h"

#define FLASH_4BYTE_ADDRESSING 1

#if FLASH_4BYTE_ADDRESSING
#define FLASH_INSTRUCTION_LENGTH (5) /* 1 Command Byte, 4 Address Bytes */
#else
#define FLASH_INSTRUCTION_LENGTH (4) /* 1 Command Byte, 3 Address Bytes */
#endif

#define STRIPE   1
#define NOSTRIPE 0

#define EXP_SIZE  1
#define IMMD_SIZE 0

#define FLASH_COMM_METHOD_SINGLE FLASH_COMM_METHOD_SINGLE_TOP
#define FLASH_COMM_METHOD_ALL    FLASH_COMM_METHOD_SINGLE_PARALLEL

void print_ISR() {
  uint32_t isr = *((uint32_t*)(qspi_base + GQSPI_ISR_OFST));

  printf("GQSPI_ISR:       %8x\n", isr);
  printf("Gen_FIFO_full:      %1x     ", ((isr >> 10) & 1u));
  printf("RX_FIFO_full:       %1x\n", ((isr >> 5) & 1u));
  printf("Gen_FIFO_Empty:     %1x     ", ((isr >> 7) & 1u));
  printf("RX_FIFO_Empty:      %1x\n", ((isr >> 11) & 1u));
  printf("Gen_FIFO_not_full:  %1x     ", ((isr >> 9) & 1u));
  printf("RX_FIFO_not_empty:  %1x\n", ((isr >> 4) & 1u));
  printf("TX_FIFO_full:       %1x\n", ((isr >> 3) & 1u));
  printf("TX_FIFO_Empty:      %1x\n", ((isr >> 8) & 1u));
  printf("TX_FIFO_not_full:   %1x\n\n", ((isr >> 2) & 1u));
}
void print_regs() {

  printf("        ---------- GQSPI Registers ----------\n");
  printf("GQSPI_Cfg:       %8x     ",
         *((uint32_t*)(qspi_base + GQSPI_CONFIG_OFST)));
  printf("QSPI_REF_CTRL:   %8x\n", *((uint32_t*)(0xF1260118)));
  printf("GQSPI_IMR:       %8x     ",
         *((uint32_t*)(qspi_base + GQSPI_IMASK_OFST)));
  printf("GQSPI_En:        %8x\n", *((uint32_t*)(qspi_base + GQSPI_EN_OFST)));
  printf("GQSPI_GF_Thresh: %8x     ",
         *((uint32_t*)(qspi_base + GQSPI_GF_THRESHOLD_OFST)));
  printf("GQSPI_TX_Thresh: %8x\n",
         *((uint32_t*)(qspi_base + GQSPI_TX_THRESHOLD_OFST)));
  printf("GQSPI_RX_Thresh: %8x     ",
         *((uint32_t*)(qspi_base + GQSPI_RX_THRESHOLD_OFST)));
  print_ISR();
  printf("\n");
}

static uint32_t command_word_generator(uint8_t imm_data, uint8_t data_xfer,
                                       uint8_t exp, uint8_t mode,
                                       uint8_t cs_lower, uint8_t cs_upper,
                                       uint8_t bus_sel, uint8_t tx, uint8_t rx,
                                       uint8_t stripe, uint8_t poll) {
  uint32_t command = 0UL;

  /* Data clean */
  data_xfer &= 1U;
  exp &= 1U;
  mode &= 3U;
  cs_lower &= 1U;
  cs_upper &= 1U;
  bus_sel &= 3U;
  tx &= 1U;
  rx &= 1U;
  stripe &= 1U;
  poll &= 1U;

  command |= ((uint32_t)imm_data) << CMD_OFFSET_IMM_DATA;
  command |= ((uint32_t)data_xfer) << CMD_OFFSET_DATA_XFER;
  command |= ((uint32_t)exp) << CMD_OFFSET_EXP;
  command |= ((uint32_t)mode) << CMD_OFFSET_MODE;
  command |= ((uint32_t)cs_lower) << CMD_OFFSET_CS_LOWER;
  command |= ((uint32_t)cs_upper) << CMD_OFFSET_CS_UPPER;
  command |= ((uint32_t)bus_sel) << CMD_OFFSET_BUS_SEL;
  command |= ((uint32_t)tx) << CMD_OFFSET_TX;
  command |= ((uint32_t)rx) << CMD_OFFSET_RX;
  command |= ((uint32_t)stripe) << CMD_OFFSET_STRIPE;
  command |= ((uint32_t)poll) << CMD_OFFSET_POLL;

  return command;
}

static uint32_t command_wrapper(uint8_t parallel, uint8_t imm_data, uint8_t exp,
                                uint8_t txrx, uint8_t stripe) {
  uint8_t bus;
  uint8_t cs_upper;
  uint8_t cs_lower;
  if (parallel == FLASH_COMM_METHOD_PARALLEL) {
    bus = (uint8_t)GQSPI_SELECT_FLASH_BUS_BOTH;
    cs_upper = 1;
    cs_lower = 1;
  } else if (parallel == FLASH_COMM_METHOD_SINGLE_TOP) {
    bus = (uint8_t)GQSPI_SELECT_FLASH_BUS_UPPER;
    cs_upper = 1;
    cs_lower = 0;
    stripe = 0;
  } else {
    bus = (uint8_t)GQSPI_SELECT_FLASH_BUS_LOWER;
    cs_upper = 0;
    cs_lower = 1;
    stripe = 0;
  }
  return command_word_generator(
      imm_data, (uint8_t)1, exp, (uint8_t)GQSPI_SELECT_MODE_SPI, cs_lower,
      cs_upper, bus, (txrx ^ 1U), txrx, stripe, (uint8_t)0);
}

void flash_writeUnlock(void) {
  return;
}

void flash_writeLock(void) {
  return;
}

static void qspi_FlushRx(void) {
  qspi_reg_write(GQSPI_FIFO_CTRL_OFST, GQSPI_FIFO_CTRL_RST_RX_MASK);
  /*
   * Wait for FIFO to clear and Empty flag to raise.
   */
  while (!(qspi_reg_read(GQSPI_ISR_OFST) & GQSPI_ISR_RXEMPTY_MASK))
    ;
}

static void qspi_FlushTx(void) {
  qspi_reg_write(GQSPI_FIFO_CTRL_OFST, GQSPI_FIFO_CTRL_RST_TX_MASK);
  /*
   * Wait for FIFO to clear and Empty flag to raise.
   */
  while (!(qspi_reg_read(GQSPI_ISR_OFST) & GQSPI_ISR_TXEMPTY_MASK))
    ;
}

static void qspi_FlushGen(void) {
  qspi_reg_write(GQSPI_FIFO_CTRL_OFST, GQSPI_FIFO_CTRL_RST_GEN_MASK);
  /*
   * Wait for FIFO to clear and Empty flag to raise.
   */
  while (!(qspi_reg_read(GQSPI_ISR_OFST) & GQSPI_ISR_GENFIFOEMPTY_MASK))
    ;
}

static void qspi_FlushAll(void) {
  qspi_FlushRx();
  qspi_FlushTx();
  qspi_FlushGen();
}

static void align_command(flash_transfer_buffer* transfer) {
  size_t i;

  if (transfer->padding == 0) {
    return;
  }

  for (i = transfer->padding; i < transfer->length; i++) {
    transfer->buffer[i - transfer->padding] = transfer->buffer[i];
  }
}

static void align_data(flash_transfer_buffer* transfer) {
  size_t i;

  if (transfer->command_len == 0) {
    return;
  }

  for (i = transfer->command_len; i < transfer->length; i++) {
    transfer->buffer[i - transfer->command_len] = transfer->buffer[i];
  }
}

flash_error flash_Transfer(flash_transfer_buffer* transfer, bool initialised) {
  uint32_t* tx_data;
  uint32_t* rx_data;
  size_t tx_length;
  size_t rx_length;
  size_t padding;
  uint32_t tx_reg;
  size_t sending = 0;
  bool start = false;
  uint32_t sr;
  uint32_t controller_command;
  uint8_t trans_dir;
  size_t length;
  size_t transfer_length = 0;
  uint32_t x = 0;
  uint32_t header_len;
  uint32_t communication_method;

  /*
   * Enable QSPI.
   */
  qspi_reg_write(GQSPI_EN_OFST, GQSPI_EN_MASK);

  if (initialised == false) {
    qspi_reg_write(GQSPI_CONFIG_OFST, QSPI_CONFIG_INIT_VAL);
    qspi_reg_write(GQSPI_SEL_OFST, GQSPI_SEL_MASK);
    qspi_reg_write(GQSPI_IDR_OFST, GQSPI_IDR_ALL_MASK);
    qspi_reg_write(GQSPI_LPBK_DLY_ADJ_OFST, GQSPI_LPBK_DLY_ADJ_USE_LPBK_MASK);
    initialised = true;
  }

  if (transfer->trans_dir == FLASH_TX_TRANS) {
    flash_transfer_trace("transfer:TX", transfer);
  } else {
    flash_transfer_trace("transfer:RX", transfer);
  }

  /*
   * Flush Rx, Tx and Command Generator FIFOs
   */
  qspi_FlushAll();

  /*
   * The RX pointer can never catch up and overtake the TX pointer.
   */
  tx_data = (uint32_t*)transfer->buffer;
  rx_data = (uint32_t*)transfer->buffer;
  tx_length = transfer->length;
  rx_length = transfer->length;
  length = transfer->length;
  padding = transfer->padding;

  trans_dir = (uint8_t)transfer->trans_dir;
  header_len = transfer->command_len;
  communication_method = transfer->comm_method;

  /* Send command and address bytes if present */
  controller_command = command_wrapper(communication_method, header_len,
                                       IMMD_SIZE, FLASH_TX_TRANS, NOSTRIPE);
  qspi_reg_write(GQSPI_GEN_FIFO_OFST, controller_command);

  while (x < header_len) {

    qspi_reg_write(GQSPI_TXD_OFST, *tx_data);
    ++tx_data;
    sr = qspi_reg_read(GQSPI_ISR_OFST);
    while ((sr & GQSPI_ISR_TXEMPTY_MASK) != 0) {
      sr = qspi_reg_read(GQSPI_ISR_OFST);
    }
    x = x + sizeof(uint32_t);
  }

  if (trans_dir == FLASH_TX_TRANS) {
    if (tx_length < x) {
      tx_length = 0;
    } else {
      tx_length = tx_length - x;
    }
  }

  length = tx_length;
  if (length == 0) {
    qspi_reg_write(GQSPI_CONFIG_OFST, qspi_reg_read(GQSPI_CONFIG_OFST) |
                                          GQSPI_CFG_START_GEN_FIFO_MASK);
  }

  /* Command generation */
  while (length != 0) {
    if (length <= 0xFF) {
      /*
       * If transfer is less then or equal to 255 bytes
       * transfer the exact amount.
       */
      transfer_length = length;
      length = length - transfer_length;

      controller_command = command_wrapper(
          communication_method, transfer_length, IMMD_SIZE, trans_dir, STRIPE);
    } else {
      /*
       * If transfer is more then 255 bytes find the largest
       * power of two in the length of bytes and transfer that
       * amount using the EXP form
       */
      for (uint32_t i = 31; i > 7; i--) {
        if ((1 << i) & length) {
          transfer_length = i;
          length = ~(1 << i) & length;
          break;
        }
      }

      controller_command = command_wrapper(
          communication_method, transfer_length, EXP_SIZE, trans_dir, STRIPE);
    }

    qspi_reg_write(GQSPI_GEN_FIFO_OFST, controller_command);

    sr = qspi_reg_read(GQSPI_ISR_OFST);
    if ((sr & GQSPI_ISR_GENFIFOFULL_MASK) != 0) {
      return FLASH_BUFFER_OVERFLOW;
    }
  }

  sr = qspi_reg_read(GQSPI_ISR_OFST);

  if (trans_dir == FLASH_TX_TRANS) {
    while (tx_length) {
      while (tx_length && (sr & GQSPI_ISR_TXFULL_MASK) == 0) {
        usleep(100);
        qspi_reg_write(GQSPI_TXD_OFST, *tx_data);
        ++tx_data;
        if (tx_length > sizeof(uint32_t))
          tx_length -= sizeof(uint32_t);
        else
          tx_length = 0;

        sr = qspi_reg_read(GQSPI_ISR_OFST);
      }
      qspi_reg_write(GQSPI_CONFIG_OFST, qspi_reg_read(GQSPI_CONFIG_OFST) |
                                            GQSPI_CFG_START_GEN_FIFO_MASK);
    }
  }

  if (trans_dir == FLASH_RX_TRANS) {
    qspi_reg_write(GQSPI_CONFIG_OFST, qspi_reg_read(GQSPI_CONFIG_OFST) |
                                          GQSPI_CFG_START_GEN_FIFO_MASK);

    sr = qspi_reg_read(GQSPI_ISR_OFST);
    while ((sr & GQSPI_ISR_RXEMPTY_MASK) != 0) {
      sr = qspi_reg_read(GQSPI_ISR_OFST);
    }

    sr = qspi_reg_read(GQSPI_ISR_OFST);
    while ((sr & GQSPI_ISR_RXEMPTY_MASK) == 0) {
      *rx_data = qspi_reg_read(GQSPI_RXD_OFST);
      ++rx_data;
      if (rx_length > sizeof(uint32_t))
        rx_length -= sizeof(uint32_t);
      else
        rx_length = 0;

      sr = qspi_reg_read(GQSPI_ISR_OFST);
    }
  }

  /*
   * Disable QSPI
   */
  qspi_reg_write(GQSPI_EN_OFST, 0);

  if (transfer->trans_dir == FLASH_RX_TRANS) {
    flash_transfer_trace("transfer end:RX", transfer);
  }

  return FLASH_NO_ERROR;
}
