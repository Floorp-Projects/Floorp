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
#include "StringOperations.h"

JNI_OJIAPITest(JNIEnv_GetStringUTFChars_3)
{
  GET_JNI_FOR_TEST

  IMPLEMENT_GetMethodID_METHOD("Test1", "Print_string", "(Ljava/lang/String;)V");
  jstring str = env->NewStringUTF(""); 
  env->CallVoidMethod(obj, MethodID, str);
  jboolean isCopy = TRUE;
  char* str_chars = (char *) env->GetStringUTFChars(str, &isCopy);
  printf("The GetStringUTFChars returned : %s\n\n", str_chars);
  if(str_chars!=NULL){
     return TestResult::PASS("GetStringUTFChars returns correct value (string is empty and isCopy is not NULL)");
  }else{
     return TestResult::FAIL("GetStringUTFChars do not returns correct value (string is empty and isCopy is not NULL)");
  }

}
