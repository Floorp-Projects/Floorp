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
#ifndef AccessingFields_h___
#define AccessingFields_h___

#define IMPLEMENT_GetFieldID_METHOD(class_name, param_name, returnType)\
                         \
        jclass clazz = env->FindClass(class_name); \
	if (!clazz) \
		return TestResult::FAIL("Can't find class !"); \
        jobject obj = env->AllocObject(clazz);     \
	if (!obj) \
		return TestResult::FAIL("Can't allocate object !\n"); \
        jfieldID fieldID = env->GetFieldID(clazz, param_name, returnType ); \
        printf("fieldID = %d\n", (int)fieldID); \

#define IMPLEMENT_GetStaticFieldID_METHOD(class_name, param_name, returnType)\
                         \
        jclass clazz = env->FindClass(class_name); \
        /*jobject obj = env->AllocObject(clazz); */    \
        jfieldID fieldID = env->GetStaticFieldID(clazz, param_name, returnType ); \
        printf("fieldID = %d\n", (int)fieldID); \

#define IMPLEMENT_GetMethodID_METHOD(class_name, func_name, returnType)\
        jclass clazz = env->FindClass(class_name);\
        jobject obj = env->AllocObject(clazz);    \
        jmethodID MethodID = env->GetMethodID(clazz, func_name, returnType);\
        printf("ID of %s method = %d\n", func_name, (int)MethodID); 

#define IMPLEMENT_GetStaticMethodID_METHOD(class_name, func_name, returnType)\
        jclass clazz = env->FindClass(class_name);\
        jobject obj = env->AllocObject(clazz);    \
        jmethodID MethodID = env->GetStaticMethodID(clazz, func_name, returnType);\
        printf("ID of static %s method = %d\n", func_name, (int)MethodID); 


#ifdef XP_UNIX
#define MAX_JLONG jlong_MAXINT  // from jri_md.h
#define MIN_JLONG jlong_MININT
#else
#define MAX_JLONG 9223372036854775807
#define MIN_JLONG -9223372036854775808
#endif

#define MAX_JINT (jint)2147483647
#define MIN_JINT (jint)-2147483648
#define MAX_JDOUBLE (jdouble)1.7976931348623157e308
#define MIN_JDOUBLE (jdouble)4.9e-324
#define MAX_JBYTE (jbyte)255
#define MIN_JBYTE (jbyte)-256
#define MAX_JFLOAT (jfloat)3.4028235e38
#define MIN_JFLOAT (jfloat)1.4e-45
#define MAX_JSHORT (jshort)32767
#define MIN_JSHORT (jshort)-32768

#endif //AccessingFields_h___
