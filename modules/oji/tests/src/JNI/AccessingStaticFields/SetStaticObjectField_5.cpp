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
#include "AccessingStaticFields.h"

JNI_OJIAPITest(JNIEnv_SetStaticObjectField_5)
{
  GET_JNI_FOR_TEST

  IMPLEMENT_GetStaticFieldID_METHOD("Test1", "static_name_string", "Ljava/lang/String;");
  jchar str_chars1[]={'a', 's', 'd', 'f'};

  jstring str = env->NewString(str_chars1, 4); 
  env->SetStaticObjectField(clazz, fieldID, str);
  jstring value = (jstring)env->GetStaticObjectField(clazz, fieldID);

  jboolean isCopy = TRUE;
  char* str_chars = (char *) env->GetStringUTFChars(value, &isCopy);
  printf("The GetStringUTFChars returned : %s\n\n", str_chars);

  jsize len = env->GetStringLength(value);
  if((str_chars != NULL) && ((int)len == 4)){
     printf("Value is correct!!\n\nIt's correct!\n");
     return TestResult::PASS("SetStaticObjectField(all right) return correct value");
  }else{
     return TestResult::FAIL("SetStaticObjectField(all right) return incorrect value!!!!");
  }

}
