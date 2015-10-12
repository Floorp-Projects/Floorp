/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <stdlib.h>
#include "nsStringAPI.h"
#include "nsXPCOM.h"
#include "nsMemory.h"

static const char kAsciiData[] = "Hello World";

static const char16_t kUnicodeData[] =
  {'H','e','l','l','o',' ','W','o','r','l','d','\0'};

static bool test_basic_1()
  {
    nsCStringContainer s;
    NS_CStringContainerInit(s);

    const char *ptr;
    uint32_t len;
    char *clone;

    NS_CStringGetData(s, &ptr);
    if (ptr == nullptr || *ptr != '\0')
      {
        NS_ERROR("unexpected result");
        return false;
      }

    NS_CStringSetData(s, kAsciiData, UINT32_MAX);
    len = NS_CStringGetData(s, &ptr);
    if (ptr == nullptr || strcmp(ptr, kAsciiData) != 0)
      {
        NS_ERROR("unexpected result");
        return false;
      }
    if (len != sizeof(kAsciiData)-1)
      {
        NS_ERROR("unexpected result");
        return false;
      }

    clone = NS_CStringCloneData(s);
    if (ptr == nullptr || strcmp(ptr, kAsciiData) != 0)
      {
        NS_ERROR("unexpected result");
        return false;
      }
    free(clone);

    nsCStringContainer temp;
    NS_CStringContainerInit(temp);
    NS_CStringCopy(temp, s);

    len = NS_CStringGetData(temp, &ptr);
    if (ptr == nullptr || strcmp(ptr, kAsciiData) != 0)
      {
        NS_ERROR("unexpected result");
        return false;
      }
    if (len != sizeof(kAsciiData)-1)
      {
        NS_ERROR("unexpected result");
        return false;
      }

    NS_CStringContainerFinish(temp);

    NS_CStringContainerFinish(s);
    return true;
  }

static bool test_basic_2()
  {
    nsStringContainer s;
    NS_StringContainerInit(s);

    const char16_t *ptr;
    uint32_t len;
    char16_t *clone;

    NS_StringGetData(s, &ptr);
    if (ptr == nullptr || *ptr != '\0')
      {
        NS_ERROR("unexpected result");
        return false;
      }

    NS_StringSetData(s, kUnicodeData, UINT32_MAX);
    len = NS_StringGetData(s, &ptr);
    if (len != sizeof(kUnicodeData)/2 - 1)
      {
        NS_ERROR("unexpected result");
        return false;
      }
    if (ptr == nullptr || memcmp(ptr, kUnicodeData, sizeof(kUnicodeData)) != 0)
      {
        NS_ERROR("unexpected result");
        return false;
      }

    clone = NS_StringCloneData(s);
    if (ptr == nullptr || memcmp(ptr, kUnicodeData, sizeof(kUnicodeData)) != 0)
      {
        NS_ERROR("unexpected result");
        return false;
      }
    free(clone);

    nsStringContainer temp;
    NS_StringContainerInit(temp);
    NS_StringCopy(temp, s);

    len = NS_StringGetData(temp, &ptr);
    if (len != sizeof(kUnicodeData)/2 - 1)
      {
        NS_ERROR("unexpected result");
        return false;
      }
    if (ptr == nullptr || memcmp(ptr, kUnicodeData, sizeof(kUnicodeData)) != 0)
      {
        NS_ERROR("unexpected result");
        return false;
      }

    NS_StringContainerFinish(temp);

    NS_StringContainerFinish(s);

    return true;
  }

static bool test_convert()
  {
    nsStringContainer s;
    NS_StringContainerInit(s);
    NS_StringSetData(s, kUnicodeData, sizeof(kUnicodeData)/2 - 1);

    nsCStringContainer temp;
    NS_CStringContainerInit(temp);

    const char *data;

    NS_UTF16ToCString(s, NS_CSTRING_ENCODING_ASCII, temp);
    NS_CStringGetData(temp, &data);
    if (strcmp(data, kAsciiData) != 0)
      return false;

    NS_UTF16ToCString(s, NS_CSTRING_ENCODING_UTF8, temp);
    NS_CStringGetData(temp, &data);
    if (strcmp(data, kAsciiData) != 0)
      return false;

    NS_CStringContainerFinish(temp);

    NS_StringContainerFinish(s);
    return true;
  }

static bool test_append()
  {
    nsCStringContainer s;
    NS_CStringContainerInit(s);

    NS_CStringSetData(s, "foo");
    NS_CStringAppendData(s, "bar");

    NS_CStringContainerFinish(s);
    return true;
  }

// Replace all occurrences of |matchVal| with |newVal|
static void ReplaceSubstring( nsACString& str,
                              const nsACString& matchVal,
                              const nsACString& newVal )
  {
    const char* sp;
    const char* mp;
    const char* np;
    uint32_t sl = NS_CStringGetData(str, &sp);
    uint32_t ml = NS_CStringGetData(matchVal, &mp);
    uint32_t nl = NS_CStringGetData(newVal, &np);

    for (const char* iter = sp; iter <= sp + sl - ml; ++iter)
      {
        if (memcmp(iter, mp, ml) == 0)
          {
            uint32_t offset = iter - sp;

            NS_CStringSetDataRange(str, offset, ml, np, nl);

            sl = NS_CStringGetData(str, &sp);

            iter = sp + offset + nl - 1;
          }
      }
  }

static bool test_replace_driver(const char *strVal,
                                  const char *matchVal,
                                  const char *newVal,
                                  const char *finalVal)
  {
    nsCStringContainer a;
    NS_CStringContainerInit(a);
    NS_CStringSetData(a, strVal);

    nsCStringContainer b;
    NS_CStringContainerInit(b);
    NS_CStringSetData(b, matchVal);

    nsCStringContainer c;
    NS_CStringContainerInit(c);
    NS_CStringSetData(c, newVal);

    ReplaceSubstring(a, b, c);

    const char *data;
    NS_CStringGetData(a, &data);
    if (strcmp(data, finalVal) != 0)
      return false;

    NS_CStringContainerFinish(c);
    NS_CStringContainerFinish(b);
    NS_CStringContainerFinish(a);
    return true;
  }

static bool test_replace()
  {
    bool rv;

    rv = test_replace_driver("hello world, hello again!",
                             "hello",
                             "goodbye",
                             "goodbye world, goodbye again!");
    if (!rv)
      return rv;

    rv = test_replace_driver("foofoofoofoo!",
                             "foo",
                             "bar",
                             "barbarbarbar!");
    if (!rv)
      return rv;

    rv = test_replace_driver("foo bar systems",
                             "xyz",
                             "crazy",
                             "foo bar systems");
    if (!rv)
      return rv;

    rv = test_replace_driver("oh",
                             "xyz",
                             "crazy",
                             "oh");
    if (!rv)
      return rv;

    return true;
  }

static const char* kWhitespace="\f\t\r\n ";

static void
CompressWhitespace(nsACString &str)
  {
    const char *p;
    int32_t i, len = (int32_t) NS_CStringGetData(str, &p);

    // trim leading whitespace

    for (i=0; i<len; ++i)
      {
        if (!strchr(kWhitespace, (char) p[i]))
          break;
      }

    if (i>0)
      {
        NS_CStringCutData(str, 0, i);
        len = (int32_t) NS_CStringGetData(str, &p);
      }

    // trim trailing whitespace

    for (i=len-1; i>=0; --i)
      {
        if (!strchr(kWhitespace, (char) p[i]))
          break;
      }

    if (++i < len)
      NS_CStringCutData(str, i, len - i);
  }

static bool test_compress_ws()
  {
    nsCStringContainer s;
    NS_CStringContainerInit(s);
    NS_CStringSetData(s, " \thello world\r  \n");
    CompressWhitespace(s);
    const char *d;
    NS_CStringGetData(s, &d);
    bool rv = !strcmp(d, "hello world");
    if (!rv)
      printf("=> \"%s\"\n", d);
    NS_CStringContainerFinish(s);
    return rv;
  }

static bool test_depend()
  {
    static const char kData[] = "hello world";

    nsCStringContainer s;
    NS_ENSURE_SUCCESS(
        NS_CStringContainerInit2(s, kData, sizeof(kData)-1,
                                 NS_CSTRING_CONTAINER_INIT_DEPEND),
        false);

    const char *sd;
    NS_CStringGetData(s, &sd);

    bool rv = (sd == kData);
    NS_CStringContainerFinish(s);
    return rv;
  }

static bool test_depend_sub()
  {
    static const char kData[] = "hello world";

    nsCStringContainer s;
    NS_ENSURE_SUCCESS(
        NS_CStringContainerInit2(s, kData, sizeof(kData)-1,
                                 NS_CSTRING_CONTAINER_INIT_DEPEND |
                                 NS_CSTRING_CONTAINER_INIT_SUBSTRING),
        false);

    bool terminated;
    const char *sd;
    uint32_t len = NS_CStringGetData(s, &sd, &terminated);

    bool rv = (sd == kData && len == sizeof(kData)-1 && !terminated);
    NS_CStringContainerFinish(s);
    return rv;
  }

static bool test_adopt()
  {
    static const char kData[] = "hello world";

    char *data = (char *) nsMemory::Clone(kData, sizeof(kData));
    if (!data)
      return false;

    nsCStringContainer s;
    NS_ENSURE_SUCCESS(
        NS_CStringContainerInit2(s, data, UINT32_MAX,
                                 NS_CSTRING_CONTAINER_INIT_ADOPT),
        false); // leaks data on failure *shrug*

    const char *sd;
    NS_CStringGetData(s, &sd);

    bool rv = (sd == data);
    NS_CStringContainerFinish(s);
    return rv;
  }

static bool test_adopt_sub()
  {
    static const char kData[] = "hello world";

    char *data = (char *) nsMemory::Clone(kData, sizeof(kData)-1);
    if (!data)
      return false;

    nsCStringContainer s;
    NS_ENSURE_SUCCESS(
        NS_CStringContainerInit2(s, data, sizeof(kData)-1,
                                 NS_CSTRING_CONTAINER_INIT_ADOPT |
                                 NS_CSTRING_CONTAINER_INIT_SUBSTRING),
        false); // leaks data on failure *shrug*

    bool terminated;
    const char *sd;
    uint32_t len = NS_CStringGetData(s, &sd, &terminated);

    bool rv = (sd == data && len == sizeof(kData)-1 && !terminated);
    NS_CStringContainerFinish(s);
    return rv;
  }

static bool test_mutation()
  {
    nsCStringContainer s;
    NS_CStringContainerInit(s);

    const char kText[] = "Every good boy does fine.";

    char *buf;
    uint32_t len = NS_CStringGetMutableData(s, sizeof(kText) - 1, &buf);
    if (!buf || len != sizeof(kText) - 1)
      return false;
    memcpy(buf, kText, sizeof(kText));

    const char *data;
    NS_CStringGetData(s, &data);
    if (strcmp(data, kText) != 0)
      return false;

    uint32_t newLen = len + 1;
    len = NS_CStringGetMutableData(s, newLen, &buf);
    if (!buf || len != newLen)
      return false;

    buf[len - 1] = '.';

    NS_CStringGetData(s, &data);
    if (strncmp(data, kText, len - 1) != 0 || data[len - 1] != '.')
      return false;

    NS_CStringContainerFinish(s);
    return true;
  }

static bool test_ascii()
{
  nsCString testCString;
  testCString.AppendASCII(kAsciiData);
  if (!testCString.EqualsLiteral(kAsciiData))
    return false;

  testCString.AssignASCII(kAsciiData);
  if (!testCString.LowerCaseEqualsLiteral("hello world"))
    return false;

  nsString testString;
  testString.AppendASCII(kAsciiData);
  if (!testString.EqualsLiteral(kAsciiData))
    return false;

  testString.AssignASCII(kAsciiData);
  if (!testString.LowerCaseEqualsLiteral("hello world"))
    return false;

  return true;
}

static bool test_chars()
{
  nsCString testCString(kAsciiData);
  if (testCString.First() != 'H')
    return false;
  if (testCString.Last() != 'd')
    return false;
  testCString.SetCharAt('u', 8);
  if (!testCString.EqualsASCII("Hello Would"))
    return false;

  nsString testString(kUnicodeData);
  if (testString.First() != 'H')
    return false;
  if (testString.Last() != 'd')
    return false;
  testString.SetCharAt('u', 8);
  if (!testString.EqualsASCII("Hello Would"))
    return false;

  return true;
}

static bool test_stripchars()
{
  nsCString test(kAsciiData);
  test.StripChars("ld");
  if (!test.EqualsLiteral("Heo Wor"))
    return false;

  test.Assign(kAsciiData);
  test.StripWhitespace();
  if (!test.EqualsLiteral("HelloWorld"))
    return false;

  return true;
}

static bool test_trim()
{
  static const char kWS[] = "\n\t\r ";
  static const char kTestString[] = " \n\tTesting...\n\r";

  nsCString test1(kTestString);
  nsCString test2(kTestString);
  nsCString test3(kTestString);

  test1.Trim(kWS);
  test2.Trim(kWS, true, false);
  test3.Trim(kWS, false, true);

  if (!test1.EqualsLiteral("Testing..."))
    return false;

  if (!test2.EqualsLiteral("Testing...\n\r"))
    return false;

  if (!test3.EqualsLiteral(" \n\tTesting..."))
    return false;

  return true;
}

static bool test_find()
{
  nsString uni(kUnicodeData);

  static const char kHello[] = "Hello";
  static const char khello[] = "hello";
  static const char kBye[] = "Bye!";

  int32_t found;

  found = uni.Find(kHello);
  if (found != 0)
    return false;

  found = uni.Find(khello, false);
  if (found != -1)
    return false;
 
  found = uni.Find(khello, true);
  if (found != 0)
    return false;

  found = uni.Find(kBye);
  if (found != -1)
    return false;

  found = uni.Find(NS_LITERAL_STRING("World"));
  if (found != 6)
    return false;

  found = uni.Find(uni);
  if (found != 0)
    return false;

  return true;
}

static bool test_compressws()
{
  nsString check(NS_LITERAL_STRING(" \tTesting  \n\t1\n 2 3\n "));
  CompressWhitespace(check);
  return check.EqualsLiteral("Testing 1 2 3");
}

static bool test_comparisons()
{
  bool result;

  // nsString

  NS_NAMED_LITERAL_STRING(shortString1, "Foo");
  NS_NAMED_LITERAL_STRING(shortString2, "Bar");
  NS_NAMED_LITERAL_STRING(shortString3, "Bar");
  NS_NAMED_LITERAL_STRING(shortString4, "bar");
  NS_NAMED_LITERAL_STRING(longString, "FooBar");

  // ==

  result = (shortString1 == shortString2);
  if (result)
    return false;

  result = (shortString2 == shortString3);
  if (!result)
    return false;

  result = (shortString3 == shortString4);
  if (result)
    return false;

  result = (shortString1 == longString);
  if (result)
    return false;

  result = (longString == shortString1);
  if (result)
    return false;

  // !=

  result = (shortString1 != shortString2);
  if (!result)
    return false;

  result = (shortString2 != shortString3);
  if (result)
    return false;

  result = (shortString3 != shortString4);
  if (!result)
    return false;

  result = (shortString1 != longString);
  if (!result)
    return false;

  result = (longString != shortString1);
  if (!result)
    return false;

  // <

  result = (shortString1 < shortString2);
  if (result)
    return false;

  result = (shortString2 < shortString1);
  if (!result)
    return false;

  result = (shortString1 < longString);
  if (!result)
    return false;

  result = (longString < shortString1);
  if (result)
    return false;

  result = (shortString2 < shortString3);
  if (result)
    return false;

  result = (shortString3 < shortString4);
  if (!result)
    return false;

  result = (shortString4 < shortString3);
  if (result)
    return false;

  // <=

  result = (shortString1 <= shortString2);
  if (result)
    return false;

  result = (shortString2 <= shortString1);
  if (!result)
    return false;

  result = (shortString1 <= longString);
  if (!result)
    return false;

  result = (longString <= shortString1);
  if (result)
    return false;

  result = (shortString2 <= shortString3);
  if (!result)
    return false;

  result = (shortString3 <= shortString4);
  if (!result)
    return false;

  result = (shortString4 <= shortString3);
  if (result)
    return false;

  // >

  result = (shortString1 > shortString2);
  if (!result)
    return false;

  result = (shortString2 > shortString1);
  if (result)
    return false;

  result = (shortString1 > longString);
  if (result)
    return false;

  result = (longString > shortString1);
  if (!result)
    return false;

  result = (shortString2 > shortString3);
  if (result)
    return false;

  result = (shortString3 > shortString4);
  if (result)
    return false;

  result = (shortString4 > shortString3);
  if (!result)
    return false;

  // >=

  result = (shortString1 >= shortString2);
  if (!result)
    return false;

  result = (shortString2 >= shortString1);
  if (result)
    return false;

  result = (shortString1 >= longString);
  if (result)
    return false;

  result = (longString >= shortString1);
  if (!result)
    return false;

  result = (shortString2 >= shortString3);
  if (!result)
    return false;

  result = (shortString3 >= shortString4);
  if (result)
    return false;

  result = (shortString4 >= shortString3);
  if (!result)
    return false;

  // nsCString

  NS_NAMED_LITERAL_CSTRING(shortCString1, "Foo");
  NS_NAMED_LITERAL_CSTRING(shortCString2, "Bar");
  NS_NAMED_LITERAL_CSTRING(shortCString3, "Bar");
  NS_NAMED_LITERAL_CSTRING(shortCString4, "bar");
  NS_NAMED_LITERAL_CSTRING(longCString, "FooBar");

  // ==

  result = (shortCString1 == shortCString2);
  if (result)
    return false;

  result = (shortCString2 == shortCString3);
  if (!result)
    return false;

  result = (shortCString3 == shortCString4);
  if (result)
    return false;

  result = (shortCString1 == longCString);
  if (result)
    return false;

  result = (longCString == shortCString1);
  if (result)
    return false;

  // !=

  result = (shortCString1 != shortCString2);
  if (!result)
    return false;

  result = (shortCString2 != shortCString3);
  if (result)
    return false;

  result = (shortCString3 != shortCString4);
  if (!result)
    return false;

  result = (shortCString1 != longCString);
  if (!result)
    return false;

  result = (longCString != shortCString1);
  if (!result)
    return false;

  // <

  result = (shortCString1 < shortCString2);
  if (result)
    return false;

  result = (shortCString2 < shortCString1);
  if (!result)
    return false;

  result = (shortCString1 < longCString);
  if (!result)
    return false;

  result = (longCString < shortCString1);
  if (result)
    return false;

  result = (shortCString2 < shortCString3);
  if (result)
    return false;

  result = (shortCString3 < shortCString4);
  if (!result)
    return false;

  result = (shortCString4 < shortCString3);
  if (result)
    return false;

  // <=

  result = (shortCString1 <= shortCString2);
  if (result)
    return false;

  result = (shortCString2 <= shortCString1);
  if (!result)
    return false;

  result = (shortCString1 <= longCString);
  if (!result)
    return false;

  result = (longCString <= shortCString1);
  if (result)
    return false;

  result = (shortCString2 <= shortCString3);
  if (!result)
    return false;

  result = (shortCString3 <= shortCString4);
  if (!result)
    return false;

  result = (shortCString4 <= shortCString3);
  if (result)
    return false;

  // >

  result = (shortCString1 > shortCString2);
  if (!result)
    return false;

  result = (shortCString2 > shortCString1);
  if (result)
    return false;

  result = (shortCString1 > longCString);
  if (result)
    return false;

  result = (longCString > shortCString1);
  if (!result)
    return false;

  result = (shortCString2 > shortCString3);
  if (result)
    return false;

  result = (shortCString3 > shortCString4);
  if (result)
    return false;

  result = (shortCString4 > shortCString3);
  if (!result)
    return false;

  // >=

  result = (shortCString1 >= shortCString2);
  if (!result)
    return false;

  result = (shortCString2 >= shortCString1);
  if (result)
    return false;

  result = (shortCString1 >= longCString);
  if (result)
    return false;

  result = (longCString >= shortCString1);
  if (!result)
    return false;

  result = (shortCString2 >= shortCString3);
  if (!result)
    return false;

  result = (shortCString3 >= shortCString4);
  if (result)
    return false;

  result = (shortCString4 >= shortCString3);
  if (!result)
    return false;

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

//----

typedef bool (*TestFunc)();

static const struct Test
  {
    const char* name;
    TestFunc    func;
  }
tests[] =
  {
    { "test_basic_1", test_basic_1 },
    { "test_basic_2", test_basic_2 },
    { "test_convert", test_convert },
    { "test_append", test_append },
    { "test_replace", test_replace },
    { "test_compress_ws", test_compress_ws },
    { "test_depend", test_depend },
    { "test_depend_sub", test_depend_sub },
    { "test_adopt", test_adopt },
    { "test_adopt_sub", test_adopt_sub },
    { "test_mutation", test_mutation },
    { "test_ascii", test_ascii },
    { "test_chars", test_chars },
    { "test_stripchars", test_stripchars },
    { "test_trim", test_trim },
    { "test_find", test_find },
    { "test_compressws", test_compressws },
    { "test_comparisons", test_comparisons },
    { "test_parse_string", test_parse_string },
    { nullptr, nullptr }
  };

//----

int main(int argc, char **argv)
  {
    int count = 1;
    if (argc > 1)
      count = atoi(argv[1]);

    while (count--)
      {
        for (const Test* t = tests; t->name != nullptr; ++t)
          {
            printf("%25s : %s\n", t->name, t->func() ? "SUCCESS" : "FAILURE");
          }
      }
    
    return 0;
  }
