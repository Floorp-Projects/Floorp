/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "JNIEnvTests.h"
#include "ArrayOperations.h"

JNI_OJIAPITest(JNIEnv_GetBooleanArrayElements_2)
{
  GET_JNI_FOR_TEST

  jbooleanArray arr = env->NewBooleanArray(5);
  jsize start = 0;
  jsize leng = 3;
  jboolean buf[3];
  buf[0] = JNI_TRUE;
  buf[1] = JNI_FALSE;
  buf[2] = 0;
  env->SetBooleanArrayRegion(arr, start, leng, buf);

  jboolean isCopy = JNI_TRUE;
  jboolean *val = env->GetBooleanArrayElements(arr, NULL);
  jboolean val0 = val[0];
  jboolean val1 = val[1];
  jboolean val2 = val[2];

  if((val0==JNI_TRUE)&&(val1==JNI_FALSE)&&(val2==0)){
     return TestResult::PASS("GetBooleanArrayElements(arr, NULL) returns correct value");
  }else{
     return TestResult::FAIL("GetBooleanArrayElements(arr, NULL) returns incorrect value");
  }

}
