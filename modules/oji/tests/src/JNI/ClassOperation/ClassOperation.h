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

#define IMPLEMENT_GetFieldID_METHOD(class_name, param_name, returnType)\
                         \
        jclass clazz = env->FindClass(class_name); \
        /*jobject obj = env->AllocObject(clazz); */    \
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
        jmethodID MethodID = env->GetMethodID(obj, func_name, returnType);\
        printf("ID of %s method = %d\n", func_name, (int)MethodID); \

