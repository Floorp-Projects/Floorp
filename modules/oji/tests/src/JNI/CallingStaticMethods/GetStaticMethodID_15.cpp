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
#include "CallingStaticMethods.h"
#include "Test1.h"

JNI_OJIAPITest(JNIEnv_GetStaticMethodID_15)
{
  GET_JNI_FOR_TEST
  IMPLEMENT_GetStaticMethodID_METHOD("Test11", "mprint_static", "(I)V");
  if(clazz == NULL){
      printf("Class is NULL!!!");
  }
  env->CallStaticVoidMethod(clazz, MethodID, 100);
  if(MethodID != NULL){
     return TestResult::PASS("GetStaticMethodID for public native method from non-abstract class, not inherited return correct value");
  }else{
     return TestResult::FAIL("GetStaticMethodID for public native method from non-abstract class, not inherited return incorrect value");
  }
}

#if defined(__cplusplus)
extern "C" 
#endif
JNIEXPORT void JNICALL Java_Test11_mprint_1static
  (JNIEnv * env, jclass clazz, jint jInt){

  jclass clazz1 = env->FindClass("Test11");
  if(clazz1==NULL) {
      printf("There is no such class\n ");
  }
  jmethodID JPrintMethodID = env->GetStaticMethodID(clazz1, "jprint_static", "(I)V");
  printf("ID of get method = %d\n",(int)JPrintMethodID);
  env->CallStaticVoidMethod(clazz1, JPrintMethodID, (int)jInt);

    printf("OK!!!\n");
}
