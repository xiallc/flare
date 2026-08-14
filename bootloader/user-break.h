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

/*
 * User break on the console.
 */

#if !defined(USER_BREAK_H)
#define USER_BREAK_H

#include <stdint.h>

/*
 * Wait the number of seconds for a break. Set second_key to `\x0' to
 * disable.
 */
bool user_break(size_t wair_seconds, char second_key);

#endif
