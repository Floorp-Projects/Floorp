/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#include <stdio.h>
#include <stdlib.h>
#include "nsString.h"
#include "nsStringBuffer.h"
#include "nsReadableUtils.h"
#include "UTFStrings.h"
#include "nsUnicharUtils.h"
#include "mozilla/HashFunctions.h"
#include "nsUTF8Utils.h"

#include "gtest/gtest.h"

using namespace mozilla;

namespace TestUTF {

TEST(UTF, Valid)
{
  for (unsigned int i = 0; i < ArrayLength(ValidStrings); ++i) {
    nsDependentCString str8(ValidStrings[i].m8);
    nsDependentString str16(ValidStrings[i].m16);

    EXPECT_TRUE(NS_ConvertUTF16toUTF8(str16).Equals(str8));

    EXPECT_TRUE(NS_ConvertUTF8toUTF16(str8).Equals(str16));

    nsCString tmp8("string ");
    AppendUTF16toUTF8(str16, tmp8);
    EXPECT_TRUE(tmp8.Equals(NS_LITERAL_CSTRING("string ") + str8));

    nsString tmp16(NS_LITERAL_STRING("string "));
    AppendUTF8toUTF16(str8, tmp16);
    EXPECT_TRUE(tmp16.Equals(NS_LITERAL_STRING("string ") + str16));

    EXPECT_EQ(CompareUTF8toUTF16(str8, str16), 0);
  }
}

TEST(UTF, Invalid16)
{
  for (unsigned int i = 0; i < ArrayLength(Invalid16Strings); ++i) {
    nsDependentString str16(Invalid16Strings[i].m16);
    nsDependentCString str8(Invalid16Strings[i].m8);

    EXPECT_TRUE(NS_ConvertUTF16toUTF8(str16).Equals(str8));

    nsCString tmp8("string ");
    AppendUTF16toUTF8(str16, tmp8);
    EXPECT_TRUE(tmp8.Equals(NS_LITERAL_CSTRING("string ") + str8));

    EXPECT_EQ(CompareUTF8toUTF16(str8, str16), 0);
  }
}

TEST(UTF, Invalid8)
{
  for (unsigned int i = 0; i < ArrayLength(Invalid8Strings); ++i) {
    nsDependentString str16(Invalid8Strings[i].m16);
    nsDependentCString str8(Invalid8Strings[i].m8);

    EXPECT_TRUE(NS_ConvertUTF8toUTF16(str8).Equals(str16));

    nsString tmp16(NS_LITERAL_STRING("string "));
    AppendUTF8toUTF16(str8, tmp16);
    EXPECT_TRUE(tmp16.Equals(NS_LITERAL_STRING("string ") + str16));

    EXPECT_EQ(CompareUTF8toUTF16(str8, str16), 0);
  }
}

TEST(UTF, Malformed8)
{
  for (unsigned int i = 0; i < ArrayLength(Malformed8Strings); ++i) {
    nsDependentString str16(Malformed8Strings[i].m16);
    nsDependentCString str8(Malformed8Strings[i].m8);

    EXPECT_TRUE(NS_ConvertUTF8toUTF16(str8).Equals(str16));

    nsString tmp16(NS_LITERAL_STRING("string "));
    AppendUTF8toUTF16(str8, tmp16);
    EXPECT_TRUE(tmp16.Equals(NS_LITERAL_STRING("string ") + str16));

    EXPECT_EQ(CompareUTF8toUTF16(str8, str16), 0);
  }
}

TEST(UTF, Hash16)
{
  for (unsigned int i = 0; i < ArrayLength(ValidStrings); ++i) {
    nsDependentCString str8(ValidStrings[i].m8);
    bool err;
    EXPECT_EQ(HashString(ValidStrings[i].m16),
              HashUTF8AsUTF16(str8.get(), str8.Length(), &err));
    EXPECT_FALSE(err);
  }

  for (unsigned int i = 0; i < ArrayLength(Invalid8Strings); ++i) {
    nsDependentCString str8(Invalid8Strings[i].m8);
    bool err;
    EXPECT_EQ(HashUTF8AsUTF16(str8.get(), str8.Length(), &err), 0u);
    EXPECT_TRUE(err);
  }

  for (unsigned int i = 0; i < ArrayLength(Malformed8Strings); ++i) {
    nsDependentCString str8(Malformed8Strings[i].m8);
    bool err;
    EXPECT_EQ(HashUTF8AsUTF16(str8.get(), str8.Length(), &err), 0u);
    EXPECT_TRUE(err);
  }
}

/**
 * This tests the handling of a non-ascii character at various locations in a
 * UTF-16 string that is being converted to UTF-8.
 */
static void NonASCII16_helper(const size_t aStrSize) {
  const size_t kTestSize = aStrSize;
  const size_t kMaxASCII = 0x80;
  const char16_t kUTF16Char = 0xC9;
  const char kUTF8Surrogates[] = {char(0xC3), char(0x89)};

  // Generate a string containing only ASCII characters.
  nsString asciiString;
  asciiString.SetLength(kTestSize);
  nsCString asciiCString;
  asciiCString.SetLength(kTestSize);

  auto str_buff = asciiString.BeginWriting();
  auto cstr_buff = asciiCString.BeginWriting();
  for (size_t i = 0; i < kTestSize; i++) {
    str_buff[i] = i % kMaxASCII;
    cstr_buff[i] = i % kMaxASCII;
  }

  // Now go through and test conversion when exactly one character will
  // result in a multibyte sequence.
  for (size_t i = 0; i < kTestSize; i++) {
    // Setup the UTF-16 string.
    nsString unicodeString(asciiString);
    auto buff = unicodeString.BeginWriting();
    buff[i] = kUTF16Char;

    // Do the conversion, make sure the length increased by 1.
    nsCString dest;
    AppendUTF16toUTF8(unicodeString, dest);
    EXPECT_EQ(dest.Length(), unicodeString.Length() + 1);

    // Build up the expected UTF-8 string.
    nsCString expected;

    // First add the leading ASCII chars.
    expected.Append(asciiCString.BeginReading(), i);

    // Now append the UTF-8 pair we expect the UTF-16 unicode char to
    // be converted to.
    for (auto& c : kUTF8Surrogates) {
      expected.Append(c);
    }

    // And finish with the trailing ASCII chars.
    expected.Append(asciiCString.BeginReading() + i + 1, kTestSize - i - 1);

    EXPECT_STREQ(dest.BeginReading(), expected.BeginReading());
  }
}

TEST(UTF, NonASCII16)
{
  // Test with various string sizes to catch any special casing.
  NonASCII16_helper(1);
  NonASCII16_helper(8);
  NonASCII16_helper(16);
  NonASCII16_helper(32);
  NonASCII16_helper(512);
}

TEST(UTF, UTF8CharEnumerator)
{
  const char* p =
      "\x61\xC0\xC2\xC2\x80\xE0\x80\x80\xE0\xA0\x80\xE1\x80\x80\xED\xBF\xBF\xED"
      "\x9F\xBF\xEE\x80\x80\xEE\x80\xFF\xF0\x90\x80\x80\xF0\x80\x80\x80\xF1\x80"
      "\x80\x80\xF4\x8F\xBF\xF4\x8F\xBF\xBF\xF4\xBF\xBF\xBF";
  const char* end = p + 49;
  EXPECT_EQ(UTF8CharEnumerator::NextChar(&p, end), 0x0061U);
  EXPECT_EQ(UTF8CharEnumerator::NextChar(&p, end), 0xFFFDU);
  EXPECT_EQ(UTF8CharEnumerator::NextChar(&p, end), 0xFFFDU);
  EXPECT_EQ(UTF8CharEnumerator::NextChar(&p, end), 0x0080U);
  EXPECT_EQ(UTF8CharEnumerator::NextChar(&p, end), 0xFFFDU);
  EXPECT_EQ(UTF8CharEnumerator::NextChar(&p, end), 0xFFFDU);
  EXPECT_EQ(UTF8CharEnumerator::NextChar(&p, end), 0xFFFDU);
  EXPECT_EQ(UTF8CharEnumerator::NextChar(&p, end), 0x0800U);
  EXPECT_EQ(UTF8CharEnumerator::NextChar(&p, end), 0x1000U);
  EXPECT_EQ(UTF8CharEnumerator::NextChar(&p, end), 0xFFFDU);
  EXPECT_EQ(UTF8CharEnumerator::NextChar(&p, end), 0xFFFDU);
  EXPECT_EQ(UTF8CharEnumerator::NextChar(&p, end), 0xFFFDU);
  EXPECT_EQ(UTF8CharEnumerator::NextChar(&p, end), 0xD7FFU);
  EXPECT_EQ(UTF8CharEnumerator::NextChar(&p, end), 0xE000U);
  EXPECT_EQ(UTF8CharEnumerator::NextChar(&p, end), 0xFFFDU);
  EXPECT_EQ(UTF8CharEnumerator::NextChar(&p, end), 0xFFFDU);
  EXPECT_EQ(UTF8CharEnumerator::NextChar(&p, end), 0x10000U);
  EXPECT_EQ(UTF8CharEnumerator::NextChar(&p, end), 0xFFFDU);
  EXPECT_EQ(UTF8CharEnumerator::NextChar(&p, end), 0xFFFDU);
  EXPECT_EQ(UTF8CharEnumerator::NextChar(&p, end), 0xFFFDU);
  EXPECT_EQ(UTF8CharEnumerator::NextChar(&p, end), 0xFFFDU);
  EXPECT_EQ(UTF8CharEnumerator::NextChar(&p, end), 0x40000U);
  EXPECT_EQ(UTF8CharEnumerator::NextChar(&p, end), 0xFFFDU);
  EXPECT_EQ(UTF8CharEnumerator::NextChar(&p, end), 0x10FFFFU);
  EXPECT_EQ(UTF8CharEnumerator::NextChar(&p, end), 0xFFFDU);
  EXPECT_EQ(UTF8CharEnumerator::NextChar(&p, end), 0xFFFDU);
  EXPECT_EQ(UTF8CharEnumerator::NextChar(&p, end), 0xFFFDU);
  EXPECT_EQ(UTF8CharEnumerator::NextChar(&p, end), 0xFFFDU);
  EXPECT_EQ(p, end);
  p = "\xC2\xB6";
  end = p + 1;
  EXPECT_EQ(UTF8CharEnumerator::NextChar(&p, end), 0xFFFDU);
  EXPECT_EQ(p, end);
  p = "\xE2\x98\x83";
  end = p + 2;
  EXPECT_EQ(UTF8CharEnumerator::NextChar(&p, end), 0xFFFDU);
  EXPECT_EQ(p, end);
  p = "\xF0\x9F\x92\xA9";
  end = p + 2;
  EXPECT_EQ(UTF8CharEnumerator::NextChar(&p, end), 0xFFFDU);
  EXPECT_EQ(p, end);
  p = "\xF0\x9F\x92\xA9";
  end = p + 3;
  EXPECT_EQ(UTF8CharEnumerator::NextChar(&p, end), 0xFFFDU);
  EXPECT_EQ(p, end);
}

TEST(UTF, UTF16CharEnumerator)
{
  const char16_t* p = u"\u0061\U0001F4A9";
  const char16_t* end = p + 3;
  EXPECT_EQ(UTF16CharEnumerator::NextChar(&p, end), 0x0061U);
  EXPECT_EQ(UTF16CharEnumerator::NextChar(&p, end), 0x1F4A9U);
  EXPECT_EQ(p, end);
  const char16_t loneHigh = 0xD83D;
  p = &loneHigh;
  end = p + 1;
  EXPECT_EQ(UTF16CharEnumerator::NextChar(&p, end), 0xFFFDU);
  EXPECT_EQ(p, end);
  const char16_t loneLow = 0xDCA9;
  p = &loneLow;
  end = p + 1;
  EXPECT_EQ(UTF16CharEnumerator::NextChar(&p, end), 0xFFFDU);
  EXPECT_EQ(p, end);
  const char16_t loneHighStr[] = {0xD83D, 0x0061};
  p = loneHighStr;
  end = p + 2;
  EXPECT_EQ(UTF16CharEnumerator::NextChar(&p, end), 0xFFFDU);
  EXPECT_EQ(UTF16CharEnumerator::NextChar(&p, end), 0x0061U);
  EXPECT_EQ(p, end);
}

}  // namespace TestUTF
