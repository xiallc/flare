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

/*---------------------------------------------------*/
/* Modified from :                                   */
/* Public Domain version of printf                   */
/* Rud Merriam, Compsult, Inc. Houston, Tx.          */
/* For Embedded Systems Programming, 1991            */
/*                                                   */
/*---------------------------------------------------*/
#include <ctype.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>

extern void outbyte(char c);

#define charptr char*

#undef isdigit
int isdigit(int c) {
  return (c >= '0') && (c <= '9');
}

#undef tolower
int tolower(int c) {
  if ((c >= 'A') && (c <= 'Z'))
    c = (c - 'A') + 'a';
  return c;
}

typedef struct params_s {
  int len;
  int num1;
  int num2;
  char pad_character;
  int do_padding;
  int left_flag;
} params_t;

/*---------------------------------------------------*/
/* The purpose of this routine is to output data the */
/* same as the standard printf function without the  */
/* overhead most run-time libraries involve. Usually */
/* the printf brings in many kilobytes of code and   */
/* that is unacceptable in most embedded systems.    */
/*---------------------------------------------------*/

/*---------------------------------------------------*/
/*                                                   */
/* This routine puts pad characters into the output  */
/* buffer.                                           */
/*                                                   */
static int padding(const int l_flag, params_t* par) {
  int i;
  int count = 0;

  if (par->do_padding && l_flag && (par->len < par->num1))
    for (i = par->len; i < par->num1; i++) {
      outbyte(par->pad_character);
      ++count;
    }

  return count;
}

/*---------------------------------------------------*/
/*                                                   */
/* This routine moves a string to the output buffer  */
/* as directed by the padding and positioning flags. */
/*                                                   */
static int outs(charptr lp, params_t* par) {
  int count;
  /* pad on left if needed                         */
  par->len = strlen(lp);
  count = padding(!(par->left_flag), par);

  /* Move string to the buffer                     */
  while (*lp && (par->num2)--) {
    outbyte(*lp++);
    ++count;
  }

  /* Pad on right if needed                        */
  /* CR 439175 - elided next stmt. Seemed bogus.   */
  /* par->len = strlen( lp);                       */
  count += padding(par->left_flag, par);

  return count;
}

int puts(const char* s) {
  int count = 0;
  while (*s != '\0') {
    outbyte(*s++);
    ++count;
  }
  outbyte('\n');
  return count + 1;
}

int putchar(int c) {
  outbyte(c);
  return 1;
}

/*---------------------------------------------------*/
/*                                                   */
/* This routine moves a number to the output buffer  */
/* as directed by the padding and positioning flags. */
/*                                                   */

static void outunum_neg(unsigned long num, const long base, const int negative,
                        params_t* par) {
  charptr cp;
  char outbuf[64];
  const char digits[] = "0123456789abcdef";

  /* Build number (backwards) in outbuf            */
  cp = outbuf;
  do {
    *cp++ = digits[(int)(num % base)];
  } while ((num /= base) > 0);
  if (negative)
    *cp++ = '-';
  *cp-- = 0;

  /* Move the converted number to the buffer and   */
  /* add in the padding where needed.              */
  par->len = strlen(outbuf);
  padding(!(par->left_flag), par);
  while (cp >= outbuf)
    outbyte(*cp--);
  padding(par->left_flag, par);
}

static void outnum(const long n, const long base, params_t* par) {
  int negative;
  unsigned long num;

  /* Check if number is negative                   */
  if (base == 10 && n < 0L) {
    negative = 1;
    num = -(n);
  } else {
    num = (n);
    negative = 0;
  }

  outunum_neg(num, base, negative, par);
}

/*---------------------------------------------------*/
/*                                                   */
/* This routine gets a number from the format        */
/* string.                                           */
/*                                                   */
int getnum(charptr* linep) {
  int n;
  charptr cp;

  n = 0;
  cp = *linep;
  while (isdigit(((int)*cp)))
    n = n * 10 + ((*cp++) - '0');
  *linep = cp;
  return (n);
}

/*---------------------------------------------------*/
/*                                                   */
/* This routine operates just like a printf/sprintf  */
/* routine. It outputs a set of data under the       */
/* control of a formatting string. Not all of the    */
/* standard C format control are supported. The ones */
/* provided are primarily those needed for embedded  */
/* systems work. Primarily the floaing point         */
/* routines are omitted. Other formats could be      */
/* added easily by following the examples shown for  */
/* the supported formats.                            */
/*                                                   */

/* void esp_printf( const func_ptr f_ptr,
   const charptr ctrl1, ...) */
int printf(const char* ctrl1, ...) {

  int long_flag;
  int size_t_flag;
  int dot_flag;
  int count = 0;

  params_t par;

  char ch;
  va_list argp;
  char* ctrl = (char*)ctrl1;

  va_start(argp, ctrl1);

  for (; *ctrl; ctrl++) {

    /* move format string chars to buffer until a  */
    /* format control is found.                    */
    if (*ctrl != '%') {
      outbyte(*ctrl);
      ++count;
      continue;
    }

    /* initialize all the flags for this format.   */
    dot_flag = long_flag = par.left_flag = par.do_padding = 0;
    par.pad_character = ' ';
    par.num2 = 32767;

  try_next:
    ch = *(++ctrl);

    if (isdigit((int)ch)) {
      if (dot_flag)
        par.num2 = getnum(&ctrl);
      else {
        if (ch == '0')
          par.pad_character = '0';

        par.num1 = getnum(&ctrl);
        par.do_padding = 1;
      }
      ctrl--;
      goto try_next;
    }

    switch (tolower((int)ch)) {
    case '%':
      outbyte('%');
      ++count;
      continue;

    case '-':
      par.left_flag = 1;
      break;

    case '.':
      dot_flag = 1;
      break;

    case 'l':
      long_flag = 1;
      break;

    case 'z':
      size_t_flag = 1;
      break;

    case 'd':
      if (long_flag || ch == 'D') {
        outnum(va_arg(argp, long), 10L, &par);
        continue;
      } else if (size_t_flag) {
        outnum(va_arg(argp, ssize_t), 10L, &par);
        continue;
      } else {
        outnum(va_arg(argp, int), 10L, &par);
        continue;
      }
    case 'u':
      if (long_flag || ch == 'U') {
        outnum(va_arg(argp, unsigned long), 10L, &par);
        continue;
      } else if (size_t_flag) {
        outnum(va_arg(argp, size_t), 10L, &par);
        continue;
      } else {
        outnum(va_arg(argp, unsigned int), 10L, &par);
        continue;
      }
    case 'p':
      outbyte('0');
      outbyte('x');
      outnum(va_arg(argp, uintptr_t), 16L, &par);
      continue;
    case 'x':
      if (long_flag) {
        outnum(va_arg(argp, long), 16L, &par);
        continue;
      } else if (size_t_flag) {
        outnum(va_arg(argp, ssize_t), 16L, &par);
        continue;
      } else {
        outnum(va_arg(argp, int), 16L, &par);
        continue;
      }
      continue;

    case 's':
      outs(va_arg(argp, char*), &par);
      continue;

    case 'c':
      outbyte(va_arg(argp, int));
      ++count;
      continue;

    case '\\':
      switch (*ctrl) {
      case 'a':
        outbyte(0x07);
        ++count;
        break;
      case 'h':
        outbyte(0x08);
        ++count;
        break;
      case 'r':
        outbyte(0x0D);
        ++count;
        break;
      case 'n':
        outbyte(0x0D);
        ++count;
        outbyte(0x0A);
        ++count;
        break;
      default:
        outbyte(*ctrl);
        ++count;
        break;
      }
      ctrl++;
      break;

    default:
      continue;
    }
    goto try_next;
  }
  va_end(argp);

  return count;
}

/*---------------------------------------------------*/
/* void esp_printf( const func_ptr f_ptr,
   const charptr ctrl1, ...) */
int sprintf(char* restrict str, const char* ctrl1, ...) {

  int long_flag;
  int size_t_flag;
  int dot_flag;
  int count = 0;

  params_t par;

  char ch;
  va_list argp;
  char* ctrl = (char*)ctrl1;

  va_start(argp, ctrl1);

  for (; *ctrl; ctrl++) {

    /* move format string chars to buffer until a  */
    /* format control is found.                    */
    if (*ctrl != '%') {
      str[count] = *ctrl;
      ++count;
      continue;
    }

    /* initialize all the flags for this format.   */
    dot_flag = long_flag = par.left_flag = par.do_padding = 0;
    par.pad_character = ' ';
    par.num2 = 32767;

  try_next:
    ch = *(++ctrl);

    if (isdigit((int)ch)) {
      if (dot_flag)
        par.num2 = getnum(&ctrl);
      else {
        if (ch == '0')
          par.pad_character = '0';

        par.num1 = getnum(&ctrl);
        par.do_padding = 1;
      }
      ctrl--;
      goto try_next;
    }

    switch (tolower((int)ch)) {
    case '%':
      str[count] = '%';
      ++count;
      continue;

    case '-':
      par.left_flag = 1;
      break;

    case '.':
      dot_flag = 1;
      break;

    case 'l':
      long_flag = 1;
      break;

    case 'z':
      size_t_flag = 1;
      break;

    case 'd':
      if (long_flag || ch == 'D') {
        outnum(va_arg(argp, long), 10L, &par);
        continue;
      } else if (size_t_flag) {
        outnum(va_arg(argp, ssize_t), 10L, &par);
        continue;
      } else {
        outnum(va_arg(argp, int), 10L, &par);
        continue;
      }
    case 'u':
      if (long_flag || ch == 'U') {
        outnum(va_arg(argp, unsigned long), 10L, &par);
        continue;
      } else if (size_t_flag) {
        outnum(va_arg(argp, size_t), 10L, &par);
        continue;
      } else {
        outnum(va_arg(argp, unsigned int), 10L, &par);
        continue;
      }
    case 'x':
      outnum((long)va_arg(argp, int), 16L, &par);
      continue;

    case 's':
      outs(va_arg(argp, char*), &par);
      continue;

    case 'c':
      str[count] = va_arg(argp, int);
      ++count;
      continue;

    case '\\':
      switch (*ctrl) {
      case 'a':
        str[count] = 0x07;
        ++count;
        break;
      case 'h':
        str[count] = 0x08;
        ++count;
        break;
      case 'r':
        str[count] = 0x0D;
        ++count;
        break;
      case 'n':
        str[count] = 0x0D;
        ++count;
        str[count] = 0x0A;
        ++count;
        break;
      default:
        str[count] = 0x0A;
        ++count;
        break;
      }
      ctrl++;
      break;

    default:
      continue;
    }
    goto try_next;
  }
  va_end(argp);

  return count;
}
