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

JNI_OJIAPITest(JNIEnv_ReleaseFloatArrayElements_2)
{
  GET_JNI_FOR_TEST

  jfloatArray arr = env->NewFloatArray(3);
  jsize start = 0;
  jsize leng = 3;
  jfloat buf[3];
  buf[0] = MAX_JFLOAT;
  buf[1] = MIN_JFLOAT;
  buf[2] = 10;
  env->SetFloatArrayRegion(arr, start, leng, buf);

  jboolean isCopy = JNI_TRUE;
  jfloat *val = env->GetFloatArrayElements(arr, &isCopy);
  jfloat val0 = val[0];
  jfloat val1 = val[1];
  jfloat val2 = val[2];
  val[0] = 0;
  val[1] = 1;
  val[2] = 2;
  printf("val0 = %d and val1 = %d and val2 = %d", val[0], val[1], val[2]);
  env->ReleaseFloatArrayElements(arr, val, JNI_ABORT);
  jfloat *valu = env->GetFloatArrayElements(arr, &isCopy);
  printf("\n\nvalu[0] = %d and valu[1] = %d and valu[2] = %d", valu[0], valu[1], valu[2]);
  if((valu[0]==MAX_JFLOAT) && (valu[1]==MIN_JFLOAT) && (valu[2]==10)){
     return TestResult::PASS("ReleaseFloatArrayElements(arr, val, JNI_ABORT) is correct");
  }else{
     return TestResult::FAIL("ReleaseFloatArrayElements(arr, val, JNI_ABORT) is incorrect");
  }
}