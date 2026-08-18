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
 * usleep as in <unistd,h>
 */

#include <unistd.h>

#include <stdio.h>

#include "board-timer.h"

int usleep(useconds_t useconds) {
  uint64_t end;
  uint64_t now;

  board_timer_get(&now);
  end = now + ((uint64_t)useconds);
  do {
    board_timer_get(&now);
  } while (now < end);

  return 0;
}
