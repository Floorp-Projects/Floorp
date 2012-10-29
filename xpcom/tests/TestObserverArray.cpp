/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=4:et:sw=4:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

  size_t count;
  int testNum = 0;

  // Basic sanity
  static int test1Expected[] = { 3, 4 };
  DO_TEST(ForwardIterator, test1Expected, { /* nothing */ });

  // Appends
  static int test2Expected[] = { 3, 4, 2 };
  DO_TEST(ForwardIterator, test2Expected,
          if (count == 1) arr.AppendElement(2);
          );
  DO_TEST(ForwardIterator, test2Expected, { /* nothing */ });

  DO_TEST(EndLimitedIterator, test2Expected,
          if (count == 1) arr.AppendElement(5);
          );

  static int test5Expected[] = { 3, 4, 2, 5 };
  DO_TEST(ForwardIterator, test5Expected, { /* nothing */ });

  // Removals
  DO_TEST(ForwardIterator, test5Expected,
          if (count == 1) arr.RemoveElementAt(0);
          );

  static int test7Expected[] = { 4, 2, 5 };
  DO_TEST(ForwardIterator, test7Expected, { /* nothing */ });

  static int test8Expected[] = { 4, 5 };
  DO_TEST(ForwardIterator, test8Expected,
          if (count == 1) arr.RemoveElementAt(1);
          );
  DO_TEST(ForwardIterator, test8Expected, { /* nothing */ });

  arr.AppendElement(2);
  arr.AppendElementUnlessExists(6);
  static int test10Expected[] = { 4, 5, 2, 6 };
  DO_TEST(ForwardIterator, test10Expected, { /* nothing */ });

  arr.AppendElementUnlessExists(5);
  DO_TEST(ForwardIterator, test10Expected, { /* nothing */ });

  static int test12Expected[] = { 4, 5, 6 };
  DO_TEST(ForwardIterator, test12Expected,
          if (count == 1) arr.RemoveElementAt(2);
          );
  DO_TEST(ForwardIterator, test12Expected, { /* nothing */ });

  // Removals + Appends
  static int test14Expected[] = { 4, 6, 7 };
  DO_TEST(ForwardIterator, test14Expected,
          if (count == 1) {
            arr.RemoveElementAt(1);
            arr.AppendElement(7);
          }
          );
  DO_TEST(ForwardIterator, test14Expected, { /* nothing */ });

  arr.AppendElement(2);
  static int test16Expected[] = { 4, 6, 7, 2 };
  DO_TEST(ForwardIterator, test16Expected, { /* nothing */ });

  static int test17Expected[] = { 4, 7, 2 };
  DO_TEST(EndLimitedIterator, test17Expected,
          if (count == 1) {
            arr.RemoveElementAt(1);
            arr.AppendElement(8);
          }
          );

  static int test18Expected[] = { 4, 7, 2, 8 };
  DO_TEST(ForwardIterator, test18Expected, { /* nothing */ });

  // Prepends
  arr.PrependElementUnlessExists(3);
  static int test19Expected[] = { 3, 4, 7, 2, 8 };
  DO_TEST(ForwardIterator, test19Expected, { /* nothing */ });

  arr.PrependElementUnlessExists(7);
  DO_TEST(ForwardIterator, test19Expected, { /* nothing */ });

  // Commented out because it fails; bug 474369 will fix
  /*  DO_TEST(ForwardIterator, test19Expected,
          if (count == 1) {
            arr.PrependElementUnlessExists(9);
          }
          );

  static int test22Expected[] = { 9, 3, 4, 7, 2, 8 };
  DO_TEST(ForwardIterator, test22Expected, { });
  */
  
  return rv;
}
