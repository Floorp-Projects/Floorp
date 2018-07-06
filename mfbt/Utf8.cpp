/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Types.h"
#include "mozilla/Utf8.h"

#include <stddef.h>
#include <stdint.h>

MFBT_API bool
mozilla::IsValidUtf8(const void* aCodeUnits, size_t aCount)
{
  const auto* s = static_cast<const unsigned char*>(aCodeUnits);
  const auto* limit = s + aCount;

  while (s < limit) {
    uint32_t n = *s++;

    // If the first byte is ASCII, it's the only one in the code point.  Have a
    // fast path that avoids all the rest of the work and looping in that case.
    if ((n & 0x80) == 0) {
      continue;
    }

    // The leading code unit determines the length of the next code point and
    // the number of bits of the leading code unit that contribute to the code
    // point's value.
    uint_fast8_t remaining;
    uint32_t min;
    if ((n & 0xE0) == 0xC0) {
      remaining = 1;
      min = 0x80;
      n &= 0x1F;
    } else if ((n & 0xF0) == 0xE0) {
      remaining = 2;
      min = 0x800;
      n &= 0x0F;
    } else if ((n & 0xF8) == 0xF0) {
      remaining = 3;
      min = 0x10000;
      n &= 0x07;
    } else {
      // UTF-8 used to have a hyper-long encoding form, but it's been removed
      // for years now.  So in this case, the string is not valid UTF-8.
      return false;
    }

    // If the code point would require more code units than remain, the encoding
    // is invalid.
    if (s + remaining > limit) {
      return false;
    }

    for (uint_fast8_t i = 0; i < remaining; i++) {
      // Every non-leading code unit in properly encoded UTF-8 has its high bit
      // set and the next-highest bit unset.
      if ((s[i] & 0xC0) != 0x80) {
        return false;
      }

      // The code point being encoded is the concatenation of all the
      // unconstrained bits.
      n = (n << 6) | (s[i] & 0x3F);
    }

    // Don't consider code points that are overlong, UTF-16 surrogates, or
    // exceed the maximum code point to be valid.
    if (n < min || (0xD800 <= n && n < 0xE000) || n >= 0x110000) {
      return false;
    }

    s += remaining;
  }

  return true;
}
