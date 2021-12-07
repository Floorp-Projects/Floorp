/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstdarg>

#ifdef _WIN32
#  include <windows.h>
#else
#  include <unistd.h>
#endif
#include <cmath>
#include <cstring>
#include "mozilla/Assertions.h"
#include "mozilla/Unused.h"
#include "FdPrintf.h"

/* Template class allowing a limited number of increments on a value */
template <typename T>
class CheckedIncrement {
 public:
  CheckedIncrement(T aValue, size_t aMaxIncrement)
      : mValue(aValue), mMaxIncrement(aMaxIncrement) {}

  T operator++(int) {
    if (!mMaxIncrement) {
      MOZ_CRASH("overflow detected");
    }
    mMaxIncrement--;
    return mValue++;
  }

  T& operator++() {
    (*this)++;
    return mValue;
  }

  void advance(T end) {
    // Only makes sense if T is a pointer type.
    size_t diff = end - mValue;
    if (diff > mMaxIncrement) {
      MOZ_CRASH("overflow detected");
    }
    mMaxIncrement -= diff;
    mValue = end;
  };

  void rewind(T pos) {
    size_t diff = mValue - pos;
    mMaxIncrement += diff;
    mValue = pos;
  }

  operator T() { return mValue; }
  T value() { return mValue; }

 private:
  T mValue;
  size_t mMaxIncrement;
};

template <typename T>
static unsigned NumDigits(T n) {
  if (n < 1) {
    // We want one digit, it will be 0.
    return 1;
  }

  double l = log10(static_cast<double>(n));
  double cl = ceil(l);
  return l == cl ? unsigned(cl) + 1 : unsigned(cl);
}

static void LeftPad(CheckedIncrement<char*>& b, size_t pad) {
  while (pad-- > 0) {
    *(b++) = ' ';
  }
}

// Write the digits into the buffer.
static void WriteDigits(CheckedIncrement<char*>& b, size_t i,
                        size_t num_digits) {
  size_t x = pow(10, double(num_digits - 1));
  do {
    *(b++) = "0123456789"[(i / x) % 10];
    x /= 10;
  } while (x > 0);
}

void FdPrintf(intptr_t aFd, const char* aFormat, ...) {
  if (aFd == 0) {
    return;
  }
  char buf[256];
  CheckedIncrement<char*> b(buf, sizeof(buf));
  CheckedIncrement<const char*> f(aFormat, strlen(aFormat) + 1);
  va_list ap;
  va_start(ap, aFormat);
  while (true) {
    switch (*f) {
      case '\0':
        goto out;

      case '%': {
        // The start of the format specifier is used if this specifier is
        // invalid.
        const char* start = f;

        // Read the field width
        f++;
        char* end = nullptr;
        size_t width = strtoul(f, &end, 10);
        // If strtol can't find a number that's okay, that means 0 in our
        // case, but we must advance f).
        f.advance(end);

        switch (*f) {
          case 'z': {
            if (*(++f) == 'u') {
              size_t i = va_arg(ap, size_t);

              size_t num_digits = NumDigits(i);
              LeftPad(b, width > num_digits ? width - num_digits : 0);
              WriteDigits(b, i, num_digits);
            } else {
              // If the format specifier is unknown then write out '%' and
              // rewind to the beginning of the specifier causing it to be
              // printed normally.
              *(b++) = '%';
              f.rewind(start);
            }
            break;
          }

          case 'p': {
            intptr_t ptr = va_arg(ap, intptr_t);
            *(b++) = '0';
            *(b++) = 'x';
            int x = sizeof(intptr_t) * 8;
            bool wrote_msb = false;
            do {
              x -= 4;
              size_t hex_digit = ptr >> x & 0xf;
              if (hex_digit || wrote_msb) {
                *(b++) = "0123456789abcdef"[hex_digit];
                wrote_msb = true;
              }
            } while (x > 0);
            if (!wrote_msb) {
              *(b++) = '0';
            }
            break;
          }

          case 's': {
            const char* str = va_arg(ap, const char*);
            size_t len = strlen(str);

            LeftPad(b, width > len ? width - len : 0);

            while (*str) {
              *(b++) = *(str++);
            }

            break;
          }

          case '%':
            // Print a single raw '%'.
            *(b++) = '%';
            break;

          default:
            // If the format specifier is unknown then write out '%' and
            // rewind to the beginning of the specifier causing it to be
            // printed normally.
            *(b++) = '%';
            f.rewind(start);
            break;
        }
        break;
      }
      default:
        *(b++) = *f;
        break;
    }
    f++;
  }
out:
#ifdef _WIN32
  // See comment in FdPrintf.h as to why WriteFile is used.
  DWORD written;
  WriteFile(reinterpret_cast<HANDLE>(aFd), buf, b - buf, &written, nullptr);
#else
  MOZ_UNUSED(write(aFd, buf, b - buf));
#endif
  va_end(ap);
}
