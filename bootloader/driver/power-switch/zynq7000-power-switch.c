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
 * Flare Power Switch.
 */

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include <driver/gpio/gpio.h>
#include <driver/io/board-io.h>
#include <driver/leds/leds.h>
#include <driver/power-switch/power-switch.h>
#include <driver/slcr/board-slcr.h>
#include <driver/uart/console.h>

/*
 * Number of seconds before acknowledging it has been pressed.
 */
#define POWER_ON_SECONDS (2)

/*
 * Console keys to enter simulate a power switch.
 */
const uint8_t unlock_keys[] = {'\x3', '\x6'};
#define UNLOCK_KEYS sizeof(unlock_keys)

/*
 * GPIO configuration fields.
 */
#define GPIO_PULLUP_ENABLE   (1 << 12)
#define GPIO_LVCMOS33        (3 << 9)
#define GPIO_SLOW_CMOS_EDGE  (0 << 8)
#define GPIO_L3_MUX_SEL_GPIO (0 << 5)
#define GPIO_L2_MUX_SEL_L3   (0 << 3)
#define GPIO_L1_MUX_SEL_L2   (0 << 2)
#define GPIO_L0_MUX_SEL_L1   (0 << 1)
#define GPIO_TRI_DISABLE     (0 << 0)

/*
 * Number of power pins to monitor.
 */
#define POWER_PINS (2)

/*
 * Power pin configuration.
 */
typedef struct {
  uint32_t pin;
  uint32_t config;
} power_pin_config;

static uint32_t gpio_config_defaults[POWER_PINS];
static const gpio_pin_def gpio_PowerPins[2] = {{
                                                 pin : 9,
                                                 output : false,
                                                 on : false,
                                                 outen : false,
                                                 volts : gpio_LVCMOS33,
                                                 pullup : true,
                                                 fast : false,
                                                 tristate : false
                                               },
                                               {
                                                 pin : 11,
                                                 output : false,
                                                 on : false,
                                                 outen : false,
                                                 volts : gpio_LVCMOS33,
                                                 pullup : true,
                                                 fast : false,
                                                 tristate : false
                                               }};

#define POWER_PIN_CONFIG                                                       \
  (GPIO_PULLUP_ENABLE | GPIO_LVCMOS33 | GPIO_TRI_DISABLE |                     \
   GPIO_L3_MUX_SEL_GPIO | GPIO_L2_MUX_SEL_L3 | GPIO_L1_MUX_SEL_L2 |            \
   GPIO_L0_MUX_SEL_L1)

static const power_pin_config power_pin_configs[POWER_PINS] = {
    {.pin = 9, .config = POWER_PIN_CONFIG},
    {.pin = 11, .config = POWER_PIN_CONFIG}};

static inline uint32_t read_gpio_config(const int pin) {
  uint32_t value = board_reg_read(0xf8000700 + (pin * 4));
  return value;
}

static inline void write_gpio_config(const int pin, const uint32_t value) {
  board_reg_write(0xf8000700 + (pin * 4), value);
}

static void gpio_config_restore(void) {
  int pin;
  board_slcr_unlock();
  for (pin = 0; pin < POWER_PINS; ++pin)
    write_gpio_config(power_pin_configs[pin].pin, gpio_config_defaults[pin]);
  board_slcr_lock();
}

static bool gpio_config_setup(void) {
  gpio_error ge;
  int pin;

  for (pin = 0; pin < POWER_PINS; ++pin)
    gpio_config_defaults[pin] = read_gpio_config(power_pin_configs[pin].pin);

  ge = gpio_setup_pins(&gpio_PowerPins[0], 2);

  return ge == GPIO_NO_ERROR;
}

static bool gpio_power_pressed(void) {
  int pin;
  for (pin = 0; pin < POWER_PINS; ++pin) {
    bool high = true;
    gpio_input(gpio_PowerPins[pin].pin, &high);
    if (!high)
      return true;
  }
  return false;
}

static inline bool have_char(void) {
  return inbyte_available();
}

static inline uint8_t get_char(void) {
  return (uint8_t)inbyte();
}

bool flare_power_on_pressed(void) {
  volatile uint32_t seconds = 0;
  bool pressed = true;
  int unlock = 0;

  if (!gpio_config_setup())
    return false;

  led_normal();

  printf("Factory Mode:     (^c^f)\b\b\b\b\b\b\b\b");

  while (seconds++ < POWER_ON_SECONDS) {
    volatile uint32_t msecs = 0;
    int unlock_last = unlock;

    printf("\b\b%1" PRIu32 " ", seconds);

    while (msecs++ < 1000) {
      pressed = gpio_power_pressed();

      if (have_char()) {
        uint8_t ch = get_char();
        if (ch == unlock_keys[unlock]) {
          ++unlock;
          if (unlock == UNLOCK_KEYS) {
            pressed = true;
            seconds = POWER_ON_SECONDS;
            break;
          }
        } else {
          unlock = 0;
        }
      }

      if (msecs == 500)
        led_toggle();

      usleep(1000);
    }

    if (unlock != unlock_last) {
      pressed = true;
      unlock_last = unlock;
    }
  }

  printf("        \b\b\b\b\b\b\b\b\b\b%s\n", pressed ? "yes" : "no");

  gpio_config_restore();

  led_normal();

  return pressed;
}
