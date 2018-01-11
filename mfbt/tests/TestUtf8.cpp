/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Utf8.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/Assertions.h"

using mozilla::ArrayLength;
using mozilla::IsValidUtf8;
using mozilla::Utf8Unit;

static void
TestUtf8Unit()
{
  Utf8Unit c('A');
  MOZ_RELEASE_ASSERT(c.toChar() == 'A');
  MOZ_RELEASE_ASSERT(c == Utf8Unit('A'));
  MOZ_RELEASE_ASSERT(c != Utf8Unit('B'));
  MOZ_RELEASE_ASSERT(c.toUint8() == 0x41);

  unsigned char asUnsigned = 'A';
  MOZ_RELEASE_ASSERT(c.toUnsignedChar() == asUnsigned);
  MOZ_RELEASE_ASSERT(Utf8Unit('B').toUnsignedChar() != asUnsigned);

  Utf8Unit first('@');
  Utf8Unit second('#');

  MOZ_RELEASE_ASSERT(first != second);

  first = second;
  MOZ_RELEASE_ASSERT(first == second);
}

static void
TestIsValidUtf8()
{
  // Note we include the U+0000 NULL in this one -- and that's fine.
  static const char asciiBytes[] = u8"How about a nice game of chess?";
  MOZ_RELEASE_ASSERT(IsValidUtf8(asciiBytes, ArrayLength(asciiBytes)));

  static const char endNonAsciiBytes[] = u8"Life is like a 🌯";
  MOZ_RELEASE_ASSERT(IsValidUtf8(endNonAsciiBytes, ArrayLength(endNonAsciiBytes) - 1));

  static const unsigned char badLeading[] = { 0x80 };
  MOZ_RELEASE_ASSERT(!IsValidUtf8(badLeading, ArrayLength(badLeading)));

  // Byte-counts

  // 1
  static const char oneBytes[] = u8"A"; // U+0041 LATIN CAPITAL LETTER A
  constexpr size_t oneBytesLen = ArrayLength(oneBytes);
  static_assert(oneBytesLen == 2, "U+0041 plus nul");
  MOZ_RELEASE_ASSERT(IsValidUtf8(oneBytes, oneBytesLen));

  // 2
  static const char twoBytes[] = u8"؆"; // U+0606 ARABIC-INDIC CUBE ROOT
  constexpr size_t twoBytesLen = ArrayLength(twoBytes);
  static_assert(twoBytesLen == 3, "U+0606 in two bytes plus nul");
  MOZ_RELEASE_ASSERT(IsValidUtf8(twoBytes, twoBytesLen));

  // 3
  static const char threeBytes[] = u8"᨞"; // U+1A1E BUGINESE PALLAWA
  constexpr size_t threeBytesLen = ArrayLength(threeBytes);
  static_assert(threeBytesLen == 4, "U+1A1E in three bytes plus nul");
  MOZ_RELEASE_ASSERT(IsValidUtf8(threeBytes, threeBytesLen));

  // 4
  static const char fourBytes[] = u8"🁡"; // U+1F061 DOMINO TILE HORIZONTAL-06-06
  constexpr size_t fourBytesLen = ArrayLength(fourBytes);
  static_assert(fourBytesLen == 5, "U+1F061 in four bytes plus nul");
  MOZ_RELEASE_ASSERT(IsValidUtf8(fourBytes, fourBytesLen));

  // Max code point
  static const char maxCodePoint[] = u8"􏿿"; // U+10FFFF
  constexpr size_t maxCodePointLen = ArrayLength(maxCodePoint);
  static_assert(maxCodePointLen == 5, "U+10FFFF in four bytes plus nul");
  MOZ_RELEASE_ASSERT(IsValidUtf8(maxCodePoint, maxCodePointLen));

  // One past max code point
  static unsigned const char onePastMaxCodePoint[] = { 0xF4, 0x90, 0x80, 0x80 };
  constexpr size_t onePastMaxCodePointLen = ArrayLength(onePastMaxCodePoint);
  MOZ_RELEASE_ASSERT(!IsValidUtf8(onePastMaxCodePoint, onePastMaxCodePointLen));

  // Surrogate-related testing

  static const unsigned char justBeforeSurrogates[] = { 0xED, 0x9F, 0xBF };
  MOZ_RELEASE_ASSERT(IsValidUtf8(justBeforeSurrogates, ArrayLength(justBeforeSurrogates)));

  static const unsigned char leastSurrogate[] = { 0xED, 0xA0, 0x80 };
  MOZ_RELEASE_ASSERT(!IsValidUtf8(leastSurrogate, ArrayLength(leastSurrogate)));

  static const unsigned char arbitraryHighSurrogate[] = { 0xED, 0xA2, 0x87 };
  MOZ_RELEASE_ASSERT(!IsValidUtf8(arbitraryHighSurrogate, ArrayLength(arbitraryHighSurrogate)));

  static const unsigned char arbitraryLowSurrogate[] = { 0xED, 0xB7, 0xAF };
  MOZ_RELEASE_ASSERT(!IsValidUtf8(arbitraryLowSurrogate, ArrayLength(arbitraryLowSurrogate)));

  static const unsigned char greatestSurrogate[] = { 0xED, 0xBF, 0xBF };
  MOZ_RELEASE_ASSERT(!IsValidUtf8(greatestSurrogate, ArrayLength(greatestSurrogate)));

  static const unsigned char justAfterSurrogates[] = { 0xEE, 0x80, 0x80 };
  MOZ_RELEASE_ASSERT(IsValidUtf8(justAfterSurrogates, ArrayLength(justAfterSurrogates)));
}

int
main()
{
  TestUtf8Unit();
  TestIsValidUtf8();
  return 0;
}
