/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
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
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
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

#include <android/log.h>

#ifdef MOZ_IPC
#include "nsXULAppAPI.h"
#endif
#include <pthread.h>
#include <prthread.h>
#include "nsXPCOMStrings.h"

#include "AndroidBridge.h"
#include "nsAppShell.h"

using namespace mozilla;

static PRUintn sJavaEnvThreadIndex = 0;

AndroidBridge *AndroidBridge::sBridge = 0;

static void
JavaThreadDetachFunc(void *arg)
{
    JNIEnv *env = (JNIEnv*) arg;
    JavaVM *vm = NULL;
    env->GetJavaVM(&vm);
    vm->DetachCurrentThread();
}

AndroidBridge *
AndroidBridge::ConstructBridge(JNIEnv *jEnv,
                               jclass jGeckoAppShellClass)
{
    /* NSS hack -- bionic doesn't handle recursive unloads correctly,
     * because library finalizer functions are called with the dynamic
     * linker lock still held.  This results in a deadlock when trying
     * to call dlclose() while we're already inside dlclose().
     * Conveniently, NSS has an env var that can prevent it from unloading.
     */
    putenv(strdup("NSS_DISABLE_UNLOAD=1"));

    sBridge = new AndroidBridge();
    if (!sBridge->Init(jEnv, jGeckoAppShellClass)) {
        delete sBridge;
        sBridge = 0;
    }

    PR_NewThreadPrivateIndex(&sJavaEnvThreadIndex, JavaThreadDetachFunc);

    return sBridge;
}

PRBool
AndroidBridge::Init(JNIEnv *jEnv,
                    jclass jGeckoAppShellClass)
{
    jEnv->GetJavaVM(&mJavaVM);

    mJNIEnv = nsnull;
    mThread = nsnull;

    mGeckoAppShellClass = (jclass) jEnv->NewGlobalRef(jGeckoAppShellClass);

    jNotifyIME = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "notifyIME", "(II)V");
    jNotifyIMEChange = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "notifyIMEChange", "(Ljava/lang/String;III)V");
    jEnableAccelerometer = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "enableAccelerometer", "(Z)V");
    jEnableLocation = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "enableLocation", "(Z)V");
    jReturnIMEQueryResult = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "returnIMEQueryResult", "(Ljava/lang/String;II)V");
    jScheduleRestart = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "scheduleRestart", "()V");
    jNotifyXreExit = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "onXreExit", "()V");
    jGetHandlersForMimeType = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "getHandlersForMimeType", "(Ljava/lang/String;)[Ljava/lang/String;");
    jGetHandlersForProtocol = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "getHandlersForProtocol", "(Ljava/lang/String;)[Ljava/lang/String;");
    jOpenUriExternal = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "openUriExternal", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Z");
    jGetMimeTypeFromExtension = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "getMimeTypeFromExtension", "(Ljava/lang/String;)Ljava/lang/String;");
    jMoveTaskToBack = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "moveTaskToBack", "()V");
    jGetClipboardText = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "getClipboardText", "()Ljava/lang/String;");
    jSetClipboardText = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "setClipboardText", "(Ljava/lang/String;)V");
    jShowAlertNotification = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "showAlertNotification", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");


    jEGLContextClass = (jclass) jEnv->NewGlobalRef(jEnv->FindClass("javax/microedition/khronos/egl/EGLContext"));
    jEGL10Class = (jclass) jEnv->NewGlobalRef(jEnv->FindClass("javax/microedition/khronos/egl/EGL10"));
    jEGLSurfaceImplClass = (jclass) jEnv->NewGlobalRef(jEnv->FindClass("com/google/android/gles_jni/EGLSurfaceImpl"));
    jEGLContextImplClass = (jclass) jEnv->NewGlobalRef(jEnv->FindClass("com/google/android/gles_jni/EGLContextImpl"));
    jEGLConfigImplClass = (jclass) jEnv->NewGlobalRef(jEnv->FindClass("com/google/android/gles_jni/EGLConfigImpl"));
    jEGLDisplayImplClass = (jclass) jEnv->NewGlobalRef(jEnv->FindClass("com/google/android/gles_jni/EGLDisplayImpl"));

    InitAndroidJavaWrappers(jEnv);

    // jEnv should NOT be cached here by anything -- the jEnv here
    // is not valid for the real gecko main thread, which is set
    // at SetMainThread time.

    return PR_TRUE;
}

JNIEnv *
AndroidBridge::AttachThread(PRBool asDaemon)
{
    JNIEnv *jEnv = (JNIEnv*) PR_GetThreadPrivate(sJavaEnvThreadIndex);
    if (jEnv)
        return jEnv;

    JavaVMAttachArgs args = {
        JNI_VERSION_1_2,
        "GeckoThread",
        NULL
    };

    jint res = 0;

    if (asDaemon) {
        res = mJavaVM->AttachCurrentThreadAsDaemon(&jEnv, &args);
    } else {
        res = mJavaVM->AttachCurrentThread(&jEnv, &args);
    }

    if (res != 0) {
        ALOG("AttachCurrentThread failed!");
        return nsnull;
    }

    PR_SetThreadPrivate(sJavaEnvThreadIndex, jEnv);

    return jEnv;
}

PRBool
AndroidBridge::SetMainThread(void *thr)
{
    if (thr) {
        mJNIEnv = AttachThread(PR_FALSE);
        if (!mJNIEnv)
            return PR_FALSE;

        mThread = thr;
    } else {
        mJNIEnv = nsnull;
        mThread = nsnull;
    }

    return PR_TRUE;
}

void
AndroidBridge::EnsureJNIThread()
{
    JNIEnv *env;
    if (mJavaVM->AttachCurrentThread(&env, NULL) != 0) {
        ALOG("EnsureJNIThread: test Attach failed!");
        return;
    }

    if ((void*)pthread_self() != mThread) {
        ALOG("###!!!!!!! Something's grabbing the JNIEnv from the wrong thread! (thr %p should be %p)",
             (void*)pthread_self(), (void*)mThread);
    }
}

void
AndroidBridge::NotifyIME(int aType, int aState)
{
    if (sBridge)
        JNI()->CallStaticVoidMethod(sBridge->mGeckoAppShellClass, 
                                    sBridge->jNotifyIME,  aType, aState);
}

void
AndroidBridge::NotifyIMEChange(const PRUnichar *aText, PRUint32 aTextLen,
                               int aStart, int aEnd, int aNewEnd)
{
    if (!sBridge) {
        return;
    }

    jvalue args[4];
    AutoLocalJNIFrame jniFrame(1);
    args[0].l = JNI()->NewString(aText, aTextLen);
    args[1].i = aStart;
    args[2].i = aEnd;
    args[3].i = aNewEnd;
    JNI()->CallStaticVoidMethodA(sBridge->mGeckoAppShellClass,
                                     sBridge->jNotifyIMEChange, args);
}

void
AndroidBridge::EnableAccelerometer(bool aEnable)
{
    mJNIEnv->CallStaticVoidMethod(mGeckoAppShellClass, jEnableAccelerometer, aEnable);
}

void
AndroidBridge::EnableLocation(bool aEnable)
{
    mJNIEnv->CallStaticVoidMethod(mGeckoAppShellClass, jEnableLocation, aEnable);
}

void
AndroidBridge::ReturnIMEQueryResult(const PRUnichar *aResult, PRUint32 aLen,
                                    int aSelStart, int aSelLen)
{
    jvalue args[3];
    AutoLocalJNIFrame jniFrame(1);
    args[0].l = mJNIEnv->NewString(aResult, aLen);
    args[1].i = aSelStart;
    args[2].i = aSelLen;
    mJNIEnv->CallStaticVoidMethodA(mGeckoAppShellClass,
                                   jReturnIMEQueryResult, args);
}

void
AndroidBridge::ScheduleRestart()
{
    ALOG("scheduling reboot");
    mJNIEnv->CallStaticVoidMethod(mGeckoAppShellClass, jScheduleRestart);
}

void
AndroidBridge::NotifyXreExit()
{
    ALOG("xre exiting");
    mJNIEnv->CallStaticVoidMethod(mGeckoAppShellClass, jNotifyXreExit);
}

PRBool
AndroidBridge::GetHandlersForMimeType(const char *aMimeType, nsStringArray* aStringArray)
{
    NS_PRECONDITION(aStringArray != nsnull, "null array pointer passed in");
    AutoLocalJNIFrame jniFrame;
    NS_ConvertUTF8toUTF16 wMimeType(aMimeType);
    jstring jstr = mJNIEnv->NewString(wMimeType.get(), wMimeType.Length());
    jobject obj = mJNIEnv->CallStaticObjectMethod(mGeckoAppShellClass,
                                                  jGetHandlersForMimeType,
                                                  jstr);
    jobjectArray arr = static_cast<jobjectArray>(obj);
    if (!arr)
        return PR_FALSE;

    jsize len = mJNIEnv->GetArrayLength(arr);

    if (!aStringArray)
        return len > 0;

    for (jsize i = 0; i < len; i++) {
        jstring jstr = static_cast<jstring>(mJNIEnv->GetObjectArrayElement(arr, i));
        nsJNIString jniStr(jstr);
        aStringArray->InsertStringAt(jniStr, i);
    }

    return PR_TRUE;
}

PRBool
AndroidBridge::GetHandlersForProtocol(const char *aScheme, nsStringArray* aStringArray)
{
    NS_PRECONDITION(aStringArray != nsnull, "null array pointer passed in");
    AutoLocalJNIFrame jniFrame;
    NS_ConvertUTF8toUTF16 wScheme(aScheme);
    jstring jstr = mJNIEnv->NewString(wScheme.get(), wScheme.Length());
    jobject obj = mJNIEnv->CallStaticObjectMethod(mGeckoAppShellClass,
                                                  jGetHandlersForProtocol,
                                                  jstr);
    jobjectArray arr = static_cast<jobjectArray>(obj);
    if (!arr)
        return PR_FALSE;

    jsize len = mJNIEnv->GetArrayLength(arr);

    if (!aStringArray)
        return len > 0;

    for (jsize i = 0; i < len; i++) {
        jstring jstr = static_cast<jstring>(mJNIEnv->GetObjectArrayElement(arr, i));
        nsJNIString jniStr(jstr);
        aStringArray->InsertStringAt(jniStr, i);
    }

    return PR_TRUE;
}

PRBool
AndroidBridge::OpenUriExternal(const nsACString& aUriSpec, const nsACString& aMimeType,
                               const nsAString& aPackageName, const nsAString& aClassName)
{
    AutoLocalJNIFrame jniFrame;
    NS_ConvertUTF8toUTF16 wUriSpec(aUriSpec);
    NS_ConvertUTF8toUTF16 wMimeType(aMimeType);
    const PRUnichar* wPackageName;
    PRUint32 packageNameLen = NS_StringGetData(aPackageName, &wPackageName);
    const PRUnichar* wClassName;
    PRUint32 classNameLen = NS_StringGetData(aClassName, &wClassName);

    jstring jstrUri = mJNIEnv->NewString(wUriSpec.get(), wUriSpec.Length());
    jstring jstrType = mJNIEnv->NewString(wMimeType.get(), wMimeType.Length());
    jstring jstrPackage = mJNIEnv->NewString(wPackageName, packageNameLen);
    jstring jstrClass = mJNIEnv->NewString(wClassName, classNameLen);
    return mJNIEnv->CallStaticBooleanMethod(mGeckoAppShellClass,
                                            jOpenUriExternal,
                                            jstrUri, jstrType, jstrPackage, jstrClass);
}

void
AndroidBridge::GetMimeTypeFromExtension(const nsACString& aFileExt, nsCString& aMimeType) {
    AutoLocalJNIFrame jniFrame;
    NS_ConvertUTF8toUTF16 wFileExt(aFileExt);
    jstring jstrExt = mJNIEnv->NewString(wFileExt.get(), wFileExt.Length());
    jstring jstrType =  static_cast<jstring>(mJNIEnv->CallStaticObjectMethod(mGeckoAppShellClass,
                                                                             jGetMimeTypeFromExtension,
                                                                             jstrExt));
    nsJNIString jniStr(jstrType);
    aMimeType.Assign(NS_ConvertUTF16toUTF8(jniStr.get()));
}

void
AndroidBridge::MoveTaskToBack()
{
    mJNIEnv->CallStaticVoidMethod(mGeckoAppShellClass, jMoveTaskToBack);
}

bool
AndroidBridge::GetClipboardText(nsAString& aText)
{
    jstring jstrType =  
        static_cast<jstring>(mJNIEnv->
                             CallStaticObjectMethod(mGeckoAppShellClass,
                                                    jGetClipboardText));
    if (!jstrType)
        return PR_FALSE;
    nsJNIString jniStr(jstrType);
    aText.Assign(jniStr);
    return PR_TRUE;
}

void
AndroidBridge::SetClipboardText(const nsAString& aText)
{
    const PRUnichar* wText;
    PRUint32 wTextLen = NS_StringGetData(aText, &wText);
    jstring jstr = mJNIEnv->NewString(wText, wTextLen);
    mJNIEnv->CallStaticObjectMethod(mGeckoAppShellClass, jSetClipboardText, jstr);
}

bool
AndroidBridge::ClipboardHasText()
{
    jstring jstrType =  
        static_cast<jstring>(mJNIEnv->
                             CallStaticObjectMethod(mGeckoAppShellClass,
                                                    jGetClipboardText));
    if (!jstrType)
        return PR_FALSE;
    return PR_TRUE;
}

void
AndroidBridge::EmptyClipboard()
{
    mJNIEnv->CallStaticObjectMethod(mGeckoAppShellClass, jSetClipboardText, nsnull);
}

void
AndroidBridge::ShowAlertNotification(const nsAString& aImageUrl,
                                     const nsAString& aAlertTitle,
                                     const nsAString& aAlertText,
                                     const nsAString& aAlertCookie,
                                     nsIObserver *aAlertListener,
                                     const nsAString& aAlertName)
{
    ALOG("ShowAlertNotification");

    AutoLocalJNIFrame jniFrame;

    if (nsAppShell::gAppShell && aAlertListener)
        nsAppShell::gAppShell->AddObserver(aAlertName, aAlertListener);

    jvalue args[5];
    args[0].l = mJNIEnv->NewString(nsPromiseFlatString(aImageUrl).get(), aImageUrl.Length());
    args[1].l = mJNIEnv->NewString(nsPromiseFlatString(aAlertTitle).get(), aAlertTitle.Length());
    args[2].l = mJNIEnv->NewString(nsPromiseFlatString(aAlertText).get(), aAlertText.Length());
    args[3].l = mJNIEnv->NewString(nsPromiseFlatString(aAlertCookie).get(), aAlertCookie.Length());
    args[4].l = mJNIEnv->NewString(nsPromiseFlatString(aAlertName).get(), aAlertName.Length());
    mJNIEnv->CallStaticVoidMethodA(mGeckoAppShellClass, jShowAlertNotification, args);
}

void
AndroidBridge::SetSurfaceView(jobject obj)
{
    mSurfaceView.Init(obj);
}

void *
AndroidBridge::CallEglCreateWindowSurface(void *dpy, void *config, AndroidGeckoSurfaceView &sview)
{
    AutoLocalJNIFrame jniFrame;

    /*
     * This is basically:
     *
     *    s = EGLContext.getEGL().eglCreateWindowSurface(new EGLDisplayImpl(dpy),
     *                                                   new EGLConfigImpl(config),
     *                                                   view.getHolder(), null);
     *    return s.mEGLSurface;
     *
     * We can't do it from java, because the EGLConfigImpl constructor is private.
     */

    jobject surfaceHolder = sview.GetSurfaceHolder();
    if (!surfaceHolder)
        return nsnull;

    // grab some fields and methods we'll need
    jmethodID constructConfig = mJNIEnv->GetMethodID(jEGLConfigImplClass, "<init>", "(I)V");
    jmethodID constructDisplay = mJNIEnv->GetMethodID(jEGLDisplayImplClass, "<init>", "(I)V");

    jmethodID getEgl = mJNIEnv->GetStaticMethodID(jEGLContextClass, "getEGL", "()Ljavax/microedition/khronos/egl/EGL;");
    jmethodID createWindowSurface = mJNIEnv->GetMethodID(jEGL10Class, "eglCreateWindowSurface", "(Ljavax/microedition/khronos/egl/EGLDisplay;Ljavax/microedition/khronos/egl/EGLConfig;Ljava/lang/Object;[I)Ljavax/microedition/khronos/egl/EGLSurface;");

    jobject egl = mJNIEnv->CallStaticObjectMethod(jEGLContextClass, getEgl);

    jobject jdpy = mJNIEnv->NewObject(jEGLDisplayImplClass, constructDisplay, (int) dpy);
    jobject jconf = mJNIEnv->NewObject(jEGLConfigImplClass, constructConfig, (int) config);

    // make the call
    jobject surf = mJNIEnv->CallObjectMethod(egl, createWindowSurface, jdpy, jconf, surfaceHolder, NULL);
    if (!surf)
        return nsnull;

    jfieldID sfield = mJNIEnv->GetFieldID(jEGLSurfaceImplClass, "mEGLSurface", "I");

    jint realSurface = mJNIEnv->GetIntField(surf, sfield);

    return (void*) realSurface;
}

bool
AndroidBridge::GetStaticStringField(const char *className, const char *fieldName, nsAString &result)
{
    AutoLocalJNIFrame jniFrame(3);
    jclass cls = mJNIEnv->FindClass(className);
    if (!cls)
        return false;

    jfieldID field = mJNIEnv->GetStaticFieldID(cls, fieldName, "Ljava/lang/String;");
    if (!field)
        return false;

    jstring jstr = (jstring) mJNIEnv->GetStaticObjectField(cls, field);
    if (!jstr)
        return false;

    result.Assign(nsJNIString(jstr));
    return true;
}

// Available for places elsewhere in the code to link to.
PRBool
mozilla_AndroidBridge_SetMainThread(void *thr)
{
    return AndroidBridge::Bridge()->SetMainThread(thr);
}

JavaVM *
mozilla_AndroidBridge_GetJavaVM()
{
    return AndroidBridge::Bridge()->VM();
}

JNIEnv *
mozilla_AndroidBridge_AttachThread(PRBool asDaemon)
{
    return AndroidBridge::Bridge()->AttachThread(asDaemon);
}

extern "C" JNIEnv * GetJNIForThread()
{
    return mozilla::AndroidBridge::JNIForThread();
}
