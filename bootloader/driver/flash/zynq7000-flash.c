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

#include "zynq7000-flash.h"
#include "flash-board.h"

#define GPIO_FLASH_WD_PIN  (4)
#define GPIO_FLASH_WD_BANK (0)
#define GPIO_FLASH_WD_EN   (1)
#define GPIO_FLASH_WD_DIS  (0)

#define GPIO_BASE       (0xe000a000)
#define GPIO_DIR_BASE   (GPIO_BASE + 0x204)
#define GPIO_DIR_OFF    (0x040)
#define GPIO_OUTEN_BASE (GPIO_BASE + 0x208)
#define GPIO_OUTEN_OFF  (0x040)
#define GPIO_MASK_BASE  (GPIO_BASE + 0x000)

size_t flash_get_padding(size_t length) {
  return (4 - (length & 3)) & 3;
}

void flash_writeUnlock(void) {
  uint32_t reg;

  board_reg_write(0xF8000008, 0xDF0D);
  board_reg_write(0xF8000700 + (GPIO_FLASH_WD_PIN * 4), 0x1600);
  board_reg_write(0xF8000004, 0x767B);

  reg = GPIO_OUTEN_BASE + (GPIO_FLASH_WD_BANK * GPIO_DIR_OFF);
  board_reg_write(reg, board_reg_read(reg) | (1 << GPIO_FLASH_WD_PIN));
  reg = GPIO_OUTEN_BASE + (GPIO_FLASH_WD_BANK * GPIO_OUTEN_OFF);
  board_reg_write(reg, board_reg_read(reg) | (1 << GPIO_FLASH_WD_PIN));

  reg = GPIO_MASK_BASE + ((GPIO_FLASH_WD_PIN / 16) * 4);
  board_reg_write(reg, ((~(1 << GPIO_FLASH_WD_PIN) & 0xffff) << 16) |
                           (GPIO_FLASH_WD_DIS << GPIO_FLASH_WD_PIN));
}

void flash_writeLock(void) {
  uint32_t reg;
  reg = GPIO_MASK_BASE + ((GPIO_FLASH_WD_PIN / 16) * 4);
  board_reg_write(reg, ((~(1 << GPIO_FLASH_WD_PIN) & 0xffff) << 16) |
                           (GPIO_FLASH_WD_EN << GPIO_FLASH_WD_PIN));
}

static void qspi_FlushRx(void) {
  while (true) {
    if ((qspi_reg_read(QSPI_REG_INTR_STATUS) & QSPI_IXR_RXNEMPTY) == 0)
      break;
    qspi_reg_read(QSPI_REG_RX_DATA);
  }
}

flash_error flash_Transfer(flash_transfer_buffer* transfer, bool initialised) {
  uint32_t* tx_data;
  uint32_t* rx_data;
  size_t tx_length;
  size_t rx_length;
  uint32_t tx_reg;
  size_t sending = 0;
  bool start = false;
  uint32_t sr;

  if (initialised == false) {
    qspi_reg_write(QSPI_REG_EN, 0);
    qspi_reg_write(QSPI_REG_LSPI_CFG, 0x00a002eb);
    qspi_FlushRx();
    qspi_reg_write(QSPI_REG_CONFIG, QSPI_CR_SSFORCE | QSPI_CR_MANSTRTEN |
                                        QSPI_CR_HOLDB_DR | QSPI_CR_BAUD_RATE |
                                        QSPI_CR_MODE_SEL);
    initialised = true;
  }

  flash_transfer_trace("transfer:TX", transfer);

  /*
   * Set the slave select.
   */
  qspi_reg_write(QSPI_REG_CONFIG,
                 qspi_reg_read(QSPI_REG_CONFIG) & ~QSPI_CR_PCS);

  /*
   * Enable SPI.
   */
  qspi_reg_write(QSPI_REG_EN, QSPI_EN_SPI_ENABLE);

  /*
   * The RX pointer can never catch up and overtake the TX pointer.
   */
  tx_data = (uint32_t*)transfer->buffer;
  rx_data = (uint32_t*)transfer->buffer;
  tx_length = transfer->length;
  rx_length = transfer->length;

  /*
   * The buffer to right aligned, that is padding is add to the front of the
   * buffer to get the correct aligment for the instruction size. This means
   * the data in the first "transfer" is not aligned in the correct bits for
   * the keyhole access to the FIFO.
   */
  switch (transfer->padding) {
  case 3:
    *tx_data >>= 24;
    tx_reg = QSPI_REG_TXD1;
    tx_length -= 1;
    sending += 1;
    start = true;
    break;
  case 2:
    *tx_data >>= 16;
    tx_reg = QSPI_REG_TXD2;
    tx_length -= 2;
    sending += 2;
    start = true;
    break;
  case 1:
    *tx_data >>= 8;
    tx_reg = QSPI_REG_TXD3;
    tx_length -= 3;
    sending += 3;
    start = true;
    break;
  default:
    tx_reg = QSPI_REG_TXD0;
    tx_length -= 4;
    sending += 4;
    if (tx_length == 0)
      start = true;
    break;
  }

  qspi_reg_write(tx_reg, *tx_data);
  ++tx_data;

  if (start) {
    qspi_reg_write(QSPI_REG_CONFIG,
                   qspi_reg_read(QSPI_REG_CONFIG) | QSPI_CR_MANSTRT);

    sr = qspi_reg_read(QSPI_REG_INTR_STATUS);
    while ((sr & QSPI_IXR_TXOW) == 0) {
      sr = qspi_reg_read(QSPI_REG_INTR_STATUS);
    }
  }

  while (tx_length || rx_length) {
    sr = qspi_reg_read(QSPI_REG_INTR_STATUS);

    if (rx_length) {
      while (start && sending) {
        if ((sr & QSPI_IXR_RXNEMPTY) != 0) {
          *rx_data = qspi_reg_read(QSPI_REG_RX_DATA);
          ++rx_data;
          if (rx_length > sizeof(uint32_t))
            rx_length -= sizeof(uint32_t);
          else
            rx_length = 0;
          if (sending > sizeof(uint32_t))
            sending -= sizeof(uint32_t);
          else
            sending = 0;
        }

        sr = qspi_reg_read(QSPI_REG_INTR_STATUS);
      }
    }

    if (tx_length) {
      start = false;
      while (tx_length && ((sr & QSPI_IXR_TXFULL) == 0)) {
        qspi_reg_write(QSPI_REG_TXD0, *tx_data);
        ++tx_data;
        if (tx_length > sizeof(uint32_t))
          tx_length -= sizeof(uint32_t);
        else
          tx_length = 0;
        sending += sizeof(uint32_t);
        start = true;

        sr = qspi_reg_read(QSPI_REG_INTR_STATUS);
      }

      if (start) {
        qspi_reg_write(QSPI_REG_CONFIG,
                       qspi_reg_read(QSPI_REG_CONFIG) | QSPI_CR_MANSTRT);
      }
    }
  }

  /*
   * Skip the command byte.
   */
  flash_transfer_buffer_skip(transfer, 1);

  /*
   * Disable the slave select.
   */
  qspi_reg_write(QSPI_REG_CONFIG, qspi_reg_read(QSPI_REG_CONFIG) | QSPI_CR_PCS);

  /*
   * Disable SPI.
   */
  qspi_reg_write(QSPI_REG_EN, 0);

  flash_transfer_trace("transfer:RX", transfer);

  return FLASH_NO_ERROR;
}
