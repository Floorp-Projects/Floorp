/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AndroidJNIWrapper_h__
#define AndroidJNIWrapper_h__

#include "mozilla/Types.h"
#include <jni.h>
#include <android/log.h>

extern "C" MOZ_EXPORT jclass jsjni_FindClass(const char *className);

/**
 * JNIEnv::FindClass alternative.
 * Callable from any thread, including code
 * invoked via the JNI that doesn't have MOZILLA_INTERNAL_API defined.
 * The caller is responsible for ensuring that the class is not leaked by
 * calling DeleteGlobalRef at an appropriate time.
 */
extern "C" MOZ_EXPORT jclass jsjni_GetGlobalClassRef(const char *className);

extern "C" MOZ_EXPORT jmethodID jsjni_GetStaticMethodID(jclass methodClass,
                                       const char *methodName,
                                       const char *signature);
extern "C" MOZ_EXPORT bool jsjni_ExceptionCheck();
extern "C" MOZ_EXPORT void jsjni_CallStaticVoidMethodA(jclass cls, jmethodID method, jvalue *values);
extern "C" MOZ_EXPORT int jsjni_CallStaticIntMethodA(jclass cls, jmethodID method, jvalue *values);
extern "C" MOZ_EXPORT jobject jsjni_GetGlobalContextRef();
extern "C" MOZ_EXPORT JavaVM* jsjni_GetVM();
extern "C" MOZ_EXPORT JNIEnv* jsjni_GetJNIForThread();

#endif /* AndroidJNIWrapper_h__ */
