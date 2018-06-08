/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/TextUtils.h"

using mozilla::AsciiAlphanumericToNumber;
using mozilla::IsAscii;
using mozilla::IsAsciiAlpha;
using mozilla::IsAsciiAlphanumeric;
using mozilla::IsAsciiDigit;
using mozilla::IsAsciiLowercaseAlpha;
using mozilla::IsAsciiUppercaseAlpha;

static void
TestIsAscii()
{
  // char

  static_assert(!IsAscii(char(-1)), "char(-1) isn't ASCII");

  static_assert(IsAscii('\0'), "nul is ASCII");

  static_assert(IsAscii('A'), "'A' is ASCII");
  static_assert(IsAscii('B'), "'B' is ASCII");
  static_assert(IsAscii('M'), "'M' is ASCII");
  static_assert(IsAscii('Y'), "'Y' is ASCII");
  static_assert(IsAscii('Z'), "'Z' is ASCII");

  static_assert(IsAscii('['), "'[' is ASCII");
  static_assert(IsAscii('`'), "'`' is ASCII");

  static_assert(IsAscii('a'), "'a' is ASCII");
  static_assert(IsAscii('b'), "'b' is ASCII");
  static_assert(IsAscii('m'), "'m' is ASCII");
  static_assert(IsAscii('y'), "'y' is ASCII");
  static_assert(IsAscii('z'), "'z' is ASCII");

  static_assert(IsAscii('{'), "'{' is ASCII");

  static_assert(IsAscii('5'), "'5' is ASCII");

  static_assert(IsAscii('\x7F'), "'\\x7F' is ASCII");
  static_assert(!IsAscii('\x80'), "'\\x80' isn't ASCII");

  // char16_t

  static_assert(!IsAscii(char16_t(-1)), "char16_t(-1) isn't ASCII");

  static_assert(IsAscii(u'\0'), "nul is ASCII");

  static_assert(IsAscii(u'A'), "u'A' is ASCII");
  static_assert(IsAscii(u'B'), "u'B' is ASCII");
  static_assert(IsAscii(u'M'), "u'M' is ASCII");
  static_assert(IsAscii(u'Y'), "u'Y' is ASCII");
  static_assert(IsAscii(u'Z'), "u'Z' is ASCII");

  static_assert(IsAscii(u'['), "u'[' is ASCII");
  static_assert(IsAscii(u'`'), "u'`' is ASCII");

  static_assert(IsAscii(u'a'), "u'a' is ASCII");
  static_assert(IsAscii(u'b'), "u'b' is ASCII");
  static_assert(IsAscii(u'm'), "u'm' is ASCII");
  static_assert(IsAscii(u'y'), "u'y' is ASCII");
  static_assert(IsAscii(u'z'), "u'z' is ASCII");

  static_assert(IsAscii(u'{'), "u'{' is ASCII");

  static_assert(IsAscii(u'5'), "u'5' is ASCII");

  static_assert(IsAscii(u'\x7F'), "u'\\x7F' is ASCII");
  static_assert(!IsAscii(u'\x80'), "u'\\x80' isn't ASCII");

  // char32_t

  static_assert(!IsAscii(char32_t(-1)), "char32_t(-1) isn't ASCII");

  static_assert(IsAscii(U'\0'), "nul is ASCII");

  static_assert(IsAscii(U'A'), "U'A' is ASCII");
  static_assert(IsAscii(U'B'), "U'B' is ASCII");
  static_assert(IsAscii(U'M'), "U'M' is ASCII");
  static_assert(IsAscii(U'Y'), "U'Y' is ASCII");
  static_assert(IsAscii(U'Z'), "U'Z' is ASCII");

  static_assert(IsAscii(U'['), "U'[' is ASCII");
  static_assert(IsAscii(U'`'), "U'`' is ASCII");

  static_assert(IsAscii(U'a'), "U'a' is ASCII");
  static_assert(IsAscii(U'b'), "U'b' is ASCII");
  static_assert(IsAscii(U'm'), "U'm' is ASCII");
  static_assert(IsAscii(U'y'), "U'y' is ASCII");
  static_assert(IsAscii(U'z'), "U'z' is ASCII");

  static_assert(IsAscii(U'{'), "U'{' is ASCII");

  static_assert(IsAscii(U'5'), "U'5' is ASCII");

  static_assert(IsAscii(U'\x7F'), "U'\\x7F' is ASCII");
  static_assert(!IsAscii(U'\x80'), "U'\\x80' isn't ASCII");
}

static void
TestIsAsciiAlpha()
{
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

  static_assert(!IsAsciiAlpha('5'), "'5' isn't ASCII alpha");

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

  static_assert(!IsAsciiAlpha(u'5'), "u'5' isn't ASCII alpha");

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

  static_assert(!IsAsciiAlpha(U'5'), "U'5' isn't ASCII alpha");
}

static void
TestIsAsciiUppercaseAlpha()
{
  // char

  static_assert(!IsAsciiUppercaseAlpha('@'), "'@' isn't ASCII alpha uppercase");
  static_assert('@' == 0x40, "'@' has value 0x40");

  static_assert('A' == 0x41, "'A' has value 0x41");
  static_assert(IsAsciiUppercaseAlpha('A'), "'A' is ASCII alpha uppercase");
  static_assert(IsAsciiUppercaseAlpha('B'), "'B' is ASCII alpha uppercase");
  static_assert(IsAsciiUppercaseAlpha('M'), "'M' is ASCII alpha uppercase");
  static_assert(IsAsciiUppercaseAlpha('Y'), "'Y' is ASCII alpha uppercase");
  static_assert(IsAsciiUppercaseAlpha('Z'), "'Z' is ASCII alpha uppercase");

  static_assert('Z' == 0x5A, "'Z' has value 0x5A");
  static_assert('[' == 0x5B, "'[' has value 0x5B");
  static_assert(!IsAsciiUppercaseAlpha('['), "'[' isn't ASCII alpha uppercase");

  static_assert(!IsAsciiUppercaseAlpha('`'), "'`' isn't ASCII alpha uppercase");
  static_assert(!IsAsciiUppercaseAlpha('a'), "'a' is ASCII alpha uppercase");
  static_assert(!IsAsciiUppercaseAlpha('b'), "'b' is ASCII alpha uppercase");
  static_assert(!IsAsciiUppercaseAlpha('m'), "'m' is ASCII alpha uppercase");
  static_assert(!IsAsciiUppercaseAlpha('y'), "'y' is ASCII alpha uppercase");
  static_assert(!IsAsciiUppercaseAlpha('z'), "'z' is ASCII alpha uppercase");
  static_assert(!IsAsciiUppercaseAlpha('{'), "'{' isn't ASCII alpha uppercase");

  // char16_t

  static_assert(!IsAsciiUppercaseAlpha(u'@'), "u'@' isn't ASCII alpha uppercase");
  static_assert(u'@' == 0x40, "u'@' has value 0x40");

  static_assert(u'A' == 0x41, "u'A' has value 0x41");
  static_assert(IsAsciiUppercaseAlpha(u'A'), "u'A' is ASCII alpha uppercase");
  static_assert(IsAsciiUppercaseAlpha(u'B'), "u'B' is ASCII alpha uppercase");
  static_assert(IsAsciiUppercaseAlpha(u'M'), "u'M' is ASCII alpha uppercase");
  static_assert(IsAsciiUppercaseAlpha(u'Y'), "u'Y' is ASCII alpha uppercase");
  static_assert(IsAsciiUppercaseAlpha(u'Z'), "u'Z' is ASCII alpha uppercase");

  static_assert(u'Z' == 0x5A, "u'Z' has value 0x5A");
  static_assert(u'[' == 0x5B, "u'[' has value 0x5B");
  static_assert(!IsAsciiUppercaseAlpha(u'['), "u'[' isn't ASCII alpha uppercase");

  static_assert(!IsAsciiUppercaseAlpha(u'`'), "u'`' isn't ASCII alpha uppercase");
  static_assert(!IsAsciiUppercaseAlpha(u'a'), "u'a' is ASCII alpha uppercase");
  static_assert(!IsAsciiUppercaseAlpha(u'b'), "u'b' is ASCII alpha uppercase");
  static_assert(!IsAsciiUppercaseAlpha(u'm'), "u'm' is ASCII alpha uppercase");
  static_assert(!IsAsciiUppercaseAlpha(u'y'), "u'y' is ASCII alpha uppercase");
  static_assert(!IsAsciiUppercaseAlpha(u'z'), "u'z' is ASCII alpha uppercase");
  static_assert(!IsAsciiUppercaseAlpha(u'{'), "u'{' isn't ASCII alpha uppercase");

  // char32_t

  static_assert(!IsAsciiUppercaseAlpha(U'@'), "U'@' isn't ASCII alpha uppercase");
  static_assert(U'@' == 0x40, "U'@' has value 0x40");

  static_assert(U'A' == 0x41, "U'A' has value 0x41");
  static_assert(IsAsciiUppercaseAlpha(U'A'), "U'A' is ASCII alpha uppercase");
  static_assert(IsAsciiUppercaseAlpha(U'B'), "U'B' is ASCII alpha uppercase");
  static_assert(IsAsciiUppercaseAlpha(U'M'), "U'M' is ASCII alpha uppercase");
  static_assert(IsAsciiUppercaseAlpha(U'Y'), "U'Y' is ASCII alpha uppercase");
  static_assert(IsAsciiUppercaseAlpha(U'Z'), "U'Z' is ASCII alpha uppercase");

  static_assert(U'Z' == 0x5A, "U'Z' has value 0x5A");
  static_assert(U'[' == 0x5B, "U'[' has value 0x5B");
  static_assert(!IsAsciiUppercaseAlpha(U'['), "U'[' isn't ASCII alpha uppercase");

  static_assert(!IsAsciiUppercaseAlpha(U'`'), "U'`' isn't ASCII alpha uppercase");
  static_assert(!IsAsciiUppercaseAlpha(U'a'), "U'a' is ASCII alpha uppercase");
  static_assert(!IsAsciiUppercaseAlpha(U'b'), "U'b' is ASCII alpha uppercase");
  static_assert(!IsAsciiUppercaseAlpha(U'm'), "U'm' is ASCII alpha uppercase");
  static_assert(!IsAsciiUppercaseAlpha(U'y'), "U'y' is ASCII alpha uppercase");
  static_assert(!IsAsciiUppercaseAlpha(U'z'), "U'z' is ASCII alpha uppercase");
  static_assert(!IsAsciiUppercaseAlpha(U'{'), "U'{' isn't ASCII alpha uppercase");
}

static void
TestIsAsciiLowercaseAlpha()
{
  // char

  static_assert(!IsAsciiLowercaseAlpha('`'), "'`' isn't ASCII alpha lowercase");
  static_assert('`' == 0x60, "'`' has value 0x60");

  static_assert('a' == 0x61, "'a' has value 0x61");
  static_assert(IsAsciiLowercaseAlpha('a'), "'a' is ASCII alpha lowercase");
  static_assert(IsAsciiLowercaseAlpha('b'), "'b' is ASCII alpha lowercase");
  static_assert(IsAsciiLowercaseAlpha('m'), "'m' is ASCII alpha lowercase");
  static_assert(IsAsciiLowercaseAlpha('y'), "'y' is ASCII alpha lowercase");
  static_assert(IsAsciiLowercaseAlpha('z'), "'z' is ASCII alpha lowercase");

  static_assert('z' == 0x7A, "'z' has value 0x7A");
  static_assert('{' == 0x7B, "'{' has value 0x7B");
  static_assert(!IsAsciiLowercaseAlpha('{'), "'{' isn't ASCII alpha lowercase");

  static_assert(!IsAsciiLowercaseAlpha('@'), "'@' isn't ASCII alpha lowercase");
  static_assert(!IsAsciiLowercaseAlpha('A'), "'A' is ASCII alpha lowercase");
  static_assert(!IsAsciiLowercaseAlpha('B'), "'B' is ASCII alpha lowercase");
  static_assert(!IsAsciiLowercaseAlpha('M'), "'M' is ASCII alpha lowercase");
  static_assert(!IsAsciiLowercaseAlpha('Y'), "'Y' is ASCII alpha lowercase");
  static_assert(!IsAsciiLowercaseAlpha('Z'), "'Z' is ASCII alpha lowercase");
  static_assert(!IsAsciiLowercaseAlpha('['), "'[' isn't ASCII alpha lowercase");

  // char16_t

  static_assert(!IsAsciiLowercaseAlpha(u'`'), "u'`' isn't ASCII alpha lowercase");
  static_assert(u'`' == 0x60, "u'`' has value 0x60");

  static_assert(u'a' == 0x61, "u'a' has value 0x61");
  static_assert(IsAsciiLowercaseAlpha(u'a'), "u'a' is ASCII alpha lowercase");
  static_assert(IsAsciiLowercaseAlpha(u'b'), "u'b' is ASCII alpha lowercase");
  static_assert(IsAsciiLowercaseAlpha(u'm'), "u'm' is ASCII alpha lowercase");
  static_assert(IsAsciiLowercaseAlpha(u'y'), "u'y' is ASCII alpha lowercase");
  static_assert(IsAsciiLowercaseAlpha(u'z'), "u'z' is ASCII alpha lowercase");

  static_assert(u'z' == 0x7A, "u'z' has value 0x7A");
  static_assert(u'{' == 0x7B, "u'{' has value 0x7B");
  static_assert(!IsAsciiLowercaseAlpha(u'{'), "u'{' isn't ASCII alpha lowercase");

  static_assert(!IsAsciiLowercaseAlpha(u'@'), "u'@' isn't ASCII alpha lowercase");
  static_assert(!IsAsciiLowercaseAlpha(u'A'), "u'A' is ASCII alpha lowercase");
  static_assert(!IsAsciiLowercaseAlpha(u'B'), "u'B' is ASCII alpha lowercase");
  static_assert(!IsAsciiLowercaseAlpha(u'M'), "u'M' is ASCII alpha lowercase");
  static_assert(!IsAsciiLowercaseAlpha(u'Y'), "u'Y' is ASCII alpha lowercase");
  static_assert(!IsAsciiLowercaseAlpha(u'Z'), "u'Z' is ASCII alpha lowercase");
  static_assert(!IsAsciiLowercaseAlpha(u'['), "u'[' isn't ASCII alpha lowercase");

  // char32_t

  static_assert(!IsAsciiLowercaseAlpha(U'`'), "U'`' isn't ASCII alpha lowercase");
  static_assert(U'`' == 0x60, "U'`' has value 0x60");

  static_assert(U'a' == 0x61, "U'a' has value 0x61");
  static_assert(IsAsciiLowercaseAlpha(U'a'), "U'a' is ASCII alpha lowercase");
  static_assert(IsAsciiLowercaseAlpha(U'b'), "U'b' is ASCII alpha lowercase");
  static_assert(IsAsciiLowercaseAlpha(U'm'), "U'm' is ASCII alpha lowercase");
  static_assert(IsAsciiLowercaseAlpha(U'y'), "U'y' is ASCII alpha lowercase");
  static_assert(IsAsciiLowercaseAlpha(U'z'), "U'z' is ASCII alpha lowercase");

  static_assert(U'z' == 0x7A, "U'z' has value 0x7A");
  static_assert(U'{' == 0x7B, "U'{' has value 0x7B");
  static_assert(!IsAsciiLowercaseAlpha(U'{'), "U'{' isn't ASCII alpha lowercase");

  static_assert(!IsAsciiLowercaseAlpha(U'@'), "U'@' isn't ASCII alpha lowercase");
  static_assert(!IsAsciiLowercaseAlpha(U'A'), "U'A' is ASCII alpha lowercase");
  static_assert(!IsAsciiLowercaseAlpha(U'B'), "U'B' is ASCII alpha lowercase");
  static_assert(!IsAsciiLowercaseAlpha(U'M'), "U'M' is ASCII alpha lowercase");
  static_assert(!IsAsciiLowercaseAlpha(U'Y'), "U'Y' is ASCII alpha lowercase");
  static_assert(!IsAsciiLowercaseAlpha(U'Z'), "U'Z' is ASCII alpha lowercase");
  static_assert(!IsAsciiLowercaseAlpha(U'['), "U'[' isn't ASCII alpha lowercase");
}

static void
TestIsAsciiAlphanumeric()
{
  // char

  static_assert(!IsAsciiAlphanumeric('/'), "'/' isn't ASCII alphanumeric");
  static_assert('/' == 0x2F, "'/' has value 0x2F");

  static_assert('0' == 0x30, "'0' has value 0x30");
  static_assert(IsAsciiAlphanumeric('0'), "'0' is ASCII alphanumeric");
  static_assert(IsAsciiAlphanumeric('1'), "'1' is ASCII alphanumeric");
  static_assert(IsAsciiAlphanumeric('5'), "'5' is ASCII alphanumeric");
  static_assert(IsAsciiAlphanumeric('8'), "'8' is ASCII alphanumeric");
  static_assert(IsAsciiAlphanumeric('9'), "'9' is ASCII alphanumeric");

  static_assert('9' == 0x39, "'9' has value 0x39");
  static_assert(':' == 0x3A, "':' has value 0x3A");
  static_assert(!IsAsciiAlphanumeric(':'), "':' isn't ASCII alphanumeric");

  static_assert(!IsAsciiAlphanumeric('@'), "'@' isn't ASCII alphanumeric");
  static_assert('@' == 0x40, "'@' has value 0x40");

  static_assert('A' == 0x41, "'A' has value 0x41");
  static_assert(IsAsciiAlphanumeric('A'), "'A' is ASCII alphanumeric");
  static_assert(IsAsciiAlphanumeric('B'), "'B' is ASCII alphanumeric");
  static_assert(IsAsciiAlphanumeric('M'), "'M' is ASCII alphanumeric");
  static_assert(IsAsciiAlphanumeric('Y'), "'Y' is ASCII alphanumeric");
  static_assert(IsAsciiAlphanumeric('Z'), "'Z' is ASCII alphanumeric");

  static_assert('Z' == 0x5A, "'Z' has value 0x5A");
  static_assert('[' == 0x5B, "'[' has value 0x5B");
  static_assert(!IsAsciiAlphanumeric('['), "'[' isn't ASCII alphanumeric");

  static_assert(!IsAsciiAlphanumeric('`'), "'`' isn't ASCII alphanumeric");
  static_assert('`' == 0x60, "'`' has value 0x60");

  static_assert('a' == 0x61, "'a' has value 0x61");
  static_assert(IsAsciiAlphanumeric('a'), "'a' is ASCII alphanumeric");
  static_assert(IsAsciiAlphanumeric('b'), "'b' is ASCII alphanumeric");
  static_assert(IsAsciiAlphanumeric('m'), "'m' is ASCII alphanumeric");
  static_assert(IsAsciiAlphanumeric('y'), "'y' is ASCII alphanumeric");
  static_assert(IsAsciiAlphanumeric('z'), "'z' is ASCII alphanumeric");

  static_assert('z' == 0x7A, "'z' has value 0x7A");
  static_assert('{' == 0x7B, "'{' has value 0x7B");
  static_assert(!IsAsciiAlphanumeric('{'), "'{' isn't ASCII alphanumeric");

  // char16_t

  static_assert(!IsAsciiAlphanumeric(u'/'), "u'/' isn't ASCII alphanumeric");
  static_assert(u'/' == 0x2F, "u'/' has value 0x2F");

  static_assert(u'0' == 0x30, "u'0' has value 0x30");
  static_assert(IsAsciiAlphanumeric(u'0'), "u'0' is ASCII alphanumeric");
  static_assert(IsAsciiAlphanumeric(u'1'), "u'1' is ASCII alphanumeric");
  static_assert(IsAsciiAlphanumeric(u'5'), "u'5' is ASCII alphanumeric");
  static_assert(IsAsciiAlphanumeric(u'8'), "u'8' is ASCII alphanumeric");
  static_assert(IsAsciiAlphanumeric(u'9'), "u'9' is ASCII alphanumeric");

  static_assert(u'9' == 0x39, "u'9' has value 0x39");
  static_assert(u':' == 0x3A, "u':' has value 0x3A");
  static_assert(!IsAsciiAlphanumeric(u':'), "u':' isn't ASCII alphanumeric");

  static_assert(!IsAsciiAlphanumeric(u'@'), "u'@' isn't ASCII alphanumeric");
  static_assert(u'@' == 0x40, "u'@' has value 0x40");

  static_assert(u'A' == 0x41, "u'A' has value 0x41");
  static_assert(IsAsciiAlphanumeric(u'A'), "u'A' is ASCII alphanumeric");
  static_assert(IsAsciiAlphanumeric(u'B'), "u'B' is ASCII alphanumeric");
  static_assert(IsAsciiAlphanumeric(u'M'), "u'M' is ASCII alphanumeric");
  static_assert(IsAsciiAlphanumeric(u'Y'), "u'Y' is ASCII alphanumeric");
  static_assert(IsAsciiAlphanumeric(u'Z'), "u'Z' is ASCII alphanumeric");

  static_assert(u'Z' == 0x5A, "u'Z' has value 0x5A");
  static_assert(u'[' == 0x5B, "u'[' has value 0x5B");
  static_assert(!IsAsciiAlphanumeric(u'['), "u'[' isn't ASCII alphanumeric");

  static_assert(!IsAsciiAlphanumeric(u'`'), "u'`' isn't ASCII alphanumeric");
  static_assert(u'`' == 0x60, "u'`' has value 0x60");

  static_assert(u'a' == 0x61, "u'a' has value 0x61");
  static_assert(IsAsciiAlphanumeric(u'a'), "u'a' is ASCII alphanumeric");
  static_assert(IsAsciiAlphanumeric(u'b'), "u'b' is ASCII alphanumeric");
  static_assert(IsAsciiAlphanumeric(u'm'), "u'm' is ASCII alphanumeric");
  static_assert(IsAsciiAlphanumeric(u'y'), "u'y' is ASCII alphanumeric");
  static_assert(IsAsciiAlphanumeric(u'z'), "u'z' is ASCII alphanumeric");

  static_assert(u'z' == 0x7A, "u'z' has value 0x7A");
  static_assert(u'{' == 0x7B, "u'{' has value 0x7B");
  static_assert(!IsAsciiAlphanumeric(u'{'), "u'{' isn't ASCII alphanumeric");

  // char32_t

  static_assert(!IsAsciiAlphanumeric(U'/'), "U'/' isn't ASCII alphanumeric");
  static_assert(U'/' == 0x2F, "U'/' has value 0x2F");

  static_assert(U'0' == 0x30, "U'0' has value 0x30");
  static_assert(IsAsciiAlphanumeric(U'0'), "U'0' is ASCII alphanumeric");
  static_assert(IsAsciiAlphanumeric(U'1'), "U'1' is ASCII alphanumeric");
  static_assert(IsAsciiAlphanumeric(U'5'), "U'5' is ASCII alphanumeric");
  static_assert(IsAsciiAlphanumeric(U'8'), "U'8' is ASCII alphanumeric");
  static_assert(IsAsciiAlphanumeric(U'9'), "U'9' is ASCII alphanumeric");

  static_assert(U'9' == 0x39, "U'9' has value 0x39");
  static_assert(U':' == 0x3A, "U':' has value 0x3A");
  static_assert(!IsAsciiAlphanumeric(U':'), "U':' isn't ASCII alphanumeric");

  static_assert(!IsAsciiAlphanumeric(U'@'), "U'@' isn't ASCII alphanumeric");
  static_assert(U'@' == 0x40, "U'@' has value 0x40");

  static_assert(U'A' == 0x41, "U'A' has value 0x41");
  static_assert(IsAsciiAlphanumeric(U'A'), "U'A' is ASCII alphanumeric");
  static_assert(IsAsciiAlphanumeric(U'B'), "U'B' is ASCII alphanumeric");
  static_assert(IsAsciiAlphanumeric(U'M'), "U'M' is ASCII alphanumeric");
  static_assert(IsAsciiAlphanumeric(U'Y'), "U'Y' is ASCII alphanumeric");
  static_assert(IsAsciiAlphanumeric(U'Z'), "U'Z' is ASCII alphanumeric");

  static_assert(U'Z' == 0x5A, "U'Z' has value 0x5A");
  static_assert(U'[' == 0x5B, "U'[' has value 0x5B");
  static_assert(!IsAsciiAlphanumeric(U'['), "U'[' isn't ASCII alphanumeric");

  static_assert(!IsAsciiAlphanumeric(U'`'), "U'`' isn't ASCII alphanumeric");
  static_assert(U'`' == 0x60, "U'`' has value 0x60");

  static_assert(U'a' == 0x61, "U'a' has value 0x61");
  static_assert(IsAsciiAlphanumeric(U'a'), "U'a' is ASCII alphanumeric");
  static_assert(IsAsciiAlphanumeric(U'b'), "U'b' is ASCII alphanumeric");
  static_assert(IsAsciiAlphanumeric(U'm'), "U'm' is ASCII alphanumeric");
  static_assert(IsAsciiAlphanumeric(U'y'), "U'y' is ASCII alphanumeric");
  static_assert(IsAsciiAlphanumeric(U'z'), "U'z' is ASCII alphanumeric");

  static_assert(U'z' == 0x7A, "U'z' has value 0x7A");
  static_assert(U'{' == 0x7B, "U'{' has value 0x7B");
  static_assert(!IsAsciiAlphanumeric(U'{'), "U'{' isn't ASCII alphanumeric");
}

static void
TestAsciiAlphanumericToNumber()
{
  // When AsciiAlphanumericToNumber becomes constexpr, make sure to convert all
  // these to just static_assert.

  // char

  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('0') == 0, "'0' converts to 0");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('1') == 1, "'1' converts to 1");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('2') == 2, "'2' converts to 2");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('3') == 3, "'3' converts to 3");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('4') == 4, "'4' converts to 4");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('5') == 5, "'5' converts to 5");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('6') == 6, "'6' converts to 6");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('7') == 7, "'7' converts to 7");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('8') == 8, "'8' converts to 8");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('9') == 9, "'9' converts to 9");

  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('A') == 10, "'A' converts to 10");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('B') == 11, "'B' converts to 11");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('C') == 12, "'C' converts to 12");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('D') == 13, "'D' converts to 13");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('E') == 14, "'E' converts to 14");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('F') == 15, "'F' converts to 15");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('G') == 16, "'G' converts to 16");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('H') == 17, "'H' converts to 17");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('I') == 18, "'I' converts to 18");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('J') == 19, "'J' converts to 19");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('K') == 20, "'K' converts to 20");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('L') == 21, "'L' converts to 21");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('M') == 22, "'M' converts to 22");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('N') == 23, "'N' converts to 23");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('O') == 24, "'O' converts to 24");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('P') == 25, "'P' converts to 25");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('Q') == 26, "'Q' converts to 26");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('R') == 27, "'R' converts to 27");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('S') == 28, "'S' converts to 28");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('T') == 29, "'T' converts to 29");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('U') == 30, "'U' converts to 30");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('V') == 31, "'V' converts to 31");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('W') == 32, "'W' converts to 32");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('X') == 33, "'X' converts to 33");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('Y') == 34, "'Y' converts to 34");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('Z') == 35, "'Z' converts to 35");

  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('a') == 10, "'a' converts to 10");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('b') == 11, "'b' converts to 11");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('c') == 12, "'c' converts to 12");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('d') == 13, "'d' converts to 13");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('e') == 14, "'e' converts to 14");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('f') == 15, "'f' converts to 15");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('g') == 16, "'g' converts to 16");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('h') == 17, "'h' converts to 17");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('i') == 18, "'i' converts to 18");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('j') == 19, "'j' converts to 19");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('k') == 20, "'k' converts to 20");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('l') == 21, "'l' converts to 21");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('m') == 22, "'m' converts to 22");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('n') == 23, "'n' converts to 23");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('o') == 24, "'o' converts to 24");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('p') == 25, "'p' converts to 25");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('q') == 26, "'q' converts to 26");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('r') == 27, "'r' converts to 27");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('s') == 28, "'s' converts to 28");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('t') == 29, "'t' converts to 29");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('u') == 30, "'u' converts to 30");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('v') == 31, "'v' converts to 31");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('w') == 32, "'w' converts to 32");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('x') == 33, "'x' converts to 33");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('y') == 34, "'y' converts to 34");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber('z') == 35, "'z' converts to 35");

  // char16_t

  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u'0') == 0, "u'0' converts to 0");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u'1') == 1, "u'1' converts to 1");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u'2') == 2, "u'2' converts to 2");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u'3') == 3, "u'3' converts to 3");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u'4') == 4, "u'4' converts to 4");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u'5') == 5, "u'5' converts to 5");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u'6') == 6, "u'6' converts to 6");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u'7') == 7, "u'7' converts to 7");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u'8') == 8, "u'8' converts to 8");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u'9') == 9, "u'9' converts to 9");

  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u'A') == 10, "u'A' converts to 10");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u'B') == 11, "u'B' converts to 11");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u'C') == 12, "u'C' converts to 12");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u'D') == 13, "u'D' converts to 13");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u'E') == 14, "u'E' converts to 14");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u'F') == 15, "u'F' converts to 15");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u'G') == 16, "u'G' converts to 16");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u'H') == 17, "u'H' converts to 17");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u'I') == 18, "u'I' converts to 18");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u'J') == 19, "u'J' converts to 19");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u'K') == 20, "u'K' converts to 20");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u'L') == 21, "u'L' converts to 21");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u'M') == 22, "u'M' converts to 22");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u'N') == 23, "u'N' converts to 23");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u'O') == 24, "u'O' converts to 24");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u'P') == 25, "u'P' converts to 25");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u'Q') == 26, "u'Q' converts to 26");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u'R') == 27, "u'R' converts to 27");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u'S') == 28, "u'S' converts to 28");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u'T') == 29, "u'T' converts to 29");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u'U') == 30, "u'U' converts to 30");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u'V') == 31, "u'V' converts to 31");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u'W') == 32, "u'W' converts to 32");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u'X') == 33, "u'X' converts to 33");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u'Y') == 34, "u'Y' converts to 34");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u'Z') == 35, "u'Z' converts to 35");

  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u'a') == 10, "u'a' converts to 10");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u'b') == 11, "u'b' converts to 11");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u'c') == 12, "u'c' converts to 12");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u'd') == 13, "u'd' converts to 13");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u'e') == 14, "u'e' converts to 14");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u'f') == 15, "u'f' converts to 15");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u'g') == 16, "u'g' converts to 16");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u'h') == 17, "u'h' converts to 17");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u'i') == 18, "u'i' converts to 18");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u'j') == 19, "u'j' converts to 19");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u'k') == 20, "u'k' converts to 20");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u'l') == 21, "u'l' converts to 21");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u'm') == 22, "u'm' converts to 22");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u'n') == 23, "u'n' converts to 23");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u'o') == 24, "u'o' converts to 24");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u'p') == 25, "u'p' converts to 25");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u'q') == 26, "u'q' converts to 26");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u'r') == 27, "u'r' converts to 27");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u's') == 28, "u's' converts to 28");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u't') == 29, "u't' converts to 29");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u'u') == 30, "u'u' converts to 30");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u'v') == 31, "u'v' converts to 31");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u'w') == 32, "u'w' converts to 32");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u'x') == 33, "u'x' converts to 33");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u'y') == 34, "u'y' converts to 34");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(u'z') == 35, "u'z' converts to 35");

  // char32_t

  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U'0') == 0, "U'0' converts to 0");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U'1') == 1, "U'1' converts to 1");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U'2') == 2, "U'2' converts to 2");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U'3') == 3, "U'3' converts to 3");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U'4') == 4, "U'4' converts to 4");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U'5') == 5, "U'5' converts to 5");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U'6') == 6, "U'6' converts to 6");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U'7') == 7, "U'7' converts to 7");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U'8') == 8, "U'8' converts to 8");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U'9') == 9, "U'9' converts to 9");

  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U'A') == 10, "U'A' converts to 10");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U'B') == 11, "U'B' converts to 11");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U'C') == 12, "U'C' converts to 12");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U'D') == 13, "U'D' converts to 13");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U'E') == 14, "U'E' converts to 14");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U'F') == 15, "U'F' converts to 15");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U'G') == 16, "U'G' converts to 16");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U'H') == 17, "U'H' converts to 17");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U'I') == 18, "U'I' converts to 18");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U'J') == 19, "U'J' converts to 19");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U'K') == 20, "U'K' converts to 20");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U'L') == 21, "U'L' converts to 21");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U'M') == 22, "U'M' converts to 22");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U'N') == 23, "U'N' converts to 23");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U'O') == 24, "U'O' converts to 24");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U'P') == 25, "U'P' converts to 25");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U'Q') == 26, "U'Q' converts to 26");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U'R') == 27, "U'R' converts to 27");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U'S') == 28, "U'S' converts to 28");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U'T') == 29, "U'T' converts to 29");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U'U') == 30, "U'U' converts to 30");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U'V') == 31, "U'V' converts to 31");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U'W') == 32, "U'W' converts to 32");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U'X') == 33, "U'X' converts to 33");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U'Y') == 34, "U'Y' converts to 34");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U'Z') == 35, "U'Z' converts to 35");

  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U'a') == 10, "U'a' converts to 10");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U'b') == 11, "U'b' converts to 11");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U'c') == 12, "U'c' converts to 12");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U'd') == 13, "U'd' converts to 13");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U'e') == 14, "U'e' converts to 14");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U'f') == 15, "U'f' converts to 15");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U'g') == 16, "U'g' converts to 16");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U'h') == 17, "U'h' converts to 17");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U'i') == 18, "U'i' converts to 18");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U'j') == 19, "U'j' converts to 19");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U'k') == 20, "U'k' converts to 20");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U'l') == 21, "U'l' converts to 21");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U'm') == 22, "U'm' converts to 22");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U'n') == 23, "U'n' converts to 23");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U'o') == 24, "U'o' converts to 24");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U'p') == 25, "U'p' converts to 25");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U'q') == 26, "U'q' converts to 26");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U'r') == 27, "U'r' converts to 27");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U's') == 28, "U's' converts to 28");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U't') == 29, "U't' converts to 29");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U'u') == 30, "U'u' converts to 30");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U'v') == 31, "U'v' converts to 31");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U'w') == 32, "U'w' converts to 32");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U'x') == 33, "U'x' converts to 33");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U'y') == 34, "U'y' converts to 34");
  MOZ_RELEASE_ASSERT(AsciiAlphanumericToNumber(U'z') == 35, "U'z' converts to 35");
}

static void
TestIsAsciiDigit()
{
  // char

  static_assert(!IsAsciiDigit('/'), "'/' isn't an ASCII digit");
  static_assert('/' == 0x2F, "'/' has value 0x2F");

  static_assert('0' == 0x30, "'0' has value 0x30");
  static_assert(IsAsciiDigit('0'), "'0' is an ASCII digit");
  static_assert(IsAsciiDigit('1'), "'1' is an ASCII digit");
  static_assert(IsAsciiDigit('5'), "'5' is an ASCII digit");
  static_assert(IsAsciiDigit('8'), "'8' is an ASCII digit");
  static_assert(IsAsciiDigit('9'), "'9' is an ASCII digit");

  static_assert('9' == 0x39, "'9' has value 0x39");
  static_assert(':' == 0x3A, "':' has value 0x3A");
  static_assert(!IsAsciiDigit(':'), "':' isn't an ASCII digit");

  static_assert(!IsAsciiDigit('@'), "'@' isn't an ASCII digit");
  static_assert(!IsAsciiDigit('A'), "'A' isn't an ASCII digit");
  static_assert(!IsAsciiDigit('B'), "'B' isn't an ASCII digit");
  static_assert(!IsAsciiDigit('M'), "'M' isn't an ASCII digit");
  static_assert(!IsAsciiDigit('Y'), "'Y' isn't an ASCII digit");
  static_assert(!IsAsciiDigit('Z'), "'Z' isn't an ASCII digit");
  static_assert(!IsAsciiDigit('['), "'[' isn't an ASCII digit");
  static_assert(!IsAsciiDigit('`'), "'`' isn't an ASCII digit");
  static_assert(!IsAsciiDigit('a'), "'a' isn't an ASCII digit");
  static_assert(!IsAsciiDigit('b'), "'b' isn't an ASCII digit");
  static_assert(!IsAsciiDigit('m'), "'m' isn't an ASCII digit");
  static_assert(!IsAsciiDigit('y'), "'y' isn't an ASCII digit");
  static_assert(!IsAsciiDigit('z'), "'z' isn't an ASCII digit");
  static_assert(!IsAsciiDigit('{'), "'{' isn't an ASCII digit");

  // char16_t

  static_assert(!IsAsciiDigit(u'/'), "u'/' isn't an ASCII digit");
  static_assert(u'/' == 0x2F, "u'/' has value 0x2F");
  static_assert(u'0' == 0x30, "u'0' has value 0x30");
  static_assert(IsAsciiDigit(u'0'), "u'0' is an ASCII digit");
  static_assert(IsAsciiDigit(u'1'), "u'1' is an ASCII digit");
  static_assert(IsAsciiDigit(u'5'), "u'5' is an ASCII digit");
  static_assert(IsAsciiDigit(u'8'), "u'8' is an ASCII digit");
  static_assert(IsAsciiDigit(u'9'), "u'9' is an ASCII digit");

  static_assert(u'9' == 0x39, "u'9' has value 0x39");
  static_assert(u':' == 0x3A, "u':' has value 0x3A");
  static_assert(!IsAsciiDigit(u':'), "u':' isn't an ASCII digit");

  static_assert(!IsAsciiDigit(u'@'), "u'@' isn't an ASCII digit");
  static_assert(!IsAsciiDigit(u'A'), "u'A' isn't an ASCII digit");
  static_assert(!IsAsciiDigit(u'B'), "u'B' isn't an ASCII digit");
  static_assert(!IsAsciiDigit(u'M'), "u'M' isn't an ASCII digit");
  static_assert(!IsAsciiDigit(u'Y'), "u'Y' isn't an ASCII digit");
  static_assert(!IsAsciiDigit(u'Z'), "u'Z' isn't an ASCII digit");
  static_assert(!IsAsciiDigit(u'['), "u'[' isn't an ASCII digit");
  static_assert(!IsAsciiDigit(u'`'), "u'`' isn't an ASCII digit");
  static_assert(!IsAsciiDigit(u'a'), "u'a' isn't an ASCII digit");
  static_assert(!IsAsciiDigit(u'b'), "u'b' isn't an ASCII digit");
  static_assert(!IsAsciiDigit(u'm'), "u'm' isn't an ASCII digit");
  static_assert(!IsAsciiDigit(u'y'), "u'y' isn't an ASCII digit");
  static_assert(!IsAsciiDigit(u'z'), "u'z' isn't an ASCII digit");
  static_assert(!IsAsciiDigit(u'{'), "u'{' isn't an ASCII digit");

  // char32_t

  static_assert(!IsAsciiDigit(U'/'), "U'/' isn't an ASCII digit");
  static_assert(U'/' == 0x2F, "U'/' has value 0x2F");

  static_assert(U'0' == 0x30, "U'0' has value 0x30");
  static_assert(IsAsciiDigit(U'0'), "U'0' is an ASCII digit");
  static_assert(IsAsciiDigit(U'1'), "U'1' is an ASCII digit");
  static_assert(IsAsciiDigit(U'5'), "U'5' is an ASCII digit");
  static_assert(IsAsciiDigit(U'8'), "U'8' is an ASCII digit");
  static_assert(IsAsciiDigit(U'9'), "U'9' is an ASCII digit");

  static_assert(U'9' == 0x39, "U'9' has value 0x39");
  static_assert(U':' == 0x3A, "U':' has value 0x3A");
  static_assert(!IsAsciiDigit(U':'), "U':' isn't an ASCII digit");

  static_assert(!IsAsciiDigit(U'@'), "U'@' isn't an ASCII digit");
  static_assert(!IsAsciiDigit(U'A'), "U'A' isn't an ASCII digit");
  static_assert(!IsAsciiDigit(U'B'), "U'B' isn't an ASCII digit");
  static_assert(!IsAsciiDigit(U'M'), "U'M' isn't an ASCII digit");
  static_assert(!IsAsciiDigit(U'Y'), "U'Y' isn't an ASCII digit");
  static_assert(!IsAsciiDigit(U'Z'), "U'Z' isn't an ASCII digit");
  static_assert(!IsAsciiDigit(U'['), "U'[' isn't an ASCII digit");
  static_assert(!IsAsciiDigit(U'`'), "U'`' isn't an ASCII digit");
  static_assert(!IsAsciiDigit(U'a'), "U'a' isn't an ASCII digit");
  static_assert(!IsAsciiDigit(U'b'), "U'b' isn't an ASCII digit");
  static_assert(!IsAsciiDigit(U'm'), "U'm' isn't an ASCII digit");
  static_assert(!IsAsciiDigit(U'y'), "U'y' isn't an ASCII digit");
  static_assert(!IsAsciiDigit(U'z'), "U'z' isn't an ASCII digit");
  static_assert(!IsAsciiDigit(U'{'), "U'{' isn't an ASCII digit");
}

int
main()
{
  TestIsAscii();
  TestIsAsciiAlpha();
  TestIsAsciiUppercaseAlpha();
  TestIsAsciiLowercaseAlpha();
  TestIsAsciiAlphanumeric();
  TestAsciiAlphanumericToNumber();
  TestIsAsciiDigit();
}
