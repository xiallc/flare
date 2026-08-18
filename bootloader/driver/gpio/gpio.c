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

/**
 * General Purpose IO.
 */

#include "gpio.h"

#include <driver/io/board-io.h>

#define GPIO_BASE         (0xe000a000)
#define GPIO_DIR_BASE     (GPIO_BASE + 0x204)
#define GPIO_DIR_OFF      (0x040)
#define GPIO_OUTEN_BASE   (GPIO_BASE + 0x208)
#define GPIO_OUTEN_OFF    (0x040)
#define GPIO_MASK_BASE    (GPIO_BASE + 0x000)
#define GPIO_DATA_RO_BASE (GPIO_BASE + 0x060)

static uint32_t gpio_bank(int pin) {
  return pin > 31 ? 1 : 0;
}

static void gpio_set_output_enable(int pin, bool state) {
  const uint32_t reg = GPIO_OUTEN_BASE + (gpio_bank(pin) * GPIO_OUTEN_OFF);
  board_reg_write(reg, board_reg_read(reg) |
                           (state ? 1 : 0) << (pin > 31 ? pin - 32 : pin));
}

static void gpio_set_direction(int pin, bool state) {
  const uint32_t reg = GPIO_DIR_BASE + (gpio_bank(pin) * GPIO_DIR_OFF);
  board_reg_write(reg, board_reg_read(reg) |
                           (state ? 1 : 0) << (pin > 31 ? pin - 32 : pin));
}

gpio_error gpio_setup_pin(const gpio_pin_def* pin) {
  uint32_t value;

  if ((pin->pin < 0) || (pin->pin > GPIO_MAX_PINS))
    return GPIO_INVALID_PIN;

  switch (pin->volts) {
  case gpio_LVCMOS18:
    value = (1 << 9);
    break;
  case gpio_LVCMOS25:
    value = (2 << 9);
    break;
  case gpio_LVCMOS33:
    value = (3 << 9);
    break;
  case gpio_HSTL:
    value = (4 << 9) | (pin->output ? (1 << 13) : 0);
    break;
  default:
    return GPIO_INVALID_VOLTAGE;
  }

  value |= pin->pullup ? (1 << 12) : 0;
  value |= pin->fast ? (1 << 8) : 0;
  value |= pin->tristate ? (1 << 0) : 0;

  board_reg_write(0xF8000008, 0xDF0D);
  board_reg_write(0xF8000700 + (pin->pin * 4), value);
  board_reg_write(0xF8000004, 0x767B);

  gpio_set_direction(pin->pin, pin->output);
  if (pin->output) {
    gpio_set_output_enable(pin->pin, pin->outen);
    gpio_output(pin->pin, pin->on);
  }

  return GPIO_NO_ERROR;
}

gpio_error gpio_setup_pins(const gpio_pin_def* pins, size_t size) {
  size_t pin;
  for (pin = 0; pin < size; ++pin, ++pins) {
    gpio_error ge = gpio_setup_pin(pins);
    if (ge != GPIO_NO_ERROR)
      return ge;
  }
  return GPIO_NO_ERROR;
}

gpio_error gpio_output_enable(int pin, bool on) {
  if ((pin < 0) || (pin > GPIO_MAX_PINS))
    return GPIO_INVALID_PIN;

  gpio_set_output_enable(pin, on);

  return GPIO_NO_ERROR;
}

gpio_error gpio_output(int pin, bool on) {
  uint32_t reg;

  if ((pin < 0) || (pin > GPIO_MAX_PINS))
    return GPIO_INVALID_PIN;

  reg = GPIO_MASK_BASE + ((pin / 16) * 4);
  pin %= 16;

  board_reg_write(reg, ((~(1 << pin) & 0xffff) << 16) | ((on ? 1 : 0) << pin));

  return GPIO_NO_ERROR;
}

gpio_error gpio_input(int pin, bool* on) {
  uint32_t reg;

  if ((pin < 0) || (pin > GPIO_MAX_PINS))
    return GPIO_INVALID_PIN;

  reg = GPIO_DATA_RO_BASE + ((pin / 32) * 4);
  pin %= 32;

  *on = (board_reg_read(reg) & (1 << pin)) == 0 ? false : true;

  return GPIO_NO_ERROR;
}
