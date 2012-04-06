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

#include "AndroidLayerViewWrapper.h"
#include "nsDebug.h"

#define ASSERT_THREAD() \
        NS_ASSERTION(pthread_self() == mThread, "Something is calling AndroidGLController from the wrong thread!")

static jfieldID jEGLSurfacePointerField = 0;

void AndroidEGLObject::Init(JNIEnv* aJEnv) {
    jclass jClass;
    jClass = reinterpret_cast<jclass>
        (aJEnv->NewGlobalRef(aJEnv->FindClass("com/google/android/gles_jni/EGLSurfaceImpl")));
    jEGLSurfacePointerField = aJEnv->GetFieldID(jClass, "mEGLSurface", "I");
}

jmethodID AndroidGLController::jSetGLVersionMethod = 0;
jmethodID AndroidGLController::jWaitForValidSurfaceMethod = 0;
jmethodID AndroidGLController::jProvideEGLSurfaceMethod = 0;

void
AndroidGLController::Init(JNIEnv* aJEnv)
{
    jclass jClass = reinterpret_cast<jclass>(aJEnv->NewGlobalRef(aJEnv->FindClass("org/mozilla/gecko/gfx/GLController")));

    jSetGLVersionMethod = aJEnv->GetMethodID(jClass, "setGLVersion", "(I)V");
    jProvideEGLSurfaceMethod = aJEnv->GetMethodID(jClass, "provideEGLSurface",
                                                  "()Ljavax/microedition/khronos/egl/EGLSurface;");
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
AndroidGLController::SetGLVersion(int aVersion)
{
    ASSERT_THREAD();
    mJEnv->CallVoidMethod(mJObj, jSetGLVersionMethod, aVersion);
}

EGLSurface
AndroidGLController::ProvideEGLSurface()
{
    ASSERT_THREAD();
    jobject jObj = mJEnv->CallObjectMethod(mJObj, jProvideEGLSurfaceMethod);
    return reinterpret_cast<EGLSurface>(mJEnv->GetIntField(jObj, jEGLSurfacePointerField));
}

void
AndroidGLController::WaitForValidSurface()
{
    ASSERT_THREAD();
    mJEnv->CallVoidMethod(mJObj, jWaitForValidSurfaceMethod);
}
