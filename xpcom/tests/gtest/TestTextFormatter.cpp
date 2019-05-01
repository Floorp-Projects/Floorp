/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTextFormatter.h"
#include "nsString.h"
#include "gtest/gtest.h"

TEST(TextFormatter, Tests)
{
  nsAutoString fmt(NS_LITERAL_STRING("%3$s %4$S %1$d %2$d %2$d %3$s"));
  char utf8[] = "Hello";
  char16_t ucs2[] = {'W',    'o',    'r',    'l',    'd',
                     0x4e00, 0xAc00, 0xFF45, 0x0103, 0x00};
  int d = 3;

  char16_t buf[256];
  nsTextFormatter::snprintf(buf, 256, fmt.get(), d, 333, utf8, ucs2);
  nsAutoString out(buf);

  const char16_t* uout = out.get();
  const char16_t expected[] = {
      0x48,   0x65,   0x6C,   0x6C,   0x6F, 0x20, 0x57, 0x6F, 0x72, 0x6C, 0x64,
      0x4E00, 0xAC00, 0xFF45, 0x0103, 0x20, 0x33, 0x20, 0x33, 0x33, 0x33, 0x20,
      0x33,   0x33,   0x33,   0x20,   0x48, 0x65, 0x6C, 0x6C, 0x6F};

  for (uint32_t i = 0; i < out.Length(); i++) {
    ASSERT_EQ(uout[i], expected[i]);
  }

  // Test that an unrecognized escape is passed through.
  nsString out2;
  nsTextFormatter::ssprintf(out2, u"%1m!", 23);
  EXPECT_STREQ("%1m!", NS_ConvertUTF16toUTF8(out2).get());

  // Treat NULL the same in both %s cases.
  nsTextFormatter::ssprintf(out2, u"%s %S", (char*)nullptr, (char16_t*)nullptr);
  EXPECT_STREQ("(null) (null)", NS_ConvertUTF16toUTF8(out2).get());

  nsTextFormatter::ssprintf(out2, u"%lld", INT64_MIN);
  EXPECT_STREQ("-9223372036854775808", NS_ConvertUTF16toUTF8(out2).get());

  // Regression test for bug 1401821.
  nsTextFormatter::ssprintf(out2, u"%*.f", 0, 23.2);
  EXPECT_STREQ("23", NS_ConvertUTF16toUTF8(out2).get());
}

/*
 * Check misordered parameters
 */

TEST(TextFormatterOrdering, orders)
{
  nsString out;

  // plain list
  nsTextFormatter::ssprintf(out, u"%S %S %S", u"1", u"2", u"3");
  EXPECT_STREQ("1 2 3", NS_ConvertUTF16toUTF8(out).get());

  // ordered list
  nsTextFormatter::ssprintf(out, u"%2$S %3$S %1$S", u"1", u"2", u"3");
  EXPECT_STREQ("2 3 1", NS_ConvertUTF16toUTF8(out).get());

  // Mixed ordered list and non-ordered does not work.  This shouldn't
  // crash (hence the calls to ssprintf) but should fail for for
  // snprintf.
  nsTextFormatter::ssprintf(out, u"%2S %S %1$S", u"1", u"2", u"3");
  nsTextFormatter::ssprintf(out, u"%S %2$S", u"1", u"2");
  char16_t buffer[1024];  // plenty big
  EXPECT_EQ(nsTextFormatter::snprintf(buffer, sizeof(buffer), u"%2S %S %1$S",
                                      u"1", u"2", u"3"),
            uint32_t(-1));
  EXPECT_EQ(
      nsTextFormatter::snprintf(buffer, sizeof(buffer), u"%S %2$S", u"1", u"2"),
      uint32_t(-1));

  // Referencing an extra param returns empty strings in release.
#ifndef DEBUG
  nsTextFormatter::ssprintf(out, u" %2$S ", u"1");
  EXPECT_STREQ("  ", NS_ConvertUTF16toUTF8(out).get());
#endif

  // Double referencing existing argument works
  nsTextFormatter::ssprintf(out, u"%1$S %1$S", u"1");
  EXPECT_STREQ("1 1", NS_ConvertUTF16toUTF8(out).get());

  // Dropping trailing argument works
  nsTextFormatter::ssprintf(out, u" %1$S ", u"1", u"2");
  EXPECT_STREQ(" 1 ", NS_ConvertUTF16toUTF8(out).get());

  // Dropping leading arguments works
  nsTextFormatter::ssprintf(out, u" %2$S ", u"1", u"2");
  EXPECT_STREQ(" 2 ", NS_ConvertUTF16toUTF8(out).get());

  // Dropping middle arguments works
  nsTextFormatter::ssprintf(out, u" %3$S %1$S ", u"1", u"2", u"3");
  EXPECT_STREQ(" 3 1 ", NS_ConvertUTF16toUTF8(out).get());
}

/*
 * Tests to validate that horrible things don't happen if the passed-in
 * variable and the formatter don't match.
 */
TEST(TextFormatterTestMismatch, format_d)
{
  nsString out;
  // just for completeness, this is our format, and works
  nsTextFormatter::ssprintf(out, u"%d", int(-1));
  EXPECT_STREQ("-1", NS_ConvertUTF16toUTF8(out).get());
  nsTextFormatter::ssprintf(out, u"%d", uint32_t(-1));
  EXPECT_STREQ("4294967295", NS_ConvertUTF16toUTF8(out).get());
#ifndef DEBUG
  nsTextFormatter::ssprintf(out, u"%d", float(3.5));
  EXPECT_STREQ("3.5", NS_ConvertUTF16toUTF8(out).get());
  nsTextFormatter::ssprintf(out, u"%d", "foo");
  EXPECT_STREQ("foo", NS_ConvertUTF16toUTF8(out).get());
  nsTextFormatter::ssprintf(out, u"%d", u"foo");
  EXPECT_STREQ("foo", NS_ConvertUTF16toUTF8(out).get());
#endif
}

TEST(TextFormatterTestMismatch, format_u)
{
  nsString out;
  nsTextFormatter::ssprintf(out, u"%u", int(-1));
  EXPECT_STREQ("4294967295", NS_ConvertUTF16toUTF8(out).get());
  // just for completeness, this is our format, and works
  nsTextFormatter::ssprintf(out, u"%u", uint32_t(-1));
  EXPECT_STREQ("4294967295", NS_ConvertUTF16toUTF8(out).get());
#ifndef DEBUG
  nsTextFormatter::ssprintf(out, u"%u", float(3.5));
  EXPECT_STREQ("3.5", NS_ConvertUTF16toUTF8(out).get());
  nsTextFormatter::ssprintf(out, u"%u", "foo");
  EXPECT_STREQ("foo", NS_ConvertUTF16toUTF8(out).get());
  nsTextFormatter::ssprintf(out, u"%u", u"foo");
  EXPECT_STREQ("foo", NS_ConvertUTF16toUTF8(out).get());
#endif
}

TEST(TextFormatterTestMismatch, format_x)
{
  nsString out;
  nsTextFormatter::ssprintf(out, u"%x", int32_t(-1));
  EXPECT_STREQ("ffffffff", NS_ConvertUTF16toUTF8(out).get());
  // just for completeness, this is our format, and works
  nsTextFormatter::ssprintf(out, u"%x", uint32_t(-1));
  EXPECT_STREQ("ffffffff", NS_ConvertUTF16toUTF8(out).get());
#ifndef DEBUG
  nsTextFormatter::ssprintf(out, u"%x", float(3.5));
  EXPECT_STREQ("3.5", NS_ConvertUTF16toUTF8(out).get());
  nsTextFormatter::ssprintf(out, u"%x", "foo");
  EXPECT_STREQ("foo", NS_ConvertUTF16toUTF8(out).get());
  nsTextFormatter::ssprintf(out, u"%x", u"foo");
  EXPECT_STREQ("foo", NS_ConvertUTF16toUTF8(out).get());
#endif
}

TEST(TextFormatterTestMismatch, format_s)
{
  nsString out;
#ifndef DEBUG
  nsTextFormatter::ssprintf(out, u"%s", int(-1));
  EXPECT_STREQ("-1", NS_ConvertUTF16toUTF8(out).get());
  nsTextFormatter::ssprintf(out, u"%s", uint32_t(-1));
  EXPECT_STREQ("4294967295", NS_ConvertUTF16toUTF8(out).get());
  nsTextFormatter::ssprintf(out, u"%s", float(3.5));
  EXPECT_STREQ("3.5", NS_ConvertUTF16toUTF8(out).get());
#endif
  // just for completeness, this is our format, and works
  nsTextFormatter::ssprintf(out, u"%s", "foo");
  EXPECT_STREQ("foo", NS_ConvertUTF16toUTF8(out).get());
#ifndef DEBUG
  nsTextFormatter::ssprintf(out, u"%s", u"foo");
  EXPECT_STREQ("foo", NS_ConvertUTF16toUTF8(out).get());
#endif
}

TEST(TextFormatterTestMismatch, format_S)
{
  nsString out;
#ifndef DEBUG
  nsTextFormatter::ssprintf(out, u"%S", int32_t(-1));
  EXPECT_STREQ("-1", NS_ConvertUTF16toUTF8(out).get());
  nsTextFormatter::ssprintf(out, u"%S", uint32_t(-1));
  EXPECT_STREQ("4294967295", NS_ConvertUTF16toUTF8(out).get());
  nsTextFormatter::ssprintf(out, u"%S", float(3.5));
  EXPECT_STREQ("3.5", NS_ConvertUTF16toUTF8(out).get());
  nsTextFormatter::ssprintf(out, u"%S", "foo");
  EXPECT_STREQ("foo", NS_ConvertUTF16toUTF8(out).get());
#endif
  // just for completeness, this is our format, and works
  nsTextFormatter::ssprintf(out, u"%S", u"foo");
  EXPECT_STREQ("foo", NS_ConvertUTF16toUTF8(out).get());
}

TEST(TextFormatterTestMismatch, format_c)
{
  nsString out;
  nsTextFormatter::ssprintf(out, u"%c", int32_t(-1));
  EXPECT_EQ(1u, out.Length());
  EXPECT_EQ((uint16_t)-1, out.CharAt(0));  // not useful for humans :-/
  nsTextFormatter::ssprintf(out, u"%c", uint32_t(-1));
  EXPECT_EQ(1u, out.Length());
  EXPECT_EQ((uint16_t)-1, out.CharAt(0));  // not useful for humans :-/
#ifndef DEBUG
  nsTextFormatter::ssprintf(out, u"%c", float(3.5));
  EXPECT_STREQ("3.5", NS_ConvertUTF16toUTF8(out).get());
  nsTextFormatter::ssprintf(out, u"%c", "foo");
  EXPECT_STREQ("foo", NS_ConvertUTF16toUTF8(out).get());
  nsTextFormatter::ssprintf(out, u"%c", u"foo");
  EXPECT_STREQ("foo", NS_ConvertUTF16toUTF8(out).get());
#endif

  // just for completeness, this is our format, and works
  nsTextFormatter::ssprintf(out, u"%c", 'c');
  EXPECT_EQ(1u, out.Length());
  EXPECT_EQ(u'c', out.CharAt(0));
  nsTextFormatter::ssprintf(out, u"%c", u'c');
  EXPECT_EQ(1u, out.Length());
  EXPECT_EQ(u'c', out.CharAt(0));
}

TEST(TextFormatterTestResults, Tests)
{
  char16_t buf[10];

  EXPECT_EQ(
      nsTextFormatter::snprintf(buf, 10, u"%s", "more than 10 characters"), 9u);
  EXPECT_EQ(buf[9], '\0');
  EXPECT_STREQ("more than", NS_ConvertUTF16toUTF8(buf).get());

  nsString out;
  nsTextFormatter::ssprintf(out, u"%s", "more than 10 characters");
  // The \0 isn't written here.
  EXPECT_EQ(out.Length(), 23u);
}
