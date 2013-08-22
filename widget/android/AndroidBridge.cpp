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
#include "AndroidJNIWrapper.h"
#include "nsAppShell.h"
#include "nsOSHelperAppService.h"
#include "nsWindow.h"
#include "mozilla/Preferences.h"
#include "nsThreadUtils.h"
#include "nsIThreadManager.h"
#include "mozilla/dom/mobilemessage/PSms.h"
#include "gfxImageSurface.h"
#include "gfxContext.h"
#include "gfxUtils.h"
#include "nsPresContext.h"
#include "nsIDocShell.h"
#include "nsPIDOMWindow.h"
#include "mozilla/dom/ScreenOrientation.h"
#include "nsIDOMWindowUtils.h"
#include "nsIDOMClientRect.h"
#include "StrongPointer.h"
#include "mozilla/ClearOnShutdown.h"
#include "nsPrintfCString.h"

#ifdef DEBUG
#define ALOG_BRIDGE(args...) ALOG(args)
#else
#define ALOG_BRIDGE(args...) ((void)0)
#endif

using namespace mozilla;

NS_IMPL_ISUPPORTS0(nsFilePickerCallback)

StaticRefPtr<AndroidBridge> AndroidBridge::sBridge;
static unsigned sJavaEnvThreadIndex = 0;
static void JavaThreadDetachFunc(void *arg);

// This is a dummy class that can be used in the template for android::sp
class AndroidRefable {
    void incStrong(void* thing) { }
    void decStrong(void* thing) { }
};

// This isn't in AndroidBridge.h because including StrongPointer.h there is gross
static android::sp<AndroidRefable> (*android_SurfaceTexture_getNativeWindow)(JNIEnv* env, jobject surfaceTexture) = nullptr;

void
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

    PR_NewThreadPrivateIndex(&sJavaEnvThreadIndex, JavaThreadDetachFunc);

    AndroidBridge *bridge = new AndroidBridge();
    if (!bridge->Init(jEnv, jGeckoAppShellClass)) {
        delete bridge;
    }
    sBridge = bridge;
}

bool
AndroidBridge::Init(JNIEnv *jEnv,
                    jclass jGeckoAppShellClass)
{
    ALOG_BRIDGE("AndroidBridge::Init");
    jEnv->GetJavaVM(&mJavaVM);

    AutoLocalJNIFrame jniFrame(jEnv);

    mJNIEnv = nullptr;
    mThread = nullptr;
    mGLControllerObj = nullptr;
    mOpenedGraphicsLibraries = false;
    mHasNativeBitmapAccess = false;
    mHasNativeWindowAccess = false;
    mHasNativeWindowFallback = false;

    mGeckoAppShellClass = (jclass) jEnv->NewGlobalRef(jGeckoAppShellClass);

#ifdef MOZ_WEBSMS_BACKEND
    jclass jAndroidSmsMessageClass = jEnv->FindClass("android/telephony/SmsMessage");
    mAndroidSmsMessageClass = (jclass) jEnv->NewGlobalRef(jAndroidSmsMessageClass);
    jCalculateLength = (jmethodID) jEnv->GetStaticMethodID(jAndroidSmsMessageClass, "calculateLength", "(Ljava/lang/CharSequence;Z)[I");
#endif

    jNotifyIME = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "notifyIME", "(I)V");
    jNotifyIMEContext = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "notifyIMEContext", "(ILjava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
    jNotifyIMEChange = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "notifyIMEChange", "(Ljava/lang/String;III)V");
    jAcknowledgeEvent = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "acknowledgeEvent", "()V");

    jEnableLocation = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "enableLocation", "(Z)V");
    jEnableLocationHighAccuracy = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "enableLocationHighAccuracy", "(Z)V");
    jEnableSensor = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "enableSensor", "(I)V");
    jDisableSensor = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "disableSensor", "(I)V");
    jScheduleRestart = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "scheduleRestart", "()V");
    jNotifyXreExit = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "onXreExit", "()V");
    jGetHandlersForMimeType = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "getHandlersForMimeType", "(Ljava/lang/String;Ljava/lang/String;)[Ljava/lang/String;");
    jGetHandlersForURL = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "getHandlersForURL", "(Ljava/lang/String;Ljava/lang/String;)[Ljava/lang/String;");
    jOpenUriExternal = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "openUriExternal", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Z");
    jGetMimeTypeFromExtensions = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "getMimeTypeFromExtensions", "(Ljava/lang/String;)Ljava/lang/String;");
    jGetExtensionFromMimeType = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "getExtensionFromMimeType", "(Ljava/lang/String;)Ljava/lang/String;");
    jMoveTaskToBack = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "moveTaskToBack", "()V");
    jShowAlertNotification = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "showAlertNotification", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
    jShowFilePickerForExtensions = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "showFilePickerForExtensions", "(Ljava/lang/String;)Ljava/lang/String;");
    jShowFilePickerForMimeType = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "showFilePickerForMimeType", "(Ljava/lang/String;)Ljava/lang/String;");
    jShowFilePickerAsync = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "showFilePickerAsync", "(Ljava/lang/String;J)V");
    jUnlockProfile = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "unlockProfile", "()Z");
    jKillAnyZombies = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "killAnyZombies", "()V");
    jAlertsProgressListener_OnProgress = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "alertsProgressListener_OnProgress", "(Ljava/lang/String;JJLjava/lang/String;)V");
    jCloseNotification = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "closeNotification", "(Ljava/lang/String;)V");
    jGetDpi = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "getDpi", "()I");
    jGetScreenDepth = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "getScreenDepth", "()I");
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
    jCreateShortcut = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "createShortcut", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
    jGetShowPasswordSetting = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "getShowPasswordSetting", "()Z");
    jInitCamera = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "initCamera", "(Ljava/lang/String;III)[I");
    jCloseCamera = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "closeCamera", "()V");
    jIsTablet = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "isTablet", "()Z");
    jEnableBatteryNotifications = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "enableBatteryNotifications", "()V");
    jDisableBatteryNotifications = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "disableBatteryNotifications", "()V");
    jGetCurrentBatteryInformation = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "getCurrentBatteryInformation", "()[D");

    jHandleGeckoMessage = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "handleGeckoMessage", "(Ljava/lang/String;)Ljava/lang/String;");
    jCheckUriVisited = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "checkUriVisited", "(Ljava/lang/String;)V");
    jMarkUriVisited = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "markUriVisited", "(Ljava/lang/String;)V");
    jSetUriTitle = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "setUriTitle", "(Ljava/lang/String;Ljava/lang/String;)V");

    jSendMessage = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "sendMessage", "(Ljava/lang/String;Ljava/lang/String;I)V");
    jGetMessage = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "getMessage", "(II)V");
    jDeleteMessage = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "deleteMessage", "(II)V");
    jCreateMessageList = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "createMessageList", "(JJ[Ljava/lang/String;IIZI)V");
    jGetNextMessageinList = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "getNextMessageInList", "(II)V");
    jClearMessageList = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "clearMessageList", "(I)V");

    jGetCurrentNetworkInformation = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "getCurrentNetworkInformation", "()[D");
    jEnableNetworkNotifications = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "enableNetworkNotifications", "()V");
    jDisableNetworkNotifications = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "disableNetworkNotifications", "()V");

    jGetScreenOrientation = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "getScreenOrientation", "()S");
    jEnableScreenOrientationNotifications = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "enableScreenOrientationNotifications", "()V");
    jDisableScreenOrientationNotifications = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "disableScreenOrientationNotifications", "()V");
    jLockScreenOrientation = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "lockScreenOrientation", "(I)V");
    jUnlockScreenOrientation = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "unlockScreenOrientation", "()V");

    jGeckoJavaSamplerClass = (jclass) jEnv->NewGlobalRef(jEnv->FindClass("org/mozilla/gecko/GeckoJavaSampler"));
    jStart = jEnv->GetStaticMethodID(jGeckoJavaSamplerClass, "start", "(II)V");
    jStop = jEnv->GetStaticMethodID(jGeckoJavaSamplerClass, "stop", "()V");
    jPause = jEnv->GetStaticMethodID(jGeckoJavaSamplerClass, "pause", "()V");
    jUnpause = jEnv->GetStaticMethodID(jGeckoJavaSamplerClass, "unpause", "()V");
    jGetThreadName = jEnv->GetStaticMethodID(jGeckoJavaSamplerClass, "getThreadName", "(I)Ljava/lang/String;");
    jGetFrameName = jEnv->GetStaticMethodID(jGeckoJavaSamplerClass, "getFrameName", "(III)Ljava/lang/String;");
    jGetSampleTime = jEnv->GetStaticMethodID(jGeckoJavaSamplerClass, "getSampleTime", "(II)D");

    jThumbnailHelperClass = (jclass) jEnv->NewGlobalRef(jEnv->FindClass("org/mozilla/gecko/ThumbnailHelper"));
    jNotifyThumbnail = jEnv->GetStaticMethodID(jThumbnailHelperClass, "notifyThumbnail", "(Ljava/nio/ByteBuffer;IZ)V");

    jStringClass = (jclass) jEnv->NewGlobalRef(jEnv->FindClass("java/lang/String"));

    jSurfaceClass = (jclass) jEnv->NewGlobalRef(jEnv->FindClass("android/view/Surface"));

    if (!GetStaticIntField("android/os/Build$VERSION", "SDK_INT", &mAPIVersion, jEnv))
        ALOG_BRIDGE("Failed to find API version");

    if (mAPIVersion <= 8 /* Froyo */) {
        jSurfacePointerField = jEnv->GetFieldID(jSurfaceClass, "mSurface", "I");
    } else {
        jSurfacePointerField = jEnv->GetFieldID(jSurfaceClass, "mNativeSurface", "I");

        // Apparently mNativeSurface doesn't exist in Key Lime Pie, so just clear the
        // exception if we have one and move on.
        if (jEnv->ExceptionCheck()) {
            jEnv->ExceptionClear();
        }
    }

    jNotifyWakeLockChanged = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "notifyWakeLockChanged", "(Ljava/lang/String;Ljava/lang/String;)V");

    jGetGfxInfoData = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "getGfxInfoData", "()Ljava/lang/String;");
    jGetProxyForURI = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "getProxyForURI", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;I)Ljava/lang/String;");
    jRegisterSurfaceTextureFrameListener = jEnv->GetStaticMethodID(jGeckoAppShellClass, "registerSurfaceTextureFrameListener", "(Ljava/lang/Object;I)V");
    jUnregisterSurfaceTextureFrameListener = jEnv->GetStaticMethodID(jGeckoAppShellClass, "unregisterSurfaceTextureFrameListener", "(Ljava/lang/Object;)V");

    jPumpMessageLoop = (jmethodID) jEnv->GetStaticMethodID(jGeckoAppShellClass, "pumpMessageLoop", "()Z");

    jAddPluginView = jEnv->GetStaticMethodID(jGeckoAppShellClass, "addPluginView", "(Landroid/view/View;FFFFZ)V");
    jRemovePluginView = jEnv->GetStaticMethodID(jGeckoAppShellClass, "removePluginView", "(Landroid/view/View;Z)V");

    jLayerView = (jclass) jEnv->NewGlobalRef(jEnv->FindClass("org/mozilla/gecko/gfx/LayerView"));

    jclass glControllerClass = jEnv->FindClass("org/mozilla/gecko/gfx/GLController");
    jProvideEGLSurfaceMethod = jEnv->GetMethodID(glControllerClass, "provideEGLSurface",
                                                  "()Ljavax/microedition/khronos/egl/EGLSurface;");

    jclass nativePanZoomControllerClass = jEnv->FindClass("org/mozilla/gecko/gfx/NativePanZoomController");
    jRequestContentRepaint = jEnv->GetMethodID(nativePanZoomControllerClass, "requestContentRepaint", "(FFFFF)V");
    jPostDelayedCallback = jEnv->GetMethodID(nativePanZoomControllerClass, "postDelayedCallback", "(J)V");

    jclass eglClass = jEnv->FindClass("com/google/android/gles_jni/EGLSurfaceImpl");
    if (eglClass) {
        jEGLSurfacePointerField = jEnv->GetFieldID(eglClass, "mEGLSurface", "I");
    } else {
        jEGLSurfacePointerField = 0;
    }

    jGetContext = (jmethodID)jEnv->GetStaticMethodID(jGeckoAppShellClass, "getContext", "()Landroid/content/Context;");

    jClipboardClass = (jclass) jEnv->NewGlobalRef(jEnv->FindClass("org/mozilla/gecko/util/Clipboard"));
    jClipboardGetText = (jmethodID) jEnv->GetStaticMethodID(jClipboardClass, "getText", "()Ljava/lang/String;");
    jClipboardSetText = (jmethodID) jEnv->GetStaticMethodID(jClipboardClass, "setText", "(Ljava/lang/CharSequence;)V");

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

    mJNIEnv = nullptr;
    mThread = nullptr;
    return true;
}

void
AndroidBridge::NotifyIME(int aType)
{
    ALOG_BRIDGE("AndroidBridge::NotifyIME");

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env, 0);
    env->CallStaticVoidMethod(sBridge->mGeckoAppShellClass,
                              sBridge->jNotifyIME, aType);
}

jstring NewJavaString(AutoLocalJNIFrame* frame, const PRUnichar* string, uint32_t len) {
    jstring ret = frame->GetEnv()->NewString(string, len);
    if (frame->CheckForException())
        return NULL;
    return ret;
}

jstring NewJavaString(AutoLocalJNIFrame* frame, const nsAString& string) {
    return NewJavaString(frame, string.BeginReading(), string.Length());
}

jstring NewJavaString(AutoLocalJNIFrame* frame, const char* string) {
    return NewJavaString(frame, NS_ConvertUTF8toUTF16(string));
}

jstring NewJavaString(AutoLocalJNIFrame* frame, const nsACString& string) {
    return NewJavaString(frame, NS_ConvertUTF8toUTF16(string));
}

void
AndroidBridge::NotifyIMEContext(int aState, const nsAString& aTypeHint,
                                const nsAString& aModeHint, const nsAString& aActionHint)
{
    ALOG_BRIDGE("AndroidBridge::NotifyIMEContext");
    if (!sBridge)
        return;

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env);

    jvalue args[4];
    args[0].i = aState;
    args[1].l = NewJavaString(&jniFrame, aTypeHint);
    args[2].l = NewJavaString(&jniFrame, aModeHint);
    args[3].l = NewJavaString(&jniFrame, aActionHint);

    env->CallStaticVoidMethodA(sBridge->mGeckoAppShellClass,
                               sBridge->jNotifyIMEContext, args);
}

void
AndroidBridge::NotifyIMEChange(const PRUnichar *aText, uint32_t aTextLen,
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
    args[0].l = NewJavaString(&jniFrame, aText, aTextLen);
    args[1].i = aStart;
    args[2].i = aEnd;
    args[3].i = aNewEnd;
    env->CallStaticVoidMethodA(sBridge->mGeckoAppShellClass,
                               sBridge->jNotifyIMEChange, args);
}

void
AndroidBridge::AcknowledgeEvent()
{
    ALOG_BRIDGE("AndroidBridge::AcknowledgeEvent");

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env, 0);
    env->CallStaticVoidMethod(mGeckoAppShellClass, jAcknowledgeEvent);
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
    jstring jstrMimeType = NewJavaString(&jniFrame, aMimeType);

    jstring jstrAction = NewJavaString(&jniFrame, aAction);

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
    jstring jstrScheme = NewJavaString(&jniFrame, aURL);
    jstring jstrAction = NewJavaString(&jniFrame, aAction);

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

    jstring jstrUri = NewJavaString(&jniFrame, aUriSpec);
    jstring jstrType = NewJavaString(&jniFrame, aMimeType);

    jstring jstrPackage = NewJavaString(&jniFrame, aPackageName);
    jstring jstrClass = NewJavaString(&jniFrame, aClassName);
    jstring jstrAction = NewJavaString(&jniFrame, aAction);
    jstring jstrTitle = NewJavaString(&jniFrame, aTitle);

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
    jstring jstrExt = NewJavaString(&jniFrame, aFileExt);
    jstring jstrType =  static_cast<jstring>(
        env->CallStaticObjectMethod(mGeckoAppShellClass,
                                    jGetMimeTypeFromExtensions,
                                    jstrExt));
    if (jniFrame.CheckForException())
        return;

    nsJNIString jniStr(jstrType, env);
    CopyUTF16toUTF8(jniStr.get(), aMimeType);
}

void
AndroidBridge::GetExtensionFromMimeType(const nsACString& aMimeType, nsACString& aFileExt)
{
    ALOG_BRIDGE("AndroidBridge::GetExtensionFromMimeType");

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env);
    jstring jstrType = NewJavaString(&jniFrame, aMimeType);
    jstring jstrExt = static_cast<jstring>(
        env->CallStaticObjectMethod(mGeckoAppShellClass,
                                    jGetExtensionFromMimeType,
                                    jstrType));
    if (jniFrame.CheckForException())
        return;

    nsJNIString jniStr(jstrExt, env);
    CopyUTF16toUTF8(jniStr.get(), aFileExt);
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
        env->CallStaticObjectMethod(jClipboardClass,
                                    jClipboardGetText));
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
    jstring jstr = NewJavaString(&jniFrame, aText);
    env->CallStaticVoidMethod(jClipboardClass, jClipboardSetText, jstr);
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
        env->CallStaticObjectMethod(jClipboardClass,
                                    jClipboardGetText));
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
    env->CallStaticVoidMethod(jClipboardClass, jClipboardSetText, nullptr);
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

    if (nsAppShell::gAppShell && aAlertListener) {
        // This will remove any observers already registered for this id
        nsAppShell::gAppShell->AddObserver(aAlertName, aAlertListener);
    }

    jvalue args[5];
    args[0].l = NewJavaString(&jniFrame, aImageUrl);
    args[1].l = NewJavaString(&jniFrame, aAlertTitle);
    args[2].l = NewJavaString(&jniFrame, aAlertText);
    args[3].l = NewJavaString(&jniFrame, aAlertCookie);
    args[4].l = NewJavaString(&jniFrame, aAlertName);
    env->CallStaticVoidMethodA(mGeckoAppShellClass, jShowAlertNotification, args);
}

void
AndroidBridge::AlertsProgressListener_OnProgress(const nsAString& aAlertName,
                                                 int64_t aProgress,
                                                 int64_t aProgressMax,
                                                 const nsAString& aAlertText)
{
    ALOG_BRIDGE("AlertsProgressListener_OnProgress");
    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env);

    jstring jstrName = NewJavaString(&jniFrame, aAlertName);
    jstring jstrText = NewJavaString(&jniFrame, aAlertText);
    env->CallStaticVoidMethod(mGeckoAppShellClass, jAlertsProgressListener_OnProgress,
                              jstrName, aProgress, aProgressMax, jstrText);
}

void
AndroidBridge::CloseNotification(const nsAString& aAlertName)
{
    ALOG_BRIDGE("CloseNotification");

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env);

    jstring jstrName = NewJavaString(&jniFrame, aAlertName);
    env->CallStaticVoidMethod(mGeckoAppShellClass, jCloseNotification, jstrName);
}


int
AndroidBridge::GetDPI()
{
    static int sDPI = 0;
    if (sDPI)
        return sDPI;

    ALOG_BRIDGE("AndroidBridge::GetDPI");
    const int DEFAULT_DPI = 160;
    JNIEnv *env = GetJNIEnv();
    if (!env)
        return DEFAULT_DPI;
    AutoLocalJNIFrame jniFrame(env);

    sDPI = (int)env->CallStaticIntMethod(mGeckoAppShellClass, jGetDpi);
    if (jniFrame.CheckForException()) {
        sDPI = 0;
        return DEFAULT_DPI;
    }

    return sDPI;
}

int
AndroidBridge::GetScreenDepth()
{
    static int sDepth = 0;
    if (sDepth)
        return sDepth;

    ALOG_BRIDGE("AndroidBridge::GetScreenDepth");
    const int DEFAULT_DEPTH = 16;
    JNIEnv *env = GetJNIEnv();
    if (!env)
        return DEFAULT_DEPTH;
    AutoLocalJNIFrame jniFrame(env);

    sDepth = (int)env->CallStaticIntMethod(mGeckoAppShellClass, jGetScreenDepth);
    if (jniFrame.CheckForException()) {
        sDepth = 0;
        return DEFAULT_DEPTH;
    }

    return sDepth;
}

void
AndroidBridge::ShowFilePickerForExtensions(nsAString& aFilePath, const nsAString& aExtensions)
{
    ALOG_BRIDGE("AndroidBridge::ShowFilePickerForExtensions");

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env);
    jstring jstrFilers = NewJavaString(&jniFrame, aExtensions);
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
    jstring jstrFilers = NewJavaString(&jniFrame, aMimeType);
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
    jstring jMimeType = NewJavaString(&jniFrame, aMimeType);
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
AndroidBridge::Vibrate(const nsTArray<uint32_t>& aPattern)
{
    ALOG_BRIDGE("AndroidBridge::Vibrate");
    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env);
    uint32_t len = aPattern.Length();
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

    jlong* elts = env->GetLongArrayElements(array, nullptr);
    elts[0] = 0;
    for (uint32_t i = 0; i < aPattern.Length(); ++i) {
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
    jstring jLocale = NewJavaString(&jniFrame, aLocale);
    env->CallStaticVoidMethod(mGeckoAppShellClass, jSetSelectedLocale, jLocale);
}

void
AndroidBridge::GetSystemColors(AndroidSystemColors *aColors)
{
    ALOG_BRIDGE("AndroidBridge::GetSystemColors");

    NS_ASSERTION(aColors != nullptr, "AndroidBridge::GetSystemColors: aColors is null!");
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

    uint32_t len = static_cast<uint32_t>(env->GetArrayLength(arr));
    jint *elements = env->GetIntArrayElements(arr, 0);

    uint32_t colorsCount = sizeof(AndroidSystemColors) / sizeof(nscolor);
    if (len < colorsCount)
        colorsCount = len;

    // Convert Android colors to nscolor by switching R and B in the ARGB 32 bit value
    nscolor *colors = (nscolor*)aColors;

    for (uint32_t i = 0; i < colorsCount; i++) {
        uint32_t androidColor = static_cast<uint32_t>(elements[i]);
        uint8_t r = (androidColor & 0x00ff0000) >> 16;
        uint8_t b = (androidColor & 0x000000ff);
        colors[i] = (androidColor & 0xff00ff00) | (b << 16) | r;
    }

    env->ReleaseIntArrayElements(arr, elements, 0);
}

void
AndroidBridge::GetIconForExtension(const nsACString& aFileExt, uint32_t aIconSize, uint8_t * const aBuf)
{
    ALOG_BRIDGE("AndroidBridge::GetIconForExtension");
    NS_ASSERTION(aBuf != nullptr, "AndroidBridge::GetIconForExtension: aBuf is null!");
    if (!aBuf)
        return;

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env);

    jstring jstrFileExt = NewJavaString(&jniFrame, aFileExt);

    jobject obj = env->CallStaticObjectMethod(mGeckoAppShellClass, jGetIconForExtension, jstrFileExt, aIconSize);
    if (jniFrame.CheckForException())
        return;

    jbyteArray arr = static_cast<jbyteArray>(obj);
    NS_ASSERTION(arr != nullptr, "AndroidBridge::GetIconForExtension: Returned pixels array is null!");
    if (!arr)
        return;

    uint32_t len = static_cast<uint32_t>(env->GetArrayLength(arr));
    jbyte *elements = env->GetByteArrayElements(arr, 0);

    uint32_t bufSize = aIconSize * aIconSize * 4;
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
AndroidBridge::SetLayerClient(JNIEnv* env, jobject jobj)
{
    // if resetting is true, that means Android destroyed our GeckoApp activity
    // and we had to recreate it, but all the Gecko-side things were not destroyed.
    // We therefore need to link up the new java objects to Gecko, and that's what
    // we do here.
    bool resetting = (mLayerClient != NULL);

    if (resetting) {
        // clear out the old layer client
        env->DeleteGlobalRef(mLayerClient->wrappedObject());
        delete mLayerClient;
        mLayerClient = NULL;
    }

    AndroidGeckoLayerClient *client = new AndroidGeckoLayerClient();
    client->Init(env->NewGlobalRef(jobj));
    mLayerClient = client;

    if (resetting) {
        // since we are re-linking the new java objects to Gecko, we need to get
        // the viewport from the compositor (since the Java copy was thrown away)
        // and we do that by setting the first-paint flag.
        nsWindow::ForceIsFirstPaint();
    }
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

void
AndroidBridge::RegisterCompositor(JNIEnv *env)
{
    ALOG_BRIDGE("AndroidBridge::RegisterCompositor");
    if (mGLControllerObj) {
        // we already have this set up, no need to do it again
        return;
    }

    if (!env) {
        env = GetJNIForThread();    // called on the compositor thread
    }
    if (!env) {
        return;
    }

    AutoLocalJNIFrame jniFrame(env);

    jmethodID registerCompositor = env->GetStaticMethodID(jLayerView, "registerCxxCompositor", "()Lorg/mozilla/gecko/gfx/GLController;");

    jobject glController = env->CallStaticObjectMethod(jLayerView, registerCompositor);
    if (jniFrame.CheckForException())
        return;

    mGLControllerObj = env->NewGlobalRef(glController);
}

EGLSurface
AndroidBridge::ProvideEGLSurface()
{
    if (!jEGLSurfacePointerField) {
        return NULL;
    }
    MOZ_ASSERT(mGLControllerObj, "AndroidBridge::ProvideEGLSurface called with a null GL controller ref");

    JNIEnv* env = GetJNIForThread(); // called on the compositor thread
    AutoLocalJNIFrame jniFrame(env);
    jobject eglSurface = env->CallObjectMethod(mGLControllerObj, jProvideEGLSurfaceMethod);
    if (jniFrame.CheckForException() || !eglSurface)
        return NULL;

    return reinterpret_cast<EGLSurface>(env->GetIntField(eglSurface, jEGLSurfacePointerField));
}

bool
AndroidBridge::GetStaticIntField(const char *className, const char *fieldName, int32_t* aInt, JNIEnv* env /* = nullptr */)
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

    *aInt = static_cast<int32_t>(env->GetStaticIntField(cls, field));

    return true;
}

bool
AndroidBridge::GetStaticStringField(const char *className, const char *fieldName, nsAString &result, JNIEnv* env /* = nullptr */)
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
    jstring jstrFile = NewJavaString(&jniFrame, aFile);

    jstring jstrMimeTypes = NewJavaString(&jniFrame, aMimeType);

    env->CallStaticVoidMethod(mGeckoAppShellClass, jScanMedia, jstrFile, jstrMimeTypes);
}

void
AndroidBridge::CreateShortcut(const nsAString& aTitle, const nsAString& aURI, const nsAString& aIconData, const nsAString& aIntent)
{
    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env);
    jstring jstrTitle = NewJavaString(&jniFrame, aTitle);
    jstring jstrURI = NewJavaString(&jniFrame, aURI);
    jstring jstrIconData = NewJavaString(&jniFrame, aIconData);
    jstring jstrIntent = NewJavaString(&jniFrame, aIntent);

    if (!jstrURI || !jstrTitle || !jstrIconData)
        return;

    env->CallStaticVoidMethod(mGeckoAppShellClass, jCreateShortcut, jstrTitle, jstrURI, jstrIconData, jstrIntent);
}

void*
AndroidBridge::GetNativeSurface(JNIEnv* env, jobject surface) {
    if (!env || !mHasNativeWindowFallback || !jSurfacePointerField)
        return nullptr;

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

            // This is only available in Honeycomb and ICS. It was removed in Jelly Bean
            ANativeWindow_fromSurfaceTexture = (void* (*)(JNIEnv*, jobject))dlsym(handle, "ANativeWindow_fromSurfaceTexture");

            mHasNativeWindowAccess = ANativeWindow_fromSurface && ANativeWindow_release && ANativeWindow_lock && ANativeWindow_unlockAndPost;

            ALOG_BRIDGE("Successfully opened libandroid.so, have native window access? %d", mHasNativeWindowAccess);
        }

        // We need one symbol from here on Jelly Bean
        handle = dlopen("libandroid_runtime.so", RTLD_LAZY | RTLD_LOCAL);
        if (handle) {
            android_SurfaceTexture_getNativeWindow = (android::sp<AndroidRefable> (*)(JNIEnv*, jobject))dlsym(handle, "_ZN7android38android_SurfaceTexture_getNativeWindowEP7_JNIEnvP8_jobject");
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
            mTracerLock = nullptr;
            mTracerCondVar = nullptr;
        }

        virtual nsresult Run() {
            MutexAutoLock lock(*mTracerLock);
            if (!AndroidBridge::Bridge())
                return NS_OK;

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
    StaticRefPtr<TracerRunnable> sTracerRunnable;

    bool InitWidgetTracing() {
        if (!sTracerRunnable)
            sTracerRunnable = new TracerRunnable();
        return true;
    }

    void CleanUpWidgetTracing() {
        sTracerRunnable = nullptr;
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

    if ((int)info.width != width || (int)info.height != height)
        return false;

    return true;
}

bool
AndroidBridge::InitCamera(const nsCString& contentType, uint32_t camera, uint32_t *width, uint32_t *height, uint32_t *fps)
{
    JNIEnv *env = GetJNIEnv();
    if (!env)
        return false;

    AutoLocalJNIFrame jniFrame(env);

    jstring jstrContentType = NewJavaString(&jniFrame, contentType);

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
    jstring jMessage = NewJavaString(&jniFrame, aMessage);
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

    AutoLocalJNIFrame jniFrame(env, 1);
    jstring jstrURI = NewJavaString(&jniFrame, aURI);
    // If creating the string fails, just bail
    if (jstrURI)
        env->CallStaticVoidMethod(mGeckoAppShellClass, jCheckUriVisited, jstrURI);
}

void
AndroidBridge::MarkURIVisited(const nsAString& aURI)
{
    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env);
    jstring jstrURI = NewJavaString(&jniFrame, aURI);
    env->CallStaticVoidMethod(mGeckoAppShellClass, jMarkUriVisited, jstrURI);
}

void
AndroidBridge::SetURITitle(const nsAString& aURI, const nsAString& aTitle)
{
    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env);
    jstring jstrURI = NewJavaString(&jniFrame, aURI);
    jstring jstrTitle = NewJavaString(&jniFrame, aTitle);
    env->CallStaticVoidMethod(mGeckoAppShellClass, jSetUriTitle, jstrURI, jstrTitle);
}

nsresult
AndroidBridge::GetSegmentInfoForText(const nsAString& aText,
                                     dom::mobilemessage::SmsSegmentInfoData* aData)
{
#ifndef MOZ_WEBSMS_BACKEND
    return NS_ERROR_FAILURE;
#else
    ALOG_BRIDGE("AndroidBridge::GetSegmentInfoForText");

    aData->segments() = 0;
    aData->charsPerSegment() = 0;
    aData->charsAvailableInLastSegment() = 0;

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return NS_ERROR_FAILURE;

    AutoLocalJNIFrame jniFrame(env);
    jstring jText = NewJavaString(&jniFrame, aText);
    jobject obj = env->CallStaticObjectMethod(mAndroidSmsMessageClass,
                                              jCalculateLength, jText, JNI_FALSE);
    if (jniFrame.CheckForException())
        return NS_ERROR_FAILURE;

    jintArray arr = static_cast<jintArray>(obj);
    if (!arr || env->GetArrayLength(arr) != 4)
        return NS_ERROR_FAILURE;

    jint* info = env->GetIntArrayElements(arr, JNI_FALSE);

    aData->segments() = info[0]; // msgCount
    aData->charsPerSegment() = info[2]; // codeUnitsRemaining
    // segmentChars = (codeUnitCount + codeUnitsRemaining) / msgCount
    aData->charsAvailableInLastSegment() = (info[1] + info[2]) / info[0];

    env->ReleaseIntArrayElements(arr, info, JNI_ABORT);
    return NS_OK;
#endif
}

void
AndroidBridge::SendMessage(const nsAString& aNumber,
                           const nsAString& aMessage,
                           nsIMobileMessageCallback* aRequest)
{
    ALOG_BRIDGE("AndroidBridge::SendMessage");

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    uint32_t requestId;
    if (!QueueSmsRequest(aRequest, &requestId))
        return;

    AutoLocalJNIFrame jniFrame(env);
    jstring jNumber = NewJavaString(&jniFrame, aNumber);
    jstring jMessage = NewJavaString(&jniFrame, aMessage);

    env->CallStaticVoidMethod(mGeckoAppShellClass, jSendMessage, jNumber, jMessage, requestId);
}

void
AndroidBridge::GetMessage(int32_t aMessageId, nsIMobileMessageCallback* aRequest)
{
    ALOG_BRIDGE("AndroidBridge::GetMessage");

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    uint32_t requestId;
    if (!QueueSmsRequest(aRequest, &requestId))
        return;

    AutoLocalJNIFrame jniFrame(env, 0);
    env->CallStaticVoidMethod(mGeckoAppShellClass, jGetMessage, aMessageId, requestId);
}

void
AndroidBridge::DeleteMessage(int32_t aMessageId, nsIMobileMessageCallback* aRequest)
{
    ALOG_BRIDGE("AndroidBridge::DeleteMessage");

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    uint32_t requestId;
    if (!QueueSmsRequest(aRequest, &requestId))
        return;

    AutoLocalJNIFrame jniFrame(env, 0);
    env->CallStaticVoidMethod(mGeckoAppShellClass, jDeleteMessage, aMessageId, requestId);
}

void
AndroidBridge::CreateMessageList(const dom::mobilemessage::SmsFilterData& aFilter, bool aReverse,
                                 nsIMobileMessageCallback* aRequest)
{
    ALOG_BRIDGE("AndroidBridge::CreateMessageList");

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    uint32_t requestId;
    if (!QueueSmsRequest(aRequest, &requestId))
        return;

    AutoLocalJNIFrame jniFrame(env);

    jobjectArray numbers =
        (jobjectArray)env->NewObjectArray(aFilter.numbers().Length(),
                                          jStringClass,
                                          NewJavaString(&jniFrame, EmptyString()));

    for (uint32_t i = 0; i < aFilter.numbers().Length(); ++i) {
        env->SetObjectArrayElement(numbers, i,
                                   NewJavaString(&jniFrame, aFilter.numbers()[i]));
    }

    env->CallStaticVoidMethod(mGeckoAppShellClass, jCreateMessageList,
                              aFilter.startDate(), aFilter.endDate(),
                              numbers, aFilter.numbers().Length(),
                              aFilter.delivery(), aReverse, requestId);
}

void
AndroidBridge::GetNextMessageInList(int32_t aListId, nsIMobileMessageCallback* aRequest)
{
    ALOG_BRIDGE("AndroidBridge::GetNextMessageInList");

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    uint32_t requestId;
    if (!QueueSmsRequest(aRequest, &requestId))
        return;

    AutoLocalJNIFrame jniFrame(env, 0);
    env->CallStaticVoidMethod(mGeckoAppShellClass, jGetNextMessageinList, aListId, requestId);
}

void
AndroidBridge::ClearMessageList(int32_t aListId)
{
    ALOG_BRIDGE("AndroidBridge::ClearMessageList");

    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env, 0);
    env->CallStaticVoidMethod(mGeckoAppShellClass, jClearMessageList, aListId);
}

bool
AndroidBridge::QueueSmsRequest(nsIMobileMessageCallback* aRequest, uint32_t* aRequestIdOut)
{
    MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");
    MOZ_ASSERT(aRequest && aRequestIdOut);

    const uint32_t length = mSmsRequests.Length();
    for (uint32_t i = 0; i < length; i++) {
        if (!(mSmsRequests)[i]) {
            (mSmsRequests)[i] = aRequest;
            *aRequestIdOut = i;
            return true;
        }
    }

    mSmsRequests.AppendElement(aRequest);

    // After AppendElement(), previous `length` points to the new tail element.
    *aRequestIdOut = length;
    return true;
}

already_AddRefed<nsIMobileMessageCallback>
AndroidBridge::DequeueSmsRequest(uint32_t aRequestId)
{
    MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

    MOZ_ASSERT(aRequestId < mSmsRequests.Length());
    if (aRequestId >= mSmsRequests.Length()) {
        return nullptr;
    }

    return mSmsRequests[aRequestId].forget();
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
    // an array of double even if we actually want a double, two booleans, and an integer.
    jobject obj = env->CallStaticObjectMethod(mGeckoAppShellClass, jGetCurrentNetworkInformation);
    if (jniFrame.CheckForException())
        return;

    jdoubleArray arr = static_cast<jdoubleArray>(obj);
    if (!arr || env->GetArrayLength(arr) != 4) {
        return;
    }

    jdouble* info = env->GetDoubleArrayElements(arr, 0);

    aNetworkInfo->bandwidth() = info[0];
    aNetworkInfo->canBeMetered() = info[1] == 1.0f;
    aNetworkInfo->isWifi() = info[2] == 1.0f;
    aNetworkInfo->dhcpGateway() = info[3];

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
        return nullptr;

    AutoLocalJNIFrame jniFrame(env);

    int err;
    void *buf;

    if ((err = AndroidBitmap_lockPixels(env, bitmap, &buf)) != 0) {
        ALOG_BRIDGE("AndroidBitmap_lockPixels failed! (error %d)", err);
        buf = nullptr;
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

    if (mHasNativeWindowFallback)
        return GetNativeSurface(aEnv, aSurface);

    return nullptr;
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

void*
AndroidBridge::AcquireNativeWindowFromSurfaceTexture(JNIEnv* aEnv, jobject aSurfaceTexture)
{
    OpenGraphicsLibraries();

    if (mHasNativeWindowAccess && ANativeWindow_fromSurfaceTexture)
        return ANativeWindow_fromSurfaceTexture(aEnv, aSurfaceTexture);

    if (mHasNativeWindowAccess && android_SurfaceTexture_getNativeWindow) {
        android::sp<AndroidRefable> window = android_SurfaceTexture_getNativeWindow(aEnv, aSurfaceTexture);
        return window.get();
    }

    return nullptr;
}

void
AndroidBridge::ReleaseNativeWindowForSurfaceTexture(void *window)
{
    if (!window)
        return;

    // FIXME: we don't ref the pointer we get, so nothing to do currently. We should ref it.
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
    struct SurfaceInfo {
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

jobject
AndroidBridge::GetContext() {
    JNIEnv *env = GetJNIForThread();
    if (!env)
        return 0;

    jobject context = env->CallStaticObjectMethod(mGeckoAppShellClass, jGetContext);
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        return 0;
    }

    return context;
}

jobject
AndroidBridge::GetGlobalContextRef() {
    JNIEnv *env = GetJNIForThread();
    if (!env)
        return 0;

    AutoLocalJNIFrame jniFrame(env, 0);

    jobject context = GetContext();

    jobject globalRef = env->NewGlobalRef(context);
    MOZ_ASSERT(globalRef);

    return globalRef;
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
AndroidBridge::SetFirstPaintViewport(const LayerIntPoint& aOffset, const CSSToLayerScale& aZoom, const CSSRect& aCssPageRect)
{
    AndroidGeckoLayerClient *client = mLayerClient;
    if (!client)
        return;

    client->SetFirstPaintViewport(aOffset, aZoom, aCssPageRect);
}

void
AndroidBridge::SetPageRect(const CSSRect& aCssPageRect)
{
    AndroidGeckoLayerClient *client = mLayerClient;
    if (!client)
        return;

    client->SetPageRect(aCssPageRect);
}

void
AndroidBridge::SyncViewportInfo(const LayerIntRect& aDisplayPort, const CSSToLayerScale& aDisplayResolution,
                                bool aLayersUpdated, ScreenPoint& aScrollOffset, CSSToScreenScale& aScale,
                                LayerMargin& aFixedLayerMargins, ScreenPoint& aOffset)
{
    AndroidGeckoLayerClient *client = mLayerClient;
    if (!client)
        return;

    client->SyncViewportInfo(aDisplayPort, aDisplayResolution, aLayersUpdated,
                             aScrollOffset, aScale, aFixedLayerMargins,
                             aOffset);
}

void AndroidBridge::SyncFrameMetrics(const ScreenPoint& aScrollOffset, float aZoom, const CSSRect& aCssPageRect,
                                     bool aLayersUpdated, const CSSRect& aDisplayPort, const CSSToLayerScale& aDisplayResolution,
                                     bool aIsFirstPaint, LayerMargin& aFixedLayerMargins, ScreenPoint& aOffset)
{
    AndroidGeckoLayerClient *client = mLayerClient;
    if (!client)
        return;

    client->SyncFrameMetrics(aScrollOffset, aZoom, aCssPageRect,
                             aLayersUpdated, aDisplayPort, aDisplayResolution,
                             aIsFirstPaint, aFixedLayerMargins, aOffset);
}

AndroidBridge::AndroidBridge()
  : mLayerClient(NULL),
    mNativePanZoomController(NULL)
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
NS_IMETHODIMP nsAndroidBridge::HandleGeckoMessage(const nsAString & message, nsAString &aRet)
{
    AndroidBridge::Bridge()->HandleGeckoMessage(message, aRet);
    return NS_OK;
}

/* nsIAndroidDisplayport getDisplayPort(in boolean aPageSizeUpdate, in boolean isBrowserContentDisplayed, in int32_t tabId, in nsIAndroidViewport metrics); */
NS_IMETHODIMP nsAndroidBridge::GetDisplayPort(bool aPageSizeUpdate, bool aIsBrowserContentDisplayed, int32_t tabId, nsIAndroidViewport* metrics, nsIAndroidDisplayport** displayPort)
{
    AndroidBridge::Bridge()->GetDisplayPort(aPageSizeUpdate, aIsBrowserContentDisplayed, tabId, metrics, displayPort);
    return NS_OK;
}

/* void displayedDocumentChanged(); */
NS_IMETHODIMP nsAndroidBridge::ContentDocumentChanged()
{
    AndroidBridge::Bridge()->ContentDocumentChanged();
    return NS_OK;
}

/* boolean isContentDocumentDisplayed(); */
NS_IMETHODIMP nsAndroidBridge::IsContentDocumentDisplayed(bool *aRet)
{
    *aRet = AndroidBridge::Bridge()->IsContentDocumentDisplayed();
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
        jEnv = static_cast<JNIEnv*>(PR_GetThreadPrivate(sJavaEnvThreadIndex));

        if (jEnv)
            return jEnv;

        int status = jVm->GetEnv((void**) &jEnv, JNI_VERSION_1_2);
        if (status) {

            status = jVm->AttachCurrentThread(&jEnv, NULL);
            if (status) {
                __android_log_print(ANDROID_LOG_INFO, "GetJNIForThread",  "Could not attach");
                return NULL;
            }
            
            PR_SetThreadPrivate(sJavaEnvThreadIndex, jEnv);
        }
        if (!jEnv) {
            __android_log_print(ANDROID_LOG_INFO, "GetJNIForThread", "returning NULL");
        }
        return jEnv;
    }
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

bool
AndroidBridge::PumpMessageLoop()
{
    JNIEnv* env = GetJNIEnv();
    if (!env)
        return false;

    AutoLocalJNIFrame jniFrame(env, 0);

    if ((void*)pthread_self() != mThread)
        return false;

    return env->CallStaticBooleanMethod(mGeckoAppShellClass, jPumpMessageLoop);
}

void
AndroidBridge::NotifyWakeLockChanged(const nsAString& topic, const nsAString& state)
{
    JNIEnv* env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env);

    jstring jstrTopic = NewJavaString(&jniFrame, topic);
    jstring jstrState = NewJavaString(&jniFrame, state);

    env->CallStaticVoidMethod(mGeckoAppShellClass, jNotifyWakeLockChanged, jstrTopic, jstrState);
}

void
AndroidBridge::ScheduleComposite()
{
    nsWindow::ScheduleComposite();
}

void
AndroidBridge::RegisterSurfaceTextureFrameListener(jobject surfaceTexture, int id)
{
    JNIEnv* env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env);

    env->CallStaticVoidMethod(mGeckoAppShellClass, jRegisterSurfaceTextureFrameListener, surfaceTexture, id);
}

void
AndroidBridge::UnregisterSurfaceTextureFrameListener(jobject surfaceTexture)
{
    // This function is called on a worker thread when the Flash plugin is unloaded.
    JNIEnv* env = GetJNIForThread();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env);

    env->CallStaticVoidMethod(mGeckoAppShellClass, jUnregisterSurfaceTextureFrameListener, surfaceTexture);
}

void
AndroidBridge::GetGfxInfoData(nsACString& aRet)
{
    ALOG_BRIDGE("AndroidBridge::GetGfxInfoData");

    JNIEnv* env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env);
    jstring jstrRet = static_cast<jstring>
        (env->CallStaticObjectMethod(mGeckoAppShellClass, jGetGfxInfoData));
    if (jniFrame.CheckForException())
        return;

    nsJNIString jniStr(jstrRet, env);
    CopyUTF16toUTF8(jniStr, aRet);
}

nsresult
AndroidBridge::GetProxyForURI(const nsACString & aSpec,
                              const nsACString & aScheme,
                              const nsACString & aHost,
                              const int32_t      aPort,
                              nsACString & aResult)
{
    JNIEnv* env = GetJNIEnv();
    if (!env)
        return NS_ERROR_FAILURE;

    AutoLocalJNIFrame jniFrame(env);

    jstring jSpec = NewJavaString(&jniFrame, aSpec);
    jstring jScheme = NewJavaString(&jniFrame, aScheme);
    jstring jHost = NewJavaString(&jniFrame, aHost);
    jstring jstrRet = static_cast<jstring>
        (env->CallStaticObjectMethod(mGeckoAppShellClass, jGetProxyForURI, jSpec, jScheme, jHost, aPort));

    if (jniFrame.CheckForException())
        return NS_ERROR_FAILURE;

    nsJNIString jniStr(jstrRet, env);
    CopyUTF16toUTF8(jniStr, aResult);
    return NS_OK;
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
AndroidBridge::AddPluginView(jobject view, const LayoutDeviceRect& rect, bool isFullScreen) {
    JNIEnv *env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env);

    nsWindow* win = nsWindow::TopWindow();
    if (!win)
        return;

    CSSRect cssRect = rect / CSSToLayoutDeviceScale(win->GetDefaultScale());
    env->CallStaticVoidMethod(sBridge->mGeckoAppShellClass,
                              sBridge->jAddPluginView, view,
                              cssRect.x, cssRect.y, cssRect.width, cssRect.height,
                              isFullScreen);
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

void
AndroidBridge::StartJavaProfiling(int aInterval, int aSamples)
{
    JNIEnv* env = GetJNIForThread();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env);

    env->CallStaticVoidMethod(AndroidBridge::Bridge()->jGeckoJavaSamplerClass,
                              AndroidBridge::Bridge()->jStart,
                              aInterval, aSamples);
}

void
AndroidBridge::StopJavaProfiling()
{
    JNIEnv* env = GetJNIForThread();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env);

    env->CallStaticVoidMethod(AndroidBridge::Bridge()->jGeckoJavaSamplerClass,
                              AndroidBridge::Bridge()->jStop);
}

void
AndroidBridge::PauseJavaProfiling()
{
    JNIEnv* env = GetJNIForThread();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env);

    env->CallStaticVoidMethod(AndroidBridge::Bridge()->jGeckoJavaSamplerClass,
                              AndroidBridge::Bridge()->jPause);
}

void
AndroidBridge::UnpauseJavaProfiling()
{
    JNIEnv* env = GetJNIForThread();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env);

    env->CallStaticVoidMethod(AndroidBridge::Bridge()->jGeckoJavaSamplerClass,
                              AndroidBridge::Bridge()->jUnpause);
}

bool
AndroidBridge::GetThreadNameJavaProfiling(uint32_t aThreadId, nsCString & aResult)
{
    JNIEnv* env = GetJNIForThread();
    if (!env)
        return false;

    AutoLocalJNIFrame jniFrame(env);

    jstring jstrThreadName = static_cast<jstring>(
        env->CallStaticObjectMethod(AndroidBridge::Bridge()->jGeckoJavaSamplerClass,
                                    AndroidBridge::Bridge()->jGetThreadName,
                                    aThreadId));

    if (!jstrThreadName)
        return false;

    nsJNIString jniStr(jstrThreadName, env);
    CopyUTF16toUTF8(jniStr.get(), aResult);
    return true;
}

bool
AndroidBridge::GetFrameNameJavaProfiling(uint32_t aThreadId, uint32_t aSampleId,
                                          uint32_t aFrameId, nsCString & aResult)
{
    JNIEnv* env = GetJNIForThread();
    if (!env)
        return false;

    AutoLocalJNIFrame jniFrame(env);

    jstring jstrSampleName = static_cast<jstring>(
        env->CallStaticObjectMethod(AndroidBridge::Bridge()->jGeckoJavaSamplerClass,
                                    AndroidBridge::Bridge()->jGetFrameName,
                                    aThreadId, aSampleId, aFrameId));

    if (!jstrSampleName)
        return false;

    nsJNIString jniStr(jstrSampleName, env);
    CopyUTF16toUTF8(jniStr.get(), aResult);
    return true;
}

double
AndroidBridge::GetSampleTimeJavaProfiling(uint32_t aThreadId, uint32_t aSampleId)
{
    JNIEnv* env = GetJNIForThread();
    if (!env)
        return 0;

    AutoLocalJNIFrame jniFrame(env);

    jdouble jSampleTime =
        env->CallStaticDoubleMethod(AndroidBridge::Bridge()->jGeckoJavaSamplerClass,
                                    AndroidBridge::Bridge()->jGetSampleTime,
                                    aThreadId, aSampleId);

    return jSampleTime;
}

void
AndroidBridge::SendThumbnail(jobject buffer, int32_t tabId, bool success) {
    // Regardless of whether we successfully captured a thumbnail, we need to
    // send a response to process the remaining entries in the queue. If we
    // don't get an env here, we'll stall the thumbnail loop, but there isn't
    // much we can do about it.
    JNIEnv* env = GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env, 0);
    env->CallStaticVoidMethod(AndroidBridge::Bridge()->jThumbnailHelperClass,
                              AndroidBridge::Bridge()->jNotifyThumbnail,
                              buffer, tabId, success);
}

nsresult AndroidBridge::CaptureThumbnail(nsIDOMWindow *window, int32_t bufW, int32_t bufH, int32_t tabId, jobject buffer)
{
    nsresult rv;
    float scale = 1.0;

    if (!buffer)
        return NS_ERROR_FAILURE;

    // take a screenshot, as wide as possible, proportional to the destination size
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

    int32_t srcX = left;
    int32_t srcY = top;
    int32_t srcW;
    int32_t srcH;

    float aspectRatio = ((float) bufW) / bufH;
    if (width / aspectRatio < height) {
        srcW = width;
        srcH = width / aspectRatio;
    } else {
        srcW = height * aspectRatio;
        srcH = height;
    }

    JNIEnv* env = GetJNIEnv();
    if (!env)
        return NS_ERROR_FAILURE;

    AutoLocalJNIFrame jniFrame(env, 0);

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
    nsCOMPtr<nsIPresShell> presShell = presContext->PresShell();
    uint32_t renderDocFlags = (nsIPresShell::RENDER_IGNORE_VIEWPORT_SCROLLING |
                               nsIPresShell::RENDER_DOCUMENT_RELATIVE);
    nsRect r(nsPresContext::CSSPixelsToAppUnits(srcX / scale),
             nsPresContext::CSSPixelsToAppUnits(srcY / scale),
             nsPresContext::CSSPixelsToAppUnits(srcW / scale),
             nsPresContext::CSSPixelsToAppUnits(srcH / scale));

    bool is24bit = (GetScreenDepth() == 24);
    uint32_t stride = bufW * (is24bit ? 4 : 2);

    void* data = env->GetDirectBufferAddress(buffer);
    if (!data)
        return NS_ERROR_FAILURE;

    nsRefPtr<gfxImageSurface> surf =
        new gfxImageSurface(static_cast<unsigned char*>(data), nsIntSize(bufW, bufH), stride,
                            is24bit ? gfxASurface::ImageFormatRGB24 :
                                      gfxASurface::ImageFormatRGB16_565);
    if (surf->CairoStatus() != 0) {
        ALOG_BRIDGE("Error creating gfxImageSurface");
        return NS_ERROR_FAILURE;
    }
    nsRefPtr<gfxContext> context = new gfxContext(surf);
    gfxPoint pt(0, 0);
    context->Translate(pt);
    context->Scale(scale * bufW / srcW, scale * bufH / srcH);
    rv = presShell->RenderDocument(r, renderDocFlags, bgColor, context);
    if (is24bit) {
        gfxUtils::ConvertBGRAtoRGBA(surf);
    }
    NS_ENSURE_SUCCESS(rv, rv);
    return NS_OK;
}

void
AndroidBridge::GetDisplayPort(bool aPageSizeUpdate, bool aIsBrowserContentDisplayed, int32_t tabId, nsIAndroidViewport* metrics, nsIAndroidDisplayport** displayPort)
{
    JNIEnv* env = GetJNIEnv();
    if (!env || !mLayerClient)
        return;
    AutoLocalJNIFrame jniFrame(env, 0);
    mLayerClient->GetDisplayPort(&jniFrame, aPageSizeUpdate, aIsBrowserContentDisplayed, tabId, metrics, displayPort);
}

void
AndroidBridge::ContentDocumentChanged()
{
    JNIEnv* env = GetJNIEnv();
    if (!env || !mLayerClient)
        return;
    AutoLocalJNIFrame jniFrame(env, 0);
    mLayerClient->ContentDocumentChanged(&jniFrame);
}

bool
AndroidBridge::IsContentDocumentDisplayed()
{
    JNIEnv* env = GetJNIEnv();
    if (!env || !mLayerClient)
        return false;
    AutoLocalJNIFrame jniFrame(env, 0);
    return mLayerClient->IsContentDocumentDisplayed(&jniFrame);
}

bool
AndroidBridge::ProgressiveUpdateCallback(bool aHasPendingNewThebesContent, const LayerRect& aDisplayPort, float aDisplayResolution, bool aDrawingCritical, gfx::Rect& aViewport, float& aScaleX, float& aScaleY)
{
    AndroidGeckoLayerClient *client = mLayerClient;
    if (!client)
        return false;

    return client->ProgressiveUpdateCallback(aHasPendingNewThebesContent, aDisplayPort, aDisplayResolution, aDrawingCritical, aViewport, aScaleX, aScaleY);
}

void
AndroidBridge::KillAnyZombies()
{
    JNIEnv* env = GetJNIEnv();
    if (!env)
        return;
    env->CallStaticVoidMethod(mGeckoAppShellClass, AndroidBridge::Bridge()->jKillAnyZombies);
}

bool
AndroidBridge::UnlockProfile()
{
    JNIEnv* env = GetJNIEnv();
    if (!env)
        return false;

    AutoLocalJNIFrame jniFrame(env, 0);
    bool ret = env->CallStaticBooleanMethod(mGeckoAppShellClass, jUnlockProfile);
    if (jniFrame.CheckForException())
        return false;

    return ret;
}

jobject
AndroidBridge::SetNativePanZoomController(jobject obj)
{
    jobject old = mNativePanZoomController;
    mNativePanZoomController = obj;
    return old;
}

void
AndroidBridge::RequestContentRepaint(const mozilla::layers::FrameMetrics& aFrameMetrics)
{
    ALOG_BRIDGE("AndroidBridge::RequestContentRepaint");
    JNIEnv* env = GetJNIForThread();    // called on the compositor thread
    if (!env || !mNativePanZoomController) {
        return;
    }

    CSSToScreenScale resolution = aFrameMetrics.CalculateResolution();
    ScreenRect dp = (aFrameMetrics.mDisplayPort + aFrameMetrics.mScrollOffset) * resolution;

    AutoLocalJNIFrame jniFrame(env, 0);
    env->CallVoidMethod(mNativePanZoomController, jRequestContentRepaint,
        dp.x, dp.y, dp.width, dp.height, resolution.scale);
}

void
AndroidBridge::HandleDoubleTap(const CSSIntPoint& aPoint)
{
    nsCString data = nsPrintfCString("{ \"x\": %d, \"y\": %d }", aPoint.x, aPoint.y);
    nsAppShell::gAppShell->PostEvent(AndroidGeckoEvent::MakeBroadcastEvent(
            NS_LITERAL_CSTRING("Gesture:DoubleTap"), data));
}

void
AndroidBridge::HandleSingleTap(const CSSIntPoint& aPoint)
{
    nsCString data = nsPrintfCString("{ \"x\": %d, \"y\": %d }", aPoint.x, aPoint.y);
    nsAppShell::gAppShell->PostEvent(AndroidGeckoEvent::MakeBroadcastEvent(
            NS_LITERAL_CSTRING("Gesture:SingleTap"), data));
}

void
AndroidBridge::HandleLongTap(const CSSIntPoint& aPoint)
{
    nsCString data = nsPrintfCString("{ \"x\": %d, \"y\": %d }", aPoint.x, aPoint.y);
    nsAppShell::gAppShell->PostEvent(AndroidGeckoEvent::MakeBroadcastEvent(
            NS_LITERAL_CSTRING("Gesture:LongPress"), data));
}

void
AndroidBridge::SendAsyncScrollDOMEvent(mozilla::layers::FrameMetrics::ViewID aScrollId,
                                       const CSSRect& aContentRect,
                                       const CSSSize& aScrollableSize)
{
    // FIXME implement this
}

void
AndroidBridge::PostDelayedTask(Task* aTask, int aDelayMs)
{
    JNIEnv* env = GetJNIForThread();
    if (!env || !mNativePanZoomController) {
        return;
    }

    // add the new task into the mDelayedTaskQueue, sorted with
    // the earliest task first in the queue
    DelayedTask* newTask = new DelayedTask(aTask, aDelayMs);
    uint32_t i = 0;
    while (i < mDelayedTaskQueue.Length()) {
        if (newTask->IsEarlierThan(mDelayedTaskQueue[i])) {
            mDelayedTaskQueue.InsertElementAt(i, newTask);
            break;
        }
        i++;
    }
    if (i == mDelayedTaskQueue.Length()) {
        // this new task will run after all the existing tasks in the queue
        mDelayedTaskQueue.AppendElement(newTask);
    }
    if (i == 0) {
        // if we're inserting it at the head of the queue, notify Java because
        // we need to get a callback at an earlier time than the last scheduled
        // callback
        AutoLocalJNIFrame jniFrame(env, 0);
        env->CallVoidMethod(mNativePanZoomController, jPostDelayedCallback, (int64_t)aDelayMs);
    }
}

int64_t
AndroidBridge::RunDelayedTasks()
{
    while (mDelayedTaskQueue.Length() > 0) {
        DelayedTask* nextTask = mDelayedTaskQueue[0];
        int64_t timeLeft = nextTask->MillisecondsToRunTime();
        if (timeLeft > 0) {
            // this task (and therefore all remaining tasks)
            // have not yet reached their runtime. return the
            // time left until we should be called again
            return timeLeft;
        }

        // we have a delayed task to run. extract it from
        // the wrapper and free the wrapper

        mDelayedTaskQueue.RemoveElementAt(0);
        Task* task = nextTask->GetTask();
        delete nextTask;

        task->Run();
    }
    return -1;
}
