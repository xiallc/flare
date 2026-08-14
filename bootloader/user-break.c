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

#include <stdio.h>

#include <sleep.h>
#include <user-break.h>

#include <driver/uart/console.h>

bool user_break(size_t wait_seconds, char second_key) {
    volatile size_t seconds = 0;
    bool have_ctrl_c = false;
    while (seconds < wait_seconds) {
        ++seconds;
        printf("\b\b\b%2zu ", wait_seconds - seconds + 1);
        volatile size_t msecs = 0;
        while (msecs < 1000) {
            ++msecs;
            if (inbyte_available()) {
                uint8_t ch = inbyte();
                if (!have_ctrl_c) {
                    switch (ch) {
                        case '\x3':
                            have_ctrl_c = true;
                            if (second_key == '\x0') {
                                return true;
                            }
                            break;
                        case '\r':
                            wait_seconds += 4;
                            break;
                        case '\x1b':
                            seconds = wait_seconds;
                            msecs = 1000;
                            break;
                        default:
                            break;
                    }
                } else {
                    if (ch == second_key) {
                        return true;
                    }
                    switch (ch) {
                        case '\r':
                            wait_seconds += 4;
                            have_ctrl_c = false;
                            break;
                        case '\x1b':
                            seconds = wait_seconds;
                            msecs = 1000;
                            break;
                        default:
                            break;
                    }
                }
            }
            usleep(1000);
        }
    }
    return false;
}
