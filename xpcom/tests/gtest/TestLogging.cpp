/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Logging.h"
#include "mozilla/Sprintf.h"
#include "gtest/gtest.h"

namespace mozilla::detail {
bool LimitFileToLessThanSize(const char* aFilename, uint32_t aSize,
                             uint16_t aLongLineSize);
}

// These format strings result in 1024 byte lines on disk regardless
// of OS, which makes various file sizes OS-agnostic.
#ifdef XP_WIN
#  define WHOLE_LINE "%01022d\n"
#  define SHORT_LINE "%0510d\n"
#else
#  define WHOLE_LINE "%01023d\n"
#  define SHORT_LINE "%0511d\n"
#endif

// Write the given number of 1k lines to the given file name.
void WriteTestLogFile(const char* name, uint32_t numLines) {
  FILE* f = fopen(name, "w");
  ASSERT_NE(f, (FILE*)nullptr);

  for (uint32_t i = 0; i < numLines; i++) {
    char buf[1024 + 1];
    SprintfLiteral(buf, WHOLE_LINE, i);
    EXPECT_TRUE(fputs(buf, f) >= 0);
  }

  uint64_t size = static_cast<uint64_t>(ftell(f));

  // Close before asserting.
  EXPECT_FALSE(fclose(f));

  ASSERT_EQ(numLines * 1024, size);
}

// Assert that the given file name has the expected size and that its
// first line is the expected line.
void AssertSizeAndFirstLine(const char* name, uint32_t expectedSize,
                            const char* expectedLine) {
  FILE* f = fopen(name, "r");
  ASSERT_NE(f, (FILE*)nullptr);

  EXPECT_FALSE(fseek(f, 0, SEEK_END));
  uint64_t size = static_cast<uint64_t>(ftell(f));

  EXPECT_FALSE(fseek(f, 0, SEEK_SET));

  char line[1024 + 1];
  const char* result = fgets(line, sizeof(line), f);

  // Close before asserting.
  EXPECT_FALSE(fclose(f));

  ASSERT_NE(result, nullptr);
  ASSERT_EQ(expectedSize, size);
  ASSERT_STREQ(expectedLine, line);
}

TEST(Logging, DoesNothingWhenNotNeededExact)
{
  char nameBuf[2048];
  SprintfLiteral(
      nameBuf, "%s_%s.moz_log",
      testing::UnitTest::GetInstance()->current_test_info()->test_case_name(),
      testing::UnitTest::GetInstance()->current_test_info()->name());

  WriteTestLogFile(nameBuf, 256);

  // Here the log file is exactly the allowed size.  It shouldn't be limited.
  ASSERT_TRUE(
      mozilla::detail::LimitFileToLessThanSize(nameBuf, 256 * 1024, 1024));

  char expectedLine[1024 + 1];
  SprintfLiteral(expectedLine, WHOLE_LINE, 0);

  AssertSizeAndFirstLine(nameBuf, 256 * 1024, expectedLine);

  EXPECT_FALSE(remove(nameBuf));
}

TEST(Logging, DoesNothingWhenNotNeededInexact)
{
  char nameBuf[2048];
  SprintfLiteral(
      nameBuf, "%s_%s.moz_log",
      testing::UnitTest::GetInstance()->current_test_info()->test_case_name(),
      testing::UnitTest::GetInstance()->current_test_info()->name());

  WriteTestLogFile(nameBuf, 200);

  // Here the log file is strictly less than the allowed size.  It shouldn't be
  // limited.
  ASSERT_TRUE(
      mozilla::detail::LimitFileToLessThanSize(nameBuf, 256 * 1024, 1024));

  char expectedLine[1024 + 1];
  SprintfLiteral(expectedLine, WHOLE_LINE, 0);

  AssertSizeAndFirstLine(nameBuf, 200 * 1024, expectedLine);

  EXPECT_FALSE(remove(nameBuf));
}

TEST(Logging, LimitsToLessThanSize)
{
  char nameBuf[2048];
  SprintfLiteral(
      nameBuf, "%s_%s.moz_log",
      testing::UnitTest::GetInstance()->current_test_info()->test_case_name(),
      testing::UnitTest::GetInstance()->current_test_info()->name());

  WriteTestLogFile(nameBuf, 300);

  ASSERT_TRUE(
      mozilla::detail::LimitFileToLessThanSize(nameBuf, 256 * 1024, 1024));

  char expectedLine[1024 + 1];
  SprintfLiteral(expectedLine, WHOLE_LINE, 300 - 256);

  AssertSizeAndFirstLine(nameBuf, 256 * 1024, expectedLine);

  EXPECT_FALSE(remove(nameBuf));
}

TEST(Logging, MayCutLongLinesExact)
{
  char nameBuf[2048];
  SprintfLiteral(
      nameBuf, "%s_%s.moz_log",
      testing::UnitTest::GetInstance()->current_test_info()->test_case_name(),
      testing::UnitTest::GetInstance()->current_test_info()->name());

  WriteTestLogFile(nameBuf, 300);

  char expectedLine[1024 + 1];

  ASSERT_TRUE(mozilla::detail::LimitFileToLessThanSize(
      nameBuf, (256 * 1024) - 512, 512));

  SprintfLiteral(expectedLine, SHORT_LINE, 300 - 256);

  // The line to be cut ends "...044\n."  We read 512 bytes (the
  // buffer size), so we're left with 512 bytes, one of which is the
  // newline.
  AssertSizeAndFirstLine(nameBuf, 256 * 1024 - 512, expectedLine);

  EXPECT_FALSE(remove(nameBuf));
}

TEST(Logging, MayCutLongLinesInexact)
{
  char nameBuf[2048];
  SprintfLiteral(
      nameBuf, "%s_%s.moz_log",
      testing::UnitTest::GetInstance()->current_test_info()->test_case_name(),
      testing::UnitTest::GetInstance()->current_test_info()->name());

  WriteTestLogFile(nameBuf, 300);

  char expectedLine[1024 + 1];

  ASSERT_TRUE(mozilla::detail::LimitFileToLessThanSize(
      nameBuf, (256 * 1024) - 512, 512));

  SprintfLiteral(expectedLine, SHORT_LINE, 300 - 256);

  // We read 512 bytes (the buffer size), so we're left with 512
  // bytes, one of which is the newline.  Notice that the limited size
  // is smaller than the requested size.
  AssertSizeAndFirstLine(nameBuf, 256 * 1024 - 512, expectedLine);

  EXPECT_FALSE(remove(nameBuf));
}
