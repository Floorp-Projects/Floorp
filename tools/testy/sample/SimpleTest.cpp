/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "nspr.h"
#include "TestySupport.h"

PR_BEGIN_EXTERN_C
PR_EXPORT(int) Testy_Init();
PR_EXPORT(int) Testy_RunTest();
PR_EXPORT(int) Testy_Shutdown();
PR_END_EXTERN_C


int Testy_Init() 
{
  return 0;
}


int Testy_RunTest() 
{
  const char* testCaseName1 = "Simple Test 1";

  Testy_LogStart(testCaseName1);
  Testy_LogComment(testCaseName1, "Comment 1");
  Testy_LogEnd(testCaseName1, true);

  const char* testCaseName2 = "Simple Test 2";

  Testy_LogStart(testCaseName2);
  Testy_LogComment(testCaseName2, "Comment 2");
  Testy_LogEnd(testCaseName2, false);

  return 0;
}

int Testy_Shutdown() 
{
  return 0;
}

