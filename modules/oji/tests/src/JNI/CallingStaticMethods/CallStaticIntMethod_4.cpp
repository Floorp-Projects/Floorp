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

JNI_OJIAPITest(JNIEnv_CallStaticIntMethod_4)
{
  GET_JNI_FOR_TEST

  jclass clazz_exp = env->FindClass("Ljava/security/PrivilegedActionException;");
  IMPLEMENT_GetStaticMethodID_METHOD("Test1", "Test1_thrown_excp_static", "(I)I");
  jint value = env->CallStaticIntMethod(clazz, MethodID, 10);
  jthrowable excep = env->ExceptionOccurred();

  if((MethodID != NULL) && (env->IsInstanceOf(excep, clazz_exp))){
     return TestResult::PASS("CallStaticIntMethod for public not inherited method that thrown exception really thrown exception");
  }else{
     return TestResult::FAIL("CallStaticIntMethod for public not inherited method that thrown exception do not really thrown exception");
  }

}

