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

#if !defined(_GPIO_H_)
#define _GPIO_H_

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*
 * Max number of pins.
 */
#define GPIO_MAX_PINS (53)

/*
 * Voltages.
 */
typedef enum {
  gpio_LVCMOS18,
  gpio_LVCMOS25,
  gpio_LVCMOS33,
  gpio_HSTL
} gpio_pin_volts;

/*
 * Pin definition.
 */
typedef struct {
  int pin;              /* The pin number. */
  bool output;          /* True for output, false for input. */
  bool on;              /* True default output is on */
  bool outen;           /* True for output enable on, false for off. */
  gpio_pin_volts volts; /* The voltage level. */
  bool pullup;          /* True to enable the pull up. */
  bool fast;            /* True to be a fast CMOS edge */
  bool tristate;        /* True to enable tri-state. */
} gpio_pin_def;

/*
 * Error codes.
 */
typedef enum {
  GPIO_NO_ERROR = 0,
  GPIO_INVALID_PIN,
  GPIO_INVALID_VOLTAGE
} gpio_error;

gpio_error gpio_setup_pin(const gpio_pin_def* pin);
gpio_error gpio_setup_pins(const gpio_pin_def* pins, size_t size);
gpio_error gpio_output_enable(int pin, bool on);
gpio_error gpio_output(int pin, bool on);
gpio_error gpio_input(int pin, bool* on);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
