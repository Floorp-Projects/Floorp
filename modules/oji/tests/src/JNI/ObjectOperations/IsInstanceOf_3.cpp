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
#include "ObjectOperations.h"

JNI_OJIAPITest(JNIEnv_IsInstanceOf_3)
{
  GET_JNI_FOR_TEST

  jclass clazz1 = env->FindClass("Test1");
  jclass clazz2 = env->FindClass("Test2");
  jclass clazz3 = env->FindClass("Test5");
  jobject obj1 = env->AllocObject(clazz1);
  jobject obj2 = env->AllocObject(clazz2);
  

  if(env->IsInstanceOf(obj1, clazz3)){
      return TestResult::FAIL("IsInstanceOf(some obj, isnt superclass or class) return incorrect value");
  }else{
      return TestResult::PASS("IsInstanceOf(some obj, isnt superclass or class) return correct value");
  }


}
