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
#include "AccessingFields.h"

JNI_OJIAPITest(JNIEnv_SetObjectField_5)
{
  GET_JNI_FOR_TEST

  IMPLEMENT_GetFieldID_METHOD("Test1","name_int_arr", "[I");
  jintArray arr = env->NewIntArray(3);
  jsize start = 0;
  jsize leng = 3;
  jint buf[3];
  buf[0] = 1;
  buf[1] = 2;
  buf[2] = 3;
  env->SetIntArrayRegion(arr, start, leng, buf);
  env->SetObjectField(obj, fieldID, NULL);
  jintArray value = (jintArray)env->GetObjectField(obj, fieldID);
  if(value==NULL){
      return TestResult::PASS("SetObjectField(object == NULL) passed");
  }else{
      return TestResult::FAIL("SetObjectField(object == NULL) failed");
  }

}
