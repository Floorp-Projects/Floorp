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

JNI_OJIAPITest(JNIEnv_NewObjectArray_5)
{
  GET_JNI_FOR_TEST
  jclass clazz = env->FindClass("Ljava/lang/String;");
  jclass clazz_incor = env->FindClass("Test1");
  jmethodID methodID = env->GetMethodID(clazz_incor, "<init>", "(Ljava/lang/String;)V");
  jobject obj_incor = env->NewObject(clazz_incor, methodID, NULL);
  jobject obj = env->AllocObject(clazz);

  jarray arr = env->NewObjectArray(5, clazz, obj_incor);

  if(arr != NULL){ //NULL is returned only if OutOfMemoryException is thrown
     return TestResult::PASS("NewObjectArray(obj is incorrect) returns correct value");
  }else{
     return TestResult::FAIL("NewObjectArray(obj is incorrect) returns incorrect value");
  }

}
