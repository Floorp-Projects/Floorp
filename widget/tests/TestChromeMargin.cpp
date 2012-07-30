/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This tests the margin parsing functionality in nsAttrValue.cpp, which
 * is accessible via nsContentUtils, and is used in setting chromemargins
 * to widget windows. It's located here due to linking issues in the
 * content directory.
 */

/* This test no longer compiles now that we've removed nsIContentUtils (bug
 * 647273).  We need to be internal code in order to include nsContentUtils.h,
 * but defining MOZILLA_INTERNAL_API is not enough to make us internal.
 */

#include "TestHarness.h"

#ifndef MOZILLA_INTERNAL_API
// some of the includes make use of internal string types
#define nsAString_h___
#define nsString_h___
#define nsStringFwd_h___
#define nsReadableUtils_h___
class nsACString;
class nsAString;
class nsAFlatString;
class nsAFlatCString;
class nsAdoptingString;
class nsAdoptingCString;
class nsXPIDLString;
template<class T> class nsReadingIterator;
#endif

#include "nscore.h"
#include "nsContentUtils.h"

#ifndef MOZILLA_INTERNAL_API
#undef nsString_h___
#undef nsAString_h___
#undef nsReadableUtils_h___
#endif

struct DATA {
  bool shouldfail;
  const char* margins;
  int top;
  int right;
  int bottom;
  int left;
};

const bool SHOULD_FAIL = true;
const int SHOULD_PASS = false;

const DATA Data[] = {
  { SHOULD_FAIL, "", 1, 2, 3, 4 },
  { SHOULD_FAIL, "1,0,0,0", 1, 2, 3, 4 },
  { SHOULD_FAIL, "1,2,0,0", 1, 2, 3, 4 },
  { SHOULD_FAIL, "1,2,3,0", 1, 2, 3, 4 },
  { SHOULD_FAIL, "4,3,2,1", 1, 2, 3, 4 },
  { SHOULD_FAIL, "azsasdasd", 0, 0, 0, 0 },
  { SHOULD_FAIL, ",azsasdasd", 0, 0, 0, 0 },
  { SHOULD_FAIL, "           ", 1, 2, 3, 4 },
  { SHOULD_FAIL, "azsdfsdfsdfsdfsdfsasdasd,asdasdasdasdasdasd,asdadasdasd,asdasdasdasd", 0, 0, 0, 0 },
  { SHOULD_FAIL, "as,as,as,as", 0, 0, 0, 0 },
  { SHOULD_FAIL, "0,0,0", 0, 0, 0, 0 },
  { SHOULD_FAIL, "0,0", 0, 0, 0, 0 },
  { SHOULD_FAIL, "4.6,1,1,1", 0, 0, 0, 0 },
  { SHOULD_FAIL, ",,,,", 0, 0, 0, 0 },
  { SHOULD_FAIL, "1, , , ,", 0, 0, 0, 0 },
  { SHOULD_FAIL, "1, , ,", 0, 0, 0, 0 },
  { SHOULD_FAIL, "@!@%^&^*()", 1, 2, 3, 4 },
  { SHOULD_PASS, "4,3,2,1", 4, 3, 2, 1 },
  { SHOULD_PASS, "-4,-3,-2,-1", -4, -3, -2, -1 },
  { SHOULD_PASS, "10000,3,2,1", 10000, 3, 2, 1 },
  { SHOULD_PASS, "4  , 3   , 2 , 1", 4, 3, 2, 1 },
  { SHOULD_PASS, "4,    3   ,2,1", 4, 3, 2, 1 },
  { SHOULD_FAIL, "4,3,2,10000000000000 --", 4, 3, 2, 10000000000000 },
  { SHOULD_PASS, "4,3,2,1000", 4, 3, 2, 1000 },
  { SHOULD_PASS, "2147483647,3,2,1000", 2147483647, 3, 2, 1000 },
  { SHOULD_PASS, "2147483647,2147483647,2147483647,2147483647", 2147483647, 2147483647, 2147483647, 2147483647 },
  { SHOULD_PASS, "-2147483647,3,2,1000", -2147483647, 3, 2, 1000 },
  { SHOULD_FAIL, "2147483648,3,2,1000", 1, 3, 2, 1000 },
  { 0, nullptr, 0, 0, 0, 0 }
};

void DoAttrValueTest()
{
  int idx = -1;
  bool didFail = false;
  while (Data[++idx].margins) {
    nsAutoString str;
    str.AssignLiteral(Data[idx].margins);
    nsIntMargin values(99,99,99,99);
    bool result = nsContentUtils::ParseIntMarginValue(str, values);

    // if the parse fails
    if (!result) {
      if (Data[idx].shouldfail)
        continue;
      fail(Data[idx].margins);
      didFail = true;
      printf("*1\n");
      continue;
    }

    if (Data[idx].shouldfail) {
      if (Data[idx].top == values.top &&
          Data[idx].right == values.right &&
          Data[idx].bottom == values.bottom &&
          Data[idx].left == values.left) {
        // not likely
        fail(Data[idx].margins);
        didFail = true;
        printf("*2\n");
        continue;
      }
      // good failure, parse failed and that's what we expected.
      continue;
    }
#if 0
    printf("%d==%d %d==%d %d==%d %d==%d\n",
      Data[idx].top, values.top,
      Data[idx].right, values.right,
      Data[idx].bottom, values.bottom,
      Data[idx].left, values.left);
#endif
    if (Data[idx].top == values.top &&
        Data[idx].right == values.right &&
        Data[idx].bottom == values.bottom &&
        Data[idx].left == values.left) {
      // good parse results
      continue;
    }
    else {
      fail(Data[idx].margins);
      didFail = true;
      printf("*3\n");
      continue;
    }
  }

  if (!didFail)
    passed("nsAttrValue margin parsing tests passed.");
}

int main(int argc, char** argv)
{
  ScopedXPCOM xpcom("");
  if (xpcom.failed())
    return 1;
  DoAttrValueTest();
  return 0;
}
