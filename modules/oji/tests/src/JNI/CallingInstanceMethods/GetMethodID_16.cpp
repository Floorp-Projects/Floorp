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
#include "CallingInstanceMethods.h"

JNI_OJIAPITest(JNIEnv_GetMethodID_16)
{
  GET_JNI_FOR_TEST
  jclass clazz = env->FindClass("Test3");
  if(clazz == NULL){
      printf("Class is NULL!!!");
      return TestResult::FAIL("GetMethodID: class not found");
  }
  jmethodID methodID = env->GetMethodID(clazz, "Test_method", "()V");
  if(methodID != NULL){
     return TestResult::PASS("GetMethodID for public method from interface return correct value (0)");
  }else{
     return TestResult::FAIL("GetMethodID for public method from interface return incorrect value (non-0)");
  }

}
