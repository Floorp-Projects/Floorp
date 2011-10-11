/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=4:et:sw=4:
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Boris Zbarsky <bzbarsky@mit.edu> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "TestHarness.h"
#include "nsTObserverArray.h"

using namespace mozilla;

typedef nsTObserverArray<int> Array;

#define DO_TEST(_type, _exp, _code)                                   \
  do {                                                                \
    ++testNum;                                                        \
    count = 0;                                                        \
    Array::_type iter(arr);                                           \
    while (iter.HasMore() && count != ArrayLength(_exp)) {            \
      _code                                                           \
      int next = iter.GetNext();                                      \
      int expected = _exp[count++];                                   \
      if (next != expected) {                                         \
        fail("During test %d at position %d got %d expected %d\n",    \
             testNum, count-1, next, expected);                       \
        rv = 1;                                                       \
      }                                                               \
    }                                                                 \
    if (iter.HasMore()) {                                             \
      fail("During test %d, iterator ran over", testNum);             \
      rv = 1;                                                         \
    }                                                                 \
    if (count != ArrayLength(_exp)) {                             \
      fail("During test %d, iterator finished too early", testNum);   \
      rv = 1;                                                         \
    }                                                                 \
  } while (0)

int main(int argc, char **argv)
{
  ScopedXPCOM xpcom("nsTObserverArrayTests");
  if (xpcom.failed()) {
    return 1;
  }

  int rv = 0;

  Array arr;
  arr.AppendElement(3);
  arr.AppendElement(4);

  int count;
  int testNum = 0;

  // Basic sanity
  static int test1Expected[] = { 3, 4 };
  DO_TEST(ForwardIterator, test1Expected, );

  // Appends
  static int test2Expected[] = { 3, 4, 2 };
  DO_TEST(ForwardIterator, test2Expected,
          if (count == 1) arr.AppendElement(2);
          );
  DO_TEST(ForwardIterator, test2Expected, );

  DO_TEST(EndLimitedIterator, test2Expected,
          if (count == 1) arr.AppendElement(5);
          );

  static int test5Expected[] = { 3, 4, 2, 5 };
  DO_TEST(ForwardIterator, test5Expected, );

  // Removals
  DO_TEST(ForwardIterator, test5Expected,
          if (count == 1) arr.RemoveElementAt(0);
          );

  static int test7Expected[] = { 4, 2, 5 };
  DO_TEST(ForwardIterator, test7Expected, );

  static int test8Expected[] = { 4, 5 };
  DO_TEST(ForwardIterator, test8Expected,
          if (count == 1) arr.RemoveElementAt(1);
          );
  DO_TEST(ForwardIterator, test8Expected, );

  arr.AppendElement(2);
  arr.AppendElementUnlessExists(6);
  static int test10Expected[] = { 4, 5, 2, 6 };
  DO_TEST(ForwardIterator, test10Expected, );

  arr.AppendElementUnlessExists(5);
  DO_TEST(ForwardIterator, test10Expected, );

  static int test12Expected[] = { 4, 5, 6 };
  DO_TEST(ForwardIterator, test12Expected,
          if (count == 1) arr.RemoveElementAt(2);
          );
  DO_TEST(ForwardIterator, test12Expected, );

  // Removals + Appends
  static int test14Expected[] = { 4, 6, 7 };
  DO_TEST(ForwardIterator, test14Expected,
          if (count == 1) {
            arr.RemoveElementAt(1);
            arr.AppendElement(7);
          }
          );
  DO_TEST(ForwardIterator, test14Expected, );

  arr.AppendElement(2);
  static int test16Expected[] = { 4, 6, 7, 2 };
  DO_TEST(ForwardIterator, test16Expected, );

  static int test17Expected[] = { 4, 7, 2 };
  DO_TEST(EndLimitedIterator, test17Expected,
          if (count == 1) {
            arr.RemoveElementAt(1);
            arr.AppendElement(8);
          }
          );

  static int test18Expected[] = { 4, 7, 2, 8 };
  DO_TEST(ForwardIterator, test18Expected, );

  // Prepends
  arr.PrependElementUnlessExists(3);
  static int test19Expected[] = { 3, 4, 7, 2, 8 };
  DO_TEST(ForwardIterator, test19Expected, );

  arr.PrependElementUnlessExists(7);
  DO_TEST(ForwardIterator, test19Expected, );

  // Commented out because it fails; bug 474369 will fix
  /*  DO_TEST(ForwardIterator, test19Expected,
          if (count == 1) {
            arr.PrependElementUnlessExists(9);
          }
          );

  static int test22Expected[] = { 9, 3, 4, 7, 2, 8 };
  DO_TEST(ForwardIterator, test22Expected, );
  */
  
  return rv;
}
