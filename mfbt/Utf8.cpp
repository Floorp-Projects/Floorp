/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Maybe.h"
#include "mozilla/TextUtils.h"
#include "mozilla/Types.h"
#include "mozilla/Utf8.h"

#include <stddef.h>
#include <stdint.h>

MFBT_API bool
mozilla::IsValidUtf8(const void* aCodeUnits, size_t aCount)
{
  const auto* s = static_cast<const unsigned char*>(aCodeUnits);
  const auto* const limit = s + aCount;

  while (s < limit) {
    unsigned char c = *s++;

    // If the first byte is ASCII, it's the only one in the code point.  Have a
    // fast path that avoids all the rest of the work and looping in that case.
    if (IsAscii(c)) {
      continue;
    }

    Maybe<char32_t> maybeCodePoint =
      DecodeOneUtf8CodePoint(Utf8Unit(c), &s, limit);
    if (maybeCodePoint.isNothing()) {
      return false;
    }
  }

  MOZ_ASSERT(s == limit);
  return true;
}
