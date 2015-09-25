/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <android/log.h>
#include <dlfcn.h>
#include <math.h>
#include <GLES2/gl2.h>

#include "mozilla/layers/CompositorChild.h"
#include "mozilla/layers/CompositorParent.h"

#include "mozilla/Hal.h"
#include "nsXULAppAPI.h"
#include <prthread.h>
#include "nsXPCOMStrings.h"
#include "AndroidBridge.h"
#include "AndroidJNIWrapper.h"
#include "AndroidBridgeUtilities.h"
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
#include "StrongPointer.h"
#include "mozilla/ClearOnShutdown.h"
#include "nsPrintfCString.h"
#include "NativeJSContainer.h"
#include "nsContentUtils.h"
#include "nsIScriptError.h"
#include "nsIHttpChannel.h"

#include "MediaCodec.h"
#include "SurfaceTexture.h"
#include "GLContextProvider.h"

#include "mozilla/dom/ContentChild.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::jni;
using namespace mozilla::widget;

AndroidBridge* AndroidBridge::sBridge = nullptr;
pthread_t AndroidBridge::sJavaUiThread;
static jobject sGlobalContext = nullptr;
nsDataHashtable<nsStringHashKey, nsString> AndroidBridge::sStoragePaths;

// This is a dummy class that can be used in the template for android::sp
class AndroidRefable {
    void incStrong(void* thing) { }
    void decStrong(void* thing) { }
};

// This isn't in AndroidBridge.h because including StrongPointer.h there is gross
static android::sp<AndroidRefable> (*android_SurfaceTexture_getNativeWindow)(JNIEnv* env, jobject surfaceTexture) = nullptr;

jclass AndroidBridge::GetClassGlobalRef(JNIEnv* env, const char* className)
{
    // First try the default class loader.
    auto classRef = ClassObject::LocalRef::Adopt(
            env, env->FindClass(className));

    if (!classRef && sBridge && sBridge->mClassLoader) {
        // If the default class loader failed but we have an app class loader, try that.
        // Clear the pending exception from failed FindClass call above.
        env->ExceptionClear();
        classRef = ClassObject::LocalRef::Adopt(env,
                env->CallObjectMethod(sBridge->mClassLoader.Get(),
                                      sBridge->mClassLoaderLoadClass,
                                      Param<String>(className, env).Get()));
    }

    if (!classRef) {
        ALOG(">>> FATAL JNI ERROR! FindClass(className=\"%s\") failed. "
             "Did ProGuard optimize away something it shouldn't have?",
             className);
        env->ExceptionDescribe();
        MOZ_CRASH();
    }

    return ClassObject::GlobalRef(env, classRef).Forget();
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
  : mLayerClient(nullptr),
    mPresentationWindow(nullptr),
    mPresentationSurface(nullptr)
{
    ALOG_BRIDGE("AndroidBridge::Init");

    JNIEnv* const jEnv = jni::GetGeckoThreadEnv();
    AutoLocalJNIFrame jniFrame(jEnv);

    mClassLoader = Object::GlobalRef(jEnv, widget::GeckoThread::ClsLoader());
    mClassLoaderLoadClass = GetMethodID(
            jEnv, jEnv->GetObjectClass(mClassLoader.Get()),
            "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");

    mMessageQueue = widget::GeckoThread::MsgQueue();
    auto msgQueueClass = ClassObject::LocalRef::Adopt(
            jEnv, jEnv->GetObjectClass(mMessageQueue.Get()));
    // mMessageQueueNext must not be null
    mMessageQueueNext = GetMethodID(
            jEnv, msgQueueClass.Get(), "next", "()Landroid/os/Message;");
    // mMessageQueueMessages may be null (e.g. due to proguard optimization)
    mMessageQueueMessages = jEnv->GetFieldID(
            msgQueueClass.Get(), "mMessages", "Landroid/os/Message;");

    mGLControllerObj = nullptr;
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

    AutoJNIClass egl(jEnv, "com/google/android/gles_jni/EGLSurfaceImpl");
    jclass eglClass = egl.getGlobalRef();
    if (eglClass) {
        // The pointer type moved to a 'long' in Android L, API version 20
        const char* jniType = mAPIVersion >= 20 ? "J" : "I";
        jEGLSurfacePointerField = egl.getField("mEGLSurface", jniType);
    } else {
        jEGLSurfacePointerField = 0;
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

// Decides if we should store thumbnails for a given docshell based on the presence
// of a Cache-Control: no-store header and the "browser.cache.disk_cache_ssl" pref.
static bool ShouldStoreThumbnail(nsIDocShell* docshell) {
    if (!docshell) {
        return false;
    }

    nsresult rv;
    nsCOMPtr<nsIChannel> channel;

    docshell->GetCurrentDocumentChannel(getter_AddRefs(channel));
    if (!channel) {
        return false;
    }

    nsCOMPtr<nsIHttpChannel> httpChannel;
    rv = channel->QueryInterface(NS_GET_IID(nsIHttpChannel), getter_AddRefs(httpChannel));
    if (!NS_SUCCEEDED(rv)) {
        return false;
    }

    // Don't store thumbnails for sites that didn't load
    uint32_t responseStatus;
    rv = httpChannel->GetResponseStatus(&responseStatus);
    if (!NS_SUCCEEDED(rv) || floor((double) (responseStatus / 100)) != 2) {
        return false;
    }

    // Cache-Control: no-store.
    bool isNoStoreResponse = false;
    httpChannel->IsNoStoreResponse(&isNoStoreResponse);
    if (isNoStoreResponse) {
        return false;
    }

    // Deny storage if we're viewing a HTTPS page with a
    // 'Cache-Control' header having a value that is not 'public'.
    nsCOMPtr<nsIURI> uri;
    rv = channel->GetURI(getter_AddRefs(uri));
    if (!NS_SUCCEEDED(rv)) {
        return false;
    }

    // Don't capture HTTPS pages unless the user enabled it
    // or the page has a Cache-Control:public header.
    bool isHttps = false;
    uri->SchemeIs("https", &isHttps);
    if (isHttps && !Preferences::GetBool("browser.cache.disk_cache_ssl", false)) {
        nsAutoCString cacheControl;
        rv = httpChannel->GetResponseHeader(NS_LITERAL_CSTRING("Cache-Control"), cacheControl);
        if (!NS_SUCCEEDED(rv)) {
            return false;
        }

        if (!cacheControl.IsEmpty() && !cacheControl.LowerCaseEqualsLiteral("public")) {
            return false;
        }
    }

    return true;
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
        aMimeType = jstrType;
    }
}

void
AndroidBridge::GetExtensionFromMimeType(const nsACString& aMimeType, nsACString& aFileExt)
{
    ALOG_BRIDGE("AndroidBridge::GetExtensionFromMimeType");

    auto jstrExt = GeckoAppShell::GetExtensionFromMimeTypeWrapper(aMimeType);

    if (jstrExt) {
        aFileExt = nsCString(jstrExt);
    }
}

bool
AndroidBridge::GetClipboardText(nsAString& aText)
{
    ALOG_BRIDGE("AndroidBridge::GetClipboardText");

    auto text = Clipboard::GetClipboardTextWrapper();

    if (text) {
        aText = nsString(text);
    }
    return !!text;
}

void
AndroidBridge::ShowAlertNotification(const nsAString& aImageUrl,
                                     const nsAString& aAlertTitle,
                                     const nsAString& aAlertText,
                                     const nsAString& aAlertCookie,
                                     nsIObserver *aAlertListener,
                                     const nsAString& aAlertName)
{
    if (nsAppShell::gAppShell && aAlertListener) {
        // This will remove any observers already registered for this id
        nsAppShell::gAppShell->PostEvent(AndroidGeckoEvent::MakeAddObserver(aAlertName, aAlertListener));
    }

    GeckoAppShell::ShowAlertNotificationWrapper
           (aImageUrl, aAlertTitle, aAlertText, aAlertCookie, aAlertName);
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
    // if resetting is true, that means Android destroyed our GeckoApp activity
    // and we had to recreate it, but all the Gecko-side things were not destroyed.
    // We therefore need to link up the new java objects to Gecko, and that's what
    // we do here.
    bool resetting = (mLayerClient != nullptr);
    __android_log_print(ANDROID_LOG_INFO, "GeckoBug1151102", "Reseting layer client: %d", resetting);

    mLayerClient = jobj;

    if (resetting) {
        // since we are re-linking the new java objects to Gecko, we need to get
        // the viewport from the compositor (since the Java copy was thrown away)
        // and we do that by setting the first-paint flag.
        nsWindow::ForceIsFirstPaint();
    }
}

void
AndroidBridge::RegisterCompositor(JNIEnv *env)
{
    if (mGLControllerObj != nullptr) {
        // we already have this set up, no need to do it again
        return;
    }

    mGLControllerObj = GLController::LocalRef(
            LayerView::RegisterCompositorWrapper());
}

EGLSurface
AndroidBridge::CreateEGLSurfaceForCompositor()
{
    if (!jEGLSurfacePointerField) {
        return nullptr;
    }
    MOZ_ASSERT(mGLControllerObj, "AndroidBridge::CreateEGLSurfaceForCompositor called with a null GL controller ref");

    auto eglSurface = mGLControllerObj->CreateEGLSurfaceForCompositorWrapper();
    if (!eglSurface) {
        return nullptr;
    }

    JNIEnv* const env = GetEnvForThread(); // called on the compositor thread
    return reinterpret_cast<EGLSurface>(mAPIVersion >= 20 ?
            env->GetLongField(eglSurface.Get(), jEGLSurfacePointerField) :
            env->GetIntField(eglSurface.Get(), jEGLSurfacePointerField));
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
            ANativeWindow_getWidth = (int (*)(void*))dlsym(handle, "ANativeWindow_getWidth");
            ANativeWindow_getHeight = (int (*)(void*))dlsym(handle, "ANativeWindow_getHeight");

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

    JNIEnv* const env = jni::GetGeckoThreadEnv();

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

    auto message = mozilla::widget::CreateNativeJSContainer(cx, object);
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
AndroidBridge::CreateMessageList(const dom::mobilemessage::SmsFilterData& aFilter, bool aReverse,
                                 nsIMobileMessageCallback* aRequest)
{
    ALOG_BRIDGE("AndroidBridge::CreateMessageList");

    JNIEnv* const env = jni::GetGeckoThreadEnv();

    uint32_t requestId;
    if (!QueueSmsRequest(aRequest, &requestId))
        return;

    AutoLocalJNIFrame jniFrame(env, 2);

    jobjectArray numbers =
        (jobjectArray)env->NewObjectArray(aFilter.numbers().Length(),
                                          jStringClass,
                                          NewJavaString(&jniFrame, EmptyString()));

    for (uint32_t i = 0; i < aFilter.numbers().Length(); ++i) {
        jstring elem = NewJavaString(&jniFrame, aFilter.numbers()[i]);
        env->SetObjectArrayElement(numbers, i, elem);
        env->DeleteLocalRef(elem);
    }

    int64_t startDate = aFilter.hasStartDate() ? aFilter.startDate() : -1;
    int64_t endDate = aFilter.hasEndDate() ? aFilter.endDate() : -1;
    GeckoAppShell::CreateMessageListWrapper(startDate, endDate,
                                            ObjectArray::Ref::From(numbers),
                                            aFilter.numbers().Length(),
                                            aFilter.delivery(),
                                            aFilter.hasRead(), aFilter.read(),
                                            aFilter.threadId(),
                                            aReverse, requestId);
}

void
AndroidBridge::GetNextMessageInList(int32_t aListId, nsIMobileMessageCallback* aRequest)
{
    ALOG_BRIDGE("AndroidBridge::GetNextMessageInList");

    uint32_t requestId;
    if (!QueueSmsRequest(aRequest, &requestId))
        return;

    GeckoAppShell::GetNextMessageInListWrapper(aListId, requestId);
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

void *
AndroidBridge::LockBitmap(jobject bitmap)
{
    JNIEnv* const env = jni::GetGeckoThreadEnv();

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
    JNIEnv* const env = jni::GetGeckoThreadEnv();

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

IntSize
AndroidBridge::GetNativeWindowSize(void* window)
{
  if (!window || !ANativeWindow_getWidth || !ANativeWindow_getHeight) {
    return IntSize(0, 0);
  }

  return IntSize(ANativeWindow_getWidth(window), ANativeWindow_getHeight(window));
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
    *bits = nullptr;
    *width = *height = *format = 0;

    if (mHasNativeWindowAccess) {
        ANativeWindow_Buffer buffer;

        if ((err = ANativeWindow_lock(window, (void*)&buffer, nullptr)) != 0) {
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

        if ((err = Surface_lock(window, &info, nullptr, true)) != 0) {
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
}

nsAndroidBridge::~nsAndroidBridge()
{
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

    aResult = nsCString(jstrRet);
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

    aResult = nsCString(jstrThreadName);
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

    aResult = nsCString(jstrSampleName);
    return true;
}

static float
GetScaleFactor(nsPresContext* aPresContext) {
    nsIPresShell* presShell = aPresContext->PresShell();
    LayoutDeviceToLayerScale cumulativeResolution(presShell->GetCumulativeResolution());
    return cumulativeResolution.scale;
}

nsresult
AndroidBridge::CaptureZoomedView(nsIDOMWindow *window, nsIntRect zoomedViewRect, Object::Param buffer,
                                  float zoomFactor) {
    nsresult rv;

    if (!buffer)
        return NS_ERROR_FAILURE;

    nsCOMPtr <nsIDOMWindowUtils> utils = do_GetInterface(window);
    if (!utils)
        return NS_ERROR_FAILURE;

    JNIEnv* const env = jni::GetGeckoThreadEnv();

    AutoLocalJNIFrame jniFrame(env, 0);

    nsCOMPtr <nsPIDOMWindow> win = do_QueryInterface(window);
    if (!win) {
        return NS_ERROR_FAILURE;
    }
    nsRefPtr <nsPresContext> presContext;

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
    SurfaceFormat format = is24bit ? SurfaceFormat::B8G8R8X8 : SurfaceFormat::R5G6B5;
    gfxImageFormat iFormat = gfx::SurfaceFormatToImageFormat(format);
    uint32_t stride = gfxASurface::FormatStrideForWidth(iFormat, zoomedViewRect.width);

    uint8_t* data = static_cast<uint8_t*> (env->GetDirectBufferAddress(buffer.Get()));
    if (!data) {
        return NS_ERROR_FAILURE;
    }

    MOZ_ASSERT (gfxPlatform::GetPlatform()->SupportsAzureContentForType(BackendType::CAIRO),
              "Need BackendType::CAIRO support");
    RefPtr < DrawTarget > dt = Factory::CreateDrawTargetForData(
        BackendType::CAIRO, data, IntSize(zoomedViewRect.width, zoomedViewRect.height), stride,
        format);
    if (!dt) {
        ALOG_BRIDGE("Error creating DrawTarget");
        return NS_ERROR_FAILURE;
    }
    nsRefPtr <gfxContext> context = new gfxContext(dt);
    context->SetMatrix(context->CurrentMatrix().Scale(zoomFactor, zoomFactor));

    rv = presShell->RenderDocument(r, renderDocFlags, bgColor, context);

    if (is24bit) {
        gfxUtils::ConvertBGRAtoRGBA(data, stride * zoomedViewRect.height);
    }

    LayerView::updateZoomedView(buffer);

    NS_ENSURE_SUCCESS(rv, rv);
    return NS_OK;
}

nsresult AndroidBridge::CaptureThumbnail(nsIDOMWindow *window, int32_t bufW, int32_t bufH, int32_t tabId, Object::Param buffer, bool &shouldStore)
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

    JNIEnv* const env = jni::GetGeckoThreadEnv();

    AutoLocalJNIFrame jniFrame(env, 0);

    nsCOMPtr<nsPIDOMWindow> win = do_QueryInterface(window);
    if (!win)
        return NS_ERROR_FAILURE;
    nsRefPtr<nsPresContext> presContext;

    nsIDocShell* docshell = win->GetDocShell();

    // Decide if callers should store this thumbnail for later use.
    shouldStore = ShouldStoreThumbnail(docshell);

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

    uint8_t* data = static_cast<uint8_t*>(env->GetDirectBufferAddress(buffer.Get()));
    if (!data)
        return NS_ERROR_FAILURE;

    MOZ_ASSERT(gfxPlatform::GetPlatform()->SupportsAzureContentForType(BackendType::CAIRO),
               "Need BackendType::CAIRO support");
    RefPtr<DrawTarget> dt =
        Factory::CreateDrawTargetForData(BackendType::CAIRO,
                                         data,
                                         IntSize(bufW, bufH),
                                         stride,
                                         is24bit ? SurfaceFormat::B8G8R8X8 :
                                                   SurfaceFormat::R5G6B5);
    if (!dt) {
        ALOG_BRIDGE("Error creating DrawTarget");
        return NS_ERROR_FAILURE;
    }
    nsRefPtr<gfxContext> context = new gfxContext(dt);
    context->SetMatrix(
      context->CurrentMatrix().Scale(scale * bufW / srcW,
                                     scale * bufH / srcH));
    rv = presShell->RenderDocument(r, renderDocFlags, bgColor, context);
    if (is24bit) {
        gfxUtils::ConvertBGRAtoRGBA(data, stride * bufH);
    }
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

void
AndroidBridge::PostTaskToUiThread(Task* aTask, int aDelayMs)
{
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
        GeckoAppShell::RequestUiThreadCallback((int64_t)aDelayMs);
    }
}

int64_t
AndroidBridge::RunDelayedUiThreadTasks()
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
    HandleUncaughtException(env);
    return rv;
}

void AndroidBridge::InputStreamClose(Object::Param obj) {
    JNIEnv* const env = GetEnvForThread();
    env->CallVoidMethod(obj.Get(), sBridge->jClose);
    HandleUncaughtException(env);
}

uint32_t AndroidBridge::InputStreamAvailable(Object::Param obj) {
    JNIEnv* const env = GetEnvForThread();
    auto rv = env->CallIntMethod(obj.Get(), sBridge->jAvailable);
    HandleUncaughtException(env);
    return rv;
}

nsresult AndroidBridge::InputStreamRead(Object::Param obj, char *aBuf, uint32_t aCount, uint32_t *aRead) {
    JNIEnv* const env = GetEnvForThread();
    auto arr = Object::LocalRef::Adopt(env, env->NewDirectByteBuffer(aBuf, aCount));
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
    aPath = nsString(path);
    return NS_OK;
}
