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

JNI_OJIAPITest(JNIEnv_SetObjectArrayElement_5)
{
  GET_JNI_FOR_TEST
  jclass clazz = env->FindClass("Ljava/lang/String;");
  jclass clazz_exp = env->FindClass("Ljava/lang/ArrayStoreException;");
  jmethodID methodID = env->GetMethodID(clazz, "<init>", "(Ljava/lang/String;)V");
  jchar str_chars[]={'T', 'e', 's', 't'};
  jstring str = env->NewString(str_chars, 4); 
  jobject obj = env->NewObject(clazz, methodID, str);
  jobjectArray arr = env->NewObjectArray(4, clazz, obj);
  jchar str_chars1[] = {'T', 'e', 's', 't', '_', 'n', 'e', 'w'};
  jstring str1 = env->NewString(str_chars1, 8);
  jclass clazz_incor = env->FindClass("Test1");
  jobject obj_incor = env->AllocObject(clazz_incor);
  env->SetObjectArrayElement(arr, (jsize)2, obj_incor);
  jthrowable excep  = env->ExceptionOccurred();
  if(env->IsInstanceOf(excep, clazz_exp)){
    //printf("ArrayIndexOutOfBoundsException is thrown. It is correct!\n");
     return TestResult::PASS("SetObjectArrayElement(incorrect object) returns correct value - empty array");
  }else{
     return TestResult::FAIL("SetObjectArrayElement(incorrect object) returns incorrect value");
  }

}
