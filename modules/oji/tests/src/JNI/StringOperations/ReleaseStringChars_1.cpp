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

JNI_OJIAPITest(JNIEnv_ReleaseStringChars_1)
{
  GET_JNI_FOR_TEST

  IMPLEMENT_GetMethodID_METHOD("Test1", "Print_string", "(Ljava/lang/String;)V");
  jchar str_chars[]={'a', 's', 'd', 'f'};

  jstring str = env->NewString(str_chars, 4); 

  jboolean *isCopy;
  const jchar* chars = env->GetStringChars(str, isCopy);
  env->ReleaseStringChars(NULL, NULL);
  return TestResult::PASS("ReleaseStringChars(NULL, NULL) correct");
  

}
