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

JNI_OJIAPITest(JNIEnv_ReleaseCharArrayElements_1)
{
  GET_JNI_FOR_TEST

  jcharArray arr = env->NewCharArray(3);
  jsize start = 0;
  jsize leng = 3;
  jchar buf[3];
  buf[0] = 'a';
  buf[1] = 'z';
  buf[2] = '1';
  env->SetCharArrayRegion(arr, start, leng, buf);

  jboolean isCopy = JNI_TRUE;
  jchar *val = env->GetCharArrayElements(arr, &isCopy);
  jchar val0 = val[0];
  jchar val1 = val[1];
  jchar val2 = val[2];
  val[0] = 'q';
  val[1] = 'w';
  val[2] = 'e';
  printf("val0 = %c and val1 = %c and val2 = %c", val[0], val[1], val[2]);
  env->ReleaseCharArrayElements(arr, val, 0);
  jchar *valu = env->GetCharArrayElements(arr, &isCopy);
  printf("\n\nvalu[0] = %c and valu[1] = %c and valu[2] = %c", valu[0], valu[1], valu[2]);
  if((valu[0]=='q') && (valu[1]=='w') && (valu[2]=='e')){
     return TestResult::PASS("ReleaseCharArrayElements(arr, val, 0) is correct");
  }else{
     return TestResult::FAIL("ReleaseCharArrayElements(arr, val, 0) is incorrect");
  }
}