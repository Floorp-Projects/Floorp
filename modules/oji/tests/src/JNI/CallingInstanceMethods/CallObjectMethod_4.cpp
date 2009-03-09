/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "JNIEnvTests.h"
#include "CallingInstanceMethods.h"

JNI_OJIAPITest(JNIEnv_CallObjectMethod_4)
{
  GET_JNI_FOR_TEST

  IMPLEMENT_GetMethodID_METHOD("Test1", "Test1_method_string", "(ZBCSIJFDLjava/lang/String;[Ljava/lang/String;)Ljava/lang/String;");
  char *path = "asdf";
  jstring jpath=env->NewStringUTF("sdsadasdasd");
  jstring value = (jstring)env->CallObjectMethod(obj, MethodID, (jboolean)JNI_TRUE, (jbyte)MIN_JBYTE, (jchar)0, (jshort)1, (jint)123, (jlong)20, (jfloat)10., (jdouble)100, (jobject)NULL, (jobject)NULL);
  if(value == NULL){
     return TestResult::PASS("CallObjectMethod for public not inherited method (sig = (ZBCSIJFDLjava/lang/String;[Ljava/lang/String;)Ljava/lang/String;) return correct value");
  }else{
     return TestResult::FAIL("CallObjectMethod for public not inherited method (sig = (ZBCSIJFDLjava/lang/String;[Ljava/lang/String;)Ljava/lang/String;) return incorrect value");
  }

}

