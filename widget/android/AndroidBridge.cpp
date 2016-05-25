/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <android/log.h>
#include <dlfcn.h>
#include <math.h>
#include <GLES2/gl2.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>

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
#include "mozilla/dom/mobilemessage/PSms.h"
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
#include "NativeJSContainer.h"
#include "nsContentUtils.h"
#include "nsIScriptError.h"
#include "nsIHttpChannel.h"

#include "MediaCodec.h"
#include "SurfaceTexture.h"
#include "GLContextProvider.h"

#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/ContentChild.h"
#include "nsIObserverService.h"
#include "MediaPrefs.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::jni;
using namespace mozilla::java;

AndroidBridge* AndroidBridge::sBridge = nullptr;
pthread_t AndroidBridge::sJavaUiThread;
static jobject sGlobalContext = nullptr;
nsDataHashtable<nsStringHashKey, nsString> AndroidBridge::sStoragePaths;

jclass AndroidBridge::GetClassGlobalRef(JNIEnv* env, const char* className)
{
    // First try the default class loader.
    auto classRef = Class::LocalRef::Adopt(
            env, env->FindClass(className));

    if (!classRef && sBridge && sBridge->mClassLoader) {
        // If the default class loader failed but we have an app class loader, try that.
        // Clear the pending exception from failed FindClass call above.
        env->ExceptionClear();
        classRef = Class::LocalRef::Adopt(env, jclass(
                env->CallObjectMethod(sBridge->mClassLoader.Get(),
                                      sBridge->mClassLoaderLoadClass,
                                      StringParam(className, env).Get())));
    }

    if (!classRef) {
        ALOG(">>> FATAL JNI ERROR! FindClass(className=\"%s\") failed. "
             "Did ProGuard optimize away something it shouldn't have?",
             className);
        env->ExceptionDescribe();
        MOZ_CRASH();
    }

    return Class::GlobalRef(env, classRef).Forget();
}

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
  : mLayerClient(nullptr)
  , mUiTaskQueueLock("UiTaskQueue")
  , mPresentationWindow(nullptr)
  , mPresentationSurface(nullptr)
{
    ALOG_BRIDGE("AndroidBridge::Init");

    JNIEnv* const jEnv = jni::GetGeckoThreadEnv();
    AutoLocalJNIFrame jniFrame(jEnv);

    mClassLoader = Object::GlobalRef(jEnv, java::GeckoThread::ClsLoader());
    mClassLoaderLoadClass = GetMethodID(
            jEnv, jEnv->GetObjectClass(mClassLoader.Get()),
            "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");

    mMessageQueue = java::GeckoThread::MsgQueue();
    auto msgQueueClass = Class::LocalRef::Adopt(
            jEnv, jEnv->GetObjectClass(mMessageQueue.Get()));
    // mMessageQueueNext must not be null
    mMessageQueueNext = GetMethodID(
            jEnv, msgQueueClass.Get(), "next", "()Landroid/os/Message;");
    // mMessageQueueMessages may be null (e.g. due to proguard optimization)
    mMessageQueueMessages = jEnv->GetFieldID(
            msgQueueClass.Get(), "mMessages", "Landroid/os/Message;");

    mOpenedGraphicsLibraries = false;
    mHasNativeBitmapAccess = false;
    mHasNativeWindowAccess = false;
    mHasNativeWindowFallback = false;

#ifdef MOZ_WEBSMS_BACKEND
    AutoJNIClass smsMessage(jEnv, "android/telephony/SmsMessage");
    mAndroidSmsMessageClass = smsMessage.getGlobalRef();
    jCalculateLength = smsMessage.getStaticMethod("calculateLength", "(Ljava/lang/CharSequence;Z)[I");
#endif

    AutoJNIClass string(jEnv, "java/lang/String");
    jStringClass = string.getGlobalRef();

    if (!GetStaticIntField("android/os/Build$VERSION", "SDK_INT", &mAPIVersion, jEnv)) {
        ALOG_BRIDGE("Failed to find API version");
    }

    AutoJNIClass surface(jEnv, "android/view/Surface");
    jSurfaceClass = surface.getGlobalRef();
    if (mAPIVersion <= 8 /* Froyo */) {
        jSurfacePointerField = surface.getField("mSurface", "I");
    } else if (mAPIVersion > 8 && mAPIVersion < 19 /* KitKat */) {
        jSurfacePointerField = surface.getField("mNativeSurface", "I");
    } else {
        // We don't know how to get this, just set it to 0
        jSurfacePointerField = 0;
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

    InitAndroidJavaWrappers(jEnv);
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

void AutoGlobalWrappedJavaObject::Dispose() {
    if (isNull()) {
        return;
    }

    GetEnvForThread()->DeleteGlobalRef(wrapped_obj);
    wrapped_obj = nullptr;
}

AutoGlobalWrappedJavaObject::~AutoGlobalWrappedJavaObject() {
    Dispose();
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

    auto arr = GeckoAppShell::GetHandlersForMimeTypeWrapper(aMimeType, aAction);
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

    auto arr = GeckoAppShell::GetHandlersForURLWrapper(aURL, aAction);
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

    auto jstrType = GeckoAppShell::GetMimeTypeFromExtensionsWrapper(aFileExt);

    if (jstrType) {
        aMimeType = jstrType->ToCString();
    }
}

void
AndroidBridge::GetExtensionFromMimeType(const nsACString& aMimeType, nsACString& aFileExt)
{
    ALOG_BRIDGE("AndroidBridge::GetExtensionFromMimeType");

    auto jstrExt = GeckoAppShell::GetExtensionFromMimeTypeWrapper(aMimeType);

    if (jstrExt) {
        aFileExt = jstrExt->ToCString();
    }
}

bool
AndroidBridge::GetClipboardText(nsAString& aText)
{
    ALOG_BRIDGE("AndroidBridge::GetClipboardText");

    auto text = Clipboard::GetClipboardTextWrapper();

    if (text) {
        aText = text->ToString();
    }
    return !!text;
}

void
AndroidBridge::ShowPersistentAlertNotification(const nsAString& aPersistentData,
                                               const nsAString& aImageUrl,
                                               const nsAString& aAlertTitle,
                                               const nsAString& aAlertText,
                                               const nsAString& aAlertCookie,
                                               const nsAString& aAlertName,
                                               nsIPrincipal* aPrincipal)
{
    nsAutoString host;
    nsAlertsUtils::GetSourceHostPort(aPrincipal, host);

    GeckoAppShell::ShowPersistentAlertNotificationWrapper
        (aPersistentData, aImageUrl, aAlertTitle, aAlertText, aAlertCookie, aAlertName, host);
}

void
AndroidBridge::ShowAlertNotification(const nsAString& aImageUrl,
                                     const nsAString& aAlertTitle,
                                     const nsAString& aAlertText,
                                     const nsAString& aAlertCookie,
                                     nsIObserver *aAlertListener,
                                     const nsAString& aAlertName,
                                     nsIPrincipal* aPrincipal)
{
    if (aAlertListener) {
        // This will remove any observers already registered for this id
        nsAppShell::PostEvent(AndroidGeckoEvent::MakeAddObserver(aAlertName, aAlertListener));
    }

    nsAutoString host;
    nsAlertsUtils::GetSourceHostPort(aPrincipal, host);

    GeckoAppShell::ShowAlertNotificationWrapper
           (aImageUrl, aAlertTitle, aAlertText, aAlertCookie, aAlertName, host);
}

int
AndroidBridge::GetDPI()
{
    static int sDPI = 0;
    if (sDPI)
        return sDPI;

    const int DEFAULT_DPI = 160;

    sDPI = GeckoAppShell::GetDpiWrapper();
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
        sDepth = GeckoAppShell::GetScreenDepthWrapper();
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
        GeckoAppShell::Vibrate1(d);
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

    GeckoAppShell::VibrateA(LongArray::Ref::From(array), -1 /* don't repeat */);
}

void
AndroidBridge::GetSystemColors(AndroidSystemColors *aColors)
{

    NS_ASSERTION(aColors != nullptr, "AndroidBridge::GetSystemColors: aColors is null!");
    if (!aColors)
        return;

    auto arr = GeckoAppShell::GetSystemColoursWrapper();
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

    auto arr = GeckoAppShell::GetIconForExtensionWrapper
                                             (NS_ConvertUTF8toUTF16(aFileExt), aIconSize);

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

void
AndroidBridge::SetLayerClient(GeckoLayerClient::Param jobj)
{
    mLayerClient = jobj;
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

void*
AndroidBridge::GetNativeSurface(JNIEnv* env, jobject surface) {
    if (!env || !mHasNativeWindowFallback || !jSurfacePointerField)
        return nullptr;

    return (void*)env->GetIntField(surface, jSurfacePointerField);
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


bool
AndroidBridge::InitCamera(const nsCString& contentType, uint32_t camera, uint32_t *width, uint32_t *height, uint32_t *fps)
{
    auto arr = GeckoAppShell::InitCameraWrapper
      (NS_ConvertUTF8toUTF16(contentType), (int32_t) camera, (int32_t) *width, (int32_t) *height);

    if (!arr)
        return false;

    JNIEnv* const env = arr.Env();
    jint *elements = env->GetIntArrayElements(arr.Get(), 0);

    *width = elements[1];
    *height = elements[2];
    *fps = elements[3];

    bool res = elements[0] == 1;

    env->ReleaseIntArrayElements(arr.Get(), elements, 0);

    return res;
}

void
AndroidBridge::GetCurrentBatteryInformation(hal::BatteryInformation* aBatteryInfo)
{
    ALOG_BRIDGE("AndroidBridge::GetCurrentBatteryInformation");

    // To prevent calling too many methods through JNI, the Java method returns
    // an array of double even if we actually want a double and a boolean.
    auto arr = GeckoAppShell::GetCurrentBatteryInformationWrapper();

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
AndroidBridge::HandleGeckoMessage(JSContext* cx, JS::HandleObject object)
{
    ALOG_BRIDGE("%s", __PRETTY_FUNCTION__);

    auto message = widget::CreateNativeJSContainer(cx, object);
    GeckoAppShell::HandleGeckoMessageWrapper(message);
}

nsresult
AndroidBridge::GetSegmentInfoForText(const nsAString& aText,
                                     nsIMobileMessageCallback* aRequest)
{
#ifndef MOZ_WEBSMS_BACKEND
    return NS_ERROR_FAILURE;
#else
    ALOG_BRIDGE("AndroidBridge::GetSegmentInfoForText");

    int32_t segments, charsPerSegment, charsAvailableInLastSegment;

    JNIEnv* const env = jni::GetGeckoThreadEnv();

    AutoLocalJNIFrame jniFrame(env, 2);
    jstring jText = NewJavaString(&jniFrame, aText);
    jobject obj = env->CallStaticObjectMethod(mAndroidSmsMessageClass,
                                              jCalculateLength, jText, JNI_FALSE);
    if (jniFrame.CheckForException())
        return NS_ERROR_FAILURE;

    jintArray arr = static_cast<jintArray>(obj);
    if (!arr || env->GetArrayLength(arr) != 4)
        return NS_ERROR_FAILURE;

    jint* info = env->GetIntArrayElements(arr, JNI_FALSE);

    segments = info[0]; // msgCount
    charsPerSegment = info[2]; // codeUnitsRemaining
    // segmentChars = (codeUnitCount + codeUnitsRemaining) / msgCount
    charsAvailableInLastSegment = (info[1] + info[2]) / info[0];

    env->ReleaseIntArrayElements(arr, info, JNI_ABORT);

    // TODO Bug 908598 - Should properly use |QueueSmsRequest(...)| to queue up
    // the nsIMobileMessageCallback just like other functions.
    return aRequest->NotifySegmentInfoForTextGot(segments,
                                                 charsPerSegment,
                                                 charsAvailableInLastSegment);
#endif
}

void
AndroidBridge::SendMessage(const nsAString& aNumber,
                           const nsAString& aMessage,
                           nsIMobileMessageCallback* aRequest)
{
    ALOG_BRIDGE("AndroidBridge::SendMessage");

    uint32_t requestId;
    if (!QueueSmsRequest(aRequest, &requestId))
        return;

    GeckoAppShell::SendMessageWrapper(aNumber, aMessage, requestId);
}

void
AndroidBridge::GetMessage(int32_t aMessageId, nsIMobileMessageCallback* aRequest)
{
    ALOG_BRIDGE("AndroidBridge::GetMessage");

    uint32_t requestId;
    if (!QueueSmsRequest(aRequest, &requestId))
        return;

    GeckoAppShell::GetMessageWrapper(aMessageId, requestId);
}

void
AndroidBridge::DeleteMessage(int32_t aMessageId, nsIMobileMessageCallback* aRequest)
{
    ALOG_BRIDGE("AndroidBridge::DeleteMessage");

    uint32_t requestId;
    if (!QueueSmsRequest(aRequest, &requestId))
        return;

    GeckoAppShell::DeleteMessageWrapper(aMessageId, requestId);
}

void
AndroidBridge::MarkMessageRead(int32_t aMessageId,
                               bool aValue,
                               bool aSendReadReport,
                               nsIMobileMessageCallback* aRequest)
{
    ALOG_BRIDGE("AndroidBridge::MarkMessageRead");

    uint32_t requestId;
    if (!QueueSmsRequest(aRequest, &requestId)) {
        return;
    }

    GeckoAppShell::MarkMessageRead(aMessageId,
                                   aValue,
                                   aSendReadReport,
                                   requestId);
}

NS_IMPL_ISUPPORTS0(MessageCursorContinueCallback)

NS_IMETHODIMP
MessageCursorContinueCallback::HandleContinue()
{
    GeckoAppShell::GetNextMessageWrapper(mRequestId);
    return NS_OK;
}

already_AddRefed<nsICursorContinueCallback>
AndroidBridge::CreateMessageCursor(bool aHasStartDate,
                                   uint64_t aStartDate,
                                   bool aHasEndDate,
                                   uint64_t aEndDate,
                                   const char16_t** aNumbers,
                                   uint32_t aNumbersCount,
                                   const nsAString& aDelivery,
                                   bool aHasRead,
                                   bool aRead,
                                   bool aHasThreadId,
                                   uint64_t aThreadId,
                                   bool aReverse,
                                   nsIMobileMessageCursorCallback* aRequest)
{
    ALOG_BRIDGE("AndroidBridge::CreateMessageCursor");

    JNIEnv* const env = jni::GetGeckoThreadEnv();

    uint32_t requestId;
    if (!QueueSmsCursorRequest(aRequest, &requestId))
        return nullptr;

    AutoLocalJNIFrame jniFrame(env, 2);

    jobjectArray numbers =
        (jobjectArray)env->NewObjectArray(aNumbersCount,
                                          jStringClass,
                                          NewJavaString(&jniFrame, EmptyString()));

    for (uint32_t i = 0; i < aNumbersCount; ++i) {
        jstring elem = NewJavaString(&jniFrame, nsDependentString(aNumbers[i]));
        env->SetObjectArrayElement(numbers, i, elem);
        env->DeleteLocalRef(elem);
    }

    int64_t startDate = aHasStartDate ? aStartDate : -1;
    int64_t endDate = aHasEndDate ? aEndDate : -1;
    GeckoAppShell::CreateMessageCursorWrapper(startDate, endDate,
                                              ObjectArray::Ref::From(numbers),
                                              aNumbersCount,
                                              aDelivery,
                                              aHasRead, aRead,
                                              aHasThreadId, aThreadId,
                                              aReverse,
                                              requestId);

    nsCOMPtr<nsICursorContinueCallback> callback = 
       new MessageCursorContinueCallback(requestId);
    return callback.forget();
}

NS_IMPL_ISUPPORTS0(ThreadCursorContinueCallback)

NS_IMETHODIMP
ThreadCursorContinueCallback::HandleContinue()
{
    GeckoAppShell::GetNextThreadWrapper(mRequestId);
    return NS_OK;
}

already_AddRefed<nsICursorContinueCallback>
AndroidBridge::CreateThreadCursor(nsIMobileMessageCursorCallback* aRequest)
{
    ALOG_BRIDGE("AndroidBridge::CreateThreadCursor");

    uint32_t requestId;
    if (!QueueSmsCursorRequest(aRequest, &requestId)) {
        return nullptr;
    }

    GeckoAppShell::CreateThreadCursorWrapper(requestId);

    nsCOMPtr<nsICursorContinueCallback> callback =
        new ThreadCursorContinueCallback(requestId);
    return callback.forget();
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

bool
AndroidBridge::QueueSmsCursorRequest(nsIMobileMessageCursorCallback* aRequest,
                                     uint32_t* aRequestIdOut)
{
    MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");
    MOZ_ASSERT(aRequest && aRequestIdOut);

    const uint32_t length = mSmsCursorRequests.Length();
    for (uint32_t i = 0; i < length; i++) {
        if (!(mSmsCursorRequests)[i]) {
            (mSmsCursorRequests)[i] = aRequest;
            *aRequestIdOut = i;
            return true;
        }
    }

    mSmsCursorRequests.AppendElement(aRequest);

    // After AppendElement(), previous `length` points to the new tail element.
    *aRequestIdOut = length;
    return true;
}

nsCOMPtr<nsIMobileMessageCursorCallback>
AndroidBridge::GetSmsCursorRequest(uint32_t aRequestId)
{
    MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

    MOZ_ASSERT(aRequestId < mSmsCursorRequests.Length());
    if (aRequestId >= mSmsCursorRequests.Length()) {
        return nullptr;
    }

    // TODO: remove on final dequeue
    return mSmsCursorRequests[aRequestId];
}

already_AddRefed<nsIMobileMessageCursorCallback>
AndroidBridge::DequeueSmsCursorRequest(uint32_t aRequestId)
{
    MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

    MOZ_ASSERT(aRequestId < mSmsCursorRequests.Length());
    if (aRequestId >= mSmsCursorRequests.Length()) {
        return nullptr;
    }

    // TODO: remove on final dequeue
    return mSmsCursorRequests[aRequestId].forget();
}

void
AndroidBridge::GetCurrentNetworkInformation(hal::NetworkInformation* aNetworkInfo)
{
    ALOG_BRIDGE("AndroidBridge::GetCurrentNetworkInformation");

    // To prevent calling too many methods through JNI, the Java method returns
    // an array of double even if we actually want an integer, a boolean, and an integer.

    auto arr = GeckoAppShell::GetCurrentNetworkInformationWrapper();

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

void*
AndroidBridge::AcquireNativeWindow(JNIEnv* aEnv, jobject aSurface)
{
    return ANativeWindow_fromSurface(aEnv, aSurface);
}

void
AndroidBridge::ReleaseNativeWindow(void *window)
{
    return ANativeWindow_release((ANativeWindow*)window);
}

IntSize
AndroidBridge::GetNativeWindowSize(void* window)
{
    if (!window) {
      return IntSize(0, 0);
    }

    return IntSize(ANativeWindow_getWidth((ANativeWindow*)window), ANativeWindow_getHeight((ANativeWindow*)window));
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

void
AndroidBridge::SetFirstPaintViewport(const LayerIntPoint& aOffset, const CSSToLayerScale& aZoom, const CSSRect& aCssPageRect)
{
    if (!mLayerClient) {
        return;
    }

    mLayerClient->SetFirstPaintViewport(float(aOffset.x), float(aOffset.y), aZoom.scale,
            aCssPageRect.x, aCssPageRect.y, aCssPageRect.XMost(), aCssPageRect.YMost());
}

void
AndroidBridge::SetPageRect(const CSSRect& aCssPageRect)
{
    if (!mLayerClient) {
        return;
    }

    mLayerClient->SetPageRect(aCssPageRect.x, aCssPageRect.y,
                              aCssPageRect.XMost(), aCssPageRect.YMost());
}

void
AndroidBridge::SyncViewportInfo(const LayerIntRect& aDisplayPort, const CSSToLayerScale& aDisplayResolution,
                                bool aLayersUpdated, int32_t aPaintSyncId, ParentLayerRect& aScrollRect, CSSToParentLayerScale& aScale,
                                ScreenMargin& aFixedLayerMargins)
{
    if (!mLayerClient) {
        ALOG_BRIDGE("Exceptional Exit: %s", __PRETTY_FUNCTION__);
        return;
    }

    ViewTransform::LocalRef viewTransform = mLayerClient->SyncViewportInfo(
            aDisplayPort.x, aDisplayPort.y,
            aDisplayPort.width, aDisplayPort.height,
            aDisplayResolution.scale, aLayersUpdated, aPaintSyncId);

    MOZ_ASSERT(viewTransform, "No view transform object!");

    aScrollRect = ParentLayerRect(viewTransform->X(), viewTransform->Y(),
                                  viewTransform->Width(), viewTransform->Height());
    aScale.scale = viewTransform->Scale();
    aFixedLayerMargins.top = viewTransform->FixedLayerMarginTop();
    aFixedLayerMargins.right = viewTransform->FixedLayerMarginRight();
    aFixedLayerMargins.bottom = viewTransform->FixedLayerMarginBottom();
    aFixedLayerMargins.left = viewTransform->FixedLayerMarginLeft();
}

void AndroidBridge::SyncFrameMetrics(const ParentLayerPoint& aScrollOffset,
                                     const CSSToParentLayerScale& aZoom,
                                     const CSSRect& aCssPageRect,
                                     const CSSRect& aDisplayPort,
                                     const CSSToLayerScale& aPaintedResolution,
                                     bool aLayersUpdated, int32_t aPaintSyncId,
                                     ScreenMargin& aFixedLayerMargins)
{
    if (!mLayerClient) {
        ALOG_BRIDGE("Exceptional Exit: %s", __PRETTY_FUNCTION__);
        return;
    }

    // convert the displayport rect from document-relative CSS pixels to
    // document-relative device pixels
    LayerIntRect dp = gfx::RoundedToInt(aDisplayPort * aPaintedResolution);
    ViewTransform::LocalRef viewTransform = mLayerClient->SyncFrameMetrics(
            aScrollOffset.x, aScrollOffset.y, aZoom.scale,
            aCssPageRect.x, aCssPageRect.y, aCssPageRect.XMost(), aCssPageRect.YMost(),
            dp.x, dp.y, dp.width, dp.height, aPaintedResolution.scale,
            aLayersUpdated, aPaintSyncId);

    MOZ_ASSERT(viewTransform, "No view transform object!");

    aFixedLayerMargins.top = viewTransform->FixedLayerMarginTop();
    aFixedLayerMargins.right = viewTransform->FixedLayerMarginRight();
    aFixedLayerMargins.bottom = viewTransform->FixedLayerMarginBottom();
    aFixedLayerMargins.left = viewTransform->FixedLayerMarginLeft();
}

/* Implementation file */
NS_IMPL_ISUPPORTS(nsAndroidBridge, nsIAndroidBridge)

nsAndroidBridge::nsAndroidBridge()
{
  AddObservers();
}

nsAndroidBridge::~nsAndroidBridge()
{
  RemoveObservers();
}

NS_IMETHODIMP nsAndroidBridge::HandleGeckoMessage(JS::HandleValue val,
                                                  JSContext *cx)
{
    if (val.isObject()) {
        JS::RootedObject object(cx, &val.toObject());
        AndroidBridge::Bridge()->HandleGeckoMessage(cx, object);
        return NS_OK;
    }

    // Now handle legacy JSON messages.
    if (!val.isString()) {
        return NS_ERROR_INVALID_ARG;
    }
    JS::RootedString jsonStr(cx, val.toString());

    JS::RootedValue jsonVal(cx);
    if (!JS_ParseJSON(cx, jsonStr, &jsonVal) || !jsonVal.isObject()) {
        return NS_ERROR_INVALID_ARG;
    }

    // Spit out a warning before sending the message.
    nsContentUtils::ReportToConsoleNonLocalized(
        NS_LITERAL_STRING("Use of JSON is deprecated. "
            "Please pass Javascript objects directly to handleGeckoMessage."),
        nsIScriptError::warningFlag,
        NS_LITERAL_CSTRING("nsIAndroidBridge"),
        nullptr);

    JS::RootedObject object(cx, &jsonVal.toObject());
    AndroidBridge::Bridge()->HandleGeckoMessage(cx, object);
    return NS_OK;
}

NS_IMETHODIMP nsAndroidBridge::GetDisplayPort(bool aPageSizeUpdate, bool aIsBrowserContentDisplayed, int32_t tabId, nsIAndroidViewport* metrics, nsIAndroidDisplayport** displayPort)
{
    AndroidBridge::Bridge()->GetDisplayPort(aPageSizeUpdate, aIsBrowserContentDisplayed, tabId, metrics, displayPort);
    return NS_OK;
}

NS_IMETHODIMP nsAndroidBridge::ContentDocumentChanged()
{
    AndroidBridge::Bridge()->ContentDocumentChanged();
    return NS_OK;
}

NS_IMETHODIMP nsAndroidBridge::IsContentDocumentDisplayed(bool *aRet)
{
    *aRet = AndroidBridge::Bridge()->IsContentDocumentDisplayed();
    return NS_OK;
}

NS_IMETHODIMP
nsAndroidBridge::Observe(nsISupports* aSubject, const char* aTopic,
                         const char16_t* aData)
{
  if (!strcmp(aTopic, "xpcom-shutdown")) {
    RemoveObservers();
  } else if (!strcmp(aTopic, "audio-playback")) {
    ALOG_BRIDGE("nsAndroidBridge::Observe, get audio-playback event.");

    nsCOMPtr<nsPIDOMWindowOuter> window = do_QueryInterface(aSubject);
    MOZ_ASSERT(window);

    nsAutoString activeStr(aData);
    if (activeStr.EqualsLiteral("inactive-nonaudible")) {
      // This state means the audio becomes silent, but it's still playing, so
      // we don't need to notify the AudioFocusAgent.
      return NS_OK;
    }

    bool isPlaying = activeStr.EqualsLiteral("active");

    UpdateAudioPlayingWindows(window, isPlaying);
  }
  return NS_OK;
}

void
nsAndroidBridge::UpdateAudioPlayingWindows(nsPIDOMWindowOuter* aWindow,
                                           bool aPlaying)
{
  // Request audio focus for the first audio playing window and abandon focus
  // for the last audio playing window.
  if (aPlaying && !mAudioPlayingWindows.Contains(aWindow)) {
    mAudioPlayingWindows.AppendElement(aWindow);
    if (mAudioPlayingWindows.Length() == 1) {
      ALOG_BRIDGE("nsAndroidBridge, request audio focus.");
      AudioFocusAgent::NotifyStartedPlaying();
    }
  } else if (!aPlaying && mAudioPlayingWindows.Contains(aWindow)) {
    mAudioPlayingWindows.RemoveElement(aWindow);
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
    obs->AddObserver(this, "audio-playback", false);
  }
}

void
nsAndroidBridge::RemoveObservers()
{
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->RemoveObserver(this, "xpcom-shutdown");
    obs->RemoveObserver(this, "audio-playback");
  }
}

uint32_t
AndroidBridge::GetScreenOrientation()
{
    ALOG_BRIDGE("AndroidBridge::GetScreenOrientation");

    int16_t orientation = GeckoAppShell::GetScreenOrientationWrapper();

    if (!orientation)
        return dom::eScreenOrientation_None;

    return static_cast<dom::ScreenOrientationInternal>(orientation);
}

uint16_t
AndroidBridge::GetScreenAngle()
{
    return GeckoAppShell::GetScreenAngle();
}

void
AndroidBridge::InvalidateAndScheduleComposite()
{
    nsWindow::InvalidateAndScheduleComposite();
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

    auto jstrRet = GeckoAppShell::GetProxyForURIWrapper(aSpec, aScheme, aHost, aPort);

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

void
AndroidBridge::AddPluginView(jobject view, const LayoutDeviceRect& rect, bool isFullScreen) {
    nsWindow* win = nsWindow::TopWindow();
    if (!win)
        return;

    CSSRect cssRect = rect / win->GetDefaultScale();
    GeckoAppShell::AddPluginViewWrapper(Object::Ref::From(view), cssRect.x, cssRect.y,
                                        cssRect.width, cssRect.height, isFullScreen);
}

extern "C"
__attribute__ ((visibility("default")))
jobject JNICALL
Java_org_mozilla_gecko_GeckoAppShell_allocateDirectBuffer(JNIEnv *env, jclass, jlong size);

bool
AndroidBridge::GetThreadNameJavaProfiling(uint32_t aThreadId, nsCString & aResult)
{
    auto jstrThreadName = GeckoJavaSampler::GetThreadNameJavaProfilingWrapper(aThreadId);

    if (!jstrThreadName)
        return false;

    aResult = jstrThreadName->ToCString();
    return true;
}

bool
AndroidBridge::GetFrameNameJavaProfiling(uint32_t aThreadId, uint32_t aSampleId,
                                          uint32_t aFrameId, nsCString & aResult)
{
    auto jstrSampleName = GeckoJavaSampler::GetFrameNameJavaProfilingWrapper
            (aThreadId, aSampleId, aFrameId);

    if (!jstrSampleName)
        return false;

    aResult = jstrSampleName->ToCString();
    return true;
}

static float
GetScaleFactor(nsPresContext* aPresContext) {
    nsIPresShell* presShell = aPresContext->PresShell();
    LayoutDeviceToLayerScale cumulativeResolution(presShell->GetCumulativeResolution());
    return cumulativeResolution.scale;
}

nsresult
AndroidBridge::CaptureZoomedView(mozIDOMWindowProxy *window, nsIntRect zoomedViewRect, ByteBuffer::Param buffer,
                                  float zoomFactor) {
    nsresult rv;

    if (!buffer)
        return NS_ERROR_FAILURE;

    nsCOMPtr <nsIDOMWindowUtils> utils = do_GetInterface(window);
    if (!utils)
        return NS_ERROR_FAILURE;

    JNIEnv* const env = jni::GetGeckoThreadEnv();

    AutoLocalJNIFrame jniFrame(env, 0);

    if (!window) {
        return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsPIDOMWindowOuter> win = nsPIDOMWindowOuter::From(window);
    RefPtr<nsPresContext> presContext;
    nsIDocShell* docshell = win->GetDocShell();

    if (docshell) {
        docshell->GetPresContext(getter_AddRefs(presContext));
    }

    if (!presContext) {
        return NS_ERROR_FAILURE;
    }
    nsCOMPtr <nsIPresShell> presShell = presContext->PresShell();

    float scaleFactor = GetScaleFactor(presContext) ;

    nscolor bgColor = NS_RGB(255, 255, 255);
    uint32_t renderDocFlags = (nsIPresShell::RENDER_IGNORE_VIEWPORT_SCROLLING | nsIPresShell::RENDER_DOCUMENT_RELATIVE);
    nsRect r(presContext->DevPixelsToAppUnits(zoomedViewRect.x / scaleFactor),
             presContext->DevPixelsToAppUnits(zoomedViewRect.y / scaleFactor ),
             presContext->DevPixelsToAppUnits(zoomedViewRect.width / scaleFactor ),
             presContext->DevPixelsToAppUnits(zoomedViewRect.height / scaleFactor ));

    bool is24bit = (GetScreenDepth() == 24);
    SurfaceFormat format = is24bit ? SurfaceFormat::B8G8R8X8 : SurfaceFormat::R5G6B5_UINT16;
    gfxImageFormat iFormat = gfx::SurfaceFormatToImageFormat(format);
    uint32_t stride = gfxASurface::FormatStrideForWidth(iFormat, zoomedViewRect.width);

    uint8_t* data = static_cast<uint8_t*>(buffer->Address());
    if (!data) {
        return NS_ERROR_FAILURE;
    }

    MOZ_ASSERT (gfxPlatform::GetPlatform()->SupportsAzureContentForType(BackendType::CAIRO),
              "Need BackendType::CAIRO support");
    RefPtr < DrawTarget > dt = Factory::CreateDrawTargetForData(
        BackendType::CAIRO, data, IntSize(zoomedViewRect.width, zoomedViewRect.height), stride,
        format);
    if (!dt || !dt->IsValid()) {
        ALOG_BRIDGE("Error creating DrawTarget");
        return NS_ERROR_FAILURE;
    }
    RefPtr<gfxContext> context = gfxContext::CreateOrNull(dt);
    MOZ_ASSERT(context); // already checked the draw target above
    context->SetMatrix(context->CurrentMatrix().Scale(zoomFactor, zoomFactor));

    rv = presShell->RenderDocument(r, renderDocFlags, bgColor, context);

    if (is24bit) {
        gfxUtils::ConvertBGRAtoRGBA(data, stride * zoomedViewRect.height);
    }

    LayerView::updateZoomedView(buffer);

    NS_ENSURE_SUCCESS(rv, rv);
    return NS_OK;
}

void
AndroidBridge::GetDisplayPort(bool aPageSizeUpdate, bool aIsBrowserContentDisplayed, int32_t tabId, nsIAndroidViewport* metrics, nsIAndroidDisplayport** displayPort)
{

    ALOG_BRIDGE("Enter: %s", __PRETTY_FUNCTION__);
    if (!mLayerClient) {
        ALOG_BRIDGE("Exceptional Exit: %s", __PRETTY_FUNCTION__);
        return;
    }

    JNIEnv* const env = jni::GetGeckoThreadEnv();
    AutoLocalJNIFrame jniFrame(env, 1);

    int width, height;
    float x, y,
        pageLeft, pageTop, pageRight, pageBottom,
        cssPageLeft, cssPageTop, cssPageRight, cssPageBottom,
        zoom;
    metrics->GetX(&x);
    metrics->GetY(&y);
    metrics->GetWidth(&width);
    metrics->GetHeight(&height);
    metrics->GetPageLeft(&pageLeft);
    metrics->GetPageTop(&pageTop);
    metrics->GetPageRight(&pageRight);
    metrics->GetPageBottom(&pageBottom);
    metrics->GetCssPageLeft(&cssPageLeft);
    metrics->GetCssPageTop(&cssPageTop);
    metrics->GetCssPageRight(&cssPageRight);
    metrics->GetCssPageBottom(&cssPageBottom);
    metrics->GetZoom(&zoom);

    auto jmetrics = ImmutableViewportMetrics::New(
            pageLeft, pageTop, pageRight, pageBottom,
            cssPageLeft, cssPageTop, cssPageRight, cssPageBottom,
            x, y, width, height,
            zoom);

    DisplayPortMetrics::LocalRef displayPortMetrics = mLayerClient->GetDisplayPort(
            aPageSizeUpdate, aIsBrowserContentDisplayed, tabId, jmetrics);

    if (!displayPortMetrics) {
        ALOG_BRIDGE("Exceptional Exit: %s", __PRETTY_FUNCTION__);
        return;
    }

    AndroidRectF rect(env, displayPortMetrics->MPosition().Get());
    float resolution = displayPortMetrics->Resolution();

    *displayPort = new nsAndroidDisplayport(rect, resolution);
    (*displayPort)->AddRef();

    ALOG_BRIDGE("Exit: %s", __PRETTY_FUNCTION__);
}

void
AndroidBridge::ContentDocumentChanged()
{
    if (!mLayerClient) {
        return;
    }
    mLayerClient->ContentDocumentChanged();
}

bool
AndroidBridge::IsContentDocumentDisplayed()
{
    if (!mLayerClient)
        return false;

    return mLayerClient->IsContentDocumentDisplayed();
}

bool
AndroidBridge::ProgressiveUpdateCallback(bool aHasPendingNewThebesContent,
                                         const LayerRect& aDisplayPort, float aDisplayResolution,
                                         bool aDrawingCritical, ParentLayerPoint& aScrollOffset,
                                         CSSToParentLayerScale& aZoom)
{
    if (!mLayerClient) {
        ALOG_BRIDGE("Exceptional Exit: %s", __PRETTY_FUNCTION__);
        return false;
    }

    ProgressiveUpdateData::LocalRef progressiveUpdateData =
            mLayerClient->ProgressiveUpdateCallback(aHasPendingNewThebesContent,
                                                    (float)aDisplayPort.x,
                                                    (float)aDisplayPort.y,
                                                    (float)aDisplayPort.width,
                                                    (float)aDisplayPort.height,
                                                           aDisplayResolution,
                                                          !aDrawingCritical);

    aScrollOffset.x = progressiveUpdateData->X();
    aScrollOffset.y = progressiveUpdateData->Y();
    aZoom.scale = progressiveUpdateData->Scale();

    return progressiveUpdateData->Abort();
}

class AndroidBridge::DelayedTask
{
    using TimeStamp = mozilla::TimeStamp;
    using TimeDuration = mozilla::TimeDuration;

public:
    DelayedTask(already_AddRefed<Runnable> aTask)
        : mTask(aTask)
        , mRunTime() // Null timestamp representing no delay.
    {}

    DelayedTask(already_AddRefed<Runnable> aTask, int aDelayMs)
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

    already_AddRefed<Runnable> TakeTask()
    {
        return mTask.forget();
    }

private:
    RefPtr<Runnable> mTask;
    const TimeStamp mRunTime;
};


void
AndroidBridge::PostTaskToUiThread(already_AddRefed<Runnable> aTask, int aDelayMs)
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
        GeckoAppShell::RequestUiThreadCallback((int64_t)aDelayMs);
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
        RefPtr<Runnable> nextTask(mUiTaskQueue[0].TakeTask());
        mUiTaskQueue.RemoveElementAt(0);

        // Unlock to allow posting new tasks reentrantly.
        MutexAutoUnlock unlock(mUiTaskQueueLock);
        nextTask->Run();
    }
    return -1;
}

void*
AndroidBridge::GetPresentationWindow()
{
    return mPresentationWindow;
}

void
AndroidBridge::SetPresentationWindow(void* aPresentationWindow)
{
     if (mPresentationWindow) {
         const bool wasAlreadyPaused = nsWindow::IsCompositionPaused();
         if (!wasAlreadyPaused) {
             nsWindow::SchedulePauseComposition();
         }

         mPresentationWindow = aPresentationWindow;
         if (mPresentationSurface) {
             // destroy the egl surface!
             // The compositor is paused so it should be okay to destroy
             // the surface here.
             mozilla::gl::GLContextProvider::DestroyEGLSurface(mPresentationSurface);
             mPresentationSurface = nullptr;
         }

         if (!wasAlreadyPaused) {
             nsWindow::ScheduleResumeComposition();
         }
     }
     else {
         mPresentationWindow = aPresentationWindow;
     }
}

EGLSurface
AndroidBridge::GetPresentationSurface()
{
    return mPresentationSurface;
}

void
AndroidBridge::SetPresentationSurface(EGLSurface aPresentationSurface)
{
    mPresentationSurface = aPresentationSurface;
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
