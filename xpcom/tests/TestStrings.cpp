/* vim:set ts=2 sw=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <stdlib.h>
#include "nsString.h"
#include "nsStringBuffer.h"
#include "nsReadableUtils.h"
#include "nsCRTGlue.h"

namespace TestStrings {

void test_assign_helper(const nsACString& in, nsACString &_retval)
  {
    _retval = in;
  }

bool test_assign()
  {
    nsCString result;
    test_assign_helper(NS_LITERAL_CSTRING("a") + NS_LITERAL_CSTRING("b"), result);
    bool r = strcmp(result.get(), "ab") == 0;
    if (!r)
      printf("[result=%s]\n", result.get());
    return r;
  }

bool test_assign_c()
  {
    nsCString c; c.Assign('c');
    bool r = strcmp(c.get(), "c") == 0;
    if (!r)
      printf("[result=%s]\n", c.get());
    return r;
  }

bool test1()
  {
    NS_NAMED_LITERAL_STRING(empty, "");
    const nsAString& aStr = empty;

    nsAutoString buf(aStr);

    int32_t n = buf.FindChar(',');

    n = buf.Length();

    buf.Cut(0, n + 1);
    n = buf.FindChar(',');

    if (n != kNotFound)
      printf("n=%d\n", n);

    return n == kNotFound;
  }

bool test2()
  {
    nsCString data("hello world");
    const nsACString& aStr = data;

    nsCString temp(aStr);
    temp.Cut(0, 6);

    bool r = strcmp(temp.get(), "world") == 0;
    if (!r)
      printf("[temp=%s]\n", temp.get());
    return r;
  }

bool test_find()
  {
    nsCString src("<!DOCTYPE blah blah blah>");

    int32_t i = src.Find("DOCTYPE", true, 2, 1);
    if (i == 2)
      return true;

    printf("i=%d\n", i);
    return false;
  }

bool test_rfind()
  {
    const char text[] = "<!DOCTYPE blah blah blah>";
    const char term[] = "bLaH";
    nsCString src(text);
    int32_t i;

    i = src.RFind(term, true, 3, -1); 
    if (i != kNotFound)
      {
        printf("unexpected result searching from offset=3, i=%d\n", i);
        return false;
      }

    i = src.RFind(term, true, -1, -1);
    if (i != 20)
      {
        printf("unexpected result searching from offset=-1, i=%d\n", i);
        return false;
      }

    i = src.RFind(term, true, 13, -1);
    if (i != 10)
      {
        printf("unexpected result searching from offset=13, i=%d\n", i);
        return false;
      }

    i = src.RFind(term, true, 22, 3);
    if (i != 20)
      {
        printf("unexpected result searching from offset=22, i=%d\n", i);
        return false;
      }

    return true;
  }

bool test_rfind_2()
  {
    const char text[] = "<!DOCTYPE blah blah blah>";
    nsCString src(text);
    int32_t i = src.RFind("TYPE", false, 5, -1); 
    if (i == 5)
      return true;

    printf("i=%d\n", i);
    return false;
  }

bool test_rfind_3()
  {
    const char text[] = "urn:mozilla:locale:en-US:necko";
    nsAutoCString value(text);
    int32_t i = value.RFind(":");
    if (i == 24)
      return true;

    printf("i=%d\n", i);
    return false;
  }

bool test_rfind_4()
  {
    nsCString value("a.msf");
    int32_t i = value.RFind(".msf");
    if (i != 1)
      {
        printf("i=%d\n", i);
        return false;
      }

    return true;
  }

bool test_findinreadable()
  {
    const char text[] = "jar:jar:file:///c:/software/mozilla/mozilla_2006_02_21.jar!/browser/chrome/classic.jar!/";
    nsAutoCString value(text);

    nsACString::const_iterator begin, end;
    value.BeginReading(begin);
    value.EndReading(end);
    nsACString::const_iterator delim_begin (begin),
                               delim_end   (end);

    // Search for last !/ at the end of the string
    if (!FindInReadable(NS_LITERAL_CSTRING("!/"), delim_begin, delim_end))
        return false;
    char *r = ToNewCString(Substring(delim_begin, delim_end));
    // Should match the first "!/" but not the last
    if ((delim_end == end) || (strcmp(r, "!/")!=0))
      {
        printf("r = %s\n", r);
        nsMemory::Free(r);
        return false;
      }
    nsMemory::Free(r);

    delim_begin = begin;
    delim_end = end;

    // Search for first jar:
    if (!FindInReadable(NS_LITERAL_CSTRING("jar:"), delim_begin, delim_end))
        return false;

    r = ToNewCString(Substring(delim_begin, delim_end));
    // Should not match the first jar:, but the second one
    if ((delim_begin != begin) || (strcmp(r, "jar:")!=0))
      {
        printf("r = %s\n", r);
        nsMemory::Free(r);
        return false;
      }
    nsMemory::Free(r);

    // Search for jar: in a Substring
    delim_begin = begin; delim_begin++;
    delim_end = end;
    if (!FindInReadable(NS_LITERAL_CSTRING("jar:"), delim_begin, delim_end))
        return false;

    r = ToNewCString(Substring(delim_begin, delim_end));
    // Should not match the first jar:, but the second one
    if ((delim_begin == begin) || (strcmp(r, "jar:")!=0))
      {
        printf("r = %s\n", r);
        nsMemory::Free(r);
        return false;
      }
    nsMemory::Free(r);

    // Should not find a match
    if (FindInReadable(NS_LITERAL_CSTRING("gecko"), delim_begin, delim_end))
        return false;

    // When no match is found, range should be empty
    if (delim_begin != delim_end) 
        return false;

    // Should not find a match (search not beyond Substring)
    delim_begin = begin; for (int i=0;i<6;i++) delim_begin++;
    delim_end = end;
    if (FindInReadable(NS_LITERAL_CSTRING("jar:"), delim_begin, delim_end))
        return false;

    // When no match is found, range should be empty
    if (delim_begin != delim_end) 
        return false;

    // Should not find a match (search not beyond Substring)
    delim_begin = begin;
    delim_end = end; for (int i=0;i<7;i++) delim_end--;
    if (FindInReadable(NS_LITERAL_CSTRING("classic"), delim_begin, delim_end))
        return false;

    // When no match is found, range should be empty
    if (delim_begin != delim_end) 
        return false;

    return true;
  }

bool test_rfindinreadable()
  {
    const char text[] = "jar:jar:file:///c:/software/mozilla/mozilla_2006_02_21.jar!/browser/chrome/classic.jar!/";
    nsAutoCString value(text);

    nsACString::const_iterator begin, end;
    value.BeginReading(begin);
    value.EndReading(end);
    nsACString::const_iterator delim_begin (begin),
                               delim_end   (end);

    // Search for last !/ at the end of the string
    if (!RFindInReadable(NS_LITERAL_CSTRING("!/"), delim_begin, delim_end))
        return false;
    char *r = ToNewCString(Substring(delim_begin, delim_end));
    // Should match the last "!/"
    if ((delim_end != end) || (strcmp(r, "!/")!=0))
      {
        printf("r = %s\n", r);
        nsMemory::Free(r);
        return false;
      }
    nsMemory::Free(r);

    delim_begin = begin;
    delim_end = end;

    // Search for last jar: but not the first one...
    if (!RFindInReadable(NS_LITERAL_CSTRING("jar:"), delim_begin, delim_end))
        return false;

    r = ToNewCString(Substring(delim_begin, delim_end));
    // Should not match the first jar:, but the second one
    if ((delim_begin == begin) || (strcmp(r, "jar:")!=0))
      {
        printf("r = %s\n", r);
        nsMemory::Free(r);
        return false;
      }
    nsMemory::Free(r);

    // Search for jar: in a Substring
    delim_begin = begin;
    delim_end = begin; for (int i=0;i<6;i++) delim_end++;
    if (!RFindInReadable(NS_LITERAL_CSTRING("jar:"), delim_begin, delim_end)) {
        printf("Search for jar: in a Substring\n");
        return false;
    }

    r = ToNewCString(Substring(delim_begin, delim_end));
    // Should not match the first jar:, but the second one
    if ((delim_begin != begin) || (strcmp(r, "jar:")!=0))
      {
        printf("r = %s\n", r);
        nsMemory::Free(r);
        return false;
      }
    nsMemory::Free(r);

    // Should not find a match
    delim_begin = begin;
    delim_end = end;
    if (RFindInReadable(NS_LITERAL_CSTRING("gecko"), delim_begin, delim_end)) {
        printf("Should not find a match\n");
        return false;
    }

    // When no match is found, range should be empty
    if (delim_begin != delim_end) {
        printf("1: When no match is found, range should be empty\n");
        return false;
    }

    // Should not find a match (search not before Substring)
    delim_begin = begin; for (int i=0;i<6;i++) delim_begin++;
    delim_end = end;
    if (RFindInReadable(NS_LITERAL_CSTRING("jar:"), delim_begin, delim_end)) {
        printf("Should not find a match (search not before Substring)\n");
        return false;
    }

    // When no match is found, range should be empty
    if (delim_begin != delim_end) {
        printf("2: When no match is found, range should be empty\n");
        return false;
    }

    // Should not find a match (search not beyond Substring)
    delim_begin = begin;
    delim_end = end; for (int i=0;i<7;i++) delim_end--;
    if (RFindInReadable(NS_LITERAL_CSTRING("classic"), delim_begin, delim_end)) {
        printf("Should not find a match (search not beyond Substring)\n");
        return false;
    }

    // When no match is found, range should be empty
    if (delim_begin != delim_end) {
        printf("3: When no match is found, range should be empty\n");
        return false;
    }

    return true;
  }

bool test_distance()
  {
    const char text[] = "abc-xyz";
    nsCString s(text);
    nsCString::const_iterator begin, end;
    s.BeginReading(begin);
    s.EndReading(end);
    size_t d = Distance(begin, end);
    bool r = (d == sizeof(text)-1);
    if (!r)
      printf("d=%u\n", d);
    return r;
  }

bool test_length()
  {
    const char text[] = "abc-xyz";
    nsCString s(text);
    size_t d = s.Length();
    bool r = (d == sizeof(text)-1);
    if (!r)
      printf("d=%u\n", d);
    return r;
  }

bool test_trim()
  {
    const char text[] = " a\t    $   ";
    const char set[] = " \t$";

    nsCString s(text);
    s.Trim(set);
    bool r = strcmp(s.get(), "a") == 0;
    if (!r)
      printf("[s=%s]\n", s.get());
    return r;
  }

bool test_replace_substr()
  {
    const char text[] = "abc-ppp-qqq-ppp-xyz";
    nsCString s(text);
    s.ReplaceSubstring("ppp", "www");
    bool r = strcmp(s.get(), "abc-www-qqq-www-xyz") == 0;
    if (!r)
      {
        printf("[s=%s]\n", s.get());
        return false;
      }

    s.Assign("foobar");
    s.ReplaceSubstring("foo", "bar");
    s.ReplaceSubstring("bar", "");
    r = strcmp(s.get(), "") == 0;
    if (!r)
      {
        printf("[s=%s]\n", s.get());
        return false;
      }

    s.Assign("foofoofoo");
    s.ReplaceSubstring("foo", "foo");
    r = strcmp(s.get(), "foofoofoo") == 0;
    if (!r)
      {
        printf("[s=%s]\n", s.get());
        return false;
      }

    s.Assign("foofoofoo");
    s.ReplaceSubstring("of", "fo");
    r = strcmp(s.get(), "fofoofooo") == 0;
    if (!r)
      {
        printf("[s=%s]\n", s.get());
        return false;
      }

    return true;
  }

bool test_replace_substr_2()
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
    if (!newAcctName.Equals(acctName))
      return false;

    return true;
  }

bool test_strip_ws()
  {
    const char text[] = " a    $   ";
    nsCString s(text);
    s.StripWhitespace();
    bool r = strcmp(s.get(), "a$") == 0;
    if (!r)
      printf("[s=%s]\n", s.get());
    return r;
  }

bool test_equals_ic()
  {
    nsCString s;
    bool r = s.LowerCaseEqualsLiteral("view-source");
    if (r)
      printf("[r=%d]\n", r);
    return !r;
  }

bool test_fixed_string()
  {
    char buf[256] = "hello world";

    nsFixedCString s(buf, sizeof(buf));

    if (s.Length() != strlen(buf))
      return false;

    if (strcmp(s.get(), buf) != 0)
      return false;

    s.Assign("foopy doopy doo");
    if (s.get() != buf)
      return false;
    
    return true;
  }

bool test_concat()
  {
    nsCString bar("bar");
    const nsACString& barRef = bar;

    const nsPromiseFlatCString& result =
        PromiseFlatCString(NS_LITERAL_CSTRING("foo") +
                           NS_LITERAL_CSTRING(",") +
                           barRef);
    if (strcmp(result.get(), "foo,bar") == 0)
      return true;

    printf("[result=%s]\n", result.get());
    return false;
  }

bool test_concat_2()
  {
    nsCString fieldTextStr("xyz");
    nsCString text("text");
    const nsACString& aText = text;

    nsAutoCString result( fieldTextStr + aText );

    if (strcmp(result.get(), "xyztext") == 0)
      return true;
    
    printf("[result=%s]\n", result.get());
    return false;
  }

bool test_concat_3()
  {
    nsCString result;
    nsCString ab("ab"), c("c");

    result = ab + result + c;
    if (strcmp(result.get(), "abc") == 0)
      return true;

    printf("[result=%s]\n", result.get());
    return false;
  }

bool test_xpidl_string()
  {
    nsXPIDLCString a, b;
    a = b;
    if (a != b)
      return false;

    a.Adopt(0);
    if (a != b)
      return false;

    a.Append("foopy");
    a.Assign(b);
    if (a != b)
      return false;

    a.Insert("", 0);
    a.Assign(b);
    if (a != b)
      return false;

    const char text[] = "hello world";
    *getter_Copies(a) = NS_strdup(text);
    if (strcmp(a, text) != 0)
      return false;

    b = a;
    if (strcmp(a, b) != 0)
      return false;

    a.Adopt(0);
    nsACString::const_iterator begin, end;
    a.BeginReading(begin);
    a.EndReading(end);
    char *r = ToNewCString(Substring(begin, end));
    if (strcmp(r, "") != 0)
      return false;
    nsMemory::Free(r);

    a.Adopt(0);
    if (a != (const char*) 0)
      return false;

    /*
    int32_t index = a.FindCharInSet("xyz");
    if (index != kNotFound)
      return false;
    */

    return true;
  }

bool test_empty_assign()
  {
    nsCString a;
    a.AssignLiteral("");

    a.AppendLiteral("");

    nsCString b;
    b.SetCapacity(0);
    return true;
  }

bool test_set_length()
  {
    const char kText[] = "Default Plugin";
    nsCString buf;
    buf.SetCapacity(sizeof(kText)-1);
    buf.Assign(kText);
    buf.SetLength(sizeof(kText)-1);
    if (strcmp(buf.get(), kText) != 0)
      return false;
    return true;
  }

bool test_substring()
  {
    nsCString super("hello world"), sub("hello");

    // this tests that |super| starts with |sub|,
    
    bool r = sub.Equals(StringHead(super, sub.Length()));
    if (!r)
      return false;

    // and verifies that |sub| does not start with |super|.

    r = super.Equals(StringHead(sub, super.Length()));
    if (r)
      return false;

    return true;
  }

#define test_append(str, int, suffix) \
  str.Truncate(); \
  str.AppendInt(suffix = int ## suffix); \
  if (!str.EqualsLiteral(#int)) { \
    fputs("Error appending " #int "\n", stderr); \
    return false; \
  }

#define test_appends(int, suffix) \
  test_append(str, int, suffix) \
  test_append(cstr, int, suffix)

#define test_appendbase(str, prefix, int, suffix, base) \
  str.Truncate(); \
  str.AppendInt(suffix = prefix ## int ## suffix, base); \
  if (!str.EqualsLiteral(#int)) { \
    fputs("Error appending " #prefix #int "\n", stderr); \
    return false; \
  }

#define test_appendbases(prefix, int, suffix, base) \
  test_appendbase(str, prefix, int, suffix, base) \
  test_appendbase(cstr, prefix, int, suffix, base)

bool test_appendint()
  {
    nsString str;
    nsCString cstr;
    int32_t L;
    uint32_t UL;
    int64_t LL;
    uint64_t ULL;
    test_appends(2147483647, L)
    test_appends(-2147483648, L)
    test_appends(4294967295, UL)
    test_appends(9223372036854775807, LL)
    test_appends(-9223372036854775808, LL)
    test_appends(18446744073709551615, ULL)
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
    return true;
  }

bool test_appendint64()
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

    if (!str.Equals(max_expected)) {
      fprintf(stderr, "Error appending INT64_MAX: Got %s\n", str.get());
      return false;
    }

    str.Truncate();
    str.AppendInt(min);
    if (!str.Equals(min_expected)) {
      fprintf(stderr, "Error appending INT64_MIN: Got %s\n", str.get());
      return false;
    }
    str.Truncate();
    str.AppendInt(min, 8);
    if (!str.Equals(min_expected_oct)) {
      fprintf(stderr, "Error appending INT64_MIN (oct): Got %s\n", str.get());
      return false;
    }


    str.Truncate();
    str.AppendInt(maxint_plus1);
    if (!str.Equals(maxint_plus1_expected)) {
      fprintf(stderr, "Error appending UINT32_MAX + 1: Got %s\n", str.get());
      return false;
    }
    str.Truncate();
    str.AppendInt(maxint_plus1, 16);
    if (!str.Equals(maxint_plus1_expected_x)) {
      fprintf(stderr, "Error appending UINT32_MAX + 1 (hex): Got %s\n", str.get());
      return false;
    }


    return true;
  }

bool test_appendfloat()
  {
    nsCString str;
    double bigdouble = 11223344556.66;
    static const char double_expected[] = "11223344556.66";
    static const char float_expected[] = "0.01";

    // AppendFloat is used to append doubles, therefore the precision must be
    // large enough (see bug 327719)
    str.AppendFloat( bigdouble );
    if (!str.Equals(double_expected)) {
      fprintf(stderr, "Error appending a big double: Got %s\n", str.get());
      return false;
    }
    
    str.Truncate();
    // AppendFloat is used to append floats (bug 327719 #27)
    str.AppendFloat( 0.1f * 0.1f );
    if (!str.Equals(float_expected)) {
      fprintf(stderr, "Error appending a float: Got %s\n", str.get());
      return false;
    }

    return true;
  }

bool test_findcharinset()
  {
    nsCString buf("hello, how are you?");

    int32_t index = buf.FindCharInSet(",?", 5);
    if (index != 5)
      return false;

    index = buf.FindCharInSet("helo", 0);
    if (index != 0)
      return false;

    index = buf.FindCharInSet("z?", 6);
    if (index != (int32_t) buf.Length()-1)
      return false;

    return true;
  }

bool test_rfindcharinset()
  {
    nsCString buf("hello, how are you?");

    int32_t index = buf.RFindCharInSet(",?", 5);
    if (index != 5)
      return false;

    index = buf.RFindCharInSet("helo", 0);
    if (index != 0)
      return false;

    index = buf.RFindCharInSet("z?", 6);
    if (index != kNotFound)
      return false;

    index = buf.RFindCharInSet("l", 5);
    if (index != 3)
      return false;

    buf.Assign("abcdefghijkabc");

    index = buf.RFindCharInSet("ab");
    if (index != 12)
      return false;

    index = buf.RFindCharInSet("ab", 11);
    if (index != 11)
      return false;

    index = buf.RFindCharInSet("ab", 10);
    if (index != 1)
      return false;

    index = buf.RFindCharInSet("ab", 0);
    if (index != 0)
      return false;

    index = buf.RFindCharInSet("cd", 1);
    if (index != kNotFound)
      return false;

    index = buf.RFindCharInSet("h");
    if (index != 7)
      return false;

    return true;
  }

bool test_stringbuffer()
  {
    const char kData[] = "hello world";

    nsStringBuffer *buf;
    
    buf = nsStringBuffer::Alloc(sizeof(kData));
    if (!buf)
      return false;
    buf->Release();
 
    buf = nsStringBuffer::Alloc(sizeof(kData));
    if (!buf)
      return false;
    char *data = (char *) buf->Data();
    memcpy(data, kData, sizeof(kData));

    nsCString str;
    buf->ToString(sizeof(kData)-1, str);

    nsStringBuffer *buf2;
    buf2 = nsStringBuffer::FromString(str);

    bool rv = (buf == buf2);

    buf->Release();
    return rv;
  }

bool test_voided()
  {
    const char kData[] = "hello world";

    nsXPIDLCString str;
    if (str)
      return false;
    if (!str.IsVoid())
      return false;
    if (!str.IsEmpty())
      return false;

    str.Assign(kData);
    if (strcmp(str, kData) != 0)
      return false;

    str.SetIsVoid(true);
    if (str)
      return false;
    if (!str.IsVoid())
      return false;
    if (!str.IsEmpty())
      return false;

    str.SetIsVoid(false);
    if (strcmp(str, "") != 0)
      return false;

    return true;
  }

bool test_voided_autostr()
  {
    const char kData[] = "hello world";

    nsAutoCString str;
    if (str.IsVoid())
      return false;
    if (!str.IsEmpty())
      return false;

    str.Assign(kData);
    if (strcmp(str.get(), kData) != 0)
      return false;

    str.SetIsVoid(true);
    if (!str.IsVoid())
      return false;
    if (!str.IsEmpty())
      return false;

    str.Assign(kData);
    if (str.IsVoid())
      return false;
    if (str.IsEmpty())
      return false;
    if (strcmp(str.get(), kData) != 0)
      return false;

    return true;
  }

bool test_voided_assignment()
  {
    nsCString a, b;
    b.SetIsVoid(true);
    a = b;
    return a.IsVoid() && a.get() == b.get();
  }

bool test_empty_assignment()
  {
    nsCString a, b;
    a = b;
    return a.get() == b.get();
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
  { nullptr, 0, 0, 0 }
};

bool test_string_tointeger()
{
  int32_t i;
  nsresult rv;
  for (const ToIntegerTest* t = kToIntegerTests; t->str; ++t) {
    int32_t result = nsAutoCString(t->str).ToInteger(&rv, t->radix);
    if (rv != t->rv || result != t->result)
      return false;
    result = nsAutoCString(t->str).ToInteger(&i, t->radix);
    if ((nsresult)i != t->rv || result != t->result)
      return false;
  }
  return true;
}

static bool test_parse_string_helper(const char* str, char separator, int len,
                                       const char* s1, const char* s2)
{
  nsCString data(str);
  nsTArray<nsCString> results;
  if (!ParseString(data, separator, results))
    return false;
  if (int(results.Length()) != len)
    return false;
  const char* strings[] = { s1, s2 };
  for (int i = 0; i < len; ++i) {
    if (!results[i].Equals(strings[i]))
      return false;
  }
  return true;
}

static bool test_parse_string_helper0(const char* str, char separator)
{
  return test_parse_string_helper(str, separator, 0, nullptr, nullptr);
}

static bool test_parse_string_helper1(const char* str, char separator, const char* s1)
{
  return test_parse_string_helper(str, separator, 1, s1, nullptr);
}

static bool test_parse_string_helper2(const char* str, char separator, const char* s1, const char* s2)
{
  return test_parse_string_helper(str, separator, 2, s1, s2);
}

static bool test_parse_string()
{
  return test_parse_string_helper1("foo, bar", '_', "foo, bar") &&
         test_parse_string_helper2("foo, bar", ',', "foo", " bar") &&
         test_parse_string_helper2("foo, bar ", ' ', "foo,", "bar") &&
         test_parse_string_helper2("foo,bar", 'o', "f", ",bar") &&
         test_parse_string_helper0("", '_') &&
         test_parse_string_helper0("  ", ' ') &&
         test_parse_string_helper1(" foo", ' ', "foo") &&
         test_parse_string_helper1("  foo", ' ', "foo");
}

static bool test_strip_chars_helper(const char16_t* str, const char16_t* strip, const nsAString& result, uint32_t offset=0)
{
  nsAutoString tmp(str);
  nsAString& data = tmp;
  data.StripChars(strip, offset);
  return data.Equals(result);
}

static bool test_strip_chars()
{
  return test_strip_chars_helper(MOZ_UTF16("foo \r \nbar"),
                                 MOZ_UTF16(" \n\r"),
                                 NS_LITERAL_STRING("foobar")) &&
         test_strip_chars_helper(MOZ_UTF16("\r\nfoo\r\n"),
                                 MOZ_UTF16(" \n\r"),
                                 NS_LITERAL_STRING("foo")) &&
         test_strip_chars_helper(MOZ_UTF16("foo"),
                                 MOZ_UTF16(" \n\r"),
                                 NS_LITERAL_STRING("foo")) &&
         test_strip_chars_helper(MOZ_UTF16("foo"),
                                 MOZ_UTF16("fo"),
                                 NS_LITERAL_STRING("")) &&
         test_strip_chars_helper(MOZ_UTF16("foo"),
                                 MOZ_UTF16("foo"),
                                 NS_LITERAL_STRING("")) &&
         test_strip_chars_helper(MOZ_UTF16(" foo"),
                                 MOZ_UTF16(" "),
                                 NS_LITERAL_STRING(" foo"), 1);
}

static bool test_huge_capacity()
{
  nsString a, b, c, d, e, f, g, h, i, j, k, l, m, n;
  nsCString n1;
  bool fail = false;
#undef ok
#define ok(x) { fail |= !(x); }

  ok(a.SetCapacity(1));
  ok(!a.SetCapacity(nsString::size_type(-1)/2));
  ok(a.SetCapacity(0));  // free the allocated memory

  ok(b.SetCapacity(1));
  ok(!b.SetCapacity(nsString::size_type(-1)/2 - 1));
  ok(b.SetCapacity(0));

  ok(c.SetCapacity(1));
  ok(!c.SetCapacity(nsString::size_type(-1)/2));
  ok(c.SetCapacity(0));

  ok(!d.SetCapacity(nsString::size_type(-1)/2 - 1));
  ok(!d.SetCapacity(nsString::size_type(-1)/2));
  ok(d.SetCapacity(0));

  ok(!e.SetCapacity(nsString::size_type(-1)/4));
  ok(!e.SetCapacity(nsString::size_type(-1)/4 + 1));
  ok(e.SetCapacity(0));

  ok(!f.SetCapacity(nsString::size_type(-1)/2));
  ok(f.SetCapacity(0));

  ok(!g.SetCapacity(nsString::size_type(-1)/4 + 1000));
  ok(!g.SetCapacity(nsString::size_type(-1)/4 + 1001));
  ok(g.SetCapacity(0));

  ok(!h.SetCapacity(nsString::size_type(-1)/4+1));
  ok(!h.SetCapacity(nsString::size_type(-1)/2));
  ok(h.SetCapacity(0));

  ok(i.SetCapacity(1));
  ok(i.SetCapacity(nsString::size_type(-1)/4 - 1000));
  ok(!i.SetCapacity(nsString::size_type(-1)/4 + 1));
  ok(i.SetCapacity(0));

  ok(j.SetCapacity(nsString::size_type(-1)/4 - 1000));
  ok(!j.SetCapacity(nsString::size_type(-1)/4 + 1));
  ok(j.SetCapacity(0));

  ok(k.SetCapacity(nsString::size_type(-1)/8 - 1000));
  ok(k.SetCapacity(nsString::size_type(-1)/4 - 1001));
  ok(k.SetCapacity(nsString::size_type(-1)/4 - 998));
  ok(!k.SetCapacity(nsString::size_type(-1)/4 + 1));
  ok(k.SetCapacity(0));

  ok(l.SetCapacity(nsString::size_type(-1)/8));
  ok(l.SetCapacity(nsString::size_type(-1)/8 + 1));
  ok(l.SetCapacity(nsString::size_type(-1)/8 + 2));
  ok(l.SetCapacity(0));

  ok(m.SetCapacity(nsString::size_type(-1)/8 + 1000));
  ok(m.SetCapacity(nsString::size_type(-1)/8 + 1001));
  ok(m.SetCapacity(0));

  ok(n.SetCapacity(nsString::size_type(-1)/8+1));
  ok(!n.SetCapacity(nsString::size_type(-1)/4));
  ok(n.SetCapacity(0));

  ok(n.SetCapacity(0));
  ok(n.SetCapacity((nsString::size_type(-1)/2 - sizeof(nsStringBuffer)) / 2 - 2));
  ok(n.SetCapacity(0));
  ok(!n.SetCapacity((nsString::size_type(-1)/2 - sizeof(nsStringBuffer)) / 2 - 1));
  ok(n.SetCapacity(0));
  ok(n1.SetCapacity(0));
  ok(n1.SetCapacity((nsCString::size_type(-1)/2 - sizeof(nsStringBuffer)) / 1 - 2));
  ok(n1.SetCapacity(0));
  ok(!n1.SetCapacity((nsCString::size_type(-1)/2 - sizeof(nsStringBuffer)) / 1 - 1));
  ok(n1.SetCapacity(0));

  // Ignore the result if the address space is less than 64-bit because
  // some of the allocations above will exhaust the address space.  
  if (sizeof(void*) >= 8) {
    return !fail;
  }
  return true;
}

static bool test_tofloat_helper(const nsString& aStr, float aExpected, bool aSuccess)
{
  int32_t result;
  return aStr.ToFloat(&result) == aExpected &&
         aSuccess ? result == NS_OK : result != NS_OK;
}

static bool test_tofloat()
{
  return \
    test_tofloat_helper(NS_LITERAL_STRING("42"), 42.f, true) &&
    test_tofloat_helper(NS_LITERAL_STRING("42.0"), 42.f, true) &&
    test_tofloat_helper(NS_LITERAL_STRING("-42"), -42.f, true) &&
    test_tofloat_helper(NS_LITERAL_STRING("+42"), 42, true) &&
    test_tofloat_helper(NS_LITERAL_STRING("13.37"), 13.37f, true) &&
    test_tofloat_helper(NS_LITERAL_STRING("1.23456789"), 1.23456789f, true) &&
    test_tofloat_helper(NS_LITERAL_STRING("1.98765432123456"), 1.98765432123456f, true) &&
    test_tofloat_helper(NS_LITERAL_STRING("0"), 0.f, true) &&
    test_tofloat_helper(NS_LITERAL_STRING("1.e5"), 100000, true) &&
    test_tofloat_helper(NS_LITERAL_STRING(""), 0.f, false) &&
    test_tofloat_helper(NS_LITERAL_STRING("42foo"), 42.f, false) &&
    test_tofloat_helper(NS_LITERAL_STRING("foo"), 0.f, false) &&
    true;
}

static bool test_todouble_helper(const nsString& aStr, double aExpected, bool aSuccess)
{
  int32_t result;
  return aStr.ToDouble(&result) == aExpected &&
         aSuccess ? result == NS_OK : result != NS_OK;
}

static bool test_todouble()
{
  return \
    test_todouble_helper(NS_LITERAL_STRING("42"), 42, true) &&
    test_todouble_helper(NS_LITERAL_STRING("42.0"), 42, true) &&
    test_todouble_helper(NS_LITERAL_STRING("-42"), -42, true) &&
    test_todouble_helper(NS_LITERAL_STRING("+42"), 42, true) &&
    test_todouble_helper(NS_LITERAL_STRING("13.37"), 13.37, true) &&
    test_todouble_helper(NS_LITERAL_STRING("1.23456789"), 1.23456789, true) &&
    test_todouble_helper(NS_LITERAL_STRING("1.98765432123456"), 1.98765432123456, true) &&
    test_todouble_helper(NS_LITERAL_STRING("123456789.98765432123456"), 123456789.98765432123456, true) &&
    test_todouble_helper(NS_LITERAL_STRING("0"), 0, true) &&
    test_todouble_helper(NS_LITERAL_STRING("1.e5"), 100000, true) &&
    test_todouble_helper(NS_LITERAL_STRING(""), 0, false) &&
    test_todouble_helper(NS_LITERAL_STRING("42foo"), 42, false) &&
    test_todouble_helper(NS_LITERAL_STRING("foo"), 0, false) &&
    true;
}

//----

typedef bool (*TestFunc)();

static const struct Test
  {
    const char* name;
    TestFunc    func;
  }
tests[] =
  {
    { "test_assign", test_assign },
    { "test_assign_c", test_assign_c },
    { "test1", test1 },
    { "test2", test2 },
    { "test_find", test_find },
    { "test_rfind", test_rfind },
    { "test_rfind_2", test_rfind_2 },
    { "test_rfind_3", test_rfind_3 },
    { "test_rfind_4", test_rfind_4 },
    { "test_findinreadable", test_findinreadable },
    { "test_rfindinreadable", test_rfindinreadable },
    { "test_distance", test_distance },
    { "test_length", test_length },
    { "test_trim", test_trim },
    { "test_replace_substr", test_replace_substr },
    { "test_replace_substr_2", test_replace_substr_2 },
    { "test_strip_ws", test_strip_ws },
    { "test_equals_ic", test_equals_ic },
    { "test_fixed_string", test_fixed_string },
    { "test_concat", test_concat },
    { "test_concat_2", test_concat_2 },
    { "test_concat_3", test_concat_3 },
    { "test_xpidl_string", test_xpidl_string },
    { "test_empty_assign", test_empty_assign },
    { "test_set_length", test_set_length },
    { "test_substring", test_substring },
    { "test_appendint", test_appendint },
    { "test_appendint64", test_appendint64 },
    { "test_appendfloat", test_appendfloat },
    { "test_findcharinset", test_findcharinset },
    { "test_rfindcharinset", test_rfindcharinset },
    { "test_stringbuffer", test_stringbuffer },
    { "test_voided", test_voided },
    { "test_voided_autostr", test_voided_autostr },
    { "test_voided_assignment", test_voided_assignment },
    { "test_empty_assignment", test_empty_assignment },
    { "test_string_tointeger", test_string_tointeger },
    { "test_parse_string", test_parse_string },
    { "test_strip_chars", test_strip_chars },
    { "test_huge_capacity", test_huge_capacity },
    { "test_tofloat", test_tofloat },
    { "test_todouble", test_todouble },
    { nullptr, nullptr }
  };

}

using namespace TestStrings;

int main(int argc, char **argv)
  {
    int count = 1;
    if (argc > 1)
      count = atoi(argv[1]);

    NS_LogInit();

    while (count--)
      {
        for (const Test* t = tests; t->name != nullptr; ++t)
          {
            printf("%25s : %s\n", t->name, t->func() ? "SUCCESS" : "FAILURE <--");
          }
      }
    
    return 0;
  }
