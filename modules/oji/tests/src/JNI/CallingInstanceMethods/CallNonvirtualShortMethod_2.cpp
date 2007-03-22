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

JNI_OJIAPITest(JNIEnv_CallNonvirtualShortMethod_2)
{
  GET_JNI_FOR_TEST

  IMPLEMENT_GetMethodID_METHOD("Test1", "Test1_method_short", "(ZBCSIJFDLjava/lang/String;[Ljava/lang/String;)S");
  char *path = "asdf";
  jstring jpath=env->NewStringUTF("sdsadasdasd");
  jvalue *args  = new jvalue[10];
  args[0].z = JNI_FALSE;
  args[1].b = MIN_JBYTE;
  args[2].c = 'a';
  args[3].s = MIN_JSHORT;
  args[4].i = 123;
  args[5].j = 0;
  args[6].f = 0;
  args[7].d = 100;
  args[8].l = jpath;
  args[9].l = NULL;
  jshort value = env->CallNonvirtualShortMethodA(obj, env->GetSuperclass(clazz), MethodID, args);
  if(value == MIN_JSHORT){
     return TestResult::PASS("CallNonvirtualShortMethodA for public not inherited method (sig = (ZBCSIJFDLjava/lang/String;[Ljava/lang/String;)S) return correct value");
  }else{
     return TestResult::FAIL("CallNonvirtualShortMethodA for public not inherited method (sig = (ZBCSIJFDLjava/lang/String;[Ljava/lang/String;)S) return incorrect value");
  }

}

