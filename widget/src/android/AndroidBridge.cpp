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

#include "nsXULAppAPI.h"
#include <pthread.h>
#include <prthread.h>
#include "nsXPCOMStrings.h"

#include "AndroidBridge.h"
#include "nsAppShell.h"
#include "nsOSHelperAppService.h"

#ifdef DEBUG
#define ALOG_BRIDGE(args...) ALOG(args)
#else
#define ALOG_BRIDGE(args...)
#endif

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
    ALOG_BRIDGE("AndroidBridge::Init");
    jEnv->GetJavaVM(&mJavaVM);

    mJNIEnv = nsnull;
    mThread = nsnull;

    mGeckoAppShellClass = (jclass) jEnv->NewGlobalRef(jGeckoAppShellClass);

    jNotifyIME = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "notifyIME", "(II)V");
    jNotifyIMEEnabled = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "notifyIMEEnabled", "(ILjava/lang/String;Ljava/lang/String;)V");
    jNotifyIMEChange = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "notifyIMEChange", "(Ljava/lang/String;III)V");
    jAcknowledgeEventSync = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "acknowledgeEventSync", "()V");

    jEnableAccelerometer = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "enableAccelerometer", "(Z)V");
    jEnableLocation = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "enableLocation", "(Z)V");
    jReturnIMEQueryResult = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "returnIMEQueryResult", "(Ljava/lang/String;II)V");
    jScheduleRestart = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "scheduleRestart", "()V");
    jNotifyAppShellReady = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "onAppShellReady", "()V");
    jNotifyXreExit = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "onXreExit", "()V");
    jGetHandlersForMimeType = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "getHandlersForMimeType", "(Ljava/lang/String;Ljava/lang/String;)[Ljava/lang/String;");
    jGetHandlersForURL = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "getHandlersForURL", "(Ljava/lang/String;Ljava/lang/String;)[Ljava/lang/String;");
    jOpenUriExternal = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "openUriExternal", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Z");
    jGetMimeTypeFromExtensions = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "getMimeTypeFromExtensions", "(Ljava/lang/String;)Ljava/lang/String;");
    jGetExtensionFromMimeType = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "getExtensionFromMimeType", "(Ljava/lang/String;)Ljava/lang/String;");
    jMoveTaskToBack = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "moveTaskToBack", "()V");
    jGetClipboardText = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "getClipboardText", "()Ljava/lang/String;");
    jSetClipboardText = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "setClipboardText", "(Ljava/lang/String;)V");
    jShowAlertNotification = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "showAlertNotification", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
    jShowFilePicker = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "showFilePicker", "(Ljava/lang/String;)Ljava/lang/String;");
    jAlertsProgressListener_OnProgress = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "alertsProgressListener_OnProgress", "(Ljava/lang/String;JJLjava/lang/String;)V");
    jAlertsProgressListener_OnCancel = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "alertsProgressListener_OnCancel", "(Ljava/lang/String;)V");
    jGetDpi = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "getDpi", "()I");
    jSetFullScreen = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "setFullScreen", "(Z)V");
    jShowInputMethodPicker = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "showInputMethodPicker", "()V");
    jHideProgressDialog = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "hideProgressDialog", "()V");
    jPerformHapticFeedback = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "performHapticFeedback", "(Z)V");
    jSetKeepScreenOn = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "setKeepScreenOn", "(Z)V");
    jIsNetworkLinkUp = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "isNetworkLinkUp", "()Z");
    jIsNetworkLinkKnown = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "isNetworkLinkKnown", "()Z");
    jSetSelectedLocale = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "setSelectedLocale", "(Ljava/lang/String;)V");

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
    ALOG_BRIDGE("AndroidBridge::AttachThread");
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
        ALOG_BRIDGE("AttachCurrentThread failed!");
        return nsnull;
    }

    PR_SetThreadPrivate(sJavaEnvThreadIndex, jEnv);

    return jEnv;
}

PRBool
AndroidBridge::SetMainThread(void *thr)
{
    ALOG_BRIDGE("AndroidBridge::SetMainThread");
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
        ALOG_BRIDGE("EnsureJNIThread: test Attach failed!");
        return;
    }

    if ((void*)pthread_self() != mThread) {
        ALOG_BRIDGE("###!!!!!!! Something's grabbing the JNIEnv from the wrong thread! (thr %p should be %p)",
             (void*)pthread_self(), (void*)mThread);
    }
}

void
AndroidBridge::NotifyIME(int aType, int aState)
{
    ALOG_BRIDGE("AndroidBridge::NotifyIME");
    if (sBridge)
        JNI()->CallStaticVoidMethod(sBridge->mGeckoAppShellClass, 
                                    sBridge->jNotifyIME,  aType, aState);
}

void
AndroidBridge::NotifyIMEEnabled(int aState, const nsAString& aTypeHint,
                                const nsAString& aActionHint)
{
    ALOG_BRIDGE("AndroidBridge::NotifyIMEEnabled");
    if (!sBridge)
        return;

    nsPromiseFlatString typeHint(aTypeHint);
    nsPromiseFlatString actionHint(aActionHint);

    jvalue args[3];
    AutoLocalJNIFrame jniFrame(1);
    args[0].i = aState;
    args[1].l = JNI()->NewString(typeHint.get(), typeHint.Length());
    args[2].l = JNI()->NewString(actionHint.get(), actionHint.Length());
    JNI()->CallStaticVoidMethodA(sBridge->mGeckoAppShellClass,
                                 sBridge->jNotifyIMEEnabled, args);
}

void
AndroidBridge::NotifyIMEChange(const PRUnichar *aText, PRUint32 aTextLen,
                               int aStart, int aEnd, int aNewEnd)
{
    ALOG_BRIDGE("AndroidBridge::NotifyIMEChange");
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
AndroidBridge::AcknowledgeEventSync()
{
    ALOG_BRIDGE("AndroidBridge::AcknowledgeEventSync");
    mJNIEnv->CallStaticVoidMethod(mGeckoAppShellClass, jAcknowledgeEventSync);
}

void
AndroidBridge::EnableAccelerometer(bool aEnable)
{
    ALOG_BRIDGE("AndroidBridge::EnableAccelerometer");
    mJNIEnv->CallStaticVoidMethod(mGeckoAppShellClass, jEnableAccelerometer, aEnable);
}

void
AndroidBridge::EnableLocation(bool aEnable)
{
    ALOG_BRIDGE("AndroidBridge::EnableLocation");
    mJNIEnv->CallStaticVoidMethod(mGeckoAppShellClass, jEnableLocation, aEnable);
}

void
AndroidBridge::ReturnIMEQueryResult(const PRUnichar *aResult, PRUint32 aLen,
                                    int aSelStart, int aSelLen)
{
    ALOG_BRIDGE("AndroidBridge::ReturnIMEQueryResult");
    jvalue args[3];
    AutoLocalJNIFrame jniFrame(1);
    args[0].l = mJNIEnv->NewString(aResult, aLen);
    args[1].i = aSelStart;
    args[2].i = aSelLen;
    mJNIEnv->CallStaticVoidMethodA(mGeckoAppShellClass,
                                   jReturnIMEQueryResult, args);
}

void
AndroidBridge::NotifyAppShellReady()
{
    ALOG_BRIDGE("AndroidBridge::NotifyAppShellReady");
    mJNIEnv->CallStaticVoidMethod(mGeckoAppShellClass, jNotifyAppShellReady);
}

void
AndroidBridge::ScheduleRestart()
{
    ALOG_BRIDGE("scheduling reboot");
    mJNIEnv->CallStaticVoidMethod(mGeckoAppShellClass, jScheduleRestart);
}

void
AndroidBridge::NotifyXreExit()
{
    ALOG_BRIDGE("xre exiting");
    mJNIEnv->CallStaticVoidMethod(mGeckoAppShellClass, jNotifyXreExit);
}

static void 
getHandlersFromStringArray(JNIEnv *aJNIEnv, jobjectArray jArr, jsize aLen,
                           nsIMutableArray *aHandlersArray,
                           nsIHandlerApp **aDefaultApp,
                           const nsAString& aAction = EmptyString(),
                           const nsACString& aMimeType = EmptyCString())
{
    nsString empty = EmptyString();
    for (jsize i = 0; i < aLen; i+=4) {
        nsJNIString name( 
            static_cast<jstring>(aJNIEnv->GetObjectArrayElement(jArr, i)));
        nsJNIString isDefault(
            static_cast<jstring>(aJNIEnv->GetObjectArrayElement(jArr, i + 1)));
        nsJNIString packageName( 
            static_cast<jstring>(aJNIEnv->GetObjectArrayElement(jArr, i + 2)));
        nsJNIString className( 
            static_cast<jstring>(aJNIEnv->GetObjectArrayElement(jArr, i + 3)));
        nsIHandlerApp* app = nsOSHelperAppService::
            CreateAndroidHandlerApp(name, className, packageName,
                                    className, aMimeType, aAction);
        
        aHandlersArray->AppendElement(app, PR_FALSE);
        if (aDefaultApp && isDefault.Length() > 0)
            *aDefaultApp = app;
    }
}

PRBool
AndroidBridge::GetHandlersForMimeType(const char *aMimeType,
                                      nsIMutableArray *aHandlersArray,
                                      nsIHandlerApp **aDefaultApp,
                                      const nsAString& aAction)
{
    ALOG_BRIDGE("AndroidBridge::GetHandlersForMimeType");

    AutoLocalJNIFrame jniFrame;
    NS_ConvertUTF8toUTF16 wMimeType(aMimeType);
    jstring jstrMimeType =
        mJNIEnv->NewString(wMimeType.get(), wMimeType.Length());
    const PRUnichar* wAction;
    PRUint32 actionLen = NS_StringGetData(aAction, &wAction);
    jstring jstrAction = mJNIEnv->NewString(wAction, actionLen);

    jobject obj = mJNIEnv->CallStaticObjectMethod(mGeckoAppShellClass,
                                                  jGetHandlersForMimeType,
                                                  jstrMimeType, jstrAction);
    jobjectArray arr = static_cast<jobjectArray>(obj);
    if (!arr)
        return PR_FALSE;

    jsize len = mJNIEnv->GetArrayLength(arr);

    if (!aHandlersArray)
        return len > 0;

    getHandlersFromStringArray(mJNIEnv, arr, len, aHandlersArray, 
                               aDefaultApp, aAction,
                               nsDependentCString(aMimeType));
    return PR_TRUE;
}

PRBool
AndroidBridge::GetHandlersForURL(const char *aURL,
                                      nsIMutableArray* aHandlersArray,
                                      nsIHandlerApp **aDefaultApp,
                                      const nsAString& aAction)
{
    ALOG_BRIDGE("AndroidBridge::GetHandlersForURL");

    AutoLocalJNIFrame jniFrame;
    NS_ConvertUTF8toUTF16 wScheme(aURL);
    jstring jstrScheme = mJNIEnv->NewString(wScheme.get(), wScheme.Length());
    const PRUnichar* wAction;
    PRUint32 actionLen = NS_StringGetData(aAction, &wAction);
    jstring jstrAction = mJNIEnv->NewString(wAction, actionLen);

    jobject obj = mJNIEnv->CallStaticObjectMethod(mGeckoAppShellClass,
                                                  jGetHandlersForURL,
                                                  jstrScheme, jstrAction);
    jobjectArray arr = static_cast<jobjectArray>(obj);
    if (!arr)
        return PR_FALSE;

    jsize len = mJNIEnv->GetArrayLength(arr);

    if (!aHandlersArray)
        return len > 0;

    getHandlersFromStringArray(mJNIEnv, arr, len, aHandlersArray, 
                               aDefaultApp, aAction);
    return PR_TRUE;
}

PRBool
AndroidBridge::OpenUriExternal(const nsACString& aUriSpec, const nsACString& aMimeType,
                               const nsAString& aPackageName, const nsAString& aClassName,
                               const nsAString& aAction, const nsAString& aTitle)
{
    ALOG_BRIDGE("AndroidBridge::OpenUriExternal");

    AutoLocalJNIFrame jniFrame;
    NS_ConvertUTF8toUTF16 wUriSpec(aUriSpec);
    NS_ConvertUTF8toUTF16 wMimeType(aMimeType);
    const PRUnichar* wPackageName;
    PRUint32 packageNameLen = NS_StringGetData(aPackageName, &wPackageName);
    const PRUnichar* wClassName;
    PRUint32 classNameLen = NS_StringGetData(aClassName, &wClassName);
    const PRUnichar* wAction;
    PRUint32 actionLen = NS_StringGetData(aAction, &wAction);
    const PRUnichar* wTitle;
    PRUint32 titleLen = NS_StringGetData(aTitle, &wTitle);

    jstring jstrUri = mJNIEnv->NewString(wUriSpec.get(), wUriSpec.Length());
    jstring jstrType = mJNIEnv->NewString(wMimeType.get(), wMimeType.Length());
    jstring jstrPackage = mJNIEnv->NewString(wPackageName, packageNameLen);
    jstring jstrClass = mJNIEnv->NewString(wClassName, classNameLen);
    jstring jstrAction = mJNIEnv->NewString(wAction, actionLen);
    jstring jstrTitle = mJNIEnv->NewString(wTitle, titleLen);

    return mJNIEnv->CallStaticBooleanMethod(mGeckoAppShellClass,
                                            jOpenUriExternal,
                                            jstrUri, jstrType, jstrPackage, 
                                            jstrClass, jstrAction, jstrTitle);
}

void
AndroidBridge::GetMimeTypeFromExtensions(const nsACString& aFileExt, nsCString& aMimeType)
{
    ALOG_BRIDGE("AndroidBridge::GetMimeTypeFromExtensions");

    AutoLocalJNIFrame jniFrame;
    NS_ConvertUTF8toUTF16 wFileExt(aFileExt);
    jstring jstrExt = mJNIEnv->NewString(wFileExt.get(), wFileExt.Length());
    jstring jstrType =  static_cast<jstring>(
        mJNIEnv->CallStaticObjectMethod(mGeckoAppShellClass,
                                        jGetMimeTypeFromExtensions,
                                        jstrExt));
    nsJNIString jniStr(jstrType);
    aMimeType.Assign(NS_ConvertUTF16toUTF8(jniStr.get()));
}

void
AndroidBridge::GetExtensionFromMimeType(const nsCString& aMimeType, nsACString& aFileExt)
{
    ALOG_BRIDGE("AndroidBridge::GetExtensionFromMimeType");

    AutoLocalJNIFrame jniFrame;
    NS_ConvertUTF8toUTF16 wMimeType(aMimeType);
    jstring jstrType = mJNIEnv->NewString(wMimeType.get(), wMimeType.Length());
    jstring jstrExt = static_cast<jstring>(
        mJNIEnv->CallStaticObjectMethod(mGeckoAppShellClass,
                                        jGetExtensionFromMimeType,
                                        jstrType));
    nsJNIString jniStr(jstrExt);
    aFileExt.Assign(NS_ConvertUTF16toUTF8(jniStr.get()));
}

void
AndroidBridge::MoveTaskToBack()
{
    ALOG_BRIDGE("AndroidBridge::MoveTaskToBack");
    mJNIEnv->CallStaticVoidMethod(mGeckoAppShellClass, jMoveTaskToBack);
}

bool
AndroidBridge::GetClipboardText(nsAString& aText)
{
    ALOG_BRIDGE("AndroidBridge::GetClipboardText");

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
    ALOG_BRIDGE("AndroidBridge::SetClipboardText");

    const PRUnichar* wText;
    PRUint32 wTextLen = NS_StringGetData(aText, &wText);
    jstring jstr = mJNIEnv->NewString(wText, wTextLen);
    mJNIEnv->CallStaticObjectMethod(mGeckoAppShellClass, jSetClipboardText, jstr);
}

bool
AndroidBridge::ClipboardHasText()
{
    ALOG_BRIDGE("AndroidBridge::ClipboardHasText");

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
    ALOG_BRIDGE("AndroidBridge::EmptyClipboard");
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
    ALOG_BRIDGE("ShowAlertNotification");
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
AndroidBridge::AlertsProgressListener_OnProgress(const nsAString& aAlertName,
                                                 PRInt64 aProgress,
                                                 PRInt64 aProgressMax,
                                                 const nsAString& aAlertText)
{
    ALOG_BRIDGE("AlertsProgressListener_OnProgress");
    AutoLocalJNIFrame jniFrame;

    jstring jstrName = mJNIEnv->NewString(nsPromiseFlatString(aAlertName).get(), aAlertName.Length());
    jstring jstrText = mJNIEnv->NewString(nsPromiseFlatString(aAlertText).get(), aAlertText.Length());
    mJNIEnv->CallStaticVoidMethod(mGeckoAppShellClass, jAlertsProgressListener_OnProgress,
                                  jstrName, aProgress, aProgressMax, jstrText);
}

void
AndroidBridge::AlertsProgressListener_OnCancel(const nsAString& aAlertName)
{
    ALOG_BRIDGE("AlertsProgressListener_OnCancel");
    AutoLocalJNIFrame jniFrame;

    jstring jstrName = mJNIEnv->NewString(nsPromiseFlatString(aAlertName).get(), aAlertName.Length());
    mJNIEnv->CallStaticVoidMethod(mGeckoAppShellClass, jAlertsProgressListener_OnCancel, jstrName);
}


int
AndroidBridge::GetDPI()
{
    ALOG_BRIDGE("AndroidBridge::GetDPI");
    return (int) mJNIEnv->CallStaticIntMethod(mGeckoAppShellClass, jGetDpi);
}

void
AndroidBridge::ShowFilePicker(nsAString& aFilePath, nsAString& aFilters)
{
    ALOG_BRIDGE("AndroidBridge::ShowFilePicker");

    AutoLocalJNIFrame jniFrame;
    jstring jstrFilers = mJNIEnv->NewString(nsPromiseFlatString(aFilters).get(),
                                            aFilters.Length());
    jstring jstr =  static_cast<jstring>(mJNIEnv->CallStaticObjectMethod(
                                             mGeckoAppShellClass,
                                             jShowFilePicker, jstrFilers));
    aFilePath.Assign(nsJNIString(jstr));
}

void
AndroidBridge::SetFullScreen(PRBool aFullScreen)
{
    ALOG_BRIDGE("AndroidBridge::SetFullScreen");
    mJNIEnv->CallStaticVoidMethod(mGeckoAppShellClass, jSetFullScreen, aFullScreen);
}

void
AndroidBridge::HideProgressDialogOnce()
{
    static bool once = false;
    if (!once) {
        ALOG_BRIDGE("AndroidBridge::HideProgressDialogOnce");
        mJNIEnv->CallStaticVoidMethod(mGeckoAppShellClass, jHideProgressDialog);
        once = true;
    }
}

void
AndroidBridge::PerformHapticFeedback(PRBool aIsLongPress)
{
    ALOG_BRIDGE("AndroidBridge::PerformHapticFeedback");
    mJNIEnv->CallStaticVoidMethod(mGeckoAppShellClass,
                                    jPerformHapticFeedback, aIsLongPress);
}

bool
AndroidBridge::IsNetworkLinkUp()
{
    ALOG_BRIDGE("AndroidBridge::IsNetworkLinkUp");
    return !!mJNIEnv->CallStaticBooleanMethod(mGeckoAppShellClass, jIsNetworkLinkUp);
}

bool
AndroidBridge::IsNetworkLinkKnown()
{
    ALOG_BRIDGE("AndroidBridge::IsNetworkLinkKnown");
    return !!mJNIEnv->CallStaticBooleanMethod(mGeckoAppShellClass, jIsNetworkLinkKnown);
}

void
AndroidBridge::SetSelectedLocale(const nsAString& aLocale)
{
    ALOG_BRIDGE("AndroidBridge::SetSelectedLocale");
    jstring jLocale = GetJNIForThread()->NewString(PromiseFlatString(aLocale).get(), aLocale.Length());
    GetJNIForThread()->CallStaticVoidMethod(mGeckoAppShellClass, jSetSelectedLocale, jLocale);
}

void
AndroidBridge::SetSurfaceView(jobject obj)
{
    mSurfaceView.Init(obj);
}

void
AndroidBridge::ShowInputMethodPicker()
{
    ALOG_BRIDGE("AndroidBridge::ShowInputMethodPicker");
    mJNIEnv->CallStaticVoidMethod(mGeckoAppShellClass, jShowInputMethodPicker);
}

void *
AndroidBridge::CallEglCreateWindowSurface(void *dpy, void *config, AndroidGeckoSurfaceView &sview)
{
    ALOG_BRIDGE("AndroidBridge::CallEglCreateWindowSurface");
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
AndroidBridge::GetStaticIntField(const char *className, const char *fieldName, PRInt32* aInt)
{
    ALOG_BRIDGE("AndroidBridge::GetStaticIntField %s", fieldName);
    AutoLocalJNIFrame jniFrame(3);
    jclass cls = mJNIEnv->FindClass(className);
    if (!cls)
        return false;

    jfieldID field = mJNIEnv->GetStaticFieldID(cls, fieldName, "I");
    if (!field)
        return false;

    *aInt = static_cast<PRInt32>(mJNIEnv->GetStaticIntField(cls, field));

    return true;
}

bool
AndroidBridge::GetStaticStringField(const char *className, const char *fieldName, nsAString &result)
{
    ALOG_BRIDGE("AndroidBridge::GetStaticIntField %s", fieldName);

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

void
AndroidBridge::SetKeepScreenOn(bool on)
{
    ALOG_BRIDGE("AndroidBridge::SetKeepScreenOn");
    JNI()->CallStaticVoidMethod(sBridge->mGeckoAppShellClass,
                                sBridge->jSetKeepScreenOn, on);
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

jclass GetGeckoAppShellClass()
{
    return mozilla::AndroidBridge::GetGeckoAppShellClass();
}
