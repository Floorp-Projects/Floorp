/* -*- Mode: C++; tab-width: 9; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/SIMD.h"

using mozilla::SIMD;

void TestTinyString() {
  const char* test = "012\n";

  MOZ_RELEASE_ASSERT(SIMD::memchr8(test, '0', 3) == test + 0x0);
  MOZ_RELEASE_ASSERT(SIMD::memchr8(test, '1', 3) == test + 0x1);
  MOZ_RELEASE_ASSERT(SIMD::memchr8(test, '2', 3) == test + 0x2);
  MOZ_RELEASE_ASSERT(SIMD::memchr8(test, '\n', 3) == nullptr);
}

void TestShortString() {
  const char* test = "0123456789\n";

  MOZ_RELEASE_ASSERT(SIMD::memchr8(test, '0', 10) == test + 0x0);
  MOZ_RELEASE_ASSERT(SIMD::memchr8(test, '1', 10) == test + 0x1);
  MOZ_RELEASE_ASSERT(SIMD::memchr8(test, '2', 10) == test + 0x2);
  MOZ_RELEASE_ASSERT(SIMD::memchr8(test, '3', 10) == test + 0x3);
  MOZ_RELEASE_ASSERT(SIMD::memchr8(test, '4', 10) == test + 0x4);
  MOZ_RELEASE_ASSERT(SIMD::memchr8(test, '5', 10) == test + 0x5);
  MOZ_RELEASE_ASSERT(SIMD::memchr8(test, '6', 10) == test + 0x6);
  MOZ_RELEASE_ASSERT(SIMD::memchr8(test, '7', 10) == test + 0x7);
  MOZ_RELEASE_ASSERT(SIMD::memchr8(test, '8', 10) == test + 0x8);
  MOZ_RELEASE_ASSERT(SIMD::memchr8(test, '9', 10) == test + 0x9);
  MOZ_RELEASE_ASSERT(SIMD::memchr8(test, '\n', 10) == nullptr);
}

void TestMediumString() {
  const char* test = "0123456789abcdef\n";

  MOZ_RELEASE_ASSERT(SIMD::memchr8(test, '0', 16) == test + 0x0);
  MOZ_RELEASE_ASSERT(SIMD::memchr8(test, '1', 16) == test + 0x1);
  MOZ_RELEASE_ASSERT(SIMD::memchr8(test, '2', 16) == test + 0x2);
  MOZ_RELEASE_ASSERT(SIMD::memchr8(test, '3', 16) == test + 0x3);
  MOZ_RELEASE_ASSERT(SIMD::memchr8(test, '4', 16) == test + 0x4);
  MOZ_RELEASE_ASSERT(SIMD::memchr8(test, '5', 16) == test + 0x5);
  MOZ_RELEASE_ASSERT(SIMD::memchr8(test, '6', 16) == test + 0x6);
  MOZ_RELEASE_ASSERT(SIMD::memchr8(test, '7', 16) == test + 0x7);
  MOZ_RELEASE_ASSERT(SIMD::memchr8(test, '8', 16) == test + 0x8);
  MOZ_RELEASE_ASSERT(SIMD::memchr8(test, '9', 16) == test + 0x9);
  MOZ_RELEASE_ASSERT(SIMD::memchr8(test, 'a', 16) == test + 0xa);
  MOZ_RELEASE_ASSERT(SIMD::memchr8(test, 'b', 16) == test + 0xb);
  MOZ_RELEASE_ASSERT(SIMD::memchr8(test, 'c', 16) == test + 0xc);
  MOZ_RELEASE_ASSERT(SIMD::memchr8(test, 'd', 16) == test + 0xd);
  MOZ_RELEASE_ASSERT(SIMD::memchr8(test, 'e', 16) == test + 0xe);
  MOZ_RELEASE_ASSERT(SIMD::memchr8(test, 'f', 16) == test + 0xf);
  MOZ_RELEASE_ASSERT(SIMD::memchr8(test, '\n', 16) == nullptr);
}

void TestLongString() {
  // NOTE: here we make sure we go all the way up to 256 to ensure we're
  // handling negative-valued chars appropriately. We don't need to bother
  // testing this side of things with char16_t's because they are very
  // sensibly guaranteed to be unsigned.
  const size_t count = 256;
  char test[count];
  for (size_t i = 0; i < count; ++i) {
    test[i] = static_cast<char>(i);
  }

  for (size_t i = 0; i < count - 1; ++i) {
    MOZ_RELEASE_ASSERT(SIMD::memchr8(test, static_cast<char>(i), count - 1) ==
                       test + i);
  }
  MOZ_RELEASE_ASSERT(
      SIMD::memchr8(test, static_cast<char>(count - 1), count - 1) == nullptr);
}

void TestGauntlet() {
  const size_t count = 256;
  char test[count];
  for (size_t i = 0; i < count; ++i) {
    test[i] = static_cast<char>(i);
  }

  for (size_t i = 0; i < count - 1; ++i) {
    for (size_t j = 0; j < count - 1; ++j) {
      for (size_t k = 0; k < count - 1; ++k) {
        if (i >= k) {
          const char* expected = nullptr;
          if (j >= k && j < i) {
            expected = test + j;
          }
          MOZ_RELEASE_ASSERT(
              SIMD::memchr8(test + k, static_cast<char>(j), i - k) == expected);
        }
      }
    }
  }
}

void TestTinyString16() {
  const char16_t* test = u"012\n";

  MOZ_RELEASE_ASSERT(SIMD::memchr16(test, u'0', 3) == test + 0x0);
  MOZ_RELEASE_ASSERT(SIMD::memchr16(test, u'1', 3) == test + 0x1);
  MOZ_RELEASE_ASSERT(SIMD::memchr16(test, u'2', 3) == test + 0x2);
  MOZ_RELEASE_ASSERT(SIMD::memchr16(test, u'\n', 3) == nullptr);
}

void TestShortString16() {
  const char16_t* test = u"0123456789\n";

  MOZ_RELEASE_ASSERT(SIMD::memchr16(test, u'0', 10) == test + 0x0);
  MOZ_RELEASE_ASSERT(SIMD::memchr16(test, u'1', 10) == test + 0x1);
  MOZ_RELEASE_ASSERT(SIMD::memchr16(test, u'2', 10) == test + 0x2);
  MOZ_RELEASE_ASSERT(SIMD::memchr16(test, u'3', 10) == test + 0x3);
  MOZ_RELEASE_ASSERT(SIMD::memchr16(test, u'4', 10) == test + 0x4);
  MOZ_RELEASE_ASSERT(SIMD::memchr16(test, u'5', 10) == test + 0x5);
  MOZ_RELEASE_ASSERT(SIMD::memchr16(test, u'6', 10) == test + 0x6);
  MOZ_RELEASE_ASSERT(SIMD::memchr16(test, u'7', 10) == test + 0x7);
  MOZ_RELEASE_ASSERT(SIMD::memchr16(test, u'8', 10) == test + 0x8);
  MOZ_RELEASE_ASSERT(SIMD::memchr16(test, u'9', 10) == test + 0x9);
  MOZ_RELEASE_ASSERT(SIMD::memchr16(test, u'\n', 10) == nullptr);
}

void TestMediumString16() {
  const char16_t* test = u"0123456789abcdef\n";

  MOZ_RELEASE_ASSERT(SIMD::memchr16(test, u'0', 16) == test + 0x0);
  MOZ_RELEASE_ASSERT(SIMD::memchr16(test, u'1', 16) == test + 0x1);
  MOZ_RELEASE_ASSERT(SIMD::memchr16(test, u'2', 16) == test + 0x2);
  MOZ_RELEASE_ASSERT(SIMD::memchr16(test, u'3', 16) == test + 0x3);
  MOZ_RELEASE_ASSERT(SIMD::memchr16(test, u'4', 16) == test + 0x4);
  MOZ_RELEASE_ASSERT(SIMD::memchr16(test, u'5', 16) == test + 0x5);
  MOZ_RELEASE_ASSERT(SIMD::memchr16(test, u'6', 16) == test + 0x6);
  MOZ_RELEASE_ASSERT(SIMD::memchr16(test, u'7', 16) == test + 0x7);
  MOZ_RELEASE_ASSERT(SIMD::memchr16(test, u'8', 16) == test + 0x8);
  MOZ_RELEASE_ASSERT(SIMD::memchr16(test, u'9', 16) == test + 0x9);
  MOZ_RELEASE_ASSERT(SIMD::memchr16(test, u'a', 16) == test + 0xa);
  MOZ_RELEASE_ASSERT(SIMD::memchr16(test, u'b', 16) == test + 0xb);
  MOZ_RELEASE_ASSERT(SIMD::memchr16(test, u'c', 16) == test + 0xc);
  MOZ_RELEASE_ASSERT(SIMD::memchr16(test, u'd', 16) == test + 0xd);
  MOZ_RELEASE_ASSERT(SIMD::memchr16(test, u'e', 16) == test + 0xe);
  MOZ_RELEASE_ASSERT(SIMD::memchr16(test, u'f', 16) == test + 0xf);
  MOZ_RELEASE_ASSERT(SIMD::memchr16(test, u'\n', 16) == nullptr);
}

void TestLongString16() {
  const size_t count = 256;
  char16_t test[count];
  for (size_t i = 0; i < count; ++i) {
    test[i] = i;
  }

  for (size_t i = 0; i < count - 1; ++i) {
    MOZ_RELEASE_ASSERT(
        SIMD::memchr16(test, static_cast<char16_t>(i), count - 1) == test + i);
  }
  MOZ_RELEASE_ASSERT(SIMD::memchr16(test, count - 1, count - 1) == nullptr);
}

void TestGauntlet16() {
  const size_t count = 257;
  char16_t test[count];
  for (size_t i = 0; i < count; ++i) {
    test[i] = i;
  }

  for (size_t i = 0; i < count - 1; ++i) {
    for (size_t j = 0; j < count - 1; ++j) {
      for (size_t k = 0; k < count - 1; ++k) {
        if (i >= k) {
          const char16_t* expected = nullptr;
          if (j >= k && j < i) {
            expected = test + j;
          }
          MOZ_RELEASE_ASSERT(SIMD::memchr16(test + k, static_cast<char16_t>(j),
                                            i - k) == expected);
        }
      }
    }
  }
}

void TestTinyString2x8() {
  const char* test = "012\n";

  MOZ_RELEASE_ASSERT(SIMD::memchr2x8(test, '0', '1', 3) == test + 0x0);
  MOZ_RELEASE_ASSERT(SIMD::memchr2x8(test, '1', '2', 3) == test + 0x1);
  MOZ_RELEASE_ASSERT(SIMD::memchr2x8(test, '2', '\n', 3) == nullptr);
  MOZ_RELEASE_ASSERT(SIMD::memchr2x8(test, '0', '2', 3) == nullptr);
  MOZ_RELEASE_ASSERT(SIMD::memchr2x8(test, '1', '\n', 3) == nullptr);
}

void TestShortString2x8() {
  const char* test = "0123456789\n";

  MOZ_RELEASE_ASSERT(SIMD::memchr2x8(test, '0', '1', 10) == test + 0x0);
  MOZ_RELEASE_ASSERT(SIMD::memchr2x8(test, '1', '2', 10) == test + 0x1);
  MOZ_RELEASE_ASSERT(SIMD::memchr2x8(test, '2', '3', 10) == test + 0x2);
  MOZ_RELEASE_ASSERT(SIMD::memchr2x8(test, '3', '4', 10) == test + 0x3);
  MOZ_RELEASE_ASSERT(SIMD::memchr2x8(test, '4', '5', 10) == test + 0x4);
  MOZ_RELEASE_ASSERT(SIMD::memchr2x8(test, '5', '6', 10) == test + 0x5);
  MOZ_RELEASE_ASSERT(SIMD::memchr2x8(test, '6', '7', 10) == test + 0x6);
  MOZ_RELEASE_ASSERT(SIMD::memchr2x8(test, '7', '8', 10) == test + 0x7);
  MOZ_RELEASE_ASSERT(SIMD::memchr2x8(test, '8', '9', 10) == test + 0x8);
  MOZ_RELEASE_ASSERT(SIMD::memchr2x8(test, '9', '\n', 10) == nullptr);
}

void TestMediumString2x8() {
  const char* test = "0123456789abcdef\n";

  MOZ_RELEASE_ASSERT(SIMD::memchr2x8(test, '0', '1', 16) == test + 0x0);
  MOZ_RELEASE_ASSERT(SIMD::memchr2x8(test, '1', '2', 16) == test + 0x1);
  MOZ_RELEASE_ASSERT(SIMD::memchr2x8(test, '2', '3', 16) == test + 0x2);
  MOZ_RELEASE_ASSERT(SIMD::memchr2x8(test, '3', '4', 16) == test + 0x3);
  MOZ_RELEASE_ASSERT(SIMD::memchr2x8(test, '4', '5', 16) == test + 0x4);
  MOZ_RELEASE_ASSERT(SIMD::memchr2x8(test, '5', '6', 16) == test + 0x5);
  MOZ_RELEASE_ASSERT(SIMD::memchr2x8(test, '6', '7', 16) == test + 0x6);
  MOZ_RELEASE_ASSERT(SIMD::memchr2x8(test, '7', '8', 16) == test + 0x7);
  MOZ_RELEASE_ASSERT(SIMD::memchr2x8(test, '8', '9', 16) == test + 0x8);
  MOZ_RELEASE_ASSERT(SIMD::memchr2x8(test, '9', 'a', 16) == test + 0x9);
  MOZ_RELEASE_ASSERT(SIMD::memchr2x8(test, 'a', 'b', 16) == test + 0xa);
  MOZ_RELEASE_ASSERT(SIMD::memchr2x8(test, 'b', 'c', 16) == test + 0xb);
  MOZ_RELEASE_ASSERT(SIMD::memchr2x8(test, 'c', 'd', 16) == test + 0xc);
  MOZ_RELEASE_ASSERT(SIMD::memchr2x8(test, 'd', 'e', 16) == test + 0xd);
  MOZ_RELEASE_ASSERT(SIMD::memchr2x8(test, 'e', 'f', 16) == test + 0xe);
  MOZ_RELEASE_ASSERT(SIMD::memchr2x8(test, 'f', '\n', 16) == nullptr);
}

void TestLongString2x8() {
  const size_t count = 256;
  char test[count];
  for (size_t i = 0; i < count; ++i) {
    test[i] = static_cast<char>(i);
  }

  for (size_t i = 0; i < count - 2; ++i) {
    MOZ_RELEASE_ASSERT(SIMD::memchr2x8(test, static_cast<char>(i),
                                       static_cast<char>(i + 1),
                                       count - 1) == test + i);
  }
  MOZ_RELEASE_ASSERT(SIMD::memchr2x8(test, static_cast<char>(count - 2),
                                     static_cast<char>(count - 1),
                                     count - 1) == nullptr);
}

void TestTinyString2x16() {
  const char16_t* test = u"012\n";

  MOZ_RELEASE_ASSERT(SIMD::memchr2x16(test, u'0', u'1', 3) == test + 0x0);
  MOZ_RELEASE_ASSERT(SIMD::memchr2x16(test, u'1', u'2', 3) == test + 0x1);
  MOZ_RELEASE_ASSERT(SIMD::memchr2x16(test, u'2', u'\n', 3) == nullptr);
  MOZ_RELEASE_ASSERT(SIMD::memchr2x16(test, u'0', u'2', 3) == nullptr);
  MOZ_RELEASE_ASSERT(SIMD::memchr2x16(test, u'1', u'\n', 3) == nullptr);
}

void TestShortString2x16() {
  const char16_t* test = u"0123456789\n";

  MOZ_RELEASE_ASSERT(SIMD::memchr2x16(test, u'0', u'1', 10) == test + 0x0);
  MOZ_RELEASE_ASSERT(SIMD::memchr2x16(test, u'1', u'2', 10) == test + 0x1);
  MOZ_RELEASE_ASSERT(SIMD::memchr2x16(test, u'2', u'3', 10) == test + 0x2);
  MOZ_RELEASE_ASSERT(SIMD::memchr2x16(test, u'3', u'4', 10) == test + 0x3);
  MOZ_RELEASE_ASSERT(SIMD::memchr2x16(test, u'4', u'5', 10) == test + 0x4);
  MOZ_RELEASE_ASSERT(SIMD::memchr2x16(test, u'5', u'6', 10) == test + 0x5);
  MOZ_RELEASE_ASSERT(SIMD::memchr2x16(test, u'6', u'7', 10) == test + 0x6);
  MOZ_RELEASE_ASSERT(SIMD::memchr2x16(test, u'7', u'8', 10) == test + 0x7);
  MOZ_RELEASE_ASSERT(SIMD::memchr2x16(test, u'8', u'9', 10) == test + 0x8);
  MOZ_RELEASE_ASSERT(SIMD::memchr2x16(test, u'9', u'\n', 10) == nullptr);
  MOZ_RELEASE_ASSERT(SIMD::memchr2x16(test, u'0', u'2', 10) == nullptr);
}

void TestMediumString2x16() {
  const char16_t* test = u"0123456789abcdef\n";

  MOZ_RELEASE_ASSERT(SIMD::memchr2x16(test, u'0', u'1', 16) == test + 0x0);
  MOZ_RELEASE_ASSERT(SIMD::memchr2x16(test, u'1', u'2', 16) == test + 0x1);
  MOZ_RELEASE_ASSERT(SIMD::memchr2x16(test, u'2', u'3', 16) == test + 0x2);
  MOZ_RELEASE_ASSERT(SIMD::memchr2x16(test, u'3', u'4', 16) == test + 0x3);
  MOZ_RELEASE_ASSERT(SIMD::memchr2x16(test, u'4', u'5', 16) == test + 0x4);
  MOZ_RELEASE_ASSERT(SIMD::memchr2x16(test, u'5', u'6', 16) == test + 0x5);
  MOZ_RELEASE_ASSERT(SIMD::memchr2x16(test, u'6', u'7', 16) == test + 0x6);
  MOZ_RELEASE_ASSERT(SIMD::memchr2x16(test, u'7', u'8', 16) == test + 0x7);
  MOZ_RELEASE_ASSERT(SIMD::memchr2x16(test, u'8', u'9', 16) == test + 0x8);
  MOZ_RELEASE_ASSERT(SIMD::memchr2x16(test, u'9', u'a', 16) == test + 0x9);
  MOZ_RELEASE_ASSERT(SIMD::memchr2x16(test, u'a', u'b', 16) == test + 0xa);
  MOZ_RELEASE_ASSERT(SIMD::memchr2x16(test, u'b', u'c', 16) == test + 0xb);
  MOZ_RELEASE_ASSERT(SIMD::memchr2x16(test, u'c', u'd', 16) == test + 0xc);
  MOZ_RELEASE_ASSERT(SIMD::memchr2x16(test, u'd', u'e', 16) == test + 0xd);
  MOZ_RELEASE_ASSERT(SIMD::memchr2x16(test, u'e', u'f', 16) == test + 0xe);
  MOZ_RELEASE_ASSERT(SIMD::memchr2x16(test, u'f', u'\n', 16) == nullptr);
  MOZ_RELEASE_ASSERT(SIMD::memchr2x16(test, u'0', u'2', 10) == nullptr);
}

void TestLongString2x16() {
  const size_t count = 257;
  char16_t test[count];
  for (size_t i = 0; i < count; ++i) {
    test[i] = static_cast<char16_t>(i);
  }

  for (size_t i = 0; i < count - 2; ++i) {
    MOZ_RELEASE_ASSERT(SIMD::memchr2x16(test, static_cast<char16_t>(i),
                                        static_cast<char16_t>(i + 1),
                                        count - 1) == test + i);
  }
  MOZ_RELEASE_ASSERT(SIMD::memchr2x16(test, static_cast<char16_t>(count - 2),
                                      static_cast<char16_t>(count - 1),
                                      count - 1) == nullptr);
}

void TestGauntlet2x8() {
  const size_t count = 256;
  char test[count * 2];
  // load in the evens
  for (size_t i = 0; i < count / 2; ++i) {
    test[i] = static_cast<char>(2 * i);
  }
  // load in the odds
  for (size_t i = 0; i < count / 2; ++i) {
    test[count / 2 + i] = static_cast<char>(2 * i + 1);
  }
  // load in evens and odds sequentially
  for (size_t i = 0; i < count; ++i) {
    test[count + i] = static_cast<char>(i);
  }

  for (size_t i = 0; i < count - 1; ++i) {
    for (size_t j = 0; j < count - 2; ++j) {
      for (size_t k = 0; k < count - 1; ++k) {
        if (i > k + 1) {
          const char* expected1 = nullptr;
          const char* expected2 = nullptr;
          if (i > j + 1) {
            expected1 = test + j + count;  // Add count to skip over odds/evens
            if (j >= k) {
              expected2 = test + j + count;
            }
          }
          char a = static_cast<char>(j);
          char b = static_cast<char>(j + 1);
          // Make sure it doesn't pick up any in the alternating odd/even
          MOZ_RELEASE_ASSERT(SIMD::memchr2x8(test + k, a, b, i - k + count) ==
                             expected1);
          // Make sure we cover smaller inputs
          MOZ_RELEASE_ASSERT(SIMD::memchr2x8(test + k + count, a, b, i - k) ==
                             expected2);
        }
      }
    }
  }
}

void TestGauntlet2x16() {
  const size_t count = 1024;
  char16_t test[count * 2];
  // load in the evens
  for (size_t i = 0; i < count / 2; ++i) {
    test[i] = static_cast<char16_t>(2 * i);
  }
  // load in the odds
  for (size_t i = 0; i < count / 2; ++i) {
    test[count / 2 + i] = static_cast<char16_t>(2 * i + 1);
  }
  // load in evens and odds sequentially
  for (size_t i = 0; i < count; ++i) {
    test[count + i] = static_cast<char16_t>(i);
  }

  for (size_t i = 0; i < count - 1; ++i) {
    for (size_t j = 0; j < count - 2; ++j) {
      for (size_t k = 0; k < count - 1; ++k) {
        if (i > k + 1) {
          const char16_t* expected1 = nullptr;
          const char16_t* expected2 = nullptr;
          if (i > j + 1) {
            expected1 = test + j + count;  // Add count to skip over odds/evens
            if (j >= k) {
              expected2 = test + j + count;
            }
          }
          char16_t a = static_cast<char16_t>(j);
          char16_t b = static_cast<char16_t>(j + 1);
          // Make sure it doesn't pick up any in the alternating odd/even
          MOZ_RELEASE_ASSERT(SIMD::memchr2x16(test + k, a, b, i - k + count) ==
                             expected1);
          // Make sure we cover smaller inputs
          MOZ_RELEASE_ASSERT(SIMD::memchr2x16(test + k + count, a, b, i - k) ==
                             expected2);
        }
      }
    }
  }
}

void TestSpecialCases() {
  // The following 4 asserts test the case where we do two overlapping checks,
  // where the first one ends with our first search character, and the second
  // one begins with our search character. Since they are overlapping, we want
  // to ensure that the search function doesn't carry the match from the
  // first check over to the second check.
  const char* test1 = "x123456789abcdey";
  MOZ_RELEASE_ASSERT(SIMD::memchr2x8(test1, 'y', 'x', 16) == nullptr);
  const char* test2 = "1000000000000000200000000000000030b000000000000a40";
  MOZ_RELEASE_ASSERT(SIMD::memchr2x8(test2, 'a', 'b', 50) == nullptr);
  const char16_t* test1wide = u"x123456y";
  MOZ_RELEASE_ASSERT(SIMD::memchr2x16(test1wide, 'y', 'x', 8) == nullptr);
  const char16_t* test2wide = u"100000002000000030b0000a40";
  MOZ_RELEASE_ASSERT(SIMD::memchr2x16(test2wide, 'a', 'b', 26) == nullptr);
}

int main(void) {
  TestTinyString();
  TestShortString();
  TestMediumString();
  TestLongString();
  TestGauntlet();

  TestTinyString16();
  TestShortString16();
  TestMediumString16();
  TestLongString16();
  TestGauntlet16();

  TestTinyString2x8();
  TestShortString2x8();
  TestMediumString2x8();
  TestLongString2x8();

  TestTinyString2x16();
  TestShortString2x16();
  TestMediumString2x16();
  TestLongString2x16();

  TestSpecialCases();

  // These are too slow to run all the time, but they should be run when making
  // meaningful changes just to be sure.
  // TestGauntlet2x8();
  // TestGauntlet2x16();

  return 0;
}
