/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstdarg>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
#include <cstring>
#include "mozilla/Assertions.h"
#include "mozilla/Unused.h"

/* Template class allowing a limited number of increments on a value */
template<typename T>
class CheckedIncrement
{
public:
  CheckedIncrement(T aValue, size_t aMaxIncrement)
    : mValue(aValue)
    , mMaxIncrement(aMaxIncrement)
  {
  }

  T operator++(int)
  {
    if (!mMaxIncrement) {
      MOZ_CRASH("overflow detected");
    }
    mMaxIncrement--;
    return mValue++;
  }

  T& operator++()
  {
    (*this)++;
    return mValue;
  }

  operator T() { return mValue; }

private:
  T mValue;
  size_t mMaxIncrement;
};

void
FdPrintf(intptr_t aFd, const char* aFormat, ...)
{
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

      case '%':
        switch (*++f) {
          case 'z': {
            if (*(++f) == 'u') {
              size_t i = va_arg(ap, size_t);
              size_t x = 1;
              // Compute the number of digits.
              while (x <= i / 10) {
                x *= 10;
              }
              // Write the digits into the buffer.
              do {
                *(b++) = "0123456789"[(i / x) % 10];
                x /= 10;
              } while (x > 0);
            } else {
              // Write out the format specifier if it's unknown.
              *(b++) = '%';
              *(b++) = 'z';
              *(b++) = *f;
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

          default:
            // Write out the format specifier if it's unknown.
            *(b++) = '%';
            *(b++) = *f;
            break;
        }
        break;

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
