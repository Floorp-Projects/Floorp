/* vim:set ts=2 sw=2 et cindent: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by IBM Corporation are Copyright (C) 2003
 * IBM Corporation.  All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@meer.net>
 *   Benjamin Smedberg <benjamin@smedbergs.us>
 *   Ben Turner <mozilla@songbirdnest.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <stdio.h>
#include <stdlib.h>
#include "nsStringAPI.h"
#include "nsXPCOM.h"
#include "nsMemory.h"

static const char kAsciiData[] = "Hello World";

static const PRUnichar kUnicodeData[] =
  {'H','e','l','l','o',' ','W','o','r','l','d','\0'};

static bool test_basic_1()
  {
    nsCStringContainer s;
    NS_CStringContainerInit(s);

    const char *ptr;
    PRUint32 len;
    char *clone;

    NS_CStringGetData(s, &ptr);
    if (ptr == nsnull || *ptr != '\0')
      {
        NS_ERROR("unexpected result");
        return PR_FALSE;
      }

    NS_CStringSetData(s, kAsciiData, PR_UINT32_MAX);
    len = NS_CStringGetData(s, &ptr);
    if (ptr == nsnull || strcmp(ptr, kAsciiData) != 0)
      {
        NS_ERROR("unexpected result");
        return PR_FALSE;
      }
    if (len != sizeof(kAsciiData)-1)
      {
        NS_ERROR("unexpected result");
        return PR_FALSE;
      }

    clone = NS_CStringCloneData(s);
    if (ptr == nsnull || strcmp(ptr, kAsciiData) != 0)
      {
        NS_ERROR("unexpected result");
        return PR_FALSE;
      }
    NS_Free(clone);

    nsCStringContainer temp;
    NS_CStringContainerInit(temp);
    NS_CStringCopy(temp, s);

    len = NS_CStringGetData(temp, &ptr);
    if (ptr == nsnull || strcmp(ptr, kAsciiData) != 0)
      {
        NS_ERROR("unexpected result");
        return PR_FALSE;
      }
    if (len != sizeof(kAsciiData)-1)
      {
        NS_ERROR("unexpected result");
        return PR_FALSE;
      }

    NS_CStringContainerFinish(temp);

    NS_CStringContainerFinish(s);
    return PR_TRUE;
  }

static bool test_basic_2()
  {
    nsStringContainer s;
    NS_StringContainerInit(s);

    const PRUnichar *ptr;
    PRUint32 len;
    PRUnichar *clone;

    NS_StringGetData(s, &ptr);
    if (ptr == nsnull || *ptr != '\0')
      {
        NS_ERROR("unexpected result");
        return PR_FALSE;
      }

    NS_StringSetData(s, kUnicodeData, PR_UINT32_MAX);
    len = NS_StringGetData(s, &ptr);
    if (len != sizeof(kUnicodeData)/2 - 1)
      {
        NS_ERROR("unexpected result");
        return PR_FALSE;
      }
    if (ptr == nsnull || memcmp(ptr, kUnicodeData, sizeof(kUnicodeData)) != 0)
      {
        NS_ERROR("unexpected result");
        return PR_FALSE;
      }

    clone = NS_StringCloneData(s);
    if (ptr == nsnull || memcmp(ptr, kUnicodeData, sizeof(kUnicodeData)) != 0)
      {
        NS_ERROR("unexpected result");
        return PR_FALSE;
      }
    NS_Free(clone);

    nsStringContainer temp;
    NS_StringContainerInit(temp);
    NS_StringCopy(temp, s);

    len = NS_StringGetData(temp, &ptr);
    if (len != sizeof(kUnicodeData)/2 - 1)
      {
        NS_ERROR("unexpected result");
        return PR_FALSE;
      }
    if (ptr == nsnull || memcmp(ptr, kUnicodeData, sizeof(kUnicodeData)) != 0)
      {
        NS_ERROR("unexpected result");
        return PR_FALSE;
      }

    NS_StringContainerFinish(temp);

    NS_StringContainerFinish(s);

    return PR_TRUE;
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
      return PR_FALSE;

    NS_UTF16ToCString(s, NS_CSTRING_ENCODING_UTF8, temp);
    NS_CStringGetData(temp, &data);
    if (strcmp(data, kAsciiData) != 0)
      return PR_FALSE;

    NS_CStringContainerFinish(temp);

    NS_StringContainerFinish(s);
    return PR_TRUE;
  }

static bool test_append()
  {
    nsCStringContainer s;
    NS_CStringContainerInit(s);

    NS_CStringSetData(s, "foo");
    NS_CStringAppendData(s, "bar");

    NS_CStringContainerFinish(s);
    return PR_TRUE;
  }

// Replace all occurrences of |matchVal| with |newVal|
static void ReplaceSubstring( nsACString& str,
                              const nsACString& matchVal,
                              const nsACString& newVal )
  {
    const char* sp, *mp, *np;
    PRUint32 sl, ml, nl;

    sl = NS_CStringGetData(str, &sp);
    ml = NS_CStringGetData(matchVal, &mp);
    nl = NS_CStringGetData(newVal, &np);

    for (const char* iter = sp; iter <= sp + sl - ml; ++iter)
      {
        if (memcmp(iter, mp, ml) == 0)
          {
            PRUint32 offset = iter - sp;

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
      return PR_FALSE;

    NS_CStringContainerFinish(c);
    NS_CStringContainerFinish(b);
    NS_CStringContainerFinish(a);
    return PR_TRUE;
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

    return PR_TRUE;
  }

static const char* kWhitespace="\b\t\r\n ";

static void
CompressWhitespace(nsACString &str)
  {
    const char *p;
    PRInt32 i, len = (PRInt32) NS_CStringGetData(str, &p);

    // trim leading whitespace

    for (i=0; i<len; ++i)
      {
        if (!strchr(kWhitespace, (char) p[i]))
          break;
      }

    if (i>0)
      {
        NS_CStringCutData(str, 0, i);
        len = (PRInt32) NS_CStringGetData(str, &p);
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
        PR_FALSE);

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
        PR_FALSE);

    bool terminated;
    const char *sd;
    PRUint32 len = NS_CStringGetData(s, &sd, &terminated);

    bool rv = (sd == kData && len == sizeof(kData)-1 && !terminated);
    NS_CStringContainerFinish(s);
    return rv;
  }

static bool test_adopt()
  {
    static const char kData[] = "hello world";

    char *data = (char *) nsMemory::Clone(kData, sizeof(kData));
    if (!data)
      return PR_FALSE;

    nsCStringContainer s;
    NS_ENSURE_SUCCESS(
        NS_CStringContainerInit2(s, data, PR_UINT32_MAX,
                                 NS_CSTRING_CONTAINER_INIT_ADOPT),
        PR_FALSE); // leaks data on failure *shrug*

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
      return PR_FALSE;

    nsCStringContainer s;
    NS_ENSURE_SUCCESS(
        NS_CStringContainerInit2(s, data, sizeof(kData)-1,
                                 NS_CSTRING_CONTAINER_INIT_ADOPT |
                                 NS_CSTRING_CONTAINER_INIT_SUBSTRING),
        PR_FALSE); // leaks data on failure *shrug*

    bool terminated;
    const char *sd;
    PRUint32 len = NS_CStringGetData(s, &sd, &terminated);

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
    PRUint32 len = NS_CStringGetMutableData(s, sizeof(kText) - 1, &buf);
    if (!buf || len != sizeof(kText) - 1)
      return PR_FALSE;
    memcpy(buf, kText, sizeof(kText));

    const char *data;
    NS_CStringGetData(s, &data);
    if (strcmp(data, kText) != 0)
      return PR_FALSE;

    PRUint32 newLen = len + 1;
    len = NS_CStringGetMutableData(s, newLen, &buf);
    if (!buf || len != newLen)
      return PR_FALSE;

    buf[len - 1] = '.';

    NS_CStringGetData(s, &data);
    if (strncmp(data, kText, len - 1) != 0 || data[len - 1] != '.')
      return PR_FALSE;

    NS_CStringContainerFinish(s);
    return PR_TRUE;
  }

static bool test_ascii()
{
  nsCString testCString;
  testCString.AppendASCII(kAsciiData);
  if (!testCString.EqualsLiteral(kAsciiData))
    return PR_FALSE;

  testCString.AssignASCII(kAsciiData);
  if (!testCString.LowerCaseEqualsLiteral("hello world"))
    return PR_FALSE;

  nsString testString;
  testString.AppendASCII(kAsciiData);
  if (!testString.EqualsLiteral(kAsciiData))
    return PR_FALSE;

  testString.AssignASCII(kAsciiData);
  if (!testString.LowerCaseEqualsLiteral("hello world"))
    return PR_FALSE;

  return PR_TRUE;
}

static bool test_chars()
{
  nsCString testCString(kAsciiData);
  if (testCString.First() != 'H')
    return PR_FALSE;
  if (testCString.Last() != 'd')
    return PR_FALSE;
  testCString.SetCharAt('u', 8);
  if (!testCString.EqualsASCII("Hello Would"))
    return PR_FALSE;

  nsString testString(kUnicodeData);
  if (testString.First() != 'H')
    return PR_FALSE;
  if (testString.Last() != 'd')
    return PR_FALSE;
  testString.SetCharAt('u', 8);
  if (!testString.EqualsASCII("Hello Would"))
    return PR_FALSE;

  return PR_TRUE;
}

static bool test_stripchars()
{
  nsCString test(kAsciiData);
  test.StripChars("ld");
  if (!test.Equals("Heo Wor"))
    return PR_FALSE;

  test.Assign(kAsciiData);
  test.StripWhitespace();
  if (!test.Equals("HelloWorld"))
    return PR_FALSE;

  return PR_TRUE;
}

static bool test_trim()
{
  static const char kWS[] = "\n\t\r ";
  static const char kTestString[] = " \n\tTesting...\n\r";

  nsCString test1(kTestString);
  nsCString test2(kTestString);
  nsCString test3(kTestString);

  test1.Trim(kWS);
  test2.Trim(kWS, PR_TRUE, PR_FALSE);
  test3.Trim(kWS, PR_FALSE, PR_TRUE);

  if (!test1.Equals("Testing..."))
    return PR_FALSE;

  if (!test2.Equals("Testing...\n\r"))
    return PR_FALSE;

  if (!test3.Equals(" \n\tTesting..."))
    return PR_FALSE;

  return PR_TRUE;
}

static bool test_find()
{
  nsString uni(kUnicodeData);

  static const char kHello[] = "Hello";
  static const char khello[] = "hello";
  static const char kBye[] = "Bye!";

  PRInt32 found;

  found = uni.Find(kHello);
  if (found != 0)
    return PR_FALSE;

  found = uni.Find(khello, false);
  if (found != -1)
    return PR_FALSE;
 
  found = uni.Find(khello, true);
  if (found != 0)
    return PR_FALSE;

  found = uni.Find(kBye);
  if (found != -1)
    return PR_FALSE;

  found = uni.Find(NS_LITERAL_STRING("World"));
  if (found != 6)
    return PR_FALSE;

  found = uni.Find(uni);
  if (found != 0)
    return PR_FALSE;

  return PR_TRUE;
}

static bool test_compressws()
{
  nsString check(NS_LITERAL_STRING(" \tTesting  \n\t1\n 2 3\n "));
  CompressWhitespace(check);
  return check.Equals(NS_LITERAL_STRING("Testing 1 2 3"));
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
    return PR_FALSE;

  result = (shortString2 == shortString3);
  if (!result)
    return PR_FALSE;

  result = (shortString3 == shortString4);
  if (result)
    return PR_FALSE;

  result = (shortString1 == longString);
  if (result)
    return PR_FALSE;

  result = (longString == shortString1);
  if (result)
    return PR_FALSE;

  // !=

  result = (shortString1 != shortString2);
  if (!result)
    return PR_FALSE;

  result = (shortString2 != shortString3);
  if (result)
    return PR_FALSE;

  result = (shortString3 != shortString4);
  if (!result)
    return PR_FALSE;

  result = (shortString1 != longString);
  if (!result)
    return PR_FALSE;

  result = (longString != shortString1);
  if (!result)
    return PR_FALSE;

  // <

  result = (shortString1 < shortString2);
  if (result)
    return PR_FALSE;

  result = (shortString2 < shortString1);
  if (!result)
    return PR_FALSE;

  result = (shortString1 < longString);
  if (!result)
    return PR_FALSE;

  result = (longString < shortString1);
  if (result)
    return PR_FALSE;

  result = (shortString2 < shortString3);
  if (result)
    return PR_FALSE;

  result = (shortString3 < shortString4);
  if (!result)
    return PR_FALSE;

  result = (shortString4 < shortString3);
  if (result)
    return PR_FALSE;

  // <=

  result = (shortString1 <= shortString2);
  if (result)
    return PR_FALSE;

  result = (shortString2 <= shortString1);
  if (!result)
    return PR_FALSE;

  result = (shortString1 <= longString);
  if (!result)
    return PR_FALSE;

  result = (longString <= shortString1);
  if (result)
    return PR_FALSE;

  result = (shortString2 <= shortString3);
  if (!result)
    return PR_FALSE;

  result = (shortString3 <= shortString4);
  if (!result)
    return PR_FALSE;

  result = (shortString4 <= shortString3);
  if (result)
    return PR_FALSE;

  // >

  result = (shortString1 > shortString2);
  if (!result)
    return PR_FALSE;

  result = (shortString2 > shortString1);
  if (result)
    return PR_FALSE;

  result = (shortString1 > longString);
  if (result)
    return PR_FALSE;

  result = (longString > shortString1);
  if (!result)
    return PR_FALSE;

  result = (shortString2 > shortString3);
  if (result)
    return PR_FALSE;

  result = (shortString3 > shortString4);
  if (result)
    return PR_FALSE;

  result = (shortString4 > shortString3);
  if (!result)
    return PR_FALSE;

  // >=

  result = (shortString1 >= shortString2);
  if (!result)
    return PR_FALSE;

  result = (shortString2 >= shortString1);
  if (result)
    return PR_FALSE;

  result = (shortString1 >= longString);
  if (result)
    return PR_FALSE;

  result = (longString >= shortString1);
  if (!result)
    return PR_FALSE;

  result = (shortString2 >= shortString3);
  if (!result)
    return PR_FALSE;

  result = (shortString3 >= shortString4);
  if (result)
    return PR_FALSE;

  result = (shortString4 >= shortString3);
  if (!result)
    return PR_FALSE;

  // nsCString

  NS_NAMED_LITERAL_CSTRING(shortCString1, "Foo");
  NS_NAMED_LITERAL_CSTRING(shortCString2, "Bar");
  NS_NAMED_LITERAL_CSTRING(shortCString3, "Bar");
  NS_NAMED_LITERAL_CSTRING(shortCString4, "bar");
  NS_NAMED_LITERAL_CSTRING(longCString, "FooBar");

  // ==

  result = (shortCString1 == shortCString2);
  if (result)
    return PR_FALSE;

  result = (shortCString2 == shortCString3);
  if (!result)
    return PR_FALSE;

  result = (shortCString3 == shortCString4);
  if (result)
    return PR_FALSE;

  result = (shortCString1 == longCString);
  if (result)
    return PR_FALSE;

  result = (longCString == shortCString1);
  if (result)
    return PR_FALSE;

  // !=

  result = (shortCString1 != shortCString2);
  if (!result)
    return PR_FALSE;

  result = (shortCString2 != shortCString3);
  if (result)
    return PR_FALSE;

  result = (shortCString3 != shortCString4);
  if (!result)
    return PR_FALSE;

  result = (shortCString1 != longCString);
  if (!result)
    return PR_FALSE;

  result = (longCString != shortCString1);
  if (!result)
    return PR_FALSE;

  // <

  result = (shortCString1 < shortCString2);
  if (result)
    return PR_FALSE;

  result = (shortCString2 < shortCString1);
  if (!result)
    return PR_FALSE;

  result = (shortCString1 < longCString);
  if (!result)
    return PR_FALSE;

  result = (longCString < shortCString1);
  if (result)
    return PR_FALSE;

  result = (shortCString2 < shortCString3);
  if (result)
    return PR_FALSE;

  result = (shortCString3 < shortCString4);
  if (!result)
    return PR_FALSE;

  result = (shortCString4 < shortCString3);
  if (result)
    return PR_FALSE;

  // <=

  result = (shortCString1 <= shortCString2);
  if (result)
    return PR_FALSE;

  result = (shortCString2 <= shortCString1);
  if (!result)
    return PR_FALSE;

  result = (shortCString1 <= longCString);
  if (!result)
    return PR_FALSE;

  result = (longCString <= shortCString1);
  if (result)
    return PR_FALSE;

  result = (shortCString2 <= shortCString3);
  if (!result)
    return PR_FALSE;

  result = (shortCString3 <= shortCString4);
  if (!result)
    return PR_FALSE;

  result = (shortCString4 <= shortCString3);
  if (result)
    return PR_FALSE;

  // >

  result = (shortCString1 > shortCString2);
  if (!result)
    return PR_FALSE;

  result = (shortCString2 > shortCString1);
  if (result)
    return PR_FALSE;

  result = (shortCString1 > longCString);
  if (result)
    return PR_FALSE;

  result = (longCString > shortCString1);
  if (!result)
    return PR_FALSE;

  result = (shortCString2 > shortCString3);
  if (result)
    return PR_FALSE;

  result = (shortCString3 > shortCString4);
  if (result)
    return PR_FALSE;

  result = (shortCString4 > shortCString3);
  if (!result)
    return PR_FALSE;

  // >=

  result = (shortCString1 >= shortCString2);
  if (!result)
    return PR_FALSE;

  result = (shortCString2 >= shortCString1);
  if (result)
    return PR_FALSE;

  result = (shortCString1 >= longCString);
  if (result)
    return PR_FALSE;

  result = (longCString >= shortCString1);
  if (!result)
    return PR_FALSE;

  result = (shortCString2 >= shortCString3);
  if (!result)
    return PR_FALSE;

  result = (shortCString3 >= shortCString4);
  if (result)
    return PR_FALSE;

  result = (shortCString4 >= shortCString3);
  if (!result)
    return PR_FALSE;

  return PR_TRUE;
}

static bool test_parse_string_helper(const char* str, char separator, int len,
                                       const char* s1, const char* s2)
{
  nsCString data(str);
  nsTArray<nsCString> results;
  if (!ParseString(data, separator, results))
    return PR_FALSE;
  if (int(results.Length()) != len)
    return PR_FALSE;
  const char* strings[] = { s1, s2 };
  for (int i = 0; i < len; ++i) {
    if (!results[i].Equals(strings[i]))
      return PR_FALSE;
  }
  return PR_TRUE;
}

static bool test_parse_string_helper0(const char* str, char separator)
{
  return test_parse_string_helper(str, separator, 0, nsnull, nsnull);
}

static bool test_parse_string_helper1(const char* str, char separator, const char* s1)
{
  return test_parse_string_helper(str, separator, 1, s1, nsnull);
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
    { nsnull, nsnull }
  };

//----

int main(int argc, char **argv)
  {
    int count = 1;
    if (argc > 1)
      count = atoi(argv[1]);

    while (count--)
      {
        for (const Test* t = tests; t->name != nsnull; ++t)
          {
            printf("%25s : %s\n", t->name, t->func() ? "SUCCESS" : "FAILURE");
          }
      }
    
    return 0;
  }
