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

JNI_OJIAPITest(JNIEnv_CallLongMethod_2)
{
  GET_JNI_FOR_TEST

  IMPLEMENT_GetMethodID_METHOD("Test1", "Test1_method_long", "(ZBCSIJFDLjava/lang/String;[Ljava/lang/String;)J");
  char *path = "asdf";
  jstring jpath=env->NewStringUTF("sdsadasdasd");
  jvalue *args  = new jvalue[10];
  args[0].z = JNI_FALSE;
  args[1].b = MIN_JBYTE;
  args[2].c = 'a';
  args[3].s = MAX_JSHORT;
  args[4].i = 123;
  args[5].j = MIN_JLONG;
  args[6].f = 0;
  args[7].d = 100;
  args[8].l = jpath;
  args[9].l = NULL;
  jlong value = env->CallLongMethodA(obj, MethodID, args);
  if(value == MIN_JLONG){
     return TestResult::PASS("CallLongMethodA for public not inherited method (sig = (ZBCSIJFDLjava/lang/String;[Ljava/lang/String;)J) return correct value");
  }else{
     return TestResult::FAIL("CallLongMethodA for public not inherited method (sig = (ZBCSIJFDLjava/lang/String;[Ljava/lang/String;)J) return incorrect value");
  }

}

