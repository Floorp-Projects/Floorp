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

JNI_OJIAPITest(JNIEnv_SetShortArrayRegion_1)
{
  GET_JNI_FOR_TEST

  jshortArray arr = env->NewShortArray(3);
  jsize start = 0;
  jsize leng = 3;
  jshort buf[3];
  buf[0] = MAX_JSHORT;
  buf[1] = MIN_JSHORT;
  buf[2] = 0;
  env->SetShortArrayRegion(arr, start, leng, buf);

  jboolean isCopy = JNI_TRUE;
  jshort *val = env->GetShortArrayElements(arr, NULL);
  jshort val0 = val[0];
  jshort val1 = val[1];
  jshort val2 = val[2];

  if((val0==MAX_JSHORT)&&(val1==MIN_JSHORT)&&(val2==0)){
     return TestResult::PASS("SetShortArrayRegion(all right) returns correct value");
  }else{
     return TestResult::FAIL("SetShortArrayRegion(all right) returns incorrect value");
  }

}
