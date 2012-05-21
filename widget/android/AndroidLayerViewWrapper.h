/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AndroidLayerViewWrapper_h__
#define AndroidLayerViewWrapper_h__

#include <jni.h>
#include <pthread.h>

class AndroidEGLObject {
public:
    static void Init(JNIEnv* aJEnv);
};

typedef void* EGLSurface;

class AndroidGLController {
public:
    static void Init(JNIEnv* aJEnv);

    void Acquire(JNIEnv* aJEnv, jobject aJObj);
    void SetGLVersion(int aVersion);
    EGLSurface ProvideEGLSurface();
    void WaitForValidSurface();

private:
    static jmethodID jSetGLVersionMethod;
    static jmethodID jWaitForValidSurfaceMethod;
    static jmethodID jProvideEGLSurfaceMethod;

    // the JNIEnv for the compositor thread
    JNIEnv* mJEnv;
    pthread_t mThread;
    jobject mJObj;
};

#endif

