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
 * Some brain dead versions of libc functions we need.
 */

#include <stdint.h>
#include <string.h>

size_t strlen(const char* s) {
  size_t c = 0;
  while (*s++)
    ++c;
  return c;
}

void* memcpy(void* dst, const void* src, size_t len) {
#if SIZE_OVER_SPEED
  volatile uint8_t* ud = dst;
  volatile const uint8_t* us = src;
  while (len--)
    *ud++ = *us++;
  return dst;
#else
#define UNALIGNED(X, Y)                                                        \
  (((uintptr_t)X & (sizeof(uintptr_t) - 1)) |                                  \
   ((uintptr_t)Y & (sizeof(uintptr_t) - 1)))
#define BIGBLOCKSIZE    (sizeof(uint32_t) << 2)
#define LITTLEBLOCKSIZE (sizeof(uint32_t))
#define TOO_SMALL(LEN)  ((LEN) < BIGBLOCKSIZE)

  volatile uint8_t* ud = dst;
  volatile const uint8_t* us = src;
  volatile uint32_t* ad;
  volatile const uint32_t* as;

  if (!TOO_SMALL(len) && !UNALIGNED(us, ud)) {
    ad = (uint32_t*)ud;
    as = (uint32_t*)us;

    while (len >= BIGBLOCKSIZE) {
      *ad++ = *as++;
      *ad++ = *as++;
      *ad++ = *as++;
      *ad++ = *as++;
      len -= BIGBLOCKSIZE;
    }

    while (len >= LITTLEBLOCKSIZE) {
      *ad++ = *as++;
      len -= LITTLEBLOCKSIZE;
    }

    ud = (uint8_t*)ad;
    us = (uint8_t*)as;
  }

  while (len--)
    *ud++ = *us++;

  return dst;
#endif
}

void* memmove(void* dst, const void* src, size_t len) {
  volatile uint8_t* ud = dst;
  volatile const uint8_t* us = src;
  if ((us < ud) && (ud < (us + len))) {
    us += len;
    ud += len;
    while (len--)
      *--ud = *--us;
  } else {
    while (len--)
      *ud++ = *us++;
  }
  return dst;
}

void* memset(void* dst, int c, size_t len) {
  volatile uint8_t* ud = dst;
  while (len--)
    *ud++ = c;
  return dst;
}

int memcmp(const void* b1, const void* b2, size_t len) {
  const volatile uint8_t* ub1 = b1;
  const volatile uint8_t* ub2 = b2;
  while (len--) {
    if (*ub1 != *ub2) {
      return *ub1 - *ub2;
    }
    ub1++;
    ub2++;
  }
  return 0;
}
