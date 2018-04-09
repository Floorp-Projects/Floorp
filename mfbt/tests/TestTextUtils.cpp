/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/TextUtils.h"

using mozilla::IsAsciiAlpha;

// char

static_assert(!IsAsciiAlpha('@'), "'@' isn't ASCII alpha");
static_assert('@' == 0x40, "'@' has value 0x40");

static_assert('A' == 0x41, "'A' has value 0x41");
static_assert(IsAsciiAlpha('A'), "'A' is ASCII alpha");
static_assert(IsAsciiAlpha('B'), "'B' is ASCII alpha");
static_assert(IsAsciiAlpha('M'), "'M' is ASCII alpha");
static_assert(IsAsciiAlpha('Y'), "'Y' is ASCII alpha");
static_assert(IsAsciiAlpha('Z'), "'Z' is ASCII alpha");

static_assert('Z' == 0x5A, "'Z' has value 0x5A");
static_assert('[' == 0x5B, "'[' has value 0x5B");
static_assert(!IsAsciiAlpha('['), "'[' isn't ASCII alpha");

static_assert(!IsAsciiAlpha('`'), "'`' isn't ASCII alpha");
static_assert('`' == 0x60, "'`' has value 0x60");

static_assert('a' == 0x61, "'a' has value 0x61");
static_assert(IsAsciiAlpha('a'), "'a' is ASCII alpha");
static_assert(IsAsciiAlpha('b'), "'b' is ASCII alpha");
static_assert(IsAsciiAlpha('m'), "'m' is ASCII alpha");
static_assert(IsAsciiAlpha('y'), "'y' is ASCII alpha");
static_assert(IsAsciiAlpha('z'), "'z' is ASCII alpha");

static_assert('z' == 0x7A, "'z' has value 0x7A");
static_assert('{' == 0x7B, "'{' has value 0x7B");
static_assert(!IsAsciiAlpha('{'), "'{' isn't ASCII alpha");

// char16_t

static_assert(!IsAsciiAlpha(u'@'), "u'@' isn't ASCII alpha");
static_assert(u'@' == 0x40, "u'@' has value 0x40");

static_assert(u'A' == 0x41, "u'A' has value 0x41");
static_assert(IsAsciiAlpha(u'A'), "u'A' is ASCII alpha");
static_assert(IsAsciiAlpha(u'B'), "u'B' is ASCII alpha");
static_assert(IsAsciiAlpha(u'M'), "u'M' is ASCII alpha");
static_assert(IsAsciiAlpha(u'Y'), "u'Y' is ASCII alpha");
static_assert(IsAsciiAlpha(u'Z'), "u'Z' is ASCII alpha");

static_assert(u'Z' == 0x5A, "u'Z' has value 0x5A");
static_assert(u'[' == 0x5B, "u'[' has value 0x5B");
static_assert(!IsAsciiAlpha(u'['), "u'[' isn't ASCII alpha");

static_assert(!IsAsciiAlpha(u'`'), "u'`' isn't ASCII alpha");
static_assert(u'`' == 0x60, "u'`' has value 0x60");

static_assert(u'a' == 0x61, "u'a' has value 0x61");
static_assert(IsAsciiAlpha(u'a'), "u'a' is ASCII alpha");
static_assert(IsAsciiAlpha(u'b'), "u'b' is ASCII alpha");
static_assert(IsAsciiAlpha(u'm'), "u'm' is ASCII alpha");
static_assert(IsAsciiAlpha(u'y'), "u'y' is ASCII alpha");
static_assert(IsAsciiAlpha(u'z'), "u'z' is ASCII alpha");

static_assert(u'z' == 0x7A, "u'z' has value 0x7A");
static_assert(u'{' == 0x7B, "u'{' has value 0x7B");
static_assert(!IsAsciiAlpha(u'{'), "u'{' isn't ASCII alpha");

// char32_t

static_assert(!IsAsciiAlpha(U'@'), "U'@' isn't ASCII alpha");
static_assert(U'@' == 0x40, "U'@' has value 0x40");

static_assert(U'A' == 0x41, "U'A' has value 0x41");
static_assert(IsAsciiAlpha(U'A'), "U'A' is ASCII alpha");
static_assert(IsAsciiAlpha(U'B'), "U'B' is ASCII alpha");
static_assert(IsAsciiAlpha(U'M'), "U'M' is ASCII alpha");
static_assert(IsAsciiAlpha(U'Y'), "U'Y' is ASCII alpha");
static_assert(IsAsciiAlpha(U'Z'), "U'Z' is ASCII alpha");

static_assert(U'Z' == 0x5A, "U'Z' has value 0x5A");
static_assert(U'[' == 0x5B, "U'[' has value 0x5B");
static_assert(!IsAsciiAlpha(U'['), "U'[' isn't ASCII alpha");

static_assert(!IsAsciiAlpha(U'`'), "U'`' isn't ASCII alpha");
static_assert(U'`' == 0x60, "U'`' has value 0x60");

static_assert(U'a' == 0x61, "U'a' has value 0x61");
static_assert(IsAsciiAlpha(U'a'), "U'a' is ASCII alpha");
static_assert(IsAsciiAlpha(U'b'), "U'b' is ASCII alpha");
static_assert(IsAsciiAlpha(U'm'), "U'm' is ASCII alpha");
static_assert(IsAsciiAlpha(U'y'), "U'y' is ASCII alpha");
static_assert(IsAsciiAlpha(U'z'), "U'z' is ASCII alpha");

static_assert(U'z' == 0x7A, "U'z' has value 0x7A");
static_assert(U'{' == 0x7B, "U'{' has value 0x7B");
static_assert(!IsAsciiAlpha(U'{'), "U'{' isn't ASCII alpha");

int
main()
{
  return 0;
}
