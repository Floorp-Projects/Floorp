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
 * The Original Code is XPCOM external strings test.
 *
 * The Initial Developer of the Original Code is
 * Mook <mook.moz@gmail.com>.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

int testVoid() {
  nsString s;
  int ret = CHECK(!s.IsVoid());
  s.SetIsVoid(PR_FALSE);
  ret += CHECK(!s.IsVoid());
  s.SetIsVoid(PR_TRUE);
  ret += CHECK(s.IsVoid());
  s.SetIsVoid(PR_FALSE);
  ret += CHECK(!s.IsVoid());
  s.SetIsVoid(PR_TRUE);
  s.AssignLiteral("hello");
  ret += CHECK(!s.IsVoid());
  return ret;
}

int main() {
  int rv = 0;
  rv += testEmpty();
  rv += testAccess();
  rv += testWrite();
  rv += testVoid();
  if (0 == rv) {
    fprintf(stderr, "PASS: StringAPI tests\n");
  }
  return rv;
}
