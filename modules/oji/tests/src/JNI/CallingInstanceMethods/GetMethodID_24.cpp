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

JNI_OJIAPITest(JNIEnv_GetMethodID_24)
{
  GET_JNI_FOR_TEST
  jclass clazz = env->FindClass("Test4");
  jmethodID MethodID = env->GetMethodID(clazz, "Test9_abs_nomod_int", "(I)I");
  printf("MethodID = %d", MethodID);
  if(clazz == NULL){
      printf("Class is NULL!!!");
  }
  if(MethodID!=NULL){
    return TestResult::PASS("GetMethodID for no-modifiers abstract method from super-abstract class return correct value");
  }else{
    return TestResult::FAIL("GetMethodID for no-modifiers abstract method from super-abstract class return incorrect value");
  }
}
