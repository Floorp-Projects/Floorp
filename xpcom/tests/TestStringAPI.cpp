/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include "nsStringAPI.h"

#define CHECK(x) \
  _doCheck(x, #x, __LINE__)

int _doCheck(bool cond, const char* msg, int line) {
  if (cond) return 0;
  fprintf(stderr, "FAIL: line %d: %s\n", line, msg);
  return 1;
}

int testEmpty() {
  nsString s;
  return CHECK(0 == s.Length()) +
         CHECK(s.IsEmpty());
}

int testAccess() {
  nsString s;
  s.Assign(NS_LITERAL_STRING("hello"));
  int res = CHECK(5 == s.Length()) +
            CHECK(s.EqualsLiteral("hello"));
  const PRUnichar *it, *end;
  int len = s.BeginReading(&it, &end);
  res += CHECK(5 == len);
  res += CHECK(PRUnichar('h') == it[0]) +
         CHECK(PRUnichar('e') == it[1]) +
         CHECK(PRUnichar('l') == it[2]) +
         CHECK(PRUnichar('l') == it[3]) +
         CHECK(PRUnichar('o') == it[4]) +
         CHECK(it + len == end);
  res += CHECK(s[0] == s.First());
  for (int i = 0; i < len; ++i) {
    res += CHECK(s[i] == it[i]);
    res += CHECK(s[i] == s.CharAt(i));
  }
  res += CHECK(it == s.BeginReading());
  res += CHECK(end == s.EndReading());
  return res;
}

int testWrite() {
  nsString s(NS_LITERAL_STRING("xyzz"));
  PRUnichar *begin, *end;
  int res = CHECK(4 == s.Length());
  PRUint32 len = s.BeginWriting(&begin, &end, 5);
  res += CHECK(5 == s.Length()) +
         CHECK(5 == len) +
         CHECK(end == begin + 5) +
         CHECK(begin == s.BeginWriting()) +
         CHECK(end == s.EndWriting());
  begin[4] = PRUnichar('y');
  res += CHECK(s.Equals(NS_LITERAL_STRING("xyzzy")));
  s.SetLength(4);
  res += CHECK(4 == s.Length()) +
         CHECK(s.Equals(NS_LITERAL_STRING("xyzz"))) +
         CHECK(!s.Equals(NS_LITERAL_STRING("xyzzy"))) +
         CHECK(!s.IsEmpty());
  s.Truncate();
  res += CHECK(0 == s.Length()) +
         CHECK(s.IsEmpty());
  const PRUnichar sample[] = { 's', 'a', 'm', 'p', 'l', 'e', '\0' };
  s.Assign(sample);
  res += CHECK(s.EqualsLiteral("sample"));
  s.Assign(sample, 4);
  res += CHECK(s.EqualsLiteral("samp"));
  s.Assign(PRUnichar('q'));
  res += CHECK(s.EqualsLiteral("q"));
  return res;
}

int testFind() {
  nsString str_haystack;
  nsString str_needle;
  str_needle.AssignLiteral("world");

  PRInt32 ret = 0;
  ret += CHECK(-1 == str_haystack.Find("world"));
  ret += CHECK(-1 == str_haystack.Find(str_needle));

  str_haystack.AssignLiteral("hello world hello world hello");
  ret += CHECK( 6 == str_haystack.Find("world"));
  ret += CHECK( 6 == str_haystack.Find(str_needle));
  ret += CHECK(-1 == str_haystack.Find("world", 20, false));
  ret += CHECK(-1 == str_haystack.Find(str_needle, 20));
  ret += CHECK(18 == str_haystack.Find("world", 12, false));
  ret += CHECK(18 == str_haystack.Find(str_needle, 12));

  nsCString cstr_haystack;
  nsCString cstr_needle;
  cstr_needle.AssignLiteral("world");
  
  ret += CHECK(-1 == cstr_haystack.Find("world"));
  ret += CHECK(-1 == cstr_haystack.Find(cstr_needle));

  cstr_haystack.AssignLiteral("hello world hello world hello");
  ret += CHECK( 6 == cstr_haystack.Find("world"));
  ret += CHECK( 6 == cstr_haystack.Find(cstr_needle));
  ret += CHECK(-1 == cstr_haystack.Find(cstr_needle, 20));
  ret += CHECK(18 == cstr_haystack.Find(cstr_needle, 12));
  ret += CHECK( 6 == cstr_haystack.Find("world", 5));

  return ret;
}

int testVoid() {
  nsString s;
  int ret = CHECK(!s.IsVoid());
  s.SetIsVoid(false);
  ret += CHECK(!s.IsVoid());
  s.SetIsVoid(true);
  ret += CHECK(s.IsVoid());
  s.SetIsVoid(false);
  ret += CHECK(!s.IsVoid());
  s.SetIsVoid(true);
  s.AssignLiteral("hello");
  ret += CHECK(!s.IsVoid());
  return ret;
}

int testRFind() {
  PRInt32 ret = 0;

  // nsString.RFind
  nsString str_haystack;
  nsString str_needle;
  str_needle.AssignLiteral("world");

  ret += CHECK(-1 == str_haystack.RFind(str_needle));
  ret += CHECK(-1 == str_haystack.RFind("world"));

  str_haystack.AssignLiteral("hello world hElLo woRlD");

  ret += CHECK( 6 == str_haystack.RFind(str_needle));
  ret += CHECK( 6 == str_haystack.RFind(str_needle, -1));
  ret += CHECK( 6 == str_haystack.RFind(str_needle, 17));
  ret += CHECK( 6 == str_haystack.RFind("world", false));
  ret += CHECK(18 == str_haystack.RFind("world", true));
  ret += CHECK( 6 == str_haystack.RFind("world", -1, false));
  ret += CHECK(18 == str_haystack.RFind("world", -1, true));
  ret += CHECK( 6 == str_haystack.RFind("world", 17, false));
  ret += CHECK( 0 == str_haystack.RFind("hello", 0, false));
  ret += CHECK(18 == str_haystack.RFind("world", 19, true));
  ret += CHECK(18 == str_haystack.RFind("world", 22, true));
  ret += CHECK(18 == str_haystack.RFind("world", 23, true));

  // nsCString.RFind
  nsCString cstr_haystack;
  nsCString cstr_needle;
  cstr_needle.AssignLiteral("world");

  ret += CHECK(-1 == cstr_haystack.RFind(cstr_needle));
  ret += CHECK(-1 == cstr_haystack.RFind("world"));
  
  cstr_haystack.AssignLiteral("hello world hElLo woRlD");

  ret += CHECK( 6 == cstr_haystack.RFind(cstr_needle));
  ret += CHECK( 6 == cstr_haystack.RFind(cstr_needle, -1));
  ret += CHECK( 6 == cstr_haystack.RFind(cstr_needle, 17));
  ret += CHECK( 6 == cstr_haystack.RFind("world", 5));
  ret += CHECK( 0 == cstr_haystack.RFind(nsDependentCString("hello"), 0));

  return ret;
}

int testCompressWhitespace() {
  PRInt32 ret = 0;

  // CompressWhitespace utility function
  nsString s;

  s.AssignLiteral("     ");
  CompressWhitespace(s);
  ret += CHECK(s.EqualsLiteral(""));

  s.AssignLiteral("  no more  leading spaces");
  CompressWhitespace(s);
  ret += CHECK(s.EqualsLiteral("no more leading spaces"));

  s.AssignLiteral("no    more trailing spaces ");
  CompressWhitespace(s);
  ret += CHECK(s.EqualsLiteral("no more trailing spaces"));

  s.AssignLiteral("   hello one    2         three    45        ");
  CompressWhitespace(s);
  ret += CHECK(s.EqualsLiteral("hello one 2 three 45"));

  return ret;
}

int main() {
  int rv = 0;
  rv += testEmpty();
  rv += testAccess();
  rv += testWrite();
  rv += testFind();
  rv += testVoid();
  rv += testRFind();
  rv += testCompressWhitespace();
  if (0 == rv) {
    fprintf(stderr, "PASS: StringAPI tests\n");
  }
  return rv;
}
