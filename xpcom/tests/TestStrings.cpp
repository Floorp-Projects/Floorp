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
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"

void test_assign_helper(const nsACString& in, nsACString &_retval)
  {
    _retval = in;
  }

PRBool test_assign()
  {
    nsCString result;
    test_assign_helper(NS_LITERAL_CSTRING("a") + NS_LITERAL_CSTRING("b"), result);
    PRBool r = strcmp(result.get(), "ab") == 0;
    if (!r)
      printf("[result=%s]\n", result.get());
    return r;
  }

PRBool test_assign_c()
  {
    nsCString c; c.Assign('c');
    PRBool r = strcmp(c.get(), "c") == 0;
    if (!r)
      printf("[result=%s]\n", c.get());
    return r;
  }

PRBool test1()
  {
    NS_NAMED_LITERAL_STRING(empty, "");
    const nsAString& aStr = empty;

    nsAutoString buf(aStr);

    PRInt32 n = buf.FindChar(',');

    n = buf.Length();

    buf.Cut(0, n + 1);
    n = buf.FindChar(',');

    if (n != kNotFound)
      printf("n=%d\n", n);

    return n == kNotFound;
  }

PRBool test2()
  {
    nsCString data("hello world");
    const nsACString& aStr = data;

    nsCString temp(aStr);
    temp.Cut(0, 6);

    PRBool r = strcmp(temp.get(), "world") == 0;
    if (!r)
      printf("[temp=%s]\n", temp.get());
    return r;
  }

PRBool test_find()
  {
    nsCString src("<!DOCTYPE blah blah blah>");

    PRInt32 i = src.Find("DOCTYPE", PR_TRUE, 2, 1);
    if (i == 2)
      return PR_TRUE;

    printf("i=%d\n", i);
    return PR_FALSE;
  }

PRBool test_rfind()
  {
    const char text[] = "<!DOCTYPE blah blah blah>";
    const char term[] = "bLaH";
    nsCString src(text);
    PRInt32 i;

    i = src.RFind(term, PR_TRUE, 3, -1); 
    if (i != kNotFound)
      {
        printf("unexpected result searching from offset=3, i=%d\n", i);
        return PR_FALSE;
      }

    i = src.RFind(term, PR_TRUE, -1, -1);
    if (i != 20)
      {
        printf("unexpected result searching from offset=-1, i=%d\n", i);
        return PR_FALSE;
      }

    i = src.RFind(term, PR_TRUE, 13, -1);
    if (i != 10)
      {
        printf("unexpected result searching from offset=13, i=%d\n", i);
        return PR_FALSE;
      }

    i = src.RFind(term, PR_TRUE, 22, 3);
    if (i != 20)
      {
        printf("unexpected result searching from offset=22, i=%d\n", i);
        return PR_FALSE;
      }

    return PR_TRUE;
  }

PRBool test_rfind_2()
  {
    const char text[] = "<!DOCTYPE blah blah blah>";
    nsCString src(text);
    PRInt32 i = src.RFind("TYPE", PR_FALSE, 5, -1); 
    if (i == 5)
      return PR_TRUE;

    printf("i=%d\n", i);
    return PR_FALSE;
  }

PRBool test_rfind_3()
  {
    const char text[] = "urn:mozilla:locale:en-US:necko";
    nsCAutoString value(text);
    PRInt32 i = value.RFind(":");
    if (i == 24)
      return PR_TRUE;

    printf("i=%d\n", i);
    return PR_FALSE;
  }

PRBool test_rfind_4()
  {
    nsCString value("a.msf");
    PRInt32 i = value.RFind(".msf");
    if (i != 1)
      {
        printf("i=%d\n", i);
        return PR_FALSE;
      }

    return PR_TRUE;
  }

PRBool test_distance()
  {
    const char text[] = "abc-xyz";
    nsCString s(text);
    nsCString::const_iterator begin, end;
    s.BeginReading(begin);
    s.EndReading(end);
    size_t d = Distance(begin, end);
    PRBool r = (d == sizeof(text)-1);
    if (!r)
      printf("d=%u\n", d);
    return r;
  }

PRBool test_length()
  {
    const char text[] = "abc-xyz";
    nsCString s(text);
    size_t d = s.Length();
    PRBool r = (d == sizeof(text)-1);
    if (!r)
      printf("d=%u\n", d);
    return r;
  }

PRBool test_trim()
  {
    const char text[] = " a\t    $   ";
    const char set[] = " \t$";

    nsCString s(text);
    s.Trim(set);
    PRBool r = strcmp(s.get(), "a") == 0;
    if (!r)
      printf("[s=%s]\n", s.get());
    return r;
  }

PRBool test_replace_substr()
  {
    const char text[] = "abc-ppp-qqq-ppp-xyz";
    nsCString s(text);
    s.ReplaceSubstring("ppp", "www");
    PRBool r = strcmp(s.get(), "abc-www-qqq-www-xyz") == 0;
    if (!r)
      {
        printf("[s=%s]\n", s.get());
        return PR_FALSE;
      }

    s.Assign("foobar");
    s.ReplaceSubstring("foo", "bar");
    s.ReplaceSubstring("bar", "");
    r = strcmp(s.get(), "") == 0;
    if (!r)
      {
        printf("[s=%s]\n", s.get());
        return PR_FALSE;
      }

    s.Assign("foofoofoo");
    s.ReplaceSubstring("foo", "foo");
    r = strcmp(s.get(), "foofoofoo") == 0;
    if (!r)
      {
        printf("[s=%s]\n", s.get());
        return PR_FALSE;
      }

    s.Assign("foofoofoo");
    s.ReplaceSubstring("of", "fo");
    r = strcmp(s.get(), "fofoofooo") == 0;
    if (!r)
      {
        printf("[s=%s]\n", s.get());
        return PR_FALSE;
      }

    return PR_TRUE;
  }

PRBool test_replace_substr_2()
  {
    const char *oldName = nsnull;
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
      return PR_FALSE;

    return PR_TRUE;
  }

PRBool test_strip_ws()
  {
    const char text[] = " a    $   ";
    nsCString s(text);
    s.StripWhitespace();
    PRBool r = strcmp(s.get(), "a$") == 0;
    if (!r)
      printf("[s=%s]\n", s.get());
    return r;
  }

PRBool test_equals_ic()
  {
    nsCString s;
    PRBool r = s.LowerCaseEqualsLiteral("view-source");
    if (r)
      printf("[r=%d]\n", r);
    return !r;
  }

PRBool test_fixed_string()
  {
    char buf[256] = "hello world";

    nsFixedCString s(buf, sizeof(buf));

    if (s.Length() != strlen(buf))
      return PR_FALSE;

    if (strcmp(s.get(), buf) != 0)
      return PR_FALSE;

    s.Assign("foopy doopy doo");
    if (s.get() != buf)
      return PR_FALSE;
    
    return PR_TRUE;
  }

PRBool test_concat()
  {
    nsCString bar("bar");
    const nsACString& barRef = bar;

    const nsPromiseFlatCString& result =
        PromiseFlatCString(NS_LITERAL_CSTRING("foo") +
                           NS_LITERAL_CSTRING(",") +
                           barRef);
    if (strcmp(result.get(), "foo,bar") == 0)
      return PR_TRUE;

    printf("[result=%s]\n", result.get());
    return PR_FALSE;
  }

PRBool test_concat_2()
  {
    nsCString fieldTextStr("xyz");
    nsCString text("text");
    const nsACString& aText = text;

    nsCAutoString result( fieldTextStr + aText );

    if (strcmp(result.get(), "xyztext") == 0)
      return PR_TRUE;
    
    printf("[result=%s]\n", result.get());
    return PR_FALSE;
  }

#if 0
PRBool test_concat_3()
  {
    nsCString a("a"), b("b");

    // THIS DOES NOT COMPILE
    const nsACString& r = a + b;

    return PR_TRUE;
  }
#endif

PRBool test_xpidl_string()
  {
    nsXPIDLCString a, b;
    a = b;
    if (a != b)
      return PR_FALSE;

    a.Adopt(0);
    if (a != b)
      return PR_FALSE;

    a.Append("foopy");
    a.Assign(b);
    if (a != b)
      return PR_FALSE;

    a.Insert("", 0);
    a.Assign(b);
    if (a != b)
      return PR_FALSE;

    const char text[] = "hello world";
    *getter_Copies(a) = nsCRT::strdup(text);
    if (strcmp(a, text) != 0)
      return PR_FALSE;

    b = a;
    if (strcmp(a, b) != 0)
      return PR_FALSE;

    a.Adopt(0);
    nsACString::const_iterator begin, end;
    a.BeginReading(begin);
    a.EndReading(end);
    char *r = ToNewCString(Substring(begin, end));
    if (strcmp(r, "") != 0)
      return PR_FALSE;
    nsMemory::Free(r);

    a.Adopt(0);
    if (a != (const char*) 0)
      return PR_FALSE;

    /*
    PRInt32 index = a.FindCharInSet("xyz");
    if (index != kNotFound)
      return PR_FALSE;
    */

    return PR_TRUE;
  }

PRBool test_empty_assign()
  {
    nsCString a;
    a.AssignLiteral("");

    a.AppendLiteral("");

    nsCString b;
    b.SetCapacity(0);
    return PR_TRUE;
  }

PRBool test_set_length()
  {
    const char kText[] = "Default Plugin";
    nsCString buf;
    buf.SetCapacity(sizeof(kText)-1);
    buf.Assign(kText);
    buf.SetLength(sizeof(kText)-1);
    if (strcmp(buf.get(), kText) != 0)
      return PR_FALSE;
    return PR_TRUE;
  }

PRBool test_substring()
  {
    nsCString super("hello world"), sub("hello");

    // this tests that |super| starts with |sub|,
    
    PRBool r = sub.Equals(StringHead(super, sub.Length()));
    if (!r)
      return PR_FALSE;

    // and verifies that |sub| does not start with |super|.

    r = super.Equals(StringHead(sub, super.Length()));
    if (r)
      return PR_FALSE;

    return PR_TRUE;
  }

PRBool test_appendint64()
  {
    nsCString str;

    PRInt64 max = LL_MaxInt();
    static const char max_expected[] = "9223372036854775807";
    PRInt64 min = LL_MinInt();
    static const char min_expected[] = "-9223372036854775808";
    static const char min_expected_oct[] = "1000000000000000000000";
    PRInt64 maxint_plus1 = LL_INIT(1, 0);
    static const char maxint_plus1_expected[] = "4294967296";
    static const char maxint_plus1_expected_x[] = "100000000";

    str.AppendInt(max);

    if (!str.Equals(max_expected)) {
      fprintf(stderr, "Error appending LL_MaxInt(): Got %s\n", str.get());
      return PR_FALSE;
    }

    str.Truncate();
    str.AppendInt(min);
    if (!str.Equals(min_expected)) {
      fprintf(stderr, "Error appending LL_MinInt(): Got %s\n", str.get());
      return PR_FALSE;
    }
    str.Truncate();
    str.AppendInt(min, 8);
    if (!str.Equals(min_expected_oct)) {
      fprintf(stderr, "Error appending LL_MinInt() (oct): Got %s\n", str.get());
      return PR_FALSE;
    }


    str.Truncate();
    str.AppendInt(maxint_plus1);
    if (!str.Equals(maxint_plus1_expected)) {
      fprintf(stderr, "Error appending PR_UINT32_MAX + 1: Got %s\n", str.get());
      return PR_FALSE;
    }
    str.Truncate();
    str.AppendInt(maxint_plus1, 16);
    if (!str.Equals(maxint_plus1_expected_x)) {
      fprintf(stderr, "Error appending PR_UINT32_MAX + 1 (hex): Got %s\n", str.get());
      return PR_FALSE;
    }


    return PR_TRUE;
  }

PRBool test_findcharinset()
  {
    nsCString buf("hello, how are you?");

    PRInt32 index = buf.FindCharInSet(",?", 5);
    if (index != 5)
      return PR_FALSE;

    index = buf.FindCharInSet("helo", 0);
    if (index != 0)
      return PR_FALSE;

    index = buf.FindCharInSet("z?", 6);
    if (index != (PRInt32) buf.Length()-1)
      return PR_FALSE;

    return PR_TRUE;
  }

PRBool test_rfindcharinset()
  {
    nsCString buf("hello, how are you?");

    PRInt32 index = buf.RFindCharInSet(",?", 5);
    if (index != 5)
      return PR_FALSE;

    index = buf.RFindCharInSet("helo", 0);
    if (index != 0)
      return PR_FALSE;

    index = buf.RFindCharInSet("z?", 6);
    if (index != kNotFound)
      return PR_FALSE;

    index = buf.RFindCharInSet("l", 5);
    if (index != 3)
      return PR_FALSE;

    buf.Assign("abcdefghijkabc");

    index = buf.RFindCharInSet("ab");
    if (index != 12)
      return PR_FALSE;

    index = buf.RFindCharInSet("ab", 11);
    if (index != 11)
      return PR_FALSE;

    index = buf.RFindCharInSet("ab", 10);
    if (index != 1)
      return PR_FALSE;

    index = buf.RFindCharInSet("ab", 0);
    if (index != 0)
      return PR_FALSE;

    index = buf.RFindCharInSet("cd", 1);
    if (index != kNotFound)
      return PR_FALSE;

    index = buf.RFindCharInSet("h");
    if (index != 7)
      return PR_FALSE;

    return PR_TRUE;
  }

//----

typedef PRBool (*TestFunc)();

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
    { "test_xpidl_string", test_xpidl_string },
    { "test_empty_assign", test_empty_assign },
    { "test_set_length", test_set_length },
    { "test_substring", test_substring },
    { "test_appendint64", test_appendint64 },
    { "test_findcharinset", test_findcharinset },
    { "test_rfindcharinset", test_rfindcharinset },
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
            printf("%25s : %s\n", t->name, t->func() ? "SUCCESS" : "FAILURE <--");
          }
      }
    
    return 0;
  }
