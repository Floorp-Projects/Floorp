/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <android/log.h>
#include <dlfcn.h>
#include <math.h>
#include <GLES2/gl2.h>

#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/layers/CompositorBridgeParent.h"

#include "mozilla/Hal.h"
#include "nsXULAppAPI.h"
#include <prthread.h>
#include "nsXPCOMStrings.h"
#include "AndroidBridge.h"
#include "AndroidJNIWrapper.h"
#include "AndroidBridgeUtilities.h"
#include "nsAlertsUtils.h"
#include "nsAppShell.h"
#include "nsOSHelperAppService.h"
#include "nsWindow.h"
#include "mozilla/Preferences.h"
#include "nsThreadUtils.h"
#include "nsIThreadManager.h"
#include "gfxPlatform.h"
#include "gfxContext.h"
#include "mozilla/gfx/2D.h"
#include "gfxUtils.h"
#include "nsPresContext.h"
#include "nsIDocShell.h"
#include "nsPIDOMWindow.h"
#include "mozilla/dom/ScreenOrientation.h"
#include "nsIDOMWindowUtils.h"
#include "nsIDOMClientRect.h"
#include "mozilla/ClearOnShutdown.h"
#include "nsPrintfCString.h"
#include "nsContentUtils.h"
#include "nsIScriptError.h"
#include "nsIHttpChannel.h"

#include "EventDispatcher.h"
#include "MediaCodec.h"
#include "SurfaceTexture.h"
#include "GLContextProvider.h"

#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/ContentChild.h"
#include "nsIObserverService.h"
#include "nsISupportsPrimitives.h"
#include "MediaPrefs.h"
#include "WidgetUtils.h"

#include "FennecJNIWrappers.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::jni;
using namespace mozilla::java;

AndroidBridge* AndroidBridge::sBridge = nullptr;
static jobject sGlobalContext = nullptr;
nsDataHashtable<nsStringHashKey, nsString> AndroidBridge::sStoragePaths;

jmethodID AndroidBridge::GetMethodID(JNIEnv* env, jclass jClass,
                              const char* methodName, const char* methodType)
{
   jmethodID methodID = env->GetMethodID(jClass, methodName, methodType);
   if (!methodID) {
       ALOG(">>> FATAL JNI ERROR! GetMethodID(methodName=\"%s\", "
            "methodType=\"%s\") failed. Did ProGuard optimize away something it shouldn't have?",
            methodName, methodType);
       env->ExceptionDescribe();
       MOZ_CRASH();
   }
   return methodID;
}

jmethodID AndroidBridge::GetStaticMethodID(JNIEnv* env, jclass jClass,
                               const char* methodName, const char* methodType)
{
  jmethodID methodID = env->GetStaticMethodID(jClass, methodName, methodType);
  if (!methodID) {
      ALOG(">>> FATAL JNI ERROR! GetStaticMethodID(methodName=\"%s\", "
           "methodType=\"%s\") failed. Did ProGuard optimize away something it shouldn't have?",
           methodName, methodType);
      env->ExceptionDescribe();
      MOZ_CRASH();
  }
  return methodID;
}

jfieldID AndroidBridge::GetFieldID(JNIEnv* env, jclass jClass,
                           const char* fieldName, const char* fieldType)
{
    jfieldID fieldID = env->GetFieldID(jClass, fieldName, fieldType);
    if (!fieldID) {
        ALOG(">>> FATAL JNI ERROR! GetFieldID(fieldName=\"%s\", "
             "fieldType=\"%s\") failed. Did ProGuard optimize away something it shouldn't have?",
             fieldName, fieldType);
        env->ExceptionDescribe();
        MOZ_CRASH();
    }
    return fieldID;
}

jfieldID AndroidBridge::GetStaticFieldID(JNIEnv* env, jclass jClass,
                           const char* fieldName, const char* fieldType)
{
    jfieldID fieldID = env->GetStaticFieldID(jClass, fieldName, fieldType);
    if (!fieldID) {
        ALOG(">>> FATAL JNI ERROR! GetStaticFieldID(fieldName=\"%s\", "
             "fieldType=\"%s\") failed. Did ProGuard optimize away something it shouldn't have?",
             fieldName, fieldType);
        env->ExceptionDescribe();
        MOZ_CRASH();
    }
    return fieldID;
}

void
AndroidBridge::ConstructBridge()
{
    /* NSS hack -- bionic doesn't handle recursive unloads correctly,
     * because library finalizer functions are called with the dynamic
     * linker lock still held.  This results in a deadlock when trying
     * to call dlclose() while we're already inside dlclose().
     * Conveniently, NSS has an env var that can prevent it from unloading.
     */
    putenv("NSS_DISABLE_UNLOAD=1");

    MOZ_ASSERT(!sBridge);
    sBridge = new AndroidBridge();

    MediaPrefs::GetSingleton();
}

void
AndroidBridge::DeconstructBridge()
{
    if (sBridge) {
        delete sBridge;
        // AndroidBridge destruction requires sBridge to still be valid,
        // so we set sBridge to nullptr after deleting it.
        sBridge = nullptr;
    }
}

AndroidBridge::~AndroidBridge()
{
}

AndroidBridge::AndroidBridge()
  : mUiTaskQueueLock("UiTaskQueue")
{
    ALOG_BRIDGE("AndroidBridge::Init");

    JNIEnv* const jEnv = jni::GetGeckoThreadEnv();
    AutoLocalJNIFrame jniFrame(jEnv);

    mMessageQueue = java::GeckoThread::MsgQueue();
    auto msgQueueClass = Class::LocalRef::Adopt(
            jEnv, jEnv->GetObjectClass(mMessageQueue.Get()));
    // mMessageQueueNext must not be null
    mMessageQueueNext = GetMethodID(
            jEnv, msgQueueClass.Get(), "next", "()Landroid/os/Message;");
    // mMessageQueueMessages may be null (e.g. due to proguard optimization)
    mMessageQueueMessages = jEnv->GetFieldID(
            msgQueueClass.Get(), "mMessages", "Landroid/os/Message;");

    AutoJNIClass string(jEnv, "java/lang/String");
    jStringClass = string.getGlobalRef();

    if (!GetStaticIntField("android/os/Build$VERSION", "SDK_INT", &mAPIVersion, jEnv)) {
        ALOG_BRIDGE("Failed to find API version");
    }

    AutoJNIClass channels(jEnv, "java/nio/channels/Channels");
    jChannels = channels.getGlobalRef();
    jChannelCreate = channels.getStaticMethod("newChannel", "(Ljava/io/InputStream;)Ljava/nio/channels/ReadableByteChannel;");

    AutoJNIClass readableByteChannel(jEnv, "java/nio/channels/ReadableByteChannel");
    jReadableByteChannel = readableByteChannel.getGlobalRef();
    jByteBufferRead = readableByteChannel.getMethod("read", "(Ljava/nio/ByteBuffer;)I");

    AutoJNIClass inputStream(jEnv, "java/io/InputStream");
    jInputStream = inputStream.getGlobalRef();
    jClose = inputStream.getMethod("close", "()V");
    jAvailable = inputStream.getMethod("available", "()I");
}

// Raw JNIEnv variants.
jstring AndroidBridge::NewJavaString(JNIEnv* env, const char16_t* string, uint32_t len) {
   jstring ret = env->NewString(reinterpret_cast<const jchar*>(string), len);
   if (env->ExceptionCheck()) {
       ALOG_BRIDGE("Exceptional exit of: %s", __PRETTY_FUNCTION__);
       env->ExceptionDescribe();
       env->ExceptionClear();
       return nullptr;
    }
    return ret;
}

jstring AndroidBridge::NewJavaString(JNIEnv* env, const nsAString& string) {
    return NewJavaString(env, string.BeginReading(), string.Length());
}

jstring AndroidBridge::NewJavaString(JNIEnv* env, const char* string) {
    return NewJavaString(env, NS_ConvertUTF8toUTF16(string));
}

jstring AndroidBridge::NewJavaString(JNIEnv* env, const nsACString& string) {
    return NewJavaString(env, NS_ConvertUTF8toUTF16(string));
}

// AutoLocalJNIFrame variants..
jstring AndroidBridge::NewJavaString(AutoLocalJNIFrame* frame, const char16_t* string, uint32_t len) {
    return NewJavaString(frame->GetEnv(), string, len);
}

jstring AndroidBridge::NewJavaString(AutoLocalJNIFrame* frame, const nsAString& string) {
    return NewJavaString(frame, string.BeginReading(), string.Length());
}

jstring AndroidBridge::NewJavaString(AutoLocalJNIFrame* frame, const char* string) {
    return NewJavaString(frame, NS_ConvertUTF8toUTF16(string));
}

jstring AndroidBridge::NewJavaString(AutoLocalJNIFrame* frame, const nsACString& string) {
    return NewJavaString(frame, NS_ConvertUTF8toUTF16(string));
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

        AutoLocalJNIFrame jniFrame(aJNIEnv, 4);
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
AndroidBridge::GetHandlersForMimeType(const nsAString& aMimeType,
                                      nsIMutableArray *aHandlersArray,
                                      nsIHandlerApp **aDefaultApp,
                                      const nsAString& aAction)
{
    ALOG_BRIDGE("AndroidBridge::GetHandlersForMimeType");

    auto arr = GeckoAppShell::GetHandlersForMimeType(aMimeType, aAction);
    if (!arr)
        return false;

    JNIEnv* const env = arr.Env();
    jsize len = env->GetArrayLength(arr.Get());

    if (!aHandlersArray)
        return len > 0;

    getHandlersFromStringArray(env, arr.Get(), len, aHandlersArray,
                               aDefaultApp, aAction,
                               NS_ConvertUTF16toUTF8(aMimeType));
    return true;
}

bool
AndroidBridge::GetHWEncoderCapability()
{
  ALOG_BRIDGE("AndroidBridge::GetHWEncoderCapability");

  bool value = GeckoAppShell::GetHWEncoderCapability();

  return value;
}


bool
AndroidBridge::GetHWDecoderCapability()
{
  ALOG_BRIDGE("AndroidBridge::GetHWDecoderCapability");

  bool value = GeckoAppShell::GetHWDecoderCapability();

  return value;
}

bool
AndroidBridge::GetHandlersForURL(const nsAString& aURL,
                                 nsIMutableArray* aHandlersArray,
                                 nsIHandlerApp **aDefaultApp,
                                 const nsAString& aAction)
{
    ALOG_BRIDGE("AndroidBridge::GetHandlersForURL");

    auto arr = GeckoAppShell::GetHandlersForURL(aURL, aAction);
    if (!arr)
        return false;

    JNIEnv* const env = arr.Env();
    jsize len = env->GetArrayLength(arr.Get());

    if (!aHandlersArray)
        return len > 0;

    getHandlersFromStringArray(env, arr.Get(), len, aHandlersArray,
                               aDefaultApp, aAction);
    return true;
}

void
AndroidBridge::GetMimeTypeFromExtensions(const nsACString& aFileExt, nsCString& aMimeType)
{
    ALOG_BRIDGE("AndroidBridge::GetMimeTypeFromExtensions");

    auto jstrType = GeckoAppShell::GetMimeTypeFromExtensions(aFileExt);

    if (jstrType) {
        aMimeType = jstrType->ToCString();
    }
}

void
AndroidBridge::GetExtensionFromMimeType(const nsACString& aMimeType, nsACString& aFileExt)
{
    ALOG_BRIDGE("AndroidBridge::GetExtensionFromMimeType");

    auto jstrExt = GeckoAppShell::GetExtensionFromMimeType(aMimeType);

    if (jstrExt) {
        aFileExt = jstrExt->ToCString();
    }
}

bool
AndroidBridge::GetClipboardText(nsAString& aText)
{
    ALOG_BRIDGE("AndroidBridge::GetClipboardText");

    auto text = Clipboard::GetText();

    if (text) {
        aText = text->ToString();
    }
    return !!text;
}

int
AndroidBridge::GetDPI()
{
    static int sDPI = 0;
    if (sDPI)
        return sDPI;

    const int DEFAULT_DPI = 160;

    sDPI = GeckoAppShell::GetDpi();
    if (!sDPI) {
        return DEFAULT_DPI;
    }

    return sDPI;
}

int
AndroidBridge::GetScreenDepth()
{
    ALOG_BRIDGE("%s", __PRETTY_FUNCTION__);

    static int sDepth = 0;
    if (sDepth)
        return sDepth;

    const int DEFAULT_DEPTH = 16;

    if (jni::IsAvailable()) {
        sDepth = GeckoAppShell::GetScreenDepth();
    }
    if (!sDepth)
        return DEFAULT_DEPTH;

    return sDepth;
}
void
AndroidBridge::Vibrate(const nsTArray<uint32_t>& aPattern)
{
    ALOG_BRIDGE("%s", __PRETTY_FUNCTION__);

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
        GeckoAppShell::Vibrate(d);
        return;
    }

    // First element of the array vibrate() expects is how long to wait
    // *before* vibrating.  For us, this is always 0.

    JNIEnv* const env = jni::GetGeckoThreadEnv();
    AutoLocalJNIFrame jniFrame(env, 1);

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

    GeckoAppShell::Vibrate(LongArray::Ref::From(array), -1 /* don't repeat */);
}

void
AndroidBridge::GetSystemColors(AndroidSystemColors *aColors)
{

    NS_ASSERTION(aColors != nullptr, "AndroidBridge::GetSystemColors: aColors is null!");
    if (!aColors)
        return;

    auto arr = GeckoAppShell::GetSystemColors();
    if (!arr)
        return;

    JNIEnv* const env = arr.Env();
    uint32_t len = static_cast<uint32_t>(env->GetArrayLength(arr.Get()));
    jint *elements = env->GetIntArrayElements(arr.Get(), 0);

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

    env->ReleaseIntArrayElements(arr.Get(), elements, 0);
}

void
AndroidBridge::GetIconForExtension(const nsACString& aFileExt, uint32_t aIconSize, uint8_t * const aBuf)
{
    ALOG_BRIDGE("AndroidBridge::GetIconForExtension");
    NS_ASSERTION(aBuf != nullptr, "AndroidBridge::GetIconForExtension: aBuf is null!");
    if (!aBuf)
        return;

    auto arr = GeckoAppShell::GetIconForExtension(NS_ConvertUTF8toUTF16(aFileExt), aIconSize);

    NS_ASSERTION(arr != nullptr, "AndroidBridge::GetIconForExtension: Returned pixels array is null!");
    if (!arr)
        return;

    JNIEnv* const env = arr.Env();
    uint32_t len = static_cast<uint32_t>(env->GetArrayLength(arr.Get()));
    jbyte *elements = env->GetByteArrayElements(arr.Get(), 0);

    uint32_t bufSize = aIconSize * aIconSize * 4;
    NS_ASSERTION(len == bufSize, "AndroidBridge::GetIconForExtension: Pixels array is incomplete!");
    if (len == bufSize)
        memcpy(aBuf, elements, bufSize);

    env->ReleaseByteArrayElements(arr.Get(), elements, 0);
}

bool
AndroidBridge::GetStaticIntField(const char *className, const char *fieldName, int32_t* aInt, JNIEnv* jEnv /* = nullptr */)
{
    ALOG_BRIDGE("AndroidBridge::GetStaticIntField %s", fieldName);

    if (!jEnv) {
        if (!jni::IsAvailable()) {
            return false;
        }
        jEnv = jni::GetGeckoThreadEnv();
    }

    AutoJNIClass cls(jEnv, className);
    jfieldID field = cls.getStaticField(fieldName, "I");

    if (!field) {
        return false;
    }

    *aInt = static_cast<int32_t>(jEnv->GetStaticIntField(cls.getRawRef(), field));
    return true;
}

bool
AndroidBridge::GetStaticStringField(const char *className, const char *fieldName, nsAString &result, JNIEnv* jEnv /* = nullptr */)
{
    ALOG_BRIDGE("AndroidBridge::GetStaticStringField %s", fieldName);

    if (!jEnv) {
        if (!jni::IsAvailable()) {
            return false;
        }
        jEnv = jni::GetGeckoThreadEnv();
    }

    AutoLocalJNIFrame jniFrame(jEnv, 1);
    AutoJNIClass cls(jEnv, className);
    jfieldID field = cls.getStaticField(fieldName, "Ljava/lang/String;");

    if (!field) {
        return false;
    }

    jstring jstr = (jstring) jEnv->GetStaticObjectField(cls.getRawRef(), field);
    if (!jstr)
        return false;

    result.Assign(nsJNIString(jstr, jEnv));
    return true;
}

namespace mozilla {
    class TracerRunnable : public Runnable{
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


void
AndroidBridge::GetCurrentBatteryInformation(hal::BatteryInformation* aBatteryInfo)
{
    ALOG_BRIDGE("AndroidBridge::GetCurrentBatteryInformation");

    // To prevent calling too many methods through JNI, the Java method returns
    // an array of double even if we actually want a double and a boolean.
    auto arr = GeckoAppShell::GetCurrentBatteryInformation();

    JNIEnv* const env = arr.Env();
    if (!arr || env->GetArrayLength(arr.Get()) != 3) {
        return;
    }

    jdouble* info = env->GetDoubleArrayElements(arr.Get(), 0);

    aBatteryInfo->level() = info[0];
    aBatteryInfo->charging() = info[1] == 1.0f;
    aBatteryInfo->remainingTime() = info[2];

    env->ReleaseDoubleArrayElements(arr.Get(), info, 0);
}

void
AndroidBridge::GetCurrentNetworkInformation(hal::NetworkInformation* aNetworkInfo)
{
    ALOG_BRIDGE("AndroidBridge::GetCurrentNetworkInformation");

    // To prevent calling too many methods through JNI, the Java method returns
    // an array of double even if we actually want an integer, a boolean, and an integer.

    auto arr = GeckoAppShell::GetCurrentNetworkInformation();

    JNIEnv* const env = arr.Env();
    if (!arr || env->GetArrayLength(arr.Get()) != 3) {
        return;
    }

    jdouble* info = env->GetDoubleArrayElements(arr.Get(), 0);

    aNetworkInfo->type() = info[0];
    aNetworkInfo->isWifi() = info[1] == 1.0f;
    aNetworkInfo->dhcpGateway() = info[2];

    env->ReleaseDoubleArrayElements(arr.Get(), info, 0);
}

jobject
AndroidBridge::GetGlobalContextRef() {
    if (sGlobalContext) {
        return sGlobalContext;
    }

    JNIEnv* const env = GetEnvForThread();
    AutoLocalJNIFrame jniFrame(env, 4);

    auto context = GeckoAppShell::GetContext();
    if (!context) {
        ALOG_BRIDGE("%s: Could not GetContext()", __FUNCTION__);
        return 0;
    }
    jclass contextClass = env->FindClass("android/content/Context");
    if (!contextClass) {
        ALOG_BRIDGE("%s: Could not find Context class.", __FUNCTION__);
        return 0;
    }
    jmethodID mid = env->GetMethodID(contextClass, "getApplicationContext",
                                     "()Landroid/content/Context;");
    if (!mid) {
        ALOG_BRIDGE("%s: Could not find getApplicationContext.", __FUNCTION__);
        return 0;
    }
    jobject appContext = env->CallObjectMethod(context.Get(), mid);
    if (!appContext) {
        ALOG_BRIDGE("%s: getApplicationContext failed.", __FUNCTION__);
        return 0;
    }

    sGlobalContext = env->NewGlobalRef(appContext);
    MOZ_ASSERT(sGlobalContext);
    return sGlobalContext;
}

/* Implementation file */
NS_IMPL_ISUPPORTS(nsAndroidBridge,
                  nsIAndroidEventDispatcher,
                  nsIAndroidBridge,
                  nsIObserver)

nsAndroidBridge::nsAndroidBridge()
{
  if (jni::IsAvailable()) {
    RefPtr<widget::EventDispatcher> dispatcher = new widget::EventDispatcher();
    dispatcher->Attach(java::EventDispatcher::GetInstance(),
                       /* window */ nullptr);
    mEventDispatcher = dispatcher;
  }

  AddObservers();
}

nsAndroidBridge::~nsAndroidBridge()
{
}

NS_IMETHODIMP nsAndroidBridge::HandleGeckoMessage(JS::HandleValue val,
                                                  JSContext *cx)
{
    // Spit out a warning before sending the message.
    nsContentUtils::ReportToConsoleNonLocalized(
        NS_LITERAL_STRING("Use of handleGeckoMessage is deprecated. "
                          "Please use EventDispatcher from Messaging.jsm."),
        nsIScriptError::warningFlag,
        NS_LITERAL_CSTRING("nsIAndroidBridge"),
        nullptr);

    JS::RootedValue jsonVal(cx);

    if (val.isObject()) {
        jsonVal = val;

    } else {
        // Handle legacy JSON messages.
        if (!val.isString()) {
            return NS_ERROR_INVALID_ARG;
        }
        JS::RootedString jsonStr(cx, val.toString());

        if (!JS_ParseJSON(cx, jsonStr, &jsonVal) || !jsonVal.isObject()) {
            JS_ClearPendingException(cx);
            return NS_ERROR_INVALID_ARG;
        }
    }

    JS::RootedObject jsonObj(cx, &jsonVal.toObject());
    JS::RootedValue typeVal(cx);

    if (!JS_GetProperty(cx, jsonObj, "type", &typeVal)) {
        JS_ClearPendingException(cx);
        return NS_ERROR_INVALID_ARG;
    }

    return Dispatch(typeVal, jsonVal, /* callback */ nullptr, cx);
}

NS_IMETHODIMP nsAndroidBridge::ContentDocumentChanged(mozIDOMWindowProxy* aWindow)
{
    AndroidBridge::Bridge()->ContentDocumentChanged(aWindow);
    return NS_OK;
}

NS_IMETHODIMP nsAndroidBridge::IsContentDocumentDisplayed(mozIDOMWindowProxy* aWindow,
                                                          bool *aRet)
{
    *aRet = AndroidBridge::Bridge()->IsContentDocumentDisplayed(aWindow);
    return NS_OK;
}

NS_IMETHODIMP
nsAndroidBridge::Observe(nsISupports* aSubject, const char* aTopic,
                         const char16_t* aData)
{
  if (!strcmp(aTopic, "xpcom-shutdown")) {
    RemoveObservers();
  } else if (!strcmp(aTopic, "media-playback")) {
    ALOG_BRIDGE("nsAndroidBridge::Observe, get media-playback event.");

    nsCOMPtr<nsISupportsPRUint64> wrapper = do_QueryInterface(aSubject);
    if (!wrapper) {
      return NS_OK;
    }

    uint64_t windowId = 0;
    nsresult rv = wrapper->GetData(&windowId);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsAutoString activeStr(aData);
    bool isPlaying = activeStr.EqualsLiteral("active");
    UpdateAudioPlayingWindows(windowId, isPlaying);
  }
  return NS_OK;
}

void
nsAndroidBridge::UpdateAudioPlayingWindows(uint64_t aWindowId,
                                           bool aPlaying)
{
  // Request audio focus for the first audio playing window and abandon focus
  // for the last audio playing window.
  if (aPlaying && !mAudioPlayingWindows.Contains(aWindowId)) {
    mAudioPlayingWindows.AppendElement(aWindowId);
    if (mAudioPlayingWindows.Length() == 1) {
      ALOG_BRIDGE("nsAndroidBridge, request audio focus.");
      AudioFocusAgent::NotifyStartedPlaying();
    }
  } else if (!aPlaying && mAudioPlayingWindows.Contains(aWindowId)) {
    mAudioPlayingWindows.RemoveElement(aWindowId);
    if (mAudioPlayingWindows.Length() == 0) {
      ALOG_BRIDGE("nsAndroidBridge, abandon audio focus.");
      AudioFocusAgent::NotifyStoppedPlaying();
    }
  }
}

void
nsAndroidBridge::AddObservers()
{
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->AddObserver(this, "xpcom-shutdown", false);
    if (jni::IsFennec()) { // No AudioFocusAgent in non-Fennec environment.
        obs->AddObserver(this, "media-playback", false);
    }
  }
}

void
nsAndroidBridge::RemoveObservers()
{
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->RemoveObserver(this, "xpcom-shutdown");
    if (jni::IsFennec()) { // No AudioFocusAgent in non-Fennec environment.
        obs->RemoveObserver(this, "media-playback");
    }
  }
}

uint32_t
AndroidBridge::GetScreenOrientation()
{
    ALOG_BRIDGE("AndroidBridge::GetScreenOrientation");

    int16_t orientation = GeckoAppShell::GetScreenOrientation();

    if (!orientation)
        return dom::eScreenOrientation_None;

    return static_cast<dom::ScreenOrientationInternal>(orientation);
}

uint16_t
AndroidBridge::GetScreenAngle()
{
    return GeckoAppShell::GetScreenAngle();
}

nsresult
AndroidBridge::GetProxyForURI(const nsACString & aSpec,
                              const nsACString & aScheme,
                              const nsACString & aHost,
                              const int32_t      aPort,
                              nsACString & aResult)
{
    if (!jni::IsAvailable()) {
        return NS_ERROR_FAILURE;
    }

    auto jstrRet = GeckoAppShell::GetProxyForURI(aSpec, aScheme, aHost, aPort);

    if (!jstrRet)
        return NS_ERROR_FAILURE;

    aResult = jstrRet->ToCString();
    return NS_OK;
}

bool
AndroidBridge::PumpMessageLoop()
{
    JNIEnv* const env = jni::GetGeckoThreadEnv();

    if (mMessageQueueMessages) {
        auto msg = Object::LocalRef::Adopt(env,
                env->GetObjectField(mMessageQueue.Get(),
                                    mMessageQueueMessages));
        // if queue.mMessages is null, queue.next() will block, which we don't
        // want. It turns out to be an order of magnitude more performant to do
        // this extra check here and block less vs. one fewer checks here and
        // more blocking.
        if (!msg) {
            return false;
        }
    }

    auto msg = Object::LocalRef::Adopt(
            env, env->CallObjectMethod(mMessageQueue.Get(), mMessageQueueNext));
    if (!msg) {
        return false;
    }

    return GeckoThread::PumpMessageLoop(msg);
}

NS_IMETHODIMP nsAndroidBridge::GetBrowserApp(nsIAndroidBrowserApp * *aBrowserApp)
{
    nsAppShell* const appShell = nsAppShell::Get();
    if (appShell)
        NS_IF_ADDREF(*aBrowserApp = appShell->GetBrowserApp());
    return NS_OK;
}

NS_IMETHODIMP nsAndroidBridge::SetBrowserApp(nsIAndroidBrowserApp *aBrowserApp)
{
    nsAppShell* const appShell = nsAppShell::Get();
    if (appShell)
        appShell->SetBrowserApp(aBrowserApp);
    return NS_OK;
}

extern "C"
__attribute__ ((visibility("default")))
jobject JNICALL
Java_org_mozilla_gecko_GeckoAppShell_allocateDirectBuffer(JNIEnv *env, jclass, jlong size);

static jni::DependentRef<java::GeckoLayerClient>
GetJavaLayerClient(mozIDOMWindowProxy* aWindow)
{
    MOZ_ASSERT(aWindow);

    nsCOMPtr<nsPIDOMWindowOuter> domWindow = nsPIDOMWindowOuter::From(aWindow);
    nsCOMPtr<nsIWidget> widget =
            widget::WidgetUtils::DOMWindowToWidget(domWindow);
    MOZ_ASSERT(widget);

    return static_cast<nsWindow*>(widget.get())->GetLayerClient();
}

void
AndroidBridge::ContentDocumentChanged(mozIDOMWindowProxy* aWindow)
{
    auto layerClient = GetJavaLayerClient(aWindow);
    if (!layerClient) {
        return;
    }
    layerClient->ContentDocumentChanged();
}

bool
AndroidBridge::IsContentDocumentDisplayed(mozIDOMWindowProxy* aWindow)
{
    auto layerClient = GetJavaLayerClient(aWindow);
    if (!layerClient) {
        return false;
    }
    return layerClient->IsContentDocumentDisplayed();
}

class AndroidBridge::DelayedTask
{
    using TimeStamp = mozilla::TimeStamp;
    using TimeDuration = mozilla::TimeDuration;

public:
    DelayedTask(already_AddRefed<nsIRunnable> aTask)
        : mTask(aTask)
        , mRunTime() // Null timestamp representing no delay.
    {}

    DelayedTask(already_AddRefed<nsIRunnable> aTask, int aDelayMs)
        : mTask(aTask)
        , mRunTime(TimeStamp::Now() + TimeDuration::FromMilliseconds(aDelayMs))
    {}

    bool IsEarlierThan(const DelayedTask& aOther) const
    {
        if (mRunTime) {
            return aOther.mRunTime ? mRunTime < aOther.mRunTime : false;
        }
        // In the case of no delay, we're earlier if aOther has a delay.
        // Otherwise, we're not earlier, to maintain task order.
        return !!aOther.mRunTime;
    }

    int64_t MillisecondsToRunTime() const
    {
        if (mRunTime) {
            return int64_t((mRunTime - TimeStamp::Now()).ToMilliseconds());
        }
        return 0;
    }

    already_AddRefed<nsIRunnable> TakeTask()
    {
        return mTask.forget();
    }

private:
    nsCOMPtr<nsIRunnable> mTask;
    const TimeStamp mRunTime;
};


void
AndroidBridge::PostTaskToUiThread(already_AddRefed<nsIRunnable> aTask, int aDelayMs)
{
    // add the new task into the mUiTaskQueue, sorted with
    // the earliest task first in the queue
    size_t i;
    DelayedTask newTask(aDelayMs ? DelayedTask(mozilla::Move(aTask), aDelayMs)
                                 : DelayedTask(mozilla::Move(aTask)));

    {
        MutexAutoLock lock(mUiTaskQueueLock);

        for (i = 0; i < mUiTaskQueue.Length(); i++) {
            if (newTask.IsEarlierThan(mUiTaskQueue[i])) {
                mUiTaskQueue.InsertElementAt(i, mozilla::Move(newTask));
                break;
            }
        }

        if (i == mUiTaskQueue.Length()) {
            // We didn't insert the task, which means we should append it.
            mUiTaskQueue.AppendElement(mozilla::Move(newTask));
        }
    }

    if (i == 0) {
        // if we're inserting it at the head of the queue, notify Java because
        // we need to get a callback at an earlier time than the last scheduled
        // callback
        GeckoThread::RequestUiThreadCallback(int64_t(aDelayMs));
    }
}

int64_t
AndroidBridge::RunDelayedUiThreadTasks()
{
    MutexAutoLock lock(mUiTaskQueueLock);

    while (!mUiTaskQueue.IsEmpty()) {
        const int64_t timeLeft = mUiTaskQueue[0].MillisecondsToRunTime();
        if (timeLeft > 0) {
            // this task (and therefore all remaining tasks)
            // have not yet reached their runtime. return the
            // time left until we should be called again
            return timeLeft;
        }

        // Retrieve task before unlocking/running.
        nsCOMPtr<nsIRunnable> nextTask(mUiTaskQueue[0].TakeTask());
        mUiTaskQueue.RemoveElementAt(0);

        // Unlock to allow posting new tasks reentrantly.
        MutexAutoUnlock unlock(mUiTaskQueueLock);
        nextTask->Run();
    }
    return -1;
}

Object::LocalRef AndroidBridge::ChannelCreate(Object::Param stream) {
    JNIEnv* const env = GetEnvForThread();
    auto rv = Object::LocalRef::Adopt(env, env->CallStaticObjectMethod(
            sBridge->jChannels, sBridge->jChannelCreate, stream.Get()));
    MOZ_CATCH_JNI_EXCEPTION(env);
    return rv;
}

void AndroidBridge::InputStreamClose(Object::Param obj) {
    JNIEnv* const env = GetEnvForThread();
    env->CallVoidMethod(obj.Get(), sBridge->jClose);
    MOZ_CATCH_JNI_EXCEPTION(env);
}

uint32_t AndroidBridge::InputStreamAvailable(Object::Param obj) {
    JNIEnv* const env = GetEnvForThread();
    auto rv = env->CallIntMethod(obj.Get(), sBridge->jAvailable);
    MOZ_CATCH_JNI_EXCEPTION(env);
    return rv;
}

nsresult AndroidBridge::InputStreamRead(Object::Param obj, char *aBuf, uint32_t aCount, uint32_t *aRead) {
    JNIEnv* const env = GetEnvForThread();
    auto arr = ByteBuffer::New(aBuf, aCount);
    jint read = env->CallIntMethod(obj.Get(), sBridge->jByteBufferRead, arr.Get());

    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return NS_ERROR_FAILURE;
    }

    if (read <= 0) {
        *aRead = 0;
        return NS_OK;
    }
    *aRead = read;
    return NS_OK;
}

nsresult AndroidBridge::GetExternalPublicDirectory(const nsAString& aType, nsAString& aPath) {
    if (XRE_IsContentProcess()) {
        nsString key(aType);
        nsAutoString path;
        if (AndroidBridge::sStoragePaths.Get(key, &path)) {
            aPath = path;
            return NS_OK;
        }

        // Lazily get the value from the parent.
        dom::ContentChild* child = dom::ContentChild::GetSingleton();
        if (child) {
          nsAutoString type(aType);
          child->SendGetDeviceStorageLocation(type, &path);
          if (!path.IsEmpty()) {
            AndroidBridge::sStoragePaths.Put(key, path);
            aPath = path;
            return NS_OK;
          }
        }

        ALOG_BRIDGE("AndroidBridge::GetExternalPublicDirectory no cache for %s",
              NS_ConvertUTF16toUTF8(aType).get());
        return NS_ERROR_NOT_AVAILABLE;
    }

    auto path = GeckoAppShell::GetExternalPublicDirectory(aType);
    if (!path) {
        return NS_ERROR_NOT_AVAILABLE;
    }
    aPath = path->ToString();
    return NS_OK;
}
