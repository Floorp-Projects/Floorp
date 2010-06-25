/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jim Mathies <jmathies@mozilla.com>
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

/* This tests the margin parsing functionality in nsAttrValue.cpp, which
 * is accessible via nsIContentUtils, and is used in setting chromemargins
 * to widget windows. It's located here due to linking issues in the
 * content directory.
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
#include "nsIContentUtils.h"

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
  { 0, nsnull, 0, 0, 0, 0 }
};

void DoAttrValueTest()
{
  nsCOMPtr<nsIContentUtils> utils =
   do_GetService("@mozilla.org/content/contentutils;1");

  if (!utils)
    fail("No nsIContentUtils");

  int idx = -1;
  bool didFail = false;
  while (Data[++idx].margins) {
    nsAutoString str;
    str.AssignLiteral(Data[idx].margins);
    nsIntMargin values(99,99,99,99);
    bool result = utils->ParseIntMarginValue(str, values);

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
