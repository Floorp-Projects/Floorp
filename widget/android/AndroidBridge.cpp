/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"
#include "mozilla/layers/CompositorChild.h"
#include "mozilla/layers/CompositorParent.h"

#include <android/log.h>
#include <dlfcn.h>

#include "mozilla/Hal.h"
#include "nsXULAppAPI.h"
#include <prthread.h>
#include "nsXPCOMStrings.h"

#include "AndroidBridge.h"
#include "nsAppShell.h"
#include "nsOSHelperAppService.h"
#include "nsWindow.h"
#include "mozilla/Preferences.h"
#include "nsThreadUtils.h"
#include "nsIThreadManager.h"
#include "mozilla/dom/sms/PSms.h"
#include "gfxImageSurface.h"
#include "gfxContext.h"
#include "nsPresContext.h"
#include "nsIDocShell.h"
#include "nsPIDOMWindow.h"
#include "mozilla/dom/ScreenOrientation.h"
#include "nsIDOMWindowUtils.h"
#include "nsIDOMClientRect.h"

#ifdef DEBUG
#define ALOG_BRIDGE(args...) ALOG(args)
#else
#define ALOG_BRIDGE(args...)
#endif

#define IME_FULLSCREEN_PREF "widget.ime.android.landscape_fullscreen"
#define IME_FULLSCREEN_THRESHOLD_PREF "widget.ime.android.fullscreen_threshold"

using namespace mozilla;

NS_IMPL_THREADSAFE_ISUPPORTS0(nsFilePickerCallback)

AndroidBridge *AndroidBridge::sBridge = 0;

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
    putenv("NSS_DISABLE_UNLOAD=1"); 

    sBridge = new AndroidBridge();
    if (!sBridge->Init(jEnv, jGeckoAppShellClass)) {
        delete sBridge;
        sBridge = 0;
    }
    return sBridge;
}

bool
AndroidBridge::Init(JNIEnv *jEnv,
                    jclass jGeckoAppShellClass)
{
    ALOG_BRIDGE("AndroidBridge::Init");
    jEnv->GetJavaVM(&mJavaVM);

    AutoLocalJNIFrame jniFrame(jEnv);

    mJNIEnv = nsnull;
    mThread = nsnull;
    mOpenedGraphicsLibraries = false;
    mHasNativeBitmapAccess = false;
    mHasNativeWindowAccess = false;
    mHasNativeWindowFallback = false;

    mGeckoAppShellClass = (jclass) jEnv->NewGlobalRef(jGeckoAppShellClass);

    jNotifyIME = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "notifyIME", "(II)V");
    jNotifyIMEEnabled = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "notifyIMEEnabled", "(ILjava/lang/String;Ljava/lang/String;Z)V");
    jNotifyIMEChange = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "notifyIMEChange", "(Ljava/lang/String;III)V");
    jNotifyScreenShot = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "notifyScreenShot", "(Ljava/nio/ByteBuffer;IIIIII)V");
    jAcknowledgeEventSync = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "acknowledgeEventSync", "()V");

    jEnableLocation = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "enableLocation", "(Z)V");
    jEnableLocationHighAccuracy = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "enableLocationHighAccuracy", "(Z)V");
    jEnableSensor = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "enableSensor", "(I)V");
    jDisableSensor = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "disableSensor", "(I)V");
    jReturnIMEQueryResult = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "returnIMEQueryResult", "(Ljava/lang/String;II)V");
    jScheduleRestart = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "scheduleRestart", "()V");
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
    jShowFilePickerForExtensions = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "showFilePickerForExtensions", "(Ljava/lang/String;)Ljava/lang/String;");
    jShowFilePickerForMimeType = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "showFilePickerForMimeType", "(Ljava/lang/String;)Ljava/lang/String;");
    jShowFilePickerAsync = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "showFilePickerAsync", "(Ljava/lang/String;J)V");
    jAlertsProgressListener_OnProgress = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "alertsProgressListener_OnProgress", "(Ljava/lang/String;JJLjava/lang/String;)V");
    jAlertsProgressListener_OnCancel = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "alertsProgressListener_OnCancel", "(Ljava/lang/String;)V");
    jGetDpi = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "getDpi", "()I");
    jSetFullScreen = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "setFullScreen", "(Z)V");
    jShowInputMethodPicker = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "showInputMethodPicker", "()V");
    jNotifyDefaultPrevented = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "notifyDefaultPrevented", "(Z)V");
    jHideProgressDialog = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "hideProgressDialog", "()V");
    jPerformHapticFeedback = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "performHapticFeedback", "(Z)V");
    jVibrate1 = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "vibrate", "(J)V");
    jVibrateA = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "vibrate", "([JI)V");
    jCancelVibrate = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "cancelVibrate", "()V");
    jSetKeepScreenOn = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "setKeepScreenOn", "(Z)V");
    jIsNetworkLinkUp = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "isNetworkLinkUp", "()Z");
    jIsNetworkLinkKnown = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "isNetworkLinkKnown", "()Z");
    jSetSelectedLocale = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "setSelectedLocale", "(Ljava/lang/String;)V");
    jScanMedia = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "scanMedia", "(Ljava/lang/String;Ljava/lang/String;)V");
    jGetSystemColors = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "getSystemColors", "()[I");
    jGetIconForExtension = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "getIconForExtension", "(Ljava/lang/String;I)[B");
    jFireAndWaitForTracerEvent = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "fireAndWaitForTracerEvent", "()V");   
    jCreateShortcut = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "createShortcut", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
    jGetShowPasswordSetting = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "getShowPasswordSetting", "()Z");
    jPostToJavaThread = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "postToJavaThread", "(Z)V");
    jInitCamera = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "initCamera", "(Ljava/lang/String;III)[I");
    jCloseCamera = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "closeCamera", "()V");
    jIsTablet = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "isTablet", "()Z");
    jEnableBatteryNotifications = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "enableBatteryNotifications", "()V");
    jDisableBatteryNotifications = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "disableBatteryNotifications", "()V");
    jGetCurrentBatteryInformation = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "getCurrentBatteryInformation", "()[D");
    jNotifyPaintedRect = jEnv->GetStaticMethodID(jGeckoAppShellClass, "notifyPaintedRect", "(FFFF)V");

    jHandleGeckoMessage = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "handleGeckoMessage", "(Ljava/lang/String;)Ljava/lang/String;");
    jCheckUriVisited = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "checkUriVisited", "(Ljava/lang/String;)V");
    jMarkUriVisited = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "markUriVisited", "(Ljava/lang/String;)V");

    jNumberOfMessages = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "getNumberOfMessagesForText", "(Ljava/lang/String;)I");
    jSendMessage = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "sendMessage", "(Ljava/lang/String;Ljava/lang/String;IJ)V");
    jSaveSentMessage = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "saveSentMessage", "(Ljava/lang/String;Ljava/lang/String;J)I");
    jGetMessage = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "getMessage", "(IIJ)V");
    jDeleteMessage = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "deleteMessage", "(IIJ)V");
    jCreateMessageList = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "createMessageList", "(JJ[Ljava/lang/String;IIZIJ)V");
    jGetNextMessageinList = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "getNextMessageInList", "(IIJ)V");
    jClearMessageList = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "clearMessageList", "(I)V");

    jGetCurrentNetworkInformation = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "getCurrentNetworkInformation", "()[D");
    jEnableNetworkNotifications = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "enableNetworkNotifications", "()V");
    jDisableNetworkNotifications = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "disableNetworkNotifications", "()V");

    jGetScreenOrientation = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "getScreenOrientation", "()S");
    jEnableScreenOrientationNotifications = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "enableScreenOrientationNotifications", "()V");
    jDisableScreenOrientationNotifications = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "disableScreenOrientationNotifications", "()V");
    jLockScreenOrientation = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "lockScreenOrientation", "(I)V");
    jUnlockScreenOrientation = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "unlockScreenOrientation", "()V");

    jEGLContextClass = (jclass) jEnv->NewGlobalRef(jEnv->FindClass("javax/microedition/khronos/egl/EGLContext"));
    jEGL10Class = (jclass) jEnv->NewGlobalRef(jEnv->FindClass("javax/microedition/khronos/egl/EGL10"));
    jEGLSurfaceImplClass = (jclass) jEnv->NewGlobalRef(jEnv->FindClass("com/google/android/gles_jni/EGLSurfaceImpl"));
    jEGLContextImplClass = (jclass) jEnv->NewGlobalRef(jEnv->FindClass("com/google/android/gles_jni/EGLContextImpl"));
    jEGLConfigImplClass = (jclass) jEnv->NewGlobalRef(jEnv->FindClass("com/google/android/gles_jni/EGLConfigImpl"));
    jEGLDisplayImplClass = (jclass) jEnv->NewGlobalRef(jEnv->FindClass("com/google/android/gles_jni/EGLDisplayImpl"));

    jStringClass = (jclass) jEnv->NewGlobalRef(jEnv->FindClass("java/lang/String"));

    jSurfaceClass = (jclass) jEnv->NewGlobalRef(jEnv->FindClass("android/view/Surface"));

    PRInt32 apiVersion = 0;
    if (!GetStaticIntField("android/os/Build$VERSION", "SDK_INT", &apiVersion, jEnv))
        ALOG_BRIDGE("Failed to find API version");

    if (apiVersion <= 8 /* Froyo */)
        jSurfacePointerField = jEnv->GetFieldID(jSurfaceClass, "mSurface", "I");
    else /* not Froyo */
        jSurfacePointerField = jEnv->GetFieldID(jSurfaceClass, "mNativeSurface", "I");

    jNotifyWakeLockChanged = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "notifyWakeLockChanged", "(Ljava/lang/String;Ljava/lang/String;)V");

#ifdef MOZ_JAVA_COMPOSITOR
    jPumpMessageLoop = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "pumpMessageLoop", "()V");

    jAddPluginView = jEnv->GetStaticMethodID(jGeckoAppShellClass, "addPluginView", "(Landroid/view/View;IIIIZ)V");
    jRemovePluginView = jEnv->GetStaticMethodID(jGeckoAppShellClass, "removePluginView", "(Landroid/view/View;Z)V");

    jCreateSurface = jEnv->GetStaticMethodID(jGeckoAppShellClass, "createSurface", "()Landroid/view/Surface;");
    jShowSurface = jEnv->GetStaticMethodID(jGeckoAppShellClass, "showSurface", "(Landroid/view/Surface;IIIIZZ)V");
    jHideSurface = jEnv->GetStaticMethodID(jGeckoAppShellClass, "hideSurface", "(Landroid/view/Surface;)V");
    jDestroySurface = jEnv->GetStaticMethodID(jGeckoAppShellClass, "destroySurface", "(Landroid/view/Surface;)V");

    jLayerView = (jclass) jEnv->NewGlobalRef(jEnv->FindClass("org/mozilla/gecko/gfx/LayerView"));

    AndroidGLController::Init(jEnv);
    AndroidEGLObject::Init(jEnv);
#else
    jAddPluginView = jEnv->GetStaticMethodID(jGeckoAppShellClass, "addPluginView", "(Landroid/view/View;DDDD)V");
    jRemovePluginView = jEnv->GetStaticMethodID(jGeckoAppShellClass, "removePluginView", "(Landroid/view/View;)V");
#endif

    InitAndroidJavaWrappers(jEnv);

    // jEnv should NOT be cached here by anything -- the jEnv here
    // is not valid for the real gecko main thread, which is set
    // at SetMainThread time.

    return true;
}

bool
AndroidBridge::SetMainThread(void *thr)
{
    ALOG_BRIDGE("AndroidBridge::SetMainThread");
    if (thr) {
        mThread = thr;
        mJavaVM->GetEnv((void**) &mJNIEnv, JNI_VERSION_1_2);
        return (bool) mJNIEnv;
    }

    mJNIEnv = nsnull;
    mThread = nsnull;
    return true;
}

void
AndroidBridge::NotifyIME(int aType, int aState)
{
    ALOG_BRIDGE("AndroidBridge::NotifyIME");

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env, 0);
    env->CallStaticVoidMethod(sBridge->mGeckoAppShellClass, 
                              sBridge->jNotifyIME,  aType, aState);
}

void
AndroidBridge::NotifyIMEEnabled(int aState, const nsAString& aTypeHint,
                                const nsAString& aActionHint)
{
    ALOG_BRIDGE("AndroidBridge::NotifyIMEEnabled");
    if (!sBridge)
        return;

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env);
    nsPromiseFlatString typeHint(aTypeHint);
    nsPromiseFlatString actionHint(aActionHint);

    jvalue args[4];
    args[0].i = aState;
    args[1].l = env->NewString(typeHint.get(), typeHint.Length());
    args[2].l = env->NewString(actionHint.get(), actionHint.Length());
    args[3].z = false;

    PRInt32 landscapeFS;
    if (NS_SUCCEEDED(Preferences::GetInt(IME_FULLSCREEN_PREF, &landscapeFS))) {
        if (landscapeFS == 1) {
            args[3].z = true;
        } else if (landscapeFS == -1){
            if (NS_SUCCEEDED(
                  Preferences::GetInt(IME_FULLSCREEN_THRESHOLD_PREF,
                                      &landscapeFS))) {
                // the threshold is hundreths of inches, so convert the 
                // threshold to pixels and multiply the height by 100
                if (nsWindow::GetAndroidScreenBounds().height  * 100 < 
                    landscapeFS * Bridge()->GetDPI()) {
                    args[3].z = true;
                }
            }

        }
    }

    env->CallStaticVoidMethodA(sBridge->mGeckoAppShellClass,
                                 sBridge->jNotifyIMEEnabled, args);
}

void
AndroidBridge::NotifyIMEChange(const PRUnichar *aText, PRUint32 aTextLen,
                               int aStart, int aEnd, int aNewEnd)
{
    ALOG_BRIDGE("AndroidBridge::NotifyIMEChange");
    if (!sBridge)
        return;

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;
    AutoLocalJNIFrame jniFrame(env);

    jvalue args[4];
    args[0].l = env->NewString(aText, aTextLen);
    args[1].i = aStart;
    args[2].i = aEnd;
    args[3].i = aNewEnd;
    env->CallStaticVoidMethodA(sBridge->mGeckoAppShellClass,
                                     sBridge->jNotifyIMEChange, args);
}

void
AndroidBridge::AcknowledgeEventSync()
{
    ALOG_BRIDGE("AndroidBridge::AcknowledgeEventSync");

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env, 0);
    env->CallStaticVoidMethod(mGeckoAppShellClass, jAcknowledgeEventSync);
}

void
AndroidBridge::EnableLocation(bool aEnable)
{
    ALOG_BRIDGE("AndroidBridge::EnableLocation");

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env, 0);
    env->CallStaticVoidMethod(mGeckoAppShellClass, jEnableLocation, aEnable);
}

void
AndroidBridge::EnableLocationHighAccuracy(bool aEnable)
{
    ALOG_BRIDGE("AndroidBridge::EnableLocationHighAccuracy");
    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env, 0);
    env->CallStaticVoidMethod(mGeckoAppShellClass, jEnableLocationHighAccuracy, aEnable);
}

void
AndroidBridge::EnableSensor(int aSensorType)
{
    ALOG_BRIDGE("AndroidBridge::EnableSensor");
    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env, 0);
    env->CallStaticVoidMethod(mGeckoAppShellClass, jEnableSensor, aSensorType);
}

void
AndroidBridge::DisableSensor(int aSensorType)
{
    ALOG_BRIDGE("AndroidBridge::DisableSensor");
    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;
    AutoLocalJNIFrame jniFrame(env, 0);
    env->CallStaticVoidMethod(mGeckoAppShellClass, jDisableSensor, aSensorType);
}

void
AndroidBridge::ReturnIMEQueryResult(const PRUnichar *aResult, PRUint32 aLen,
                                    int aSelStart, int aSelLen)
{
    ALOG_BRIDGE("AndroidBridge::ReturnIMEQueryResult");

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env);
    jvalue args[3];
    args[0].l = env->NewString(aResult, aLen);
    args[1].i = aSelStart;
    args[2].i = aSelLen;
    env->CallStaticVoidMethodA(mGeckoAppShellClass,
                               jReturnIMEQueryResult, args);
}

void
AndroidBridge::ScheduleRestart()
{
    ALOG_BRIDGE("scheduling reboot");

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env, 0);
    env->CallStaticVoidMethod(mGeckoAppShellClass, jScheduleRestart);
}

void
AndroidBridge::NotifyXreExit()
{
    ALOG_BRIDGE("xre exiting");

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env, 0);
    env->CallStaticVoidMethod(mGeckoAppShellClass, jNotifyXreExit);
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
            static_cast<jstring>(aJNIEnv->GetObjectArrayElement(jArr, i)), aJNIEnv);
        nsJNIString isDefault(
            static_cast<jstring>(aJNIEnv->GetObjectArrayElement(jArr, i + 1)), aJNIEnv);
        nsJNIString packageName( 
            static_cast<jstring>(aJNIEnv->GetObjectArrayElement(jArr, i + 2)), aJNIEnv);
        nsJNIString className( 
            static_cast<jstring>(aJNIEnv->GetObjectArrayElement(jArr, i + 3)), aJNIEnv);
        nsIHandlerApp* app = nsOSHelperAppService::
            CreateAndroidHandlerApp(name, className, packageName,
                                    className, aMimeType, aAction);
        
        aHandlersArray->AppendElement(app, false);
        if (aDefaultApp && isDefault.Length() > 0)
            *aDefaultApp = app;
    }
}

bool
AndroidBridge::GetHandlersForMimeType(const char *aMimeType,
                                      nsIMutableArray *aHandlersArray,
                                      nsIHandlerApp **aDefaultApp,
                                      const nsAString& aAction)
{
    ALOG_BRIDGE("AndroidBridge::GetHandlersForMimeType");

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return false;

    AutoLocalJNIFrame jniFrame(env); 
    NS_ConvertUTF8toUTF16 wMimeType(aMimeType);
    jstring jstrMimeType =
        env->NewString(wMimeType.get(), wMimeType.Length());

    jstring jstrAction = env->NewString(nsPromiseFlatString(aAction).get(),
                                            aAction.Length());

    jobject obj = env->CallStaticObjectMethod(mGeckoAppShellClass,
                                                  jGetHandlersForMimeType,
                                                  jstrMimeType, jstrAction);
    if (jniFrame.CheckForException())
        return false;

    jobjectArray arr = static_cast<jobjectArray>(obj);
    if (!arr)
        return false;

    jsize len = env->GetArrayLength(arr);

    if (!aHandlersArray)
        return len > 0;

    getHandlersFromStringArray(env, arr, len, aHandlersArray, 
                               aDefaultApp, aAction,
                               nsDependentCString(aMimeType));
    return true;
}

bool
AndroidBridge::GetHandlersForURL(const char *aURL,
                                 nsIMutableArray* aHandlersArray,
                                 nsIHandlerApp **aDefaultApp,
                                 const nsAString& aAction)
{
    ALOG_BRIDGE("AndroidBridge::GetHandlersForURL");

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return false;

    AutoLocalJNIFrame jniFrame(env); 
    NS_ConvertUTF8toUTF16 wScheme(aURL);
    jstring jstrScheme = env->NewString(wScheme.get(), wScheme.Length());
    jstring jstrAction = env->NewString(nsPromiseFlatString(aAction).get(),
                                            aAction.Length());

    jobject obj = env->CallStaticObjectMethod(mGeckoAppShellClass,
                                                  jGetHandlersForURL,
                                                  jstrScheme, jstrAction);
    if (jniFrame.CheckForException())
        return false;

    jobjectArray arr = static_cast<jobjectArray>(obj);
    if (!arr)
        return false;

    jsize len = env->GetArrayLength(arr);

    if (!aHandlersArray)
        return len > 0;

    getHandlersFromStringArray(env, arr, len, aHandlersArray, 
                               aDefaultApp, aAction);
    return true;
}

bool
AndroidBridge::OpenUriExternal(const nsACString& aUriSpec, const nsACString& aMimeType,
                               const nsAString& aPackageName, const nsAString& aClassName,
                               const nsAString& aAction, const nsAString& aTitle)
{
    ALOG_BRIDGE("AndroidBridge::OpenUriExternal");

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return false;

    AutoLocalJNIFrame jniFrame(env); 
    NS_ConvertUTF8toUTF16 wUriSpec(aUriSpec);
    NS_ConvertUTF8toUTF16 wMimeType(aMimeType);

    jstring jstrUri = env->NewString(wUriSpec.get(), wUriSpec.Length());
    jstring jstrType = env->NewString(wMimeType.get(), wMimeType.Length());

    jstring jstrPackage = env->NewString(nsPromiseFlatString(aPackageName).get(),
                                             aPackageName.Length());
    jstring jstrClass = env->NewString(nsPromiseFlatString(aClassName).get(),
                                           aClassName.Length());
    jstring jstrAction = env->NewString(nsPromiseFlatString(aAction).get(),
                                            aAction.Length());
    jstring jstrTitle = env->NewString(nsPromiseFlatString(aTitle).get(),
                                           aTitle.Length());

    bool ret = env->CallStaticBooleanMethod(mGeckoAppShellClass,
                                            jOpenUriExternal,
                                            jstrUri, jstrType, jstrPackage, 
                                            jstrClass, jstrAction, jstrTitle);
    if (jniFrame.CheckForException())
        return false;

    return ret;
}

void
AndroidBridge::GetMimeTypeFromExtensions(const nsACString& aFileExt, nsCString& aMimeType)
{
    ALOG_BRIDGE("AndroidBridge::GetMimeTypeFromExtensions");

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env); 
    NS_ConvertUTF8toUTF16 wFileExt(aFileExt);
    jstring jstrExt = env->NewString(wFileExt.get(), wFileExt.Length());
    jstring jstrType =  static_cast<jstring>(
        env->CallStaticObjectMethod(mGeckoAppShellClass,
                                        jGetMimeTypeFromExtensions,
                                        jstrExt));
    if (jniFrame.CheckForException())
        return;

    nsJNIString jniStr(jstrType, env);
    aMimeType.Assign(NS_ConvertUTF16toUTF8(jniStr.get()));
}

void
AndroidBridge::GetExtensionFromMimeType(const nsACString& aMimeType, nsACString& aFileExt)
{
    ALOG_BRIDGE("AndroidBridge::GetExtensionFromMimeType");

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env); 
    NS_ConvertUTF8toUTF16 wMimeType(aMimeType);
    jstring jstrType = env->NewString(wMimeType.get(), wMimeType.Length());
    jstring jstrExt = static_cast<jstring>(
        env->CallStaticObjectMethod(mGeckoAppShellClass,
                                        jGetExtensionFromMimeType,
                                        jstrType));
    if (jniFrame.CheckForException())
        return;

    nsJNIString jniStr(jstrExt, env);
    aFileExt.Assign(NS_ConvertUTF16toUTF8(jniStr.get()));
}

void
AndroidBridge::MoveTaskToBack()
{
    ALOG_BRIDGE("AndroidBridge::MoveTaskToBack");

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env, 0);
    env->CallStaticVoidMethod(mGeckoAppShellClass, jMoveTaskToBack);
}

bool
AndroidBridge::GetClipboardText(nsAString& aText)
{
    ALOG_BRIDGE("AndroidBridge::GetClipboardText");

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return false;

    AutoLocalJNIFrame jniFrame(env);
    jstring jstrType = static_cast<jstring>(
        env->CallStaticObjectMethod(mGeckoAppShellClass,
                                    jGetClipboardText));
    if (jniFrame.CheckForException() || !jstrType)
        return false;

    nsJNIString jniStr(jstrType, env);
    aText.Assign(jniStr);
    return true;
}

void
AndroidBridge::SetClipboardText(const nsAString& aText)
{
    ALOG_BRIDGE("AndroidBridge::SetClipboardText");

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env); 
    jstring jstr = env->NewString(nsPromiseFlatString(aText).get(),
                                      aText.Length());
    env->CallStaticVoidMethod(mGeckoAppShellClass, jSetClipboardText, jstr);
}

bool
AndroidBridge::ClipboardHasText()
{
    ALOG_BRIDGE("AndroidBridge::ClipboardHasText");

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return false;

    AutoLocalJNIFrame jniFrame(env);
    jstring jstrType = static_cast<jstring>(
        env->CallStaticObjectMethod(mGeckoAppShellClass,
                                    jGetClipboardText));
    if (jniFrame.CheckForException() || !jstrType)
        return false;

    return true;
}

void
AndroidBridge::EmptyClipboard()
{
    ALOG_BRIDGE("AndroidBridge::EmptyClipboard");

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env, 0);
    env->CallStaticVoidMethod(mGeckoAppShellClass, jSetClipboardText, nsnull);
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

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env); 

    if (nsAppShell::gAppShell && aAlertListener)
        nsAppShell::gAppShell->AddObserver(aAlertName, aAlertListener);

    jvalue args[5];
    args[0].l = env->NewString(nsPromiseFlatString(aImageUrl).get(), aImageUrl.Length());
    args[1].l = env->NewString(nsPromiseFlatString(aAlertTitle).get(), aAlertTitle.Length());
    args[2].l = env->NewString(nsPromiseFlatString(aAlertText).get(), aAlertText.Length());
    args[3].l = env->NewString(nsPromiseFlatString(aAlertCookie).get(), aAlertCookie.Length());
    args[4].l = env->NewString(nsPromiseFlatString(aAlertName).get(), aAlertName.Length());
    env->CallStaticVoidMethodA(mGeckoAppShellClass, jShowAlertNotification, args);
}

void
AndroidBridge::AlertsProgressListener_OnProgress(const nsAString& aAlertName,
                                                 PRInt64 aProgress,
                                                 PRInt64 aProgressMax,
                                                 const nsAString& aAlertText)
{
    ALOG_BRIDGE("AlertsProgressListener_OnProgress");
    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env); 

    jstring jstrName = env->NewString(nsPromiseFlatString(aAlertName).get(), aAlertName.Length());
    jstring jstrText = env->NewString(nsPromiseFlatString(aAlertText).get(), aAlertText.Length());
    env->CallStaticVoidMethod(mGeckoAppShellClass, jAlertsProgressListener_OnProgress,
                                  jstrName, aProgress, aProgressMax, jstrText);
}

void
AndroidBridge::AlertsProgressListener_OnCancel(const nsAString& aAlertName)
{
    ALOG_BRIDGE("AlertsProgressListener_OnCancel");

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env); 

    jstring jstrName = env->NewString(nsPromiseFlatString(aAlertName).get(), aAlertName.Length());
    env->CallStaticVoidMethod(mGeckoAppShellClass, jAlertsProgressListener_OnCancel, jstrName);
}


int
AndroidBridge::GetDPI()
{
    ALOG_BRIDGE("AndroidBridge::GetDPI");
    const int DEFAULT_DPI = 72; // what is a better value?
    JNIEnv *env = GetJNIEnv();
    if (!env)
        return DEFAULT_DPI;
    AutoLocalJNIFrame jniFrame(env);

    int dpi = (int)env->CallStaticIntMethod(mGeckoAppShellClass, jGetDpi);
    if (jniFrame.CheckForException())
        return DEFAULT_DPI;

    return dpi;
}

void
AndroidBridge::ShowFilePickerForExtensions(nsAString& aFilePath, const nsAString& aExtensions)
{
    ALOG_BRIDGE("AndroidBridge::ShowFilePickerForExtensions");

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env); 
    jstring jstrFilers = env->NewString(nsPromiseFlatString(aExtensions).get(),
                                        aExtensions.Length());
    jstring jstr = static_cast<jstring>(env->CallStaticObjectMethod(
                                        mGeckoAppShellClass,
                                        jShowFilePickerForExtensions, jstrFilers));
    if (jniFrame.CheckForException())
        return;

    aFilePath.Assign(nsJNIString(jstr, env));
}

void
AndroidBridge::ShowFilePickerForMimeType(nsAString& aFilePath, const nsAString& aMimeType)
{
    ALOG_BRIDGE("AndroidBridge::ShowFilePickerForMimeType");

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env); 
    jstring jstrFilers = env->NewString(nsPromiseFlatString(aMimeType).get(),
                                            aMimeType.Length());
    jstring jstr = static_cast<jstring>(env->CallStaticObjectMethod(
                                            mGeckoAppShellClass,
                                            jShowFilePickerForMimeType, jstrFilers));
    if (jniFrame.CheckForException())
        return;

    aFilePath.Assign(nsJNIString(jstr, env));
}

void
AndroidBridge::ShowFilePickerAsync(const nsAString& aMimeType, nsFilePickerCallback* callback)
{
    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env); 
    jstring jMimeType = env->NewString(nsPromiseFlatString(aMimeType).get(),
                                       aMimeType.Length());
    callback->AddRef();
    env->CallStaticVoidMethod(mGeckoAppShellClass, jShowFilePickerAsync, jMimeType, (jlong) callback);
}

void
AndroidBridge::SetFullScreen(bool aFullScreen)
{
    ALOG_BRIDGE("AndroidBridge::SetFullScreen");

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env, 0);
    env->CallStaticVoidMethod(mGeckoAppShellClass, jSetFullScreen, aFullScreen);
}

void
AndroidBridge::HideProgressDialogOnce()
{
    static bool once = false;
    if (once)
        return;

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env, 0);
    env->CallStaticVoidMethod(mGeckoAppShellClass, jHideProgressDialog);
    if (jniFrame.CheckForException())
        return;

    once = true;
}

void
AndroidBridge::PerformHapticFeedback(bool aIsLongPress)
{
    ALOG_BRIDGE("AndroidBridge::PerformHapticFeedback");

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env, 0);
    env->CallStaticVoidMethod(mGeckoAppShellClass,
                                    jPerformHapticFeedback, aIsLongPress);
}

void
AndroidBridge::Vibrate(const nsTArray<PRUint32>& aPattern)
{
    ALOG_BRIDGE("AndroidBridge::Vibrate");
    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env);
    PRUint32 len = aPattern.Length();
    if (!len) {
        ALOG_BRIDGE("  invalid 0-length array");
        return;
    }

    // It's clear if this worth special-casing, but it creates less
    // java junk, so dodges the GC.
    if (len == 1) {
        jlong d = aPattern[0];
        if (d < 0) {
            ALOG_BRIDGE("  invalid vibration duration < 0");
            return;
        }
        env->CallStaticVoidMethod(mGeckoAppShellClass, jVibrate1, d);
        return;
    }

    // First element of the array vibrate() expects is how long to wait
    // *before* vibrating.  For us, this is always 0.

    jlongArray array = env->NewLongArray(len + 1);
    if (!array) {
        ALOG_BRIDGE("  failed to allocate array");
        return;
    }

    jlong* elts = env->GetLongArrayElements(array, nsnull);
    elts[0] = 0;
    for (PRUint32 i = 0; i < aPattern.Length(); ++i) {
        jlong d = aPattern[i];
        if (d < 0) {
            ALOG_BRIDGE("  invalid vibration duration < 0");
            env->ReleaseLongArrayElements(array, elts, JNI_ABORT);
            return;
        }
        elts[i + 1] = d;
    }
    env->ReleaseLongArrayElements(array, elts, 0);

    env->CallStaticVoidMethod(mGeckoAppShellClass, jVibrateA,
                                  array, -1/*don't repeat*/);
    // GC owns |array| now?
}

void
AndroidBridge::CancelVibrate()
{
    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env, 0);
    env->CallStaticVoidMethod(mGeckoAppShellClass, jCancelVibrate);
}

bool
AndroidBridge::IsNetworkLinkUp()
{
    ALOG_BRIDGE("AndroidBridge::IsNetworkLinkUp");

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return false;

    AutoLocalJNIFrame jniFrame(env, 0);
    bool ret = !!env->CallStaticBooleanMethod(mGeckoAppShellClass, jIsNetworkLinkUp);
    if (jniFrame.CheckForException())
        return false;

    return ret;
}

bool
AndroidBridge::IsNetworkLinkKnown()
{
    ALOG_BRIDGE("AndroidBridge::IsNetworkLinkKnown");

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return false;

    AutoLocalJNIFrame jniFrame(env, 0);
    bool ret = !!env->CallStaticBooleanMethod(mGeckoAppShellClass, jIsNetworkLinkKnown);
    if (jniFrame.CheckForException())
        return false;

    return ret;
}

void
AndroidBridge::SetSelectedLocale(const nsAString& aLocale)
{
    ALOG_BRIDGE("AndroidBridge::SetSelectedLocale");

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env); 
    jstring jLocale = env->NewString(PromiseFlatString(aLocale).get(), aLocale.Length());
    env->CallStaticVoidMethod(mGeckoAppShellClass, jSetSelectedLocale, jLocale);
}

void
AndroidBridge::GetSystemColors(AndroidSystemColors *aColors)
{
    ALOG_BRIDGE("AndroidBridge::GetSystemColors");

    NS_ASSERTION(aColors != nsnull, "AndroidBridge::GetSystemColors: aColors is null!");
    if (!aColors)
        return;

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env); 

    jobject obj = env->CallStaticObjectMethod(mGeckoAppShellClass, jGetSystemColors);
    if (jniFrame.CheckForException())
        return;

    jintArray arr = static_cast<jintArray>(obj);
    if (!arr)
        return;

    jsize len = env->GetArrayLength(arr);
    jint *elements = env->GetIntArrayElements(arr, 0);

    PRUint32 colorsCount = sizeof(AndroidSystemColors) / sizeof(nscolor);
    if (len < colorsCount)
        colorsCount = len;

    // Convert Android colors to nscolor by switching R and B in the ARGB 32 bit value
    nscolor *colors = (nscolor*)aColors;

    for (PRUint32 i = 0; i < colorsCount; i++) {
        PRUint32 androidColor = static_cast<PRUint32>(elements[i]);
        PRUint8 r = (androidColor & 0x00ff0000) >> 16;
        PRUint8 b = (androidColor & 0x000000ff);
        colors[i] = androidColor & 0xff00ff00 | b << 16 | r;
    }

    env->ReleaseIntArrayElements(arr, elements, 0);
}

void
AndroidBridge::GetIconForExtension(const nsACString& aFileExt, PRUint32 aIconSize, PRUint8 * const aBuf)
{
    ALOG_BRIDGE("AndroidBridge::GetIconForExtension");
    NS_ASSERTION(aBuf != nsnull, "AndroidBridge::GetIconForExtension: aBuf is null!");
    if (!aBuf)
        return;

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env); 

    nsString fileExt;
    CopyUTF8toUTF16(aFileExt, fileExt);
    jstring jstrFileExt = env->NewString(nsPromiseFlatString(fileExt).get(), fileExt.Length());
    
    jobject obj = env->CallStaticObjectMethod(mGeckoAppShellClass, jGetIconForExtension, jstrFileExt, aIconSize);
    if (jniFrame.CheckForException())
        return;

    jbyteArray arr = static_cast<jbyteArray>(obj);
    NS_ASSERTION(arr != nsnull, "AndroidBridge::GetIconForExtension: Returned pixels array is null!");
    if (!arr)
        return;

    jsize len = env->GetArrayLength(arr);
    jbyte *elements = env->GetByteArrayElements(arr, 0);

    PRUint32 bufSize = aIconSize * aIconSize * 4;
    NS_ASSERTION(len == bufSize, "AndroidBridge::GetIconForExtension: Pixels array is incomplete!");
    if (len == bufSize)
        memcpy(aBuf, elements, bufSize);

    env->ReleaseByteArrayElements(arr, elements, 0);
}

bool
AndroidBridge::GetShowPasswordSetting()
{
    ALOG_BRIDGE("AndroidBridge::GetShowPasswordSetting");

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return false;

    AutoLocalJNIFrame jniFrame(env, 0);
    bool ret = env->CallStaticBooleanMethod(mGeckoAppShellClass, jGetShowPasswordSetting);
    if (jniFrame.CheckForException())
        return false;

    return ret;
}

void
AndroidBridge::SetSurfaceView(jobject obj)
{
    mSurfaceView.Init(obj);
}

void
AndroidBridge::SetLayerClient(jobject obj)
{
    AndroidGeckoLayerClient *client = new AndroidGeckoLayerClient();
    client->Init(obj);
    mLayerClient = client;
}

void
AndroidBridge::ShowInputMethodPicker()
{
    ALOG_BRIDGE("AndroidBridge::ShowInputMethodPicker");

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env, 0);
    env->CallStaticVoidMethod(mGeckoAppShellClass, jShowInputMethodPicker);
}

void *
AndroidBridge::CallEglCreateWindowSurface(void *dpy, void *config, AndroidGeckoSurfaceView &sview)
{
    ALOG_BRIDGE("AndroidBridge::CallEglCreateWindowSurface");

    JNIEnv *env = GetJNIForThread();        // called on the compositor thread
    if (!env)
        return NULL;

    AutoLocalJNIFrame jniFrame(env);

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

    jobject surfaceHolder = sview.GetSurfaceHolder(&jniFrame);
    if (!surfaceHolder)
        return nsnull;

    // grab some fields and methods we'll need
    jmethodID constructConfig = env->GetMethodID(jEGLConfigImplClass, "<init>", "(I)V");
    jmethodID constructDisplay = env->GetMethodID(jEGLDisplayImplClass, "<init>", "(I)V");

    jmethodID getEgl = env->GetStaticMethodID(jEGLContextClass, "getEGL", "()Ljavax/microedition/khronos/egl/EGL;");
    jmethodID createWindowSurface = env->GetMethodID(jEGL10Class, "eglCreateWindowSurface", "(Ljavax/microedition/khronos/egl/EGLDisplay;Ljavax/microedition/khronos/egl/EGLConfig;Ljava/lang/Object;[I)Ljavax/microedition/khronos/egl/EGLSurface;");

    jobject egl = env->CallStaticObjectMethod(jEGLContextClass, getEgl);
    if (jniFrame.CheckForException())
        return nsnull;

    jobject jdpy = env->NewObject(jEGLDisplayImplClass, constructDisplay, (int) dpy);
    jobject jconf = env->NewObject(jEGLConfigImplClass, constructConfig, (int) config);

    // make the call
    jobject surf = env->CallObjectMethod(egl, createWindowSurface, jdpy, jconf, surfaceHolder, NULL);
    if (jniFrame.CheckForException() || !surf)
        return nsnull;

    jfieldID sfield = env->GetFieldID(jEGLSurfaceImplClass, "mEGLSurface", "I");

    jint realSurface = env->GetIntField(surf, sfield);

    return (void*) realSurface;
}

static AndroidGLController sController;

void
AndroidBridge::RegisterCompositor()
{
    ALOG_BRIDGE("AndroidBridge::RegisterCompositor");
    JNIEnv *env = GetJNIForThread();    // called on the compositor thread
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env);

    jmethodID registerCompositor = env->GetStaticMethodID(jLayerView, "registerCxxCompositor", "()Lorg/mozilla/gecko/gfx/GLController;");

    jobject glController = env->CallStaticObjectMethod(jLayerView, registerCompositor);
    if (jniFrame.CheckForException())
        return;

    sController.Acquire(env, glController);
    sController.SetGLVersion(2);
}

EGLSurface
AndroidBridge::ProvideEGLSurface()
{
    sController.WaitForValidSurface();
    return sController.ProvideEGLSurface();
}

bool
AndroidBridge::GetStaticIntField(const char *className, const char *fieldName, PRInt32* aInt, JNIEnv* env /* = nsnull */)
{
    ALOG_BRIDGE("AndroidBridge::GetStaticIntField %s", fieldName);

    if (!env) {
        env = GetJNIEnv();
        if (!env)
            return false;
    }

    AutoLocalJNIFrame jniFrame(env);
    jclass cls = env->FindClass(className);
    if (!cls)
        return false;

    jfieldID field = env->GetStaticFieldID(cls, fieldName, "I");
    if (!field)
        return false;

    *aInt = static_cast<PRInt32>(env->GetStaticIntField(cls, field));

    return true;
}

bool
AndroidBridge::GetStaticStringField(const char *className, const char *fieldName, nsAString &result, JNIEnv* env /* = nsnull */)
{
    ALOG_BRIDGE("AndroidBridge::GetStaticStringField %s", fieldName);

    if (!env) {
        env = GetJNIEnv();
        if (!env)
            return false;
    }

    AutoLocalJNIFrame jniFrame(env);
    jclass cls = env->FindClass(className);
    if (!cls)
        return false;

    jfieldID field = env->GetStaticFieldID(cls, fieldName, "Ljava/lang/String;");
    if (!field)
        return false;

    jstring jstr = (jstring) env->GetStaticObjectField(cls, field);
    if (!jstr)
        return false;

    result.Assign(nsJNIString(jstr, env));
    return true;
}

void
AndroidBridge::SetKeepScreenOn(bool on)
{
    ALOG_BRIDGE("AndroidBridge::SetKeepScreenOn");

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env, 0);
    env->CallStaticVoidMethod(sBridge->mGeckoAppShellClass,
                              sBridge->jSetKeepScreenOn, on);
}

// Available for places elsewhere in the code to link to.
bool
mozilla_AndroidBridge_SetMainThread(void *thr)
{
    return AndroidBridge::Bridge()->SetMainThread(thr);
}

jclass GetGeckoAppShellClass()
{
    return mozilla::AndroidBridge::GetGeckoAppShellClass();
}

void
AndroidBridge::ScanMedia(const nsAString& aFile, const nsACString& aMimeType)
{
    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env); 
    jstring jstrFile = env->NewString(nsPromiseFlatString(aFile).get(), aFile.Length());

    nsString mimeType2;
    CopyUTF8toUTF16(aMimeType, mimeType2);
    jstring jstrMimeTypes = env->NewString(nsPromiseFlatString(mimeType2).get(), mimeType2.Length());

    env->CallStaticVoidMethod(mGeckoAppShellClass, jScanMedia, jstrFile, jstrMimeTypes);
}

void
AndroidBridge::CreateShortcut(const nsAString& aTitle, const nsAString& aURI, const nsAString& aIconData, const nsAString& aIntent)
{
    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env); 
    jstring jstrTitle = env->NewString(nsPromiseFlatString(aTitle).get(), aTitle.Length());
    jstring jstrURI = env->NewString(nsPromiseFlatString(aURI).get(), aURI.Length());
    jstring jstrIconData = env->NewString(nsPromiseFlatString(aIconData).get(), aIconData.Length());
    jstring jstrIntent = env->NewString(nsPromiseFlatString(aIntent).get(), aIntent.Length());
  
    if (!jstrURI || !jstrTitle || !jstrIconData)
        return;
    
    env->CallStaticVoidMethod(mGeckoAppShellClass, jCreateShortcut, jstrTitle, jstrURI, jstrIconData, jstrIntent);
}

void
AndroidBridge::PostToJavaThread(JNIEnv *env, nsIRunnable* aRunnable, bool aMainThread)
{
    mRunnableQueue.AppendObject(aRunnable);

    AutoLocalJNIFrame jniFrame(env, 0);
    env->CallStaticVoidMethod(mGeckoAppShellClass, jPostToJavaThread, (jboolean)aMainThread);
}

void
AndroidBridge::ExecuteNextRunnable(JNIEnv *env)
{
    if (mRunnableQueue.Count() > 0) {
        nsIRunnable* r = mRunnableQueue[0];
        r->Run();
        mRunnableQueue.RemoveObjectAt(0);
    }
}

void*
AndroidBridge::GetNativeSurface(JNIEnv* env, jobject surface) {
    if (!env || !mHasNativeWindowFallback)
        return nsnull;

    return (void*)env->GetIntField(surface, jSurfacePointerField);
}

void
AndroidBridge::OpenGraphicsLibraries()
{
    if (!mOpenedGraphicsLibraries) {
        // Try to dlopen libjnigraphics.so for direct bitmap access on
        // Android 2.2+ (API level 8)
        mOpenedGraphicsLibraries = true;
        mHasNativeWindowAccess = false;
        mHasNativeWindowFallback = false;
        mHasNativeBitmapAccess = false;

        void *handle = dlopen("libjnigraphics.so", RTLD_LAZY | RTLD_LOCAL);
        if (handle) {
            AndroidBitmap_getInfo = (int (*)(JNIEnv *, jobject, void *))dlsym(handle, "AndroidBitmap_getInfo");
            AndroidBitmap_lockPixels = (int (*)(JNIEnv *, jobject, void **))dlsym(handle, "AndroidBitmap_lockPixels");
            AndroidBitmap_unlockPixels = (int (*)(JNIEnv *, jobject))dlsym(handle, "AndroidBitmap_unlockPixels");

            mHasNativeBitmapAccess = AndroidBitmap_getInfo && AndroidBitmap_lockPixels && AndroidBitmap_unlockPixels;

            ALOG_BRIDGE("Successfully opened libjnigraphics.so, have native bitmap access? %d", mHasNativeBitmapAccess);
        }

        // Try to dlopen libandroid.so for and native window access on
        // Android 2.3+ (API level 9)
        handle = dlopen("libandroid.so", RTLD_LAZY | RTLD_LOCAL);
        if (handle) {
            ANativeWindow_fromSurface = (void* (*)(JNIEnv*, jobject))dlsym(handle, "ANativeWindow_fromSurface");
            ANativeWindow_release = (void (*)(void*))dlsym(handle, "ANativeWindow_release");
            ANativeWindow_setBuffersGeometry = (int (*)(void*, int, int, int)) dlsym(handle, "ANativeWindow_setBuffersGeometry");
            ANativeWindow_lock = (int (*)(void*, void*, void*)) dlsym(handle, "ANativeWindow_lock");
            ANativeWindow_unlockAndPost = (int (*)(void*))dlsym(handle, "ANativeWindow_unlockAndPost");

            mHasNativeWindowAccess = ANativeWindow_fromSurface && ANativeWindow_release && ANativeWindow_lock && ANativeWindow_unlockAndPost;

            ALOG_BRIDGE("Successfully opened libandroid.so, have native window access? %d", mHasNativeWindowAccess);
        }

        if (mHasNativeWindowAccess)
            return;

        // Look up Surface functions, used for native window (surface) fallback
        handle = dlopen("libsurfaceflinger_client.so", RTLD_LAZY);
        if (handle) {
            Surface_lock = (int (*)(void*, void*, void*, bool))dlsym(handle, "_ZN7android7Surface4lockEPNS0_11SurfaceInfoEPNS_6RegionEb");
            Surface_unlockAndPost = (int (*)(void*))dlsym(handle, "_ZN7android7Surface13unlockAndPostEv");

            handle = dlopen("libui.so", RTLD_LAZY);
            if (handle) {
                Region_constructor = (void (*)(void*))dlsym(handle, "_ZN7android6RegionC1Ev");
                Region_set = (void (*)(void*, void*))dlsym(handle, "_ZN7android6Region3setERKNS_4RectE");

                mHasNativeWindowFallback = Surface_lock && Surface_unlockAndPost && Region_constructor && Region_set;
            }
        }
    }
}

void
AndroidBridge::FireAndWaitForTracerEvent() {
    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env, 0);
    env->CallStaticVoidMethod(mGeckoAppShellClass, 
                              jFireAndWaitForTracerEvent);
}


namespace mozilla {
    class TracerRunnable : public nsRunnable{
    public:
        TracerRunnable() {
            mTracerLock = new Mutex("TracerRunnable");
            mTracerCondVar = new CondVar(*mTracerLock, "TracerRunnable");
            mMainThread = do_GetMainThread();
            
        }
        ~TracerRunnable() {
            delete mTracerCondVar;
            delete mTracerLock;
            mTracerLock = nsnull;
            mTracerCondVar = nsnull;
        }

        virtual nsresult Run() {
            MutexAutoLock lock(*mTracerLock);
            if (!AndroidBridge::Bridge())
                return NS_OK;
            
            AndroidBridge::Bridge()->FireAndWaitForTracerEvent();
            mHasRun = true;
            mTracerCondVar->Notify();
            return NS_OK;
        }
        
        bool Fire() {
            if (!mTracerLock || !mTracerCondVar)
                return false;
            MutexAutoLock lock(*mTracerLock);
            mHasRun = false;
            mMainThread->Dispatch(this, NS_DISPATCH_NORMAL);
            while (!mHasRun)
                mTracerCondVar->Wait();
            return true;
        }

        void Signal() {
            MutexAutoLock lock(*mTracerLock);
            mHasRun = true;
            mTracerCondVar->Notify();
        }
    private:
        Mutex* mTracerLock;
        CondVar* mTracerCondVar;
        bool mHasRun;
        nsCOMPtr<nsIThread> mMainThread;

    };
    nsCOMPtr<TracerRunnable> sTracerRunnable;

    bool InitWidgetTracing() {
        if (!sTracerRunnable)
            sTracerRunnable = new TracerRunnable();
        return true;
    }

    void CleanUpWidgetTracing() {
        if (sTracerRunnable)
            delete sTracerRunnable;
        sTracerRunnable = nsnull;
    }

    bool FireAndWaitForTracerEvent() {
        if (sTracerRunnable)
            return sTracerRunnable->Fire();
        return false;
    }

   void SignalTracerThread()
   {
       if (sTracerRunnable)
           return sTracerRunnable->Signal();
   }

}
bool
AndroidBridge::HasNativeBitmapAccess()
{
    OpenGraphicsLibraries();

    return mHasNativeBitmapAccess;
}

bool
AndroidBridge::ValidateBitmap(jobject bitmap, int width, int height)
{
    // This structure is defined in Android API level 8's <android/bitmap.h>
    // Because we can't depend on this, we get the function pointers via dlsym
    // and define this struct ourselves.
    struct BitmapInfo {
        uint32_t width;
        uint32_t height;
        uint32_t stride;
        uint32_t format;
        uint32_t flags;
    };

    int err;
    struct BitmapInfo info = { 0, };

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return false;

    if ((err = AndroidBitmap_getInfo(env, bitmap, &info)) != 0) {
        ALOG_BRIDGE("AndroidBitmap_getInfo failed! (error %d)", err);
        return false;
    }

    if (info.width != width || info.height != height)
        return false;

    return true;
}

bool
AndroidBridge::InitCamera(const nsCString& contentType, PRUint32 camera, PRUint32 *width, PRUint32 *height, PRUint32 *fps)
{
    JNIEnv *env = GetJNIEnv();
    if (!env)
        return false;

    AutoLocalJNIFrame jniFrame(env); 

    NS_ConvertASCIItoUTF16 s(contentType);
    jstring jstrContentType = env->NewString(s.get(), NS_strlen(s.get()));

    jobject obj = env->CallStaticObjectMethod(mGeckoAppShellClass, jInitCamera, jstrContentType, camera, *width, *height);
    if (jniFrame.CheckForException())
        return false;

    jintArray arr = static_cast<jintArray>(obj);
    if (!arr)
        return false;

    jint *elements = env->GetIntArrayElements(arr, 0);

    *width = elements[1];
    *height = elements[2];
    *fps = elements[3];

    bool res = elements[0] == 1;

    env->ReleaseIntArrayElements(arr, elements, 0);

    return res;
}

void
AndroidBridge::CloseCamera()
{
    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env, 0);

    env->CallStaticVoidMethod(mGeckoAppShellClass, jCloseCamera);
}

void
AndroidBridge::EnableBatteryNotifications()
{
    ALOG_BRIDGE("AndroidBridge::EnableBatteryObserver");

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env, 0);
    env->CallStaticVoidMethod(mGeckoAppShellClass, jEnableBatteryNotifications);
}

void
AndroidBridge::DisableBatteryNotifications()
{
    ALOG_BRIDGE("AndroidBridge::DisableBatteryNotifications");

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env, 0);
    env->CallStaticVoidMethod(mGeckoAppShellClass, jDisableBatteryNotifications);
}

void
AndroidBridge::GetCurrentBatteryInformation(hal::BatteryInformation* aBatteryInfo)
{
    ALOG_BRIDGE("AndroidBridge::GetCurrentBatteryInformation");

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env); 

    // To prevent calling too many methods through JNI, the Java method returns
    // an array of double even if we actually want a double and a boolean.
    jobject obj = env->CallStaticObjectMethod(mGeckoAppShellClass, jGetCurrentBatteryInformation);
    if (jniFrame.CheckForException())
        return;

    jdoubleArray arr = static_cast<jdoubleArray>(obj);
    if (!arr || env->GetArrayLength(arr) != 3) {
        return;
    }

    jdouble* info = env->GetDoubleArrayElements(arr, 0);

    aBatteryInfo->level() = info[0];
    aBatteryInfo->charging() = info[1] == 1.0f;
    aBatteryInfo->remainingTime() = info[2];

    env->ReleaseDoubleArrayElements(arr, info, 0);
}

void
AndroidBridge::HandleGeckoMessage(const nsAString &aMessage, nsAString &aRet)
{
    ALOG_BRIDGE("%s", __PRETTY_FUNCTION__);

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env);
    jstring jMessage = env->NewString(nsPromiseFlatString(aMessage).get(), aMessage.Length());
    jstring returnMessage =  static_cast<jstring>(env->CallStaticObjectMethod(mGeckoAppShellClass, jHandleGeckoMessage, jMessage));
    if (jniFrame.CheckForException())
        return;

    nsJNIString jniStr(returnMessage, env);
    aRet.Assign(jniStr);
    ALOG_BRIDGE("leaving %s", __PRETTY_FUNCTION__);
}

void
AndroidBridge::CheckURIVisited(const nsAString& aURI)
{
    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env);
    jstring jstrURI = env->NewString(nsPromiseFlatString(aURI).get(), aURI.Length());
    env->CallStaticVoidMethod(mGeckoAppShellClass, jCheckUriVisited, jstrURI);
}

void
AndroidBridge::MarkURIVisited(const nsAString& aURI)
{
    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env);
    jstring jstrURI = env->NewString(nsPromiseFlatString(aURI).get(), aURI.Length());
    env->CallStaticVoidMethod(mGeckoAppShellClass, jMarkUriVisited, jstrURI);
}

PRUint16
AndroidBridge::GetNumberOfMessagesForText(const nsAString& aText)
{
    ALOG_BRIDGE("AndroidBridge::GetNumberOfMessagesForText");

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return 0;

    AutoLocalJNIFrame jniFrame(env); 
    jstring jText = env->NewString(PromiseFlatString(aText).get(), aText.Length());
    PRUint16 ret = env->CallStaticIntMethod(mGeckoAppShellClass, jNumberOfMessages, jText);
    if (jniFrame.CheckForException())
        return 0;

    return ret;
}

void
AndroidBridge::SendMessage(const nsAString& aNumber, const nsAString& aMessage, PRInt32 aRequestId, PRUint64 aProcessId)
{
    ALOG_BRIDGE("AndroidBridge::SendMessage");

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env);
    jstring jNumber = env->NewString(PromiseFlatString(aNumber).get(), aNumber.Length());
    jstring jMessage = env->NewString(PromiseFlatString(aMessage).get(), aMessage.Length());

    env->CallStaticVoidMethod(mGeckoAppShellClass, jSendMessage, jNumber, jMessage, aRequestId, aProcessId);
}

PRInt32
AndroidBridge::SaveSentMessage(const nsAString& aRecipient,
                                const nsAString& aBody, PRUint64 aDate)
{
    ALOG_BRIDGE("AndroidBridge::SaveSentMessage");

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return 0;

    AutoLocalJNIFrame jniFrame(env);
    jstring jRecipient = env->NewString(PromiseFlatString(aRecipient).get(), aRecipient.Length());
    jstring jBody = env->NewString(PromiseFlatString(aBody).get(), aBody.Length());
    PRInt32 ret = env->CallStaticIntMethod(mGeckoAppShellClass, jSaveSentMessage, jRecipient, jBody, aDate);
    if (jniFrame.CheckForException())
        return 0;

    return ret;
}

void
AndroidBridge::GetMessage(PRInt32 aMessageId, PRInt32 aRequestId, PRUint64 aProcessId)
{
    ALOG_BRIDGE("AndroidBridge::GetMessage");

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env, 0);
    env->CallStaticVoidMethod(mGeckoAppShellClass, jGetMessage, aMessageId, aRequestId, aProcessId);
}

void
AndroidBridge::DeleteMessage(PRInt32 aMessageId, PRInt32 aRequestId, PRUint64 aProcessId)
{
    ALOG_BRIDGE("AndroidBridge::DeleteMessage");

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env, 0);
    env->CallStaticVoidMethod(mGeckoAppShellClass, jDeleteMessage, aMessageId, aRequestId, aProcessId);
}

void
AndroidBridge::CreateMessageList(const dom::sms::SmsFilterData& aFilter, bool aReverse,
                                 PRInt32 aRequestId, PRUint64 aProcessId)
{
    ALOG_BRIDGE("AndroidBridge::CreateMessageList");

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env); 

    jobjectArray numbers =
        (jobjectArray)env->NewObjectArray(aFilter.numbers().Length(),
                                        jStringClass,
                                        env->NewStringUTF(""));

    for (PRUint32 i = 0; i < aFilter.numbers().Length(); ++i) {
        env->SetObjectArrayElement(numbers, i,
                                   env->NewStringUTF(NS_ConvertUTF16toUTF8(aFilter.numbers()[i]).get()));
    }

    env->CallStaticVoidMethod(mGeckoAppShellClass, jCreateMessageList,
                                aFilter.startDate(), aFilter.endDate(),
                                numbers, aFilter.numbers().Length(),
                                aFilter.delivery(), aReverse, aRequestId,
                                aProcessId);
}

void
AndroidBridge::GetNextMessageInList(PRInt32 aListId, PRInt32 aRequestId, PRUint64 aProcessId)
{
    ALOG_BRIDGE("AndroidBridge::GetNextMessageInList");

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env, 0);
    env->CallStaticVoidMethod(mGeckoAppShellClass, jGetNextMessageinList, aListId, aRequestId, aProcessId);
}

void
AndroidBridge::ClearMessageList(PRInt32 aListId)
{
    ALOG_BRIDGE("AndroidBridge::ClearMessageList");

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env, 0);
    env->CallStaticVoidMethod(mGeckoAppShellClass, jClearMessageList, aListId);
}

void
AndroidBridge::GetCurrentNetworkInformation(hal::NetworkInformation* aNetworkInfo)
{
    ALOG_BRIDGE("AndroidBridge::GetCurrentNetworkInformation");

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env); 

    // To prevent calling too many methods through JNI, the Java method returns
    // an array of double even if we actually want a double and a boolean.
    jobject obj = env->CallStaticObjectMethod(mGeckoAppShellClass, jGetCurrentNetworkInformation);
    if (jniFrame.CheckForException())
        return;

    jdoubleArray arr = static_cast<jdoubleArray>(obj);
    if (!arr || env->GetArrayLength(arr) != 2) {
        return;
    }

    jdouble* info = env->GetDoubleArrayElements(arr, 0);

    aNetworkInfo->bandwidth() = info[0];
    aNetworkInfo->canBeMetered() = info[1] == 1.0f;

    env->ReleaseDoubleArrayElements(arr, info, 0);
}

void
AndroidBridge::EnableNetworkNotifications()
{
    ALOG_BRIDGE("AndroidBridge::EnableNetworkNotifications");
    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env, 0);
    env->CallStaticVoidMethod(mGeckoAppShellClass, jEnableNetworkNotifications);
}

void
AndroidBridge::DisableNetworkNotifications()
{
    ALOG_BRIDGE("AndroidBridge::DisableNetworkNotifications");
    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env, 0);
    env->CallStaticVoidMethod(mGeckoAppShellClass, jDisableNetworkNotifications);
}

void *
AndroidBridge::LockBitmap(jobject bitmap)
{
    JNIEnv *env = GetJNIEnv();
    if (!env)
        return nsnull;

    AutoLocalJNIFrame jniFrame(env);

    int err;
    void *buf;

    if ((err = AndroidBitmap_lockPixels(env, bitmap, &buf)) != 0) {
        ALOG_BRIDGE("AndroidBitmap_lockPixels failed! (error %d)", err);
        buf = nsnull;
    }

    return buf;
}

void
AndroidBridge::UnlockBitmap(jobject bitmap)
{
    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env);

    int err;

    if ((err = AndroidBitmap_unlockPixels(env, bitmap)) != 0)
        ALOG_BRIDGE("AndroidBitmap_unlockPixels failed! (error %d)", err);
}


bool
AndroidBridge::HasNativeWindowAccess()
{
    OpenGraphicsLibraries();

    // We have a fallback hack in place, so return true if that will work as well
    return mHasNativeWindowAccess || mHasNativeWindowFallback;
}

void*
AndroidBridge::AcquireNativeWindow(JNIEnv* aEnv, jobject aSurface)
{
    OpenGraphicsLibraries();

    if (mHasNativeWindowAccess)
        return ANativeWindow_fromSurface(aEnv, aSurface);
    else if (mHasNativeWindowFallback)
        return GetNativeSurface(aEnv, aSurface);
    else
        return nsnull;
}

void
AndroidBridge::ReleaseNativeWindow(void *window)
{
    if (!window)
        return;

    if (mHasNativeWindowAccess)
        ANativeWindow_release(window);

    // XXX: we don't ref the pointer we get from the fallback (GetNativeSurface), so we
    // have nothing to do here. We should probably ref it.
}

bool
AndroidBridge::SetNativeWindowFormat(void *window, int width, int height, int format)
{
    if (mHasNativeWindowAccess)
        return ANativeWindow_setBuffersGeometry(window, width, height, format) == 0;
    else
        return false; //unimplemented in fallback
}

bool
AndroidBridge::LockWindow(void *window, unsigned char **bits, int *width, int *height, int *format, int *stride)
{
    /* Copied from native_window.h in Android NDK (platform-9) */
    typedef struct ANativeWindow_Buffer {
        // The number of pixels that are show horizontally.
        int32_t width;

        // The number of pixels that are shown vertically.
        int32_t height;

        // The number of *pixels* that a line in the buffer takes in
        // memory.  This may be >= width.
        int32_t stride;

        // The format of the buffer.  One of WINDOW_FORMAT_*
        int32_t format;

        // The actual bits.
        void* bits;

        // Do not touch.
        uint32_t reserved[6];
    } ANativeWindow_Buffer;

    // Very similar to the above, but the 'usage' field is included. We use this
    // in the fallback case when NDK support is not available
    typedef struct SurfaceInfo {
        uint32_t    w;
        uint32_t    h;
        uint32_t    s;
        uint32_t    usage;
        uint32_t    format;
        unsigned char* bits;
        uint32_t    reserved[2];
    };

    int err;
    *bits = NULL;
    *width = *height = *format = 0;
    
    if (mHasNativeWindowAccess) {
        ANativeWindow_Buffer buffer;

        if ((err = ANativeWindow_lock(window, (void*)&buffer, NULL)) != 0) {
            ALOG_BRIDGE("ANativeWindow_lock failed! (error %d)", err);
            return false;
        }

        *bits = (unsigned char*)buffer.bits;
        *width = buffer.width;
        *height = buffer.height;
        *format = buffer.format;
        *stride = buffer.stride;
    } else if (mHasNativeWindowFallback) {
        SurfaceInfo info;

        if ((err = Surface_lock(window, &info, NULL, true)) != 0) {
            ALOG_BRIDGE("Surface_lock failed! (error %d)", err);
            return false;
        }

        *bits = info.bits;
        *width = info.w;
        *height = info.h;
        *format = info.format;
        *stride = info.s;
    } else return false;

    return true;
}

bool
AndroidBridge::UnlockWindow(void* window)
{
    int err;

    if (!HasNativeWindowAccess())
        return false;

    if (mHasNativeWindowAccess && (err = ANativeWindow_unlockAndPost(window)) != 0) {
        ALOG_BRIDGE("ANativeWindow_unlockAndPost failed! (error %d)", err);
        return false;
    } else if (mHasNativeWindowFallback && (err = Surface_unlockAndPost(window)) != 0) {
        ALOG_BRIDGE("Surface_unlockAndPost failed! (error %d)", err);
        return false;
    }

    return true;
}

bool
AndroidBridge::IsTablet()
{
    JNIEnv *env = GetJNIEnv();
    if (!env)
        return false;

    AutoLocalJNIFrame jniFrame(env, 0);

    bool ret = env->CallStaticBooleanMethod(mGeckoAppShellClass, jIsTablet);
    if (jniFrame.CheckForException())
        return false;

    return ret;
}

void
AndroidBridge::SetFirstPaintViewport(const nsIntPoint& aOffset, float aZoom, const nsIntRect& aPageRect, const gfx::Rect& aCssPageRect)
{
    AndroidGeckoLayerClient *client = mLayerClient;
    if (!client)
        return;

    client->SetFirstPaintViewport(aOffset, aZoom, aPageRect, aCssPageRect);
}

void
AndroidBridge::SetPageRect(float aZoom, const nsIntRect& aPageRect, const gfx::Rect& aCssPageRect)
{
    AndroidGeckoLayerClient *client = mLayerClient;
    if (!client)
        return;

    client->SetPageRect(aZoom, aPageRect, aCssPageRect);
}

void
AndroidBridge::SyncViewportInfo(const nsIntRect& aDisplayPort, float aDisplayResolution, bool aLayersUpdated,
                                nsIntPoint& aScrollOffset, float& aScaleX, float& aScaleY)
{
    AndroidGeckoLayerClient *client = mLayerClient;
    if (!client)
        return;

    client->SyncViewportInfo(aDisplayPort, aDisplayResolution, aLayersUpdated, aScrollOffset, aScaleX, aScaleY);
}

AndroidBridge::AndroidBridge()
: mLayerClient(NULL)
{
}

AndroidBridge::~AndroidBridge()
{
}

/* Implementation file */
NS_IMPL_ISUPPORTS1(nsAndroidBridge, nsIAndroidBridge)

nsAndroidBridge::nsAndroidBridge()
{
}

nsAndroidBridge::~nsAndroidBridge()
{
}

/* void handleGeckoEvent (in AString message); */
NS_IMETHODIMP nsAndroidBridge::HandleGeckoMessage(const nsAString & message, nsAString &aRet NS_OUTPARAM)
{
    AndroidBridge::Bridge()->HandleGeckoMessage(message, aRet);
    return NS_OK;
}

void
AndroidBridge::NotifyDefaultPrevented(bool aDefaultPrevented)
{
    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env, 0);
    env->CallStaticVoidMethod(mGeckoAppShellClass, jNotifyDefaultPrevented, (jboolean)aDefaultPrevented);
}


// DO NOT USE THIS unless you need to access JNI from
// non-main threads.  This is probably not what you want.
// Questions, ask blassey or dougt.

static void
JavaThreadDetachFunc(void *arg)
{
    JNIEnv *env = (JNIEnv*) arg;
    JavaVM *vm = NULL;
    env->GetJavaVM(&vm);
    vm->DetachCurrentThread();
}

extern "C" {
    __attribute__ ((visibility("default")))
    JNIEnv * GetJNIForThread()
    {
        JNIEnv *jEnv = NULL;
        JavaVM *jVm  = mozilla::AndroidBridge::GetVM();
        if (!jVm) {
            __android_log_print(ANDROID_LOG_INFO, "GetJNIForThread", "Returned a null VM");
            return NULL;
        }
        int status = jVm->GetEnv((void**) &jEnv, JNI_VERSION_1_2);
        if (status < 0) {

            status = jVm->AttachCurrentThread(&jEnv, NULL);
            if (status < 0) {
                __android_log_print(ANDROID_LOG_INFO, "GetJNIForThread",  "Could not attach");
                return NULL;
            }
            static PRUintn sJavaEnvThreadIndex = 0;
            PR_NewThreadPrivateIndex(&sJavaEnvThreadIndex, JavaThreadDetachFunc);
            PR_SetThreadPrivate(sJavaEnvThreadIndex, jEnv);
        }
        if (!jEnv) {
            __android_log_print(ANDROID_LOG_INFO, "GetJNIForThread", "returning NULL");
        }
        return jEnv;
    }
}

jobject
AndroidBridge::CreateSurface()
{
#ifndef MOZ_JAVA_COMPOSITOR
    return NULL;
#else
    JNIEnv* env = GetJNIEnv();
    if (!env)
        return nsnull;

    AutoLocalJNIFrame jniFrame(env);

    jobject surface = env->CallStaticObjectMethod(mGeckoAppShellClass, jCreateSurface);
    if (jniFrame.CheckForException())
        return nsnull;

    if (surface)
        surface = env->NewGlobalRef(surface);
  
    return surface;
#endif
}

void
AndroidBridge::DestroySurface(jobject surface)
{
#ifdef MOZ_JAVA_COMPOSITOR
    JNIEnv* env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env);

    env->CallStaticVoidMethod(mGeckoAppShellClass, jDestroySurface, surface);
    env->DeleteGlobalRef(surface);
#endif
}

void
AndroidBridge::ShowSurface(jobject surface, const gfxRect& aRect, bool aInverted, bool aBlend)
{
#ifdef MOZ_JAVA_COMPOSITOR
    JNIEnv* env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env, 0);

    env->CallStaticVoidMethod(mGeckoAppShellClass, jShowSurface, surface,
                             (int)aRect.x, (int)aRect.y,
                             (int)aRect.width, (int)aRect.height,
                             aInverted, aBlend);
#endif
}

void
AndroidBridge::HideSurface(jobject surface)
{
#ifdef MOZ_JAVA_COMPOSITOR
    JNIEnv* env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env, 0);

    env->CallStaticVoidMethod(mGeckoAppShellClass, jHideSurface, surface);
#endif
}

uint32_t
AndroidBridge::GetScreenOrientation()
{
    ALOG_BRIDGE("AndroidBridge::GetScreenOrientation");
    JNIEnv* env = GetJNIEnv();
    if (!env)
        return dom::eScreenOrientation_None;

    AutoLocalJNIFrame jniFrame(env, 0);

    jshort orientation = env->CallStaticShortMethod(mGeckoAppShellClass, jGetScreenOrientation);
    if (jniFrame.CheckForException())
        return dom::eScreenOrientation_None;

    return static_cast<dom::ScreenOrientation>(orientation);
}

void
AndroidBridge::EnableScreenOrientationNotifications()
{
    ALOG_BRIDGE("AndroidBridge::EnableScreenOrientationNotifications");
    JNIEnv* env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env, 0);
    env->CallStaticVoidMethod(mGeckoAppShellClass, jEnableScreenOrientationNotifications);
}

void
AndroidBridge::DisableScreenOrientationNotifications()
{
    ALOG_BRIDGE("AndroidBridge::DisableScreenOrientationNotifications");
    JNIEnv* env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env, 0);
    env->CallStaticVoidMethod(mGeckoAppShellClass, jDisableScreenOrientationNotifications);
}

void
AndroidBridge::LockScreenOrientation(uint32_t aOrientation)
{
    ALOG_BRIDGE("AndroidBridge::LockScreenOrientation");
    JNIEnv* env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env, 0);
    env->CallStaticVoidMethod(mGeckoAppShellClass, jLockScreenOrientation, aOrientation);
}

void
AndroidBridge::UnlockScreenOrientation()
{
    ALOG_BRIDGE("AndroidBridge::UnlockScreenOrientation");
    JNIEnv* env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env, 0);
    env->CallStaticVoidMethod(mGeckoAppShellClass, jUnlockScreenOrientation);
}

void
AndroidBridge::PumpMessageLoop()
{
#if MOZ_JAVA_COMPOSITOR
    JNIEnv* env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env, 0);

    if ((void*)pthread_self() != mThread)
        return;

    env->CallStaticVoidMethod(mGeckoAppShellClass, jPumpMessageLoop);
#endif
}

void
AndroidBridge::NotifyWakeLockChanged(const nsAString& topic, const nsAString& state)
{
    JNIEnv* env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env);

    jstring jstrTopic = env->NewString(nsPromiseFlatString(topic).get(),
                                       topic.Length());
    jstring jstrState = env->NewString(nsPromiseFlatString(state).get(),
                                       state.Length());

    env->CallStaticVoidMethod(mGeckoAppShellClass, jNotifyWakeLockChanged, jstrTopic, jstrState);
}

/* attribute nsIAndroidBrowserApp browserApp; */
NS_IMETHODIMP nsAndroidBridge::GetBrowserApp(nsIAndroidBrowserApp * *aBrowserApp)
{
    if (nsAppShell::gAppShell)
        nsAppShell::gAppShell->GetBrowserApp(aBrowserApp);
    return NS_OK;
}

NS_IMETHODIMP nsAndroidBridge::SetBrowserApp(nsIAndroidBrowserApp *aBrowserApp)
{
    if (nsAppShell::gAppShell)
        nsAppShell::gAppShell->SetBrowserApp(aBrowserApp);
    return NS_OK;
}

void
AndroidBridge::AddPluginView(jobject view, const gfxRect& rect, bool isFullScreen) {
    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env);

#if MOZ_JAVA_COMPOSITOR
    env->CallStaticVoidMethod(sBridge->mGeckoAppShellClass,
                              sBridge->jAddPluginView, view,
                              (int)rect.x, (int)rect.y, (int)rect.width, (int)rect.height,
                              isFullScreen);
#else
    env->CallStaticVoidMethod(sBridge->mGeckoAppShellClass,
                              sBridge->jAddPluginView, view,
                              rect.x, rect.y, rect.width, rect.height);
#endif
}

void
AndroidBridge::RemovePluginView(jobject view, bool isFullScreen)
{
    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env, 0);
    env->CallStaticVoidMethod(mGeckoAppShellClass, jRemovePluginView, view, isFullScreen);
}

extern "C"
__attribute__ ((visibility("default")))
jobject JNICALL
Java_org_mozilla_gecko_GeckoAppShell_allocateDirectBuffer(JNIEnv *env, jclass, jlong size);


nsresult AndroidBridge::TakeScreenshot(nsIDOMWindow *window, PRInt32 srcX, PRInt32 srcY, PRInt32 srcW, PRInt32 srcH, PRInt32 dstW, PRInt32 dstH, PRInt32 tabId, float scale, PRInt32 token)
{
    nsresult rv;

    // take a screenshot, as wide as possible, proportional to the destination size
    if (!srcW && !srcH) {
        nsCOMPtr<nsIDOMWindowUtils> utils = do_GetInterface(window);
        if (!utils)
            return NS_ERROR_FAILURE;

        nsCOMPtr<nsIDOMClientRect> rect;
        rv = utils->GetRootBounds(getter_AddRefs(rect));
        NS_ENSURE_SUCCESS(rv, rv);
        if (!rect)
            return NS_ERROR_FAILURE;

        float left, top, width, height;
        rect->GetLeft(&left);
        rect->GetTop(&top);
        rect->GetWidth(&width);
        rect->GetHeight(&height);

        if (width == 0 || height == 0)
            return NS_ERROR_FAILURE;

        float aspectRatio = ((float) dstW) / dstH;
        if (width / aspectRatio < height) {
            srcW = width;
            srcH = width / aspectRatio;
        } else {
            srcW = height * aspectRatio;
            srcH = height;
        }

        srcX = left;
        srcY = top;
    }

    JNIEnv* env = GetJNIEnv();
    if (!env)
        return NS_OK;

    AutoLocalJNIFrame jniFrame(env);

    nsCOMPtr<nsPIDOMWindow> win = do_QueryInterface(window);
    if (!win)
        return NS_ERROR_FAILURE;
    nsRefPtr<nsPresContext> presContext;
    nsIDocShell* docshell = win->GetDocShell();
    if (docshell) {
        docshell->GetPresContext(getter_AddRefs(presContext));
    }
    if (!presContext)
        return NS_ERROR_FAILURE;
    nscolor bgColor = NS_RGB(255, 255, 255);
    nsIPresShell* presShell = presContext->PresShell();
    PRUint32 renderDocFlags = (nsIPresShell::RENDER_IGNORE_VIEWPORT_SCROLLING |
                               nsIPresShell::RENDER_DOCUMENT_RELATIVE);
    nsRect r(nsPresContext::CSSPixelsToAppUnits(srcX / scale),
             nsPresContext::CSSPixelsToAppUnits(srcY / scale),
             nsPresContext::CSSPixelsToAppUnits(srcW / scale),
             nsPresContext::CSSPixelsToAppUnits(srcH / scale));

    PRUint32 stride = dstW * 2;
    PRUint32 bufferSize = dstH * stride;

    jobject buffer = Java_org_mozilla_gecko_GeckoAppShell_allocateDirectBuffer(env, NULL, bufferSize);
    if (!buffer)
        return NS_OK;

    void* data = env->GetDirectBufferAddress(buffer);
    memset(data, 0, bufferSize);
    nsRefPtr<gfxImageSurface> surf = new gfxImageSurface(static_cast<unsigned char*>(data), nsIntSize(dstW, dstH), stride, gfxASurface::ImageFormatRGB16_565);
    nsRefPtr<gfxContext> context = new gfxContext(surf);
    context->Scale(scale * dstW / srcW, scale * dstH / srcH);
    rv = presShell->RenderDocument(r, renderDocFlags, bgColor, context);
    NS_ENSURE_SUCCESS(rv, rv);
    env->CallStaticVoidMethod(AndroidBridge::Bridge()->mGeckoAppShellClass, AndroidBridge::Bridge()->jNotifyScreenShot, buffer, tabId, srcX * dstW / srcW , srcY * dstH / srcH, dstW, dstH, token);
    return NS_OK;
}

void
AndroidBridge::NotifyPaintedRect(float top, float left, float bottom, float right)
{
    JNIEnv* env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env, 0);
    env->CallStaticVoidMethod(AndroidBridge::Bridge()->mGeckoAppShellClass, AndroidBridge::Bridge()->jNotifyPaintedRect, top, left, bottom, right);
}
