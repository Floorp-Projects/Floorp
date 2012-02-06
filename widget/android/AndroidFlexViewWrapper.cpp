/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
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
* The Original Code is Mozilla Android code.
*
* The Initial Developer of the Original Code is Mozilla Foundation.
* Portions created by the Initial Developer are Copyright (C) 2011-2012
* the Initial Developer. All Rights Reserved.
*
* Contributor(s):
* Patrick Walton <pcwalton@mozilla.com>
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

#include "AndroidFlexViewWrapper.h"


static AndroidGLController sController;

static const char *sEGLDisplayClassName = "com/google/android/gles_jni/EGLDisplayImpl";
static const char *sEGLDisplayPointerFieldName = "mEGLDisplay";
static jfieldID jEGLDisplayPointerField = 0;

static const char *sEGLConfigClassName = "com/google/android/gles_jni/EGLConfigImpl";
static const char *sEGLConfigPointerFieldName = "mEGLConfig";
static jfieldID jEGLConfigPointerField = 0;

static const char *sEGLContextClassName = "com/google/android/gles_jni/EGLContextImpl";
static const char *sEGLContextPointerFieldName = "mEGLContext";
static jfieldID jEGLContextPointerField = 0;

static const char *sEGLSurfaceClassName = "com/google/android/gles_jni/EGLSurfaceImpl";
static const char *sEGLSurfacePointerFieldName = "mEGLSurface";
static jfieldID jEGLSurfacePointerField = 0;

void AndroidEGLObject::Init(JNIEnv* aJEnv) {
    jclass jClass;
    jClass = reinterpret_cast<jclass>
        (aJEnv->NewGlobalRef(aJEnv->FindClass(sEGLDisplayClassName)));
    jEGLDisplayPointerField = aJEnv->GetFieldID(jClass, sEGLDisplayPointerFieldName, "I");
    jClass = reinterpret_cast<jclass>
        (aJEnv->NewGlobalRef(aJEnv->FindClass(sEGLConfigClassName)));
    jEGLConfigPointerField = aJEnv->GetFieldID(jClass, sEGLConfigPointerFieldName, "I");
    jClass = reinterpret_cast<jclass>
        (aJEnv->NewGlobalRef(aJEnv->FindClass(sEGLContextClassName)));
    jEGLContextPointerField = aJEnv->GetFieldID(jClass, sEGLContextPointerFieldName, "I");
    jClass = reinterpret_cast<jclass>
        (aJEnv->NewGlobalRef(aJEnv->FindClass(sEGLSurfaceClassName)));
    jEGLSurfacePointerField = aJEnv->GetFieldID(jClass, sEGLSurfacePointerFieldName, "I");
}

jmethodID AndroidGLController::jSetGLVersionMethod = 0;
jmethodID AndroidGLController::jInitGLContextMethod = 0;
jmethodID AndroidGLController::jDisposeGLContextMethod = 0;
jmethodID AndroidGLController::jGetEGLDisplayMethod = 0;
jmethodID AndroidGLController::jGetEGLConfigMethod = 0;
jmethodID AndroidGLController::jGetEGLContextMethod = 0;
jmethodID AndroidGLController::jGetEGLSurfaceMethod = 0;
jmethodID AndroidGLController::jHasSurfaceMethod = 0;
jmethodID AndroidGLController::jSwapBuffersMethod = 0;
jmethodID AndroidGLController::jCheckForLostContextMethod = 0;
jmethodID AndroidGLController::jWaitForValidSurfaceMethod = 0;
jmethodID AndroidGLController::jGetWidthMethod = 0;
jmethodID AndroidGLController::jGetHeightMethod = 0;
jmethodID AndroidGLController::jProvideEGLSurfaceMethod = 0;

void
AndroidGLController::Init(JNIEnv *aJEnv)
{
    const char *className = "org/mozilla/gecko/gfx/GLController";
    jclass jClass = reinterpret_cast<jclass>(aJEnv->NewGlobalRef(aJEnv->FindClass(className)));

    jSetGLVersionMethod = aJEnv->GetMethodID(jClass, "setGLVersion", "(I)V");
    jInitGLContextMethod = aJEnv->GetMethodID(jClass, "initGLContext", "()V");
    jDisposeGLContextMethod = aJEnv->GetMethodID(jClass, "disposeGLContext", "()V");
    jGetEGLDisplayMethod = aJEnv->GetMethodID(jClass, "getEGLDisplay",
                                              "()Ljavax/microedition/khronos/egl/EGLDisplay;");
    jGetEGLConfigMethod = aJEnv->GetMethodID(jClass, "getEGLConfig",
                                             "()Ljavax/microedition/khronos/egl/EGLConfig;");
    jGetEGLContextMethod = aJEnv->GetMethodID(jClass, "getEGLContext",
                                              "()Ljavax/microedition/khronos/egl/EGLContext;");
    jGetEGLSurfaceMethod = aJEnv->GetMethodID(jClass, "getEGLSurface",
                                              "()Ljavax/microedition/khronos/egl/EGLSurface;");
    jProvideEGLSurfaceMethod = aJEnv->GetMethodID(jClass, "provideEGLSurface",
                                                  "()Ljavax/microedition/khronos/egl/EGLSurface;");
    jHasSurfaceMethod = aJEnv->GetMethodID(jClass, "hasSurface", "()Z");
    jSwapBuffersMethod = aJEnv->GetMethodID(jClass, "swapBuffers", "()Z");
    jCheckForLostContextMethod = aJEnv->GetMethodID(jClass, "checkForLostContext", "()Z");
    jWaitForValidSurfaceMethod = aJEnv->GetMethodID(jClass, "waitForValidSurface", "()V");
    jGetWidthMethod = aJEnv->GetMethodID(jClass, "getWidth", "()I");
    jGetHeightMethod = aJEnv->GetMethodID(jClass, "getHeight", "()I");
}

void
AndroidGLController::Acquire(JNIEnv* aJEnv, jobject aJObj)
{
    mJEnv = aJEnv;
    mJObj = aJEnv->NewGlobalRef(aJObj);
}

void
AndroidGLController::Acquire(JNIEnv* aJEnv)
{
    mJEnv = aJEnv;
}

void
AndroidGLController::Release()
{
    if (mJObj) {
        mJEnv->DeleteGlobalRef(mJObj);
        mJObj = NULL;
    }

    mJEnv = NULL;
}

void
AndroidGLController::SetGLVersion(int aVersion)
{
    mJEnv->CallVoidMethod(mJObj, jSetGLVersionMethod, aVersion);
}

void
AndroidGLController::InitGLContext()
{
    mJEnv->CallVoidMethod(mJObj, jInitGLContextMethod);
}

void
AndroidGLController::DisposeGLContext()
{
    mJEnv->CallVoidMethod(mJObj, jDisposeGLContextMethod);
}

EGLDisplay
AndroidGLController::GetEGLDisplay()
{
    jobject jObj = mJEnv->CallObjectMethod(mJObj, jGetEGLDisplayMethod);
    return reinterpret_cast<EGLDisplay>(mJEnv->GetIntField(jObj, jEGLDisplayPointerField));
}

EGLConfig
AndroidGLController::GetEGLConfig()
{
    jobject jObj = mJEnv->CallObjectMethod(mJObj, jGetEGLConfigMethod);
    return reinterpret_cast<EGLConfig>(mJEnv->GetIntField(jObj, jEGLConfigPointerField));
}

EGLContext
AndroidGLController::GetEGLContext()
{
    jobject jObj = mJEnv->CallObjectMethod(mJObj, jGetEGLContextMethod);
    return reinterpret_cast<EGLContext>(mJEnv->GetIntField(jObj, jEGLContextPointerField));
}

EGLSurface
AndroidGLController::GetEGLSurface()
{
    jobject jObj = mJEnv->CallObjectMethod(mJObj, jGetEGLSurfaceMethod);
    return reinterpret_cast<EGLSurface>(mJEnv->GetIntField(jObj, jEGLSurfacePointerField));
}

EGLSurface
AndroidGLController::ProvideEGLSurface()
{
    jobject jObj = mJEnv->CallObjectMethod(mJObj, jProvideEGLSurfaceMethod);
    return reinterpret_cast<EGLSurface>(mJEnv->GetIntField(jObj, jEGLSurfacePointerField));
}

bool
AndroidGLController::HasSurface()
{
    return mJEnv->CallBooleanMethod(mJObj, jHasSurfaceMethod);
}

bool
AndroidGLController::SwapBuffers()
{
    return mJEnv->CallBooleanMethod(mJObj, jSwapBuffersMethod);
}

bool
AndroidGLController::CheckForLostContext()
{
    return mJEnv->CallBooleanMethod(mJObj, jCheckForLostContextMethod);
}

void
AndroidGLController::WaitForValidSurface()
{
    mJEnv->CallVoidMethod(mJObj, jWaitForValidSurfaceMethod);
}

int
AndroidGLController::GetWidth()
{
    return mJEnv->CallIntMethod(mJObj, jGetWidthMethod);
}

int
AndroidGLController::GetHeight()
{
    return mJEnv->CallIntMethod(mJObj, jGetHeightMethod);
}


