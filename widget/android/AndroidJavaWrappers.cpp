/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AndroidJavaWrappers.h"

using namespace mozilla;

nsJNIString::nsJNIString(jstring jstr, JNIEnv *jenv)
{
    if (!jstr) {
        SetIsVoid(true);
        return;
    }
    JNIEnv *jni = jenv;
    if (!jni) {
        jni = jni::GetGeckoThreadEnv();
    }
    const jchar* jCharPtr = jni->GetStringChars(jstr, nullptr);

    if (!jCharPtr) {
        SetIsVoid(true);
        return;
    }

    jsize len = jni->GetStringLength(jstr);

    if (len <= 0) {
        SetIsVoid(true);
    } else {
        Assign(reinterpret_cast<const char16_t*>(jCharPtr), len);
    }
    jni->ReleaseStringChars(jstr, jCharPtr);
}

nsJNICString::nsJNICString(jstring jstr, JNIEnv *jenv)
{
    if (!jstr) {
        SetIsVoid(true);
        return;
    }
    JNIEnv *jni = jenv;
    if (!jni) {
        jni = jni::GetGeckoThreadEnv();
    }
    const char* jCharPtr = jni->GetStringUTFChars(jstr, nullptr);

    if (!jCharPtr) {
        SetIsVoid(true);
        return;
    }

    jsize len = jni->GetStringUTFLength(jstr);

    if (len <= 0) {
        SetIsVoid(true);
    } else {
        Assign(jCharPtr, len);
    }
    jni->ReleaseStringUTFChars(jstr, jCharPtr);
}
