/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/TextUtils.h"

using mozilla::IsAsciiAlpha;

int
main()
{
  // char

  MOZ_RELEASE_ASSERT(!IsAsciiAlpha('@'), "'@' isn't ASCII alpha");
  MOZ_RELEASE_ASSERT('@' == 0x40, "'@' has value 0x40");

  MOZ_RELEASE_ASSERT('A' == 0x41, "'A' has value 0x41");
  MOZ_RELEASE_ASSERT(IsAsciiAlpha('A'), "'A' is ASCII alpha");
  MOZ_RELEASE_ASSERT(IsAsciiAlpha('B'), "'B' is ASCII alpha");
  MOZ_RELEASE_ASSERT(IsAsciiAlpha('M'), "'M' is ASCII alpha");
  MOZ_RELEASE_ASSERT(IsAsciiAlpha('Y'), "'Y' is ASCII alpha");
  MOZ_RELEASE_ASSERT(IsAsciiAlpha('Z'), "'Z' is ASCII alpha");

  MOZ_RELEASE_ASSERT('Z' == 0x5A, "'Z' has value 0x5A");
  MOZ_RELEASE_ASSERT('[' == 0x5B, "'[' has value 0x5B");
  MOZ_RELEASE_ASSERT(!IsAsciiAlpha('['), "'[' isn't ASCII alpha");

  MOZ_RELEASE_ASSERT(!IsAsciiAlpha('`'), "'`' isn't ASCII alpha");
  MOZ_RELEASE_ASSERT('`' == 0x60, "'`' has value 0x60");

  MOZ_RELEASE_ASSERT('a' == 0x61, "'a' has value 0x61");
  MOZ_RELEASE_ASSERT(IsAsciiAlpha('a'), "'a' is ASCII alpha");
  MOZ_RELEASE_ASSERT(IsAsciiAlpha('b'), "'b' is ASCII alpha");
  MOZ_RELEASE_ASSERT(IsAsciiAlpha('m'), "'m' is ASCII alpha");
  MOZ_RELEASE_ASSERT(IsAsciiAlpha('y'), "'y' is ASCII alpha");
  MOZ_RELEASE_ASSERT(IsAsciiAlpha('z'), "'z' is ASCII alpha");

  MOZ_RELEASE_ASSERT('z' == 0x7A, "'z' has value 0x7A");
  MOZ_RELEASE_ASSERT('{' == 0x7B, "'{' has value 0x7B");
  MOZ_RELEASE_ASSERT(!IsAsciiAlpha('{'), "'{' isn't ASCII alpha");

  // char16_t

  MOZ_RELEASE_ASSERT(!IsAsciiAlpha(u'@'), "u'@' isn't ASCII alpha");
  MOZ_RELEASE_ASSERT(u'@' == 0x40, "u'@' has value 0x40");

  MOZ_RELEASE_ASSERT(u'A' == 0x41, "u'A' has value 0x41");
  MOZ_RELEASE_ASSERT(IsAsciiAlpha(u'A'), "u'A' is ASCII alpha");
  MOZ_RELEASE_ASSERT(IsAsciiAlpha(u'B'), "u'B' is ASCII alpha");
  MOZ_RELEASE_ASSERT(IsAsciiAlpha(u'M'), "u'M' is ASCII alpha");
  MOZ_RELEASE_ASSERT(IsAsciiAlpha(u'Y'), "u'Y' is ASCII alpha");
  MOZ_RELEASE_ASSERT(IsAsciiAlpha(u'Z'), "u'Z' is ASCII alpha");

  MOZ_RELEASE_ASSERT(u'Z' == 0x5A, "u'Z' has value 0x5A");
  MOZ_RELEASE_ASSERT(u'[' == 0x5B, "u'[' has value 0x5B");
  MOZ_RELEASE_ASSERT(!IsAsciiAlpha(u'['), "u'[' isn't ASCII alpha");

  MOZ_RELEASE_ASSERT(!IsAsciiAlpha(u'`'), "u'`' isn't ASCII alpha");
  MOZ_RELEASE_ASSERT(u'`' == 0x60, "u'`' has value 0x60");

  MOZ_RELEASE_ASSERT(u'a' == 0x61, "u'a' has value 0x61");
  MOZ_RELEASE_ASSERT(IsAsciiAlpha(u'a'), "u'a' is ASCII alpha");
  MOZ_RELEASE_ASSERT(IsAsciiAlpha(u'b'), "u'b' is ASCII alpha");
  MOZ_RELEASE_ASSERT(IsAsciiAlpha(u'm'), "u'm' is ASCII alpha");
  MOZ_RELEASE_ASSERT(IsAsciiAlpha(u'y'), "u'y' is ASCII alpha");
  MOZ_RELEASE_ASSERT(IsAsciiAlpha(u'z'), "u'z' is ASCII alpha");

  MOZ_RELEASE_ASSERT(u'z' == 0x7A, "u'z' has value 0x7A");
  MOZ_RELEASE_ASSERT(u'{' == 0x7B, "u'{' has value 0x7B");
  MOZ_RELEASE_ASSERT(!IsAsciiAlpha(u'{'), "u'{' isn't ASCII alpha");

  // char32_t

  MOZ_RELEASE_ASSERT(!IsAsciiAlpha(U'@'), "U'@' isn't ASCII alpha");
  MOZ_RELEASE_ASSERT(U'@' == 0x40, "U'@' has value 0x40");

  MOZ_RELEASE_ASSERT(U'A' == 0x41, "U'A' has value 0x41");
  MOZ_RELEASE_ASSERT(IsAsciiAlpha(U'A'), "U'A' is ASCII alpha");
  MOZ_RELEASE_ASSERT(IsAsciiAlpha(U'B'), "U'B' is ASCII alpha");
  MOZ_RELEASE_ASSERT(IsAsciiAlpha(U'M'), "U'M' is ASCII alpha");
  MOZ_RELEASE_ASSERT(IsAsciiAlpha(U'Y'), "U'Y' is ASCII alpha");
  MOZ_RELEASE_ASSERT(IsAsciiAlpha(U'Z'), "U'Z' is ASCII alpha");

  MOZ_RELEASE_ASSERT(U'Z' == 0x5A, "U'Z' has value 0x5A");
  MOZ_RELEASE_ASSERT(U'[' == 0x5B, "U'[' has value 0x5B");
  MOZ_RELEASE_ASSERT(!IsAsciiAlpha(U'['), "U'[' isn't ASCII alpha");

  MOZ_RELEASE_ASSERT(!IsAsciiAlpha(U'`'), "U'`' isn't ASCII alpha");
  MOZ_RELEASE_ASSERT(U'`' == 0x60, "U'`' has value 0x60");

  MOZ_RELEASE_ASSERT(U'a' == 0x61, "U'a' has value 0x61");
  MOZ_RELEASE_ASSERT(IsAsciiAlpha(U'a'), "U'a' is ASCII alpha");
  MOZ_RELEASE_ASSERT(IsAsciiAlpha(U'b'), "U'b' is ASCII alpha");
  MOZ_RELEASE_ASSERT(IsAsciiAlpha(U'm'), "U'm' is ASCII alpha");
  MOZ_RELEASE_ASSERT(IsAsciiAlpha(U'y'), "U'y' is ASCII alpha");
  MOZ_RELEASE_ASSERT(IsAsciiAlpha(U'z'), "U'z' is ASCII alpha");

  MOZ_RELEASE_ASSERT(U'z' == 0x7A, "U'z' has value 0x7A");
  MOZ_RELEASE_ASSERT(U'{' == 0x7B, "U'{' has value 0x7B");
  MOZ_RELEASE_ASSERT(!IsAsciiAlpha(U'{'), "U'{' isn't ASCII alpha");

  return 0;
}
