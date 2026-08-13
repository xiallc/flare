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
 * Xilinx Zynq UART.
 */

#include <stdint.h>

#include <driver/io/board-io.h>
#include <driver/uart/board-uart.h>

#define ZYNQ_UART_SR         (0x2c)
#define ZYNQ_UART_FIFO       (0x30)

#define ZYNQ_UART_SR_TXFULL  (1 << 4)
#define ZYNQ_UART_SR_TXEMPTY (1 << 3)
#define ZYNQ_UART_SR_RXEMPTY (1 << 1)

void
board_uart_send(uint32_t uart, uint8_t c)
{
  while ((board_reg_read(uart + ZYNQ_UART_SR) & ZYNQ_UART_SR_TXFULL) != 0);
  board_reg_write(uart + ZYNQ_UART_FIFO, c);
}

bool
board_uart_tx_idle(uint32_t uart)
{
  return (board_reg_read(uart + ZYNQ_UART_SR) & ZYNQ_UART_SR_TXEMPTY) != 0;
}

bool
board_uart_receive_idle(uint32_t uart)
{
  return (board_reg_read(uart + ZYNQ_UART_SR) & ZYNQ_UART_SR_RXEMPTY) != 0;
}

uint8_t
board_uart_receive(uint32_t uart)
{
  while (board_uart_receive_idle(uart));
  return board_reg_read(uart + ZYNQ_UART_FIFO);
}


void
board_uart_setup(uint32_t uart)
{
  /*
   * Do nothing at this point in time. Set up is painted by the PS7.
   */
  (void) uart;
}
