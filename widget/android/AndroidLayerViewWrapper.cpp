/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AndroidLayerViewWrapper.h"
#include "AndroidBridge.h"
#include "nsDebug.h"

using namespace mozilla;

#define ASSERT_THREAD() \
        NS_ASSERTION(pthread_self() == mThread, "Something is calling AndroidGLController from the wrong thread!")

static jfieldID jEGLSurfacePointerField = 0;

void AndroidEGLObject::Init(JNIEnv* aJEnv) {
    jclass jClass;
    jClass = reinterpret_cast<jclass>
        (aJEnv->NewGlobalRef(aJEnv->FindClass("com/google/android/gles_jni/EGLSurfaceImpl")));
    jEGLSurfacePointerField = aJEnv->GetFieldID(jClass, "mEGLSurface", "I");
}

jmethodID AndroidGLController::jWaitForValidSurfaceMethod = 0;
jmethodID AndroidGLController::jProvideEGLSurfaceMethod = 0;
jmethodID AndroidGLController::jResumeCompositorIfValidMethod = 0;

void
AndroidGLController::Init(JNIEnv* aJEnv)
{
    jclass jClass = reinterpret_cast<jclass>(aJEnv->NewGlobalRef(aJEnv->FindClass("org/mozilla/gecko/gfx/GLController")));

    jProvideEGLSurfaceMethod = aJEnv->GetMethodID(jClass, "provideEGLSurface",
                                                  "()Ljavax/microedition/khronos/egl/EGLSurface;");
    jResumeCompositorIfValidMethod = aJEnv->GetMethodID(jClass, "resumeCompositorIfValid", "()V");
    jWaitForValidSurfaceMethod = aJEnv->GetMethodID(jClass, "waitForValidSurface", "()V");
}

void
AndroidGLController::Acquire(JNIEnv* aJEnv, jobject aJObj)
{
    mJEnv = aJEnv;
    mThread = pthread_self();
    mJObj = aJEnv->NewGlobalRef(aJObj);
}

void
AndroidGLController::Reacquire(JNIEnv *aJEnv, jobject aJObj)
{
    aJEnv->DeleteGlobalRef(mJObj);
    mJObj = aJEnv->NewGlobalRef(aJObj);

    AutoLocalJNIFrame jniFrame(aJEnv, 0);
    aJEnv->CallVoidMethod(mJObj, jResumeCompositorIfValidMethod);
}

EGLSurface
AndroidGLController::ProvideEGLSurface()
{
    ASSERT_THREAD();
    AutoLocalJNIFrame jniFrame(mJEnv);
    jobject jObj = mJEnv->CallObjectMethod(mJObj, jProvideEGLSurfaceMethod);
    if (jniFrame.CheckForException())
        return NULL;

    return reinterpret_cast<EGLSurface>(mJEnv->GetIntField(jObj, jEGLSurfacePointerField));
}

void
AndroidGLController::WaitForValidSurface()
{
    ASSERT_THREAD();
    AutoLocalJNIFrame jniFrame(mJEnv, 0);
    mJEnv->CallVoidMethod(mJObj, jWaitForValidSurfaceMethod);
}
