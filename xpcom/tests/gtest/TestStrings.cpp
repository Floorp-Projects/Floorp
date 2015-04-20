/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <stdlib.h>
#include "nsString.h"
#include "nsStringBuffer.h"
#include "nsReadableUtils.h"
#include "nsCRTGlue.h"
#include "nsRefPtr.h"
#include "nsTArray.h"
#include "gtest/gtest.h"

namespace TestStrings {

using mozilla::fallible;

void test_assign_helper(const nsACString& in, nsACString &_retval)
{
  _retval = in;
}

TEST(Strings, assign)
{
  nsCString result;
  test_assign_helper(NS_LITERAL_CSTRING("a") + NS_LITERAL_CSTRING("b"), result);
  EXPECT_STREQ(result.get(), "ab");
}

TEST(Strings, assign_c)
{
  nsCString c; c.Assign('c');
  EXPECT_STREQ(c.get(), "c");
}

TEST(Strings, test1)
{
  NS_NAMED_LITERAL_STRING(empty, "");
  const nsAString& aStr = empty;

  nsAutoString buf(aStr);

  int32_t n = buf.FindChar(',');
  EXPECT_EQ(n, kNotFound);

  n = buf.Length();

  buf.Cut(0, n + 1);
  n = buf.FindChar(',');

  EXPECT_EQ(n, kNotFound);
}

TEST(Strings, test2)
{
  nsCString data("hello world");
  const nsACString& aStr = data;

  nsCString temp(aStr);
  temp.Cut(0, 6);

  EXPECT_STREQ(temp.get(), "world");
}

TEST(Strings, find)
{
  nsCString src("<!DOCTYPE blah blah blah>");

  int32_t i = src.Find("DOCTYPE", true, 2, 1);
  EXPECT_EQ(i, 2);
}

TEST(Strings, rfind)
{
  const char text[] = "<!DOCTYPE blah blah blah>";
  const char term[] = "bLaH";
  nsCString src(text);
  int32_t i;

  i = src.RFind(term, true, 3, -1); 
  EXPECT_EQ(i, kNotFound);

  i = src.RFind(term, true, -1, -1);
  EXPECT_EQ(i, 20);

  i = src.RFind(term, true, 13, -1);
  EXPECT_EQ(i, 10);

  i = src.RFind(term, true, 22, 3);
  EXPECT_EQ(i, 20);
}

TEST(Strings, rfind_2)
{
  const char text[] = "<!DOCTYPE blah blah blah>";
  nsCString src(text);
  int32_t i = src.RFind("TYPE", false, 5, -1); 
  EXPECT_EQ(i, 5);
}

TEST(Strings, rfind_3)
{
  const char text[] = "urn:mozilla:locale:en-US:necko";
  nsAutoCString value(text);
  int32_t i = value.RFind(":");
  EXPECT_EQ(i, 24);
}

TEST(Strings, rfind_4)
{
  nsCString value("a.msf");
  int32_t i = value.RFind(".msf");
  EXPECT_EQ(i, 1);
}

TEST(Strings, findinreadable)
{
  const char text[] = "jar:jar:file:///c:/software/mozilla/mozilla_2006_02_21.jar!/browser/chrome/classic.jar!/";
  nsAutoCString value(text);

  nsACString::const_iterator begin, end;
  value.BeginReading(begin);
  value.EndReading(end);
  nsACString::const_iterator delim_begin (begin),
                             delim_end   (end);

  // Search for last !/ at the end of the string
  EXPECT_TRUE(FindInReadable(NS_LITERAL_CSTRING("!/"), delim_begin, delim_end));
  char *r = ToNewCString(Substring(delim_begin, delim_end));
  // Should match the first "!/" but not the last
  EXPECT_NE(delim_end, end);
  EXPECT_STREQ(r, "!/");
  free(r);

  delim_begin = begin;
  delim_end = end;

  // Search for first jar:
  EXPECT_TRUE(FindInReadable(NS_LITERAL_CSTRING("jar:"), delim_begin, delim_end));

  r = ToNewCString(Substring(delim_begin, delim_end));
  // Should not match the first jar:, but the second one
  EXPECT_EQ(delim_begin, begin);
  EXPECT_STREQ(r, "jar:");
  free(r);

  // Search for jar: in a Substring
  delim_begin = begin; delim_begin++;
  delim_end = end;
  EXPECT_TRUE(FindInReadable(NS_LITERAL_CSTRING("jar:"), delim_begin, delim_end));

  r = ToNewCString(Substring(delim_begin, delim_end));
  // Should not match the first jar:, but the second one
  EXPECT_NE(delim_begin, begin);
  EXPECT_STREQ(r, "jar:");
  free(r);

  // Should not find a match
  EXPECT_FALSE(FindInReadable(NS_LITERAL_CSTRING("gecko"), delim_begin, delim_end));

  // When no match is found, range should be empty
  EXPECT_EQ(delim_begin, delim_end);

  // Should not find a match (search not beyond Substring)
  delim_begin = begin; for (int i=0;i<6;i++) delim_begin++;
  delim_end = end;
  EXPECT_FALSE(FindInReadable(NS_LITERAL_CSTRING("jar:"), delim_begin, delim_end));

  // When no match is found, range should be empty
  EXPECT_EQ(delim_begin, delim_end);

  // Should not find a match (search not beyond Substring)
  delim_begin = begin;
  delim_end = end; for (int i=0;i<7;i++) delim_end--;
  EXPECT_FALSE(FindInReadable(NS_LITERAL_CSTRING("classic"), delim_begin, delim_end));

  // When no match is found, range should be empty
  EXPECT_EQ(delim_begin, delim_end);
}

TEST(Strings, rfindinreadable)
{
  const char text[] = "jar:jar:file:///c:/software/mozilla/mozilla_2006_02_21.jar!/browser/chrome/classic.jar!/";
  nsAutoCString value(text);

  nsACString::const_iterator begin, end;
  value.BeginReading(begin);
  value.EndReading(end);
  nsACString::const_iterator delim_begin (begin),
                             delim_end   (end);

  // Search for last !/ at the end of the string
  EXPECT_TRUE(RFindInReadable(NS_LITERAL_CSTRING("!/"), delim_begin, delim_end));
  char *r = ToNewCString(Substring(delim_begin, delim_end));
  // Should match the last "!/"
  EXPECT_EQ(delim_end, end);
  EXPECT_STREQ(r, "!/");
  free(r);

  delim_begin = begin;
  delim_end = end;

  // Search for last jar: but not the first one...
  EXPECT_TRUE(RFindInReadable(NS_LITERAL_CSTRING("jar:"), delim_begin, delim_end));

  r = ToNewCString(Substring(delim_begin, delim_end));
  // Should not match the first jar:, but the second one
  EXPECT_NE(delim_begin, begin);
  EXPECT_STREQ(r, "jar:");
  free(r);

  // Search for jar: in a Substring
  delim_begin = begin;
  delim_end = begin; for (int i=0;i<6;i++) delim_end++;
  EXPECT_TRUE(RFindInReadable(NS_LITERAL_CSTRING("jar:"), delim_begin, delim_end));

  r = ToNewCString(Substring(delim_begin, delim_end));
  // Should not match the first jar:, but the second one
  EXPECT_EQ(delim_begin, begin);
  EXPECT_STREQ(r, "jar:");
  free(r);

  // Should not find a match
  delim_begin = begin;
  delim_end = end;
  EXPECT_FALSE(RFindInReadable(NS_LITERAL_CSTRING("gecko"), delim_begin, delim_end));

  // When no match is found, range should be empty
  EXPECT_EQ(delim_begin, delim_end);

  // Should not find a match (search not before Substring)
  delim_begin = begin; for (int i=0;i<6;i++) delim_begin++;
  delim_end = end;
  EXPECT_FALSE(RFindInReadable(NS_LITERAL_CSTRING("jar:"), delim_begin, delim_end));

  // When no match is found, range should be empty
  EXPECT_EQ(delim_begin, delim_end);

  // Should not find a match (search not beyond Substring)
  delim_begin = begin;
  delim_end = end; for (int i=0;i<7;i++) delim_end--;
  EXPECT_FALSE(RFindInReadable(NS_LITERAL_CSTRING("classic"), delim_begin, delim_end));

  // When no match is found, range should be empty
  EXPECT_EQ(delim_begin, delim_end);
}

TEST(Strings, distance)
{
  const char text[] = "abc-xyz";
  nsCString s(text);
  nsCString::const_iterator begin, end;
  s.BeginReading(begin);
  s.EndReading(end);
  size_t d = Distance(begin, end);
  EXPECT_EQ(d, sizeof(text) - 1);
}

TEST(Strings, length)
{
  const char text[] = "abc-xyz";
  nsCString s(text);
  size_t d = s.Length();
  EXPECT_EQ(d, sizeof(text) - 1);
}

TEST(Strings, trim)
{
  const char text[] = " a\t    $   ";
  const char set[] = " \t$";

  nsCString s(text);
  s.Trim(set);
  EXPECT_STREQ(s.get(), "a");
}

TEST(Strings, replace_substr)
{
  const char text[] = "abc-ppp-qqq-ppp-xyz";
  nsCString s(text);
  s.ReplaceSubstring("ppp", "www");
  EXPECT_STREQ(s.get(), "abc-www-qqq-www-xyz");

  s.Assign("foobar");
  s.ReplaceSubstring("foo", "bar");
  s.ReplaceSubstring("bar", "");
  EXPECT_STREQ(s.get(), "");

  s.Assign("foofoofoo");
  s.ReplaceSubstring("foo", "foo");
  EXPECT_STREQ(s.get(), "foofoofoo");

  s.Assign("foofoofoo");
  s.ReplaceSubstring("of", "fo");
  EXPECT_STREQ(s.get(), "fofoofooo");
}

TEST(Strings, replace_substr_2)
{
  const char *oldName = nullptr;
  const char *newName = "user";
  nsString acctName; acctName.AssignLiteral("forums.foo.com");
  nsAutoString newAcctName, oldVal, newVal;
  oldVal.AssignWithConversion(oldName);
  newVal.AssignWithConversion(newName);
  newAcctName.Assign(acctName);

  // here, oldVal is empty.  we are testing that this function
  // does not hang.  see bug 235355.
  newAcctName.ReplaceSubstring(oldVal, newVal);

  // we expect that newAcctName will be unchanged.
  EXPECT_TRUE(newAcctName.Equals(acctName));
}

TEST(Strings, replace_substr_3)
{
  nsCString s;
  s.Assign("abcabcabc");
  s.ReplaceSubstring("ca", "X");
  EXPECT_STREQ(s.get(), "abXbXbc");

  s.Assign("abcabcabc");
  s.ReplaceSubstring("ca", "XYZ");
  EXPECT_STREQ(s.get(), "abXYZbXYZbc");

  s.Assign("abcabcabc");
  s.ReplaceSubstring("ca", "XY");
  EXPECT_STREQ(s.get(), "abXYbXYbc");

  s.Assign("abcabcabc");
  s.ReplaceSubstring("ca", "XYZ!");
  EXPECT_STREQ(s.get(), "abXYZ!bXYZ!bc");

  s.Assign("abcdabcdabcd");
  s.ReplaceSubstring("bcd", "X");
  EXPECT_STREQ(s.get(), "aXaXaX");

  s.Assign("abcdabcdabcd");
  s.ReplaceSubstring("bcd", "XYZ!");
  EXPECT_STREQ(s.get(), "aXYZ!aXYZ!aXYZ!");

  s.Assign("abcdabcdabcd");
  s.ReplaceSubstring("bcd", "XY");
  EXPECT_STREQ(s.get(), "aXYaXYaXY");

  s.Assign("abcdabcdabcd");
  s.ReplaceSubstring("bcd", "XYZABC");
  EXPECT_STREQ(s.get(), "aXYZABCaXYZABCaXYZABC");

  s.Assign("abcdabcdabcd");
  s.ReplaceSubstring("bcd", "XYZ");
  EXPECT_STREQ(s.get(), "aXYZaXYZaXYZ");

  s.Assign("abcdabcdabcd");
  s.ReplaceSubstring("bcd", "XYZ!");
  EXPECT_STREQ(s.get(), "aXYZ!aXYZ!aXYZ!");

  s.Assign("abcdabcdabcd");
  s.ReplaceSubstring("ab", "X");
  EXPECT_STREQ(s.get(), "XcdXcdXcd");

  s.Assign("abcdabcdabcd");
  s.ReplaceSubstring("ab", "XYZABC");
  EXPECT_STREQ(s.get(), "XYZABCcdXYZABCcdXYZABCcd");

  s.Assign("abcdabcdabcd");
  s.ReplaceSubstring("ab", "XY");
  EXPECT_STREQ(s.get(), "XYcdXYcdXYcd");

  s.Assign("abcdabcdabcd");
  s.ReplaceSubstring("ab", "XYZ!");
  EXPECT_STREQ(s.get(), "XYZ!cdXYZ!cdXYZ!cd");

  s.Assign("abcdabcdabcd");
  s.ReplaceSubstring("notfound", "X");
  EXPECT_STREQ(s.get(), "abcdabcdabcd");

  s.Assign("abcdabcdabcd");
  s.ReplaceSubstring("notfound", "longlongstring");
  EXPECT_STREQ(s.get(), "abcdabcdabcd");
}

TEST(Strings, strip_ws)
{
  const char text[] = " a    $   ";
  nsCString s(text);
  s.StripWhitespace();
  EXPECT_STREQ(s.get(), "a$");
}

TEST(Strings, equals_ic)
{
  nsCString s;
  EXPECT_FALSE(s.LowerCaseEqualsLiteral("view-source"));
}

TEST(Strings, fixed_string)
{
  char buf[256] = "hello world";

  nsFixedCString s(buf, sizeof(buf));

  EXPECT_EQ(s.Length(), strlen(buf));

  EXPECT_STREQ(s.get(), buf);

  s.Assign("foopy doopy doo");
  EXPECT_EQ(s.get(), buf);
}

TEST(Strings, concat)
{
  nsCString bar("bar");
  const nsACString& barRef = bar;

  const nsPromiseFlatCString& result =
      PromiseFlatCString(NS_LITERAL_CSTRING("foo") +
                         NS_LITERAL_CSTRING(",") +
                         barRef);
  EXPECT_STREQ(result.get(), "foo,bar");
}

TEST(Strings, concat_2)
{
  nsCString fieldTextStr("xyz");
  nsCString text("text");
  const nsACString& aText = text;

  nsAutoCString result( fieldTextStr + aText );

  EXPECT_STREQ(result.get(), "xyztext");
}

TEST(Strings, concat_3)
{
  nsCString result;
  nsCString ab("ab"), c("c");

  result = ab + result + c;
  EXPECT_STREQ(result.get(), "abc");
}

TEST(Strings, xpidl_string)
{
  nsXPIDLCString a, b;
  a = b;
  EXPECT_TRUE(a == b);

  a.Adopt(0);
  EXPECT_TRUE(a == b);

  a.Append("foopy");
  a.Assign(b);
  EXPECT_TRUE(a == b);

  a.Insert("", 0);
  a.Assign(b);
  EXPECT_TRUE(a == b);

  const char text[] = "hello world";
  *getter_Copies(a) = NS_strdup(text);
  EXPECT_STREQ(a, text);

  b = a;
  EXPECT_STREQ(a, b);

  a.Adopt(0);
  nsACString::const_iterator begin, end;
  a.BeginReading(begin);
  a.EndReading(end);
  char *r = ToNewCString(Substring(begin, end));
  EXPECT_STREQ(r, "");
  free(r);

  a.Adopt(0);
  EXPECT_TRUE(a.IsVoid());

  int32_t index = a.FindCharInSet("xyz");
  EXPECT_EQ(index, kNotFound);
}

TEST(Strings, empty_assign)
{
  nsCString a;
  a.AssignLiteral("");

  a.AppendLiteral("");

  nsCString b;
  b.SetCapacity(0);
}

TEST(Strings, set_length)
{
  const char kText[] = "Default Plugin";
  nsCString buf;
  buf.SetCapacity(sizeof(kText)-1);
  buf.Assign(kText);
  buf.SetLength(sizeof(kText)-1);
  EXPECT_STREQ(buf.get(), kText);
}

TEST(Strings, substring)
{
  nsCString super("hello world"), sub("hello");

  // this tests that |super| starts with |sub|,
  
  EXPECT_TRUE(sub.Equals(StringHead(super, sub.Length())));

  // and verifies that |sub| does not start with |super|.

  EXPECT_FALSE(super.Equals(StringHead(sub, super.Length())));
}

#define test_append_expect(str, int, suffix, expect) \
  str.Truncate(); \
  str.AppendInt(suffix = int); \
  EXPECT_TRUE(str.EqualsLiteral(expect));

#define test_appends_expect(int, suffix, expect) \
  test_append_expect(str, int, suffix, expect) \
  test_append_expect(cstr, int, suffix, expect)

#define test_appendbase(str, prefix, int, suffix, base) \
  str.Truncate(); \
  str.AppendInt(suffix = prefix ## int ## suffix, base); \
  EXPECT_TRUE(str.EqualsLiteral(#int));

#define test_appendbases(prefix, int, suffix, base) \
  test_appendbase(str, prefix, int, suffix, base) \
  test_appendbase(cstr, prefix, int, suffix, base)

TEST(Strings, appendint)
{
  nsString str;
  nsCString cstr;
  int32_t L;
  uint32_t UL;
  int64_t LL;
  uint64_t ULL;
  test_appends_expect(INT32_MAX, L, "2147483647")
  test_appends_expect(INT32_MIN, L, "-2147483648")
  test_appends_expect(UINT32_MAX, UL, "4294967295")
  test_appends_expect(INT64_MAX, LL, "9223372036854775807")
  test_appends_expect(INT64_MIN, LL, "-9223372036854775808")
  test_appends_expect(UINT64_MAX, ULL, "18446744073709551615")
  test_appendbases(0, 17777777777, L, 8)
  test_appendbases(0, 20000000000, L, 8)
  test_appendbases(0, 37777777777, UL, 8)
  test_appendbases(0, 777777777777777777777, LL, 8)
  test_appendbases(0, 1000000000000000000000, LL, 8)
  test_appendbases(0, 1777777777777777777777, ULL, 8)
  test_appendbases(0x, 7fffffff, L, 16)
  test_appendbases(0x, 80000000, L, 16)
  test_appendbases(0x, ffffffff, UL, 16)
  test_appendbases(0x, 7fffffffffffffff, LL, 16)
  test_appendbases(0x, 8000000000000000, LL, 16)
  test_appendbases(0x, ffffffffffffffff, ULL, 16)
}

TEST(Strings, appendint64)
{
  nsCString str;

  int64_t max = INT64_MAX;
  static const char max_expected[] = "9223372036854775807";
  int64_t min = INT64_MIN;
  static const char min_expected[] = "-9223372036854775808";
  static const char min_expected_oct[] = "1000000000000000000000";
  int64_t maxint_plus1 = 1LL << 32;
  static const char maxint_plus1_expected[] = "4294967296";
  static const char maxint_plus1_expected_x[] = "100000000";

  str.AppendInt(max);

  EXPECT_TRUE(str.Equals(max_expected));

  str.Truncate();
  str.AppendInt(min);
  EXPECT_TRUE(str.Equals(min_expected));
  str.Truncate();
  str.AppendInt(min, 8);
  EXPECT_TRUE(str.Equals(min_expected_oct));


  str.Truncate();
  str.AppendInt(maxint_plus1);
  EXPECT_TRUE(str.Equals(maxint_plus1_expected));
  str.Truncate();
  str.AppendInt(maxint_plus1, 16);
  EXPECT_TRUE(str.Equals(maxint_plus1_expected_x));
}

TEST(Strings, appendfloat)
{
  nsCString str;
  double bigdouble = 11223344556.66;
  static const char double_expected[] = "11223344556.66";
  static const char float_expected[] = "0.01";

  // AppendFloat is used to append doubles, therefore the precision must be
  // large enough (see bug 327719)
  str.AppendFloat( bigdouble );
  EXPECT_TRUE(str.Equals(double_expected));

  str.Truncate();
  // AppendFloat is used to append floats (bug 327719 #27)
  str.AppendFloat( 0.1f * 0.1f );
  EXPECT_TRUE(str.Equals(float_expected));
}

TEST(Strings, findcharinset)
{
  nsCString buf("hello, how are you?");

  int32_t index = buf.FindCharInSet(",?", 5);
  EXPECT_EQ(index, 5);

  index = buf.FindCharInSet("helo", 0);
  EXPECT_EQ(index, 0);

  index = buf.FindCharInSet("z?", 6);
  EXPECT_EQ(index, (int32_t) buf.Length() - 1);
}

TEST(Strings, rfindcharinset)
{
  nsCString buf("hello, how are you?");

  int32_t index = buf.RFindCharInSet(",?", 5);
  EXPECT_EQ(index, 5);

  index = buf.RFindCharInSet("helo", 0);
  EXPECT_EQ(index, 0);

  index = buf.RFindCharInSet("z?", 6);
  EXPECT_EQ(index, kNotFound);

  index = buf.RFindCharInSet("l", 5);
  EXPECT_EQ(index, 3);

  buf.Assign("abcdefghijkabc");

  index = buf.RFindCharInSet("ab");
  EXPECT_EQ(index, 12);

  index = buf.RFindCharInSet("ab", 11);
  EXPECT_EQ(index, 11);

  index = buf.RFindCharInSet("ab", 10);
  EXPECT_EQ(index, 1);

  index = buf.RFindCharInSet("ab", 0);
  EXPECT_EQ(index, 0);

  index = buf.RFindCharInSet("cd", 1);
  EXPECT_EQ(index, kNotFound);

  index = buf.RFindCharInSet("h");
  EXPECT_EQ(index, 7);
}

TEST(Strings, stringbuffer)
{
  const char kData[] = "hello world";

  nsRefPtr<nsStringBuffer> buf;

  buf = nsStringBuffer::Alloc(sizeof(kData));
  EXPECT_TRUE(!!buf);

  buf = nsStringBuffer::Alloc(sizeof(kData));
  EXPECT_TRUE(!!buf);
  char *data = (char *) buf->Data();
  memcpy(data, kData, sizeof(kData));

  nsCString str;
  buf->ToString(sizeof(kData)-1, str);

  nsStringBuffer *buf2;
  buf2 = nsStringBuffer::FromString(str);

  EXPECT_EQ(buf, buf2);
}

TEST(Strings, voided)
{
  const char kData[] = "hello world";

  nsXPIDLCString str;
  EXPECT_FALSE(str);
  EXPECT_TRUE(str.IsVoid());
  EXPECT_TRUE(str.IsEmpty());

  str.Assign(kData);
  EXPECT_STREQ(str, kData);

  str.SetIsVoid(true);
  EXPECT_FALSE(str);
  EXPECT_TRUE(str.IsVoid());
  EXPECT_TRUE(str.IsEmpty());

  str.SetIsVoid(false);
  EXPECT_STREQ(str, "");
}

TEST(Strings, voided_autostr)
{
  const char kData[] = "hello world";

  nsAutoCString str;
  EXPECT_FALSE(str.IsVoid());
  EXPECT_TRUE(str.IsEmpty());

  str.Assign(kData);
  EXPECT_STREQ(str.get(), kData);

  str.SetIsVoid(true);
  EXPECT_TRUE(str.IsVoid());
  EXPECT_TRUE(str.IsEmpty());

  str.Assign(kData);
  EXPECT_FALSE(str.IsVoid());
  EXPECT_FALSE(str.IsEmpty());
  EXPECT_STREQ(str.get(), kData);
}

TEST(Strings, voided_assignment)
{
  nsCString a, b;
  b.SetIsVoid(true);
  a = b;
  EXPECT_TRUE(a.IsVoid());
  EXPECT_EQ(a.get(), b.get());
}

TEST(Strings, empty_assignment)
{
  nsCString a, b;
  a = b;
  EXPECT_EQ(a.get(), b.get());
}

struct ToIntegerTest
{
  const char *str;
  uint32_t radix;
  int32_t result;
  nsresult rv;
};

static const ToIntegerTest kToIntegerTests[] = {
  { "123", 10, 123, NS_OK },
  { "7b", 16, 123, NS_OK },
  { "90194313659", 10, 0, NS_ERROR_ILLEGAL_VALUE },
  { nullptr, 0, 0, NS_OK }
};

TEST(Strings, string_tointeger)
{
  nsresult rv;
  for (const ToIntegerTest* t = kToIntegerTests; t->str; ++t) {
    int32_t result = nsAutoCString(t->str).ToInteger(&rv, t->radix);
    EXPECT_EQ(rv, t->rv);
    EXPECT_EQ(result, t->result);
    result = nsAutoCString(t->str).ToInteger(&rv, t->radix);
    EXPECT_EQ(rv, t->rv);
    EXPECT_EQ(result, t->result);
  }
}

static void test_parse_string_helper(const char* str, char separator, int len,
                                       const char* s1, const char* s2)
{
  nsCString data(str);
  nsTArray<nsCString> results;
  EXPECT_TRUE(ParseString(data, separator, results));
  EXPECT_EQ(int(results.Length()), len);
  const char* strings[] = { s1, s2 };
  for (int i = 0; i < len; ++i) {
    EXPECT_TRUE(results[i].Equals(strings[i]));
  }
}

static void test_parse_string_helper0(const char* str, char separator)
{
  test_parse_string_helper(str, separator, 0, nullptr, nullptr);
}

static void test_parse_string_helper1(const char* str, char separator, const char* s1)
{
  test_parse_string_helper(str, separator, 1, s1, nullptr);
}

static void test_parse_string_helper2(const char* str, char separator, const char* s1, const char* s2)
{
  test_parse_string_helper(str, separator, 2, s1, s2);
}

TEST(String, parse_string)
{
  test_parse_string_helper1("foo, bar", '_', "foo, bar");
  test_parse_string_helper2("foo, bar", ',', "foo", " bar");
  test_parse_string_helper2("foo, bar ", ' ', "foo,", "bar");
  test_parse_string_helper2("foo,bar", 'o', "f", ",bar");
  test_parse_string_helper0("", '_');
  test_parse_string_helper0("  ", ' ');
  test_parse_string_helper1(" foo", ' ', "foo");
  test_parse_string_helper1("  foo", ' ', "foo");
}

static void test_strip_chars_helper(const char16_t* str, const char16_t* strip, const nsAString& result, uint32_t offset=0)
{
  nsAutoString tmp(str);
  nsAString& data = tmp;
  data.StripChars(strip, offset);
  EXPECT_TRUE(data.Equals(result));
}

TEST(String, strip_chars)
{
  test_strip_chars_helper(MOZ_UTF16("foo \r \nbar"),
                          MOZ_UTF16(" \n\r"),
                          NS_LITERAL_STRING("foobar"));
  test_strip_chars_helper(MOZ_UTF16("\r\nfoo\r\n"),
                          MOZ_UTF16(" \n\r"),
                          NS_LITERAL_STRING("foo"));
  test_strip_chars_helper(MOZ_UTF16("foo"),
                          MOZ_UTF16(" \n\r"),
                          NS_LITERAL_STRING("foo"));
  test_strip_chars_helper(MOZ_UTF16("foo"),
                          MOZ_UTF16("fo"),
                          NS_LITERAL_STRING(""));
  test_strip_chars_helper(MOZ_UTF16("foo"),
                          MOZ_UTF16("foo"),
                          NS_LITERAL_STRING(""));
  test_strip_chars_helper(MOZ_UTF16(" foo"),
                          MOZ_UTF16(" "),
                          NS_LITERAL_STRING(" foo"), 1);
}

TEST(Strings, huge_capacity)
{
  nsString a, b, c, d, e, f, g, h, i, j, k, l, m, n;
  nsCString n1;

  // Ignore the result if the address space is less than 64-bit because
  // some of the allocations above will exhaust the address space.
  if (sizeof(void*) >= 8) {
    EXPECT_TRUE(a.SetCapacity(1, fallible));
    EXPECT_FALSE(a.SetCapacity(nsString::size_type(-1)/2, fallible));
    EXPECT_TRUE(a.SetCapacity(0, fallible));  // free the allocated memory

    EXPECT_TRUE(b.SetCapacity(1, fallible));
    EXPECT_FALSE(b.SetCapacity(nsString::size_type(-1)/2 - 1, fallible));
    EXPECT_TRUE(b.SetCapacity(0, fallible));

    EXPECT_TRUE(c.SetCapacity(1, fallible));
    EXPECT_FALSE(c.SetCapacity(nsString::size_type(-1)/2, fallible));
    EXPECT_TRUE(c.SetCapacity(0, fallible));

    EXPECT_FALSE(d.SetCapacity(nsString::size_type(-1)/2 - 1, fallible));
    EXPECT_FALSE(d.SetCapacity(nsString::size_type(-1)/2, fallible));
    EXPECT_TRUE(d.SetCapacity(0, fallible));

    EXPECT_FALSE(e.SetCapacity(nsString::size_type(-1)/4, fallible));
    EXPECT_FALSE(e.SetCapacity(nsString::size_type(-1)/4 + 1, fallible));
    EXPECT_TRUE(e.SetCapacity(0, fallible));

    EXPECT_FALSE(f.SetCapacity(nsString::size_type(-1)/2, fallible));
    EXPECT_TRUE(f.SetCapacity(0, fallible));

    EXPECT_FALSE(g.SetCapacity(nsString::size_type(-1)/4 + 1000, fallible));
    EXPECT_FALSE(g.SetCapacity(nsString::size_type(-1)/4 + 1001, fallible));
    EXPECT_TRUE(g.SetCapacity(0, fallible));

    EXPECT_FALSE(h.SetCapacity(nsString::size_type(-1)/4+1, fallible));
    EXPECT_FALSE(h.SetCapacity(nsString::size_type(-1)/2, fallible));
    EXPECT_TRUE(h.SetCapacity(0, fallible));

    EXPECT_TRUE(i.SetCapacity(1, fallible));
    EXPECT_TRUE(i.SetCapacity(nsString::size_type(-1)/4 - 1000, fallible));
    EXPECT_FALSE(i.SetCapacity(nsString::size_type(-1)/4 + 1, fallible));
    EXPECT_TRUE(i.SetCapacity(0, fallible));

    EXPECT_TRUE(j.SetCapacity(nsString::size_type(-1)/4 - 1000, fallible));
    EXPECT_FALSE(j.SetCapacity(nsString::size_type(-1)/4 + 1, fallible));
    EXPECT_TRUE(j.SetCapacity(0, fallible));

    EXPECT_TRUE(k.SetCapacity(nsString::size_type(-1)/8 - 1000, fallible));
    EXPECT_TRUE(k.SetCapacity(nsString::size_type(-1)/4 - 1001, fallible));
    EXPECT_TRUE(k.SetCapacity(nsString::size_type(-1)/4 - 998, fallible));
    EXPECT_FALSE(k.SetCapacity(nsString::size_type(-1)/4 + 1, fallible));
    EXPECT_TRUE(k.SetCapacity(0, fallible));

    EXPECT_TRUE(l.SetCapacity(nsString::size_type(-1)/8, fallible));
    EXPECT_TRUE(l.SetCapacity(nsString::size_type(-1)/8 + 1, fallible));
    EXPECT_TRUE(l.SetCapacity(nsString::size_type(-1)/8 + 2, fallible));
    EXPECT_TRUE(l.SetCapacity(0, fallible));

    EXPECT_TRUE(m.SetCapacity(nsString::size_type(-1)/8 + 1000, fallible));
    EXPECT_TRUE(m.SetCapacity(nsString::size_type(-1)/8 + 1001, fallible));
    EXPECT_TRUE(m.SetCapacity(0, fallible));

    EXPECT_TRUE(n.SetCapacity(nsString::size_type(-1)/8+1, fallible));
    EXPECT_FALSE(n.SetCapacity(nsString::size_type(-1)/4, fallible));
    EXPECT_TRUE(n.SetCapacity(0, fallible));

    EXPECT_TRUE(n.SetCapacity(0, fallible));
    EXPECT_TRUE(n.SetCapacity((nsString::size_type(-1)/2 - sizeof(nsStringBuffer)) / 2 - 2, fallible));
    EXPECT_TRUE(n.SetCapacity(0, fallible));
    EXPECT_FALSE(n.SetCapacity((nsString::size_type(-1)/2 - sizeof(nsStringBuffer)) / 2 - 1, fallible));
    EXPECT_TRUE(n.SetCapacity(0, fallible));
    EXPECT_TRUE(n1.SetCapacity(0, fallible));
    EXPECT_TRUE(n1.SetCapacity((nsCString::size_type(-1)/2 - sizeof(nsStringBuffer)) / 1 - 2, fallible));
    EXPECT_TRUE(n1.SetCapacity(0, fallible));
    EXPECT_FALSE(n1.SetCapacity((nsCString::size_type(-1)/2 - sizeof(nsStringBuffer)) / 1 - 1, fallible));
    EXPECT_TRUE(n1.SetCapacity(0, fallible));
  }
}

static void test_tofloat_helper(const nsString& aStr, float aExpected, bool aSuccess)
{
  nsresult result;
  EXPECT_EQ(aStr.ToFloat(&result), aExpected);
  if (aSuccess) {
    EXPECT_EQ(result, NS_OK);
  } else {
    EXPECT_NE(result, NS_OK);
  }
}

TEST(Strings, tofloat)
{
  test_tofloat_helper(NS_LITERAL_STRING("42"), 42.f, true);
  test_tofloat_helper(NS_LITERAL_STRING("42.0"), 42.f, true);
  test_tofloat_helper(NS_LITERAL_STRING("-42"), -42.f, true);
  test_tofloat_helper(NS_LITERAL_STRING("+42"), 42, true);
  test_tofloat_helper(NS_LITERAL_STRING("13.37"), 13.37f, true);
  test_tofloat_helper(NS_LITERAL_STRING("1.23456789"), 1.23456789f, true);
  test_tofloat_helper(NS_LITERAL_STRING("1.98765432123456"), 1.98765432123456f, true);
  test_tofloat_helper(NS_LITERAL_STRING("0"), 0.f, true);
  test_tofloat_helper(NS_LITERAL_STRING("1.e5"), 100000, true);
  test_tofloat_helper(NS_LITERAL_STRING(""), 0.f, false);
  test_tofloat_helper(NS_LITERAL_STRING("42foo"), 42.f, false);
  test_tofloat_helper(NS_LITERAL_STRING("foo"), 0.f, false);
}

static void test_todouble_helper(const nsString& aStr, double aExpected, bool aSuccess)
{
  nsresult result;
  EXPECT_EQ(aStr.ToDouble(&result), aExpected);
  if (aSuccess) {
    EXPECT_EQ(result, NS_OK);
  } else {
    EXPECT_NE(result, NS_OK);
  }
}

TEST(Strings, todouble)
{
  test_todouble_helper(NS_LITERAL_STRING("42"), 42, true);
  test_todouble_helper(NS_LITERAL_STRING("42.0"), 42, true);
  test_todouble_helper(NS_LITERAL_STRING("-42"), -42, true);
  test_todouble_helper(NS_LITERAL_STRING("+42"), 42, true);
  test_todouble_helper(NS_LITERAL_STRING("13.37"), 13.37, true);
  test_todouble_helper(NS_LITERAL_STRING("1.23456789"), 1.23456789, true);
  test_todouble_helper(NS_LITERAL_STRING("1.98765432123456"), 1.98765432123456, true);
  test_todouble_helper(NS_LITERAL_STRING("123456789.98765432123456"), 123456789.98765432123456, true);
  test_todouble_helper(NS_LITERAL_STRING("0"), 0, true);
  test_todouble_helper(NS_LITERAL_STRING("1.e5"), 100000, true);
  test_todouble_helper(NS_LITERAL_STRING(""), 0, false);
  test_todouble_helper(NS_LITERAL_STRING("42foo"), 42, false);
  test_todouble_helper(NS_LITERAL_STRING("foo"), 0, false);
}

}
