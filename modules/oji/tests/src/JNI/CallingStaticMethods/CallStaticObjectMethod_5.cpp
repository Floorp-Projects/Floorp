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

JNI_OJIAPITest(JNIEnv_CallStaticObjectMethod_5)
{
  GET_JNI_FOR_TEST

  IMPLEMENT_GetStaticMethodID_METHOD("Test1", "Test1_method5_static", "(ZBCSIJFDLjava/lang/String;[Ljava/lang/String;)[Ljava/lang/String;");
  char *path = "asdf";
  jstring jpath=env->NewStringUTF("sdsadasdasd");
  jclass clazz_arr = env->FindClass("Ljava/lang/String;");
  jmethodID methodID_obj = env->GetMethodID(clazz_arr, "<init>", "(Ljava/lang/String;)V");
  jchar str_chars[]={'T', 'e', 's', 't'};
  jstring str = env->NewString(str_chars, 4); 
  jobject obj_arr = env->NewObject(clazz_arr, methodID_obj, str);
  jobjectArray arr = env->NewObjectArray(4, clazz_arr, obj_arr);
  jvalue *args  = new jvalue[10];
  args[0].z = JNI_FALSE;
  args[1].b = MIN_JBYTE;
  args[2].c = 'a';
  args[3].s = MAX_JSHORT;
  args[4].i = 123;
  args[5].j = 0;
  args[6].f = 0;
  args[7].d = 100;
  args[8].l = jpath;
  args[9].l = arr;
  jobjectArray value = (jobjectArray)env->CallStaticObjectMethodA(clazz, MethodID, args);
  jstring str_ret = (jstring)env->GetObjectArrayElement(value, (jsize) 1);
  char* str_chars_ret = (char *) env->GetStringUTFChars(str, NULL);
  jsize len_ret = env->GetArrayLength(value);
  if((len_ret == 4) && (strcmp(str_chars_ret, "Test") == 0)){
     return TestResult::PASS("CallObjectMethodA for public not inherited method (sig = (ZBCSIJFDLjava/lang/String;[Ljava/lang/String;)[Ljava/lang/String;) return correct value");
  }else{
     return TestResult::FAIL("CallObjectMethodA for public not inherited method (sig = (ZBCSIJFDLjava/lang/String;[Ljava/lang/String;)[Ljava/lang/String;) return incorrect value");
  }

}

