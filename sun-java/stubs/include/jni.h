/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef JNI_H
#define JNI_H

#include <stdio.h>
#include <stdarg.h>

#include "jni_md.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char	jboolean;
typedef unsigned short	jchar;
typedef short		jshort;
typedef float		jfloat;
typedef double		jdouble;

typedef jint            jsize;

typedef void *jobject;
typedef jobject jref;
typedef jobject jclass;

typedef void *jbyteArray;
typedef void *jobjectArray;

typedef void JNIEnvInterface;
typedef const JNIEnvInterface *JNIEnv;

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* JNI_H */


