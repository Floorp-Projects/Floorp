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

JNI_OJIAPITest(JNIEnv_CallStaticIntMethod_2)
{
  GET_JNI_FOR_TEST

  IMPLEMENT_GetStaticMethodID_METHOD("Test1", "Get_static_int_field", "()I");
  jfieldID fieldID = env->GetStaticFieldID(clazz, "static_name_int", "I");
  env->SetStaticIntField(clazz, fieldID, 11);
  jvalue *args = new jvalue[1];
  args[0].i = 10;

  jint value = env->CallStaticIntMethodA(clazz, MethodID, args);
  printf("The static int field in clazz is : %d\n", (int)value);
  if((MethodID!=NULL) && (value == 11)){
      return TestResult::PASS("CallStaticIntMethodA with sig = \"()I\" return correct value");
  }else{
      return TestResult::FAIL("CallStaticIntMethodA with sig = \"()I\" return incorrect value");
  }

}
