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

#ifndef AndroidFlexViewWrapper_h__
#define AndroidFlexViewWrapper_h__

#include <jni.h>
//#include <GLES/gl.h>
//#include <GLES/glext.h>
#include <cassert>
#include <cstdlib>
#include <pthread.h>
#include <android/log.h>

typedef void *NativeType;

class AndroidEGLObject {
public:
    AndroidEGLObject(JNIEnv* aJEnv, jobject aJObj)
    : mPtr(reinterpret_cast<NativeType>(aJEnv->GetIntField(aJObj, jPointerField))) {}

    static void Init(JNIEnv* aJEnv);

    NativeType const& operator*() const {
        return mPtr;
    }

private:
    static jfieldID jPointerField;
    static const char* sClassName;
    static const char* sPointerFieldName;

    const NativeType mPtr;
};

typedef void *EGLConfig;
typedef void *EGLContext;
typedef void *EGLDisplay;
typedef void *EGLSurface;

class AndroidGLController {
public:
    static void Init(JNIEnv* aJEnv);

    void Acquire(JNIEnv *aJEnv, jobject aJObj);
    void Acquire(JNIEnv *aJEnv);
    void Release();

    void SetGLVersion(int aVersion);
    void InitGLContext();
    void DisposeGLContext();
    EGLDisplay GetEGLDisplay();
    EGLConfig GetEGLConfig();
    EGLContext GetEGLContext();
    EGLSurface GetEGLSurface();
    EGLSurface ProvideEGLSurface();
    bool HasSurface();
    bool SwapBuffers();
    bool CheckForLostContext();
    void WaitForValidSurface();
    int GetWidth();
    int GetHeight();

private:
    static jmethodID jSetGLVersionMethod;
    static jmethodID jInitGLContextMethod;
    static jmethodID jDisposeGLContextMethod;
    static jmethodID jGetEGLDisplayMethod;
    static jmethodID jGetEGLConfigMethod;
    static jmethodID jGetEGLContextMethod;
    static jmethodID jGetEGLSurfaceMethod;
    static jmethodID jHasSurfaceMethod;
    static jmethodID jSwapBuffersMethod;
    static jmethodID jCheckForLostContextMethod;
    static jmethodID jWaitForValidSurfaceMethod;
    static jmethodID jGetWidthMethod;
    static jmethodID jGetHeightMethod;
    static jmethodID jProvideEGLSurfaceMethod;

    JNIEnv *mJEnv;
    jobject mJObj;
};

#endif

