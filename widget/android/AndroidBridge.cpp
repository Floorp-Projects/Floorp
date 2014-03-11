/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
#include "AndroidBridgeUtilities.h"
#include "nsAppShell.h"
#include "nsOSHelperAppService.h"
#include "nsWindow.h"
#include "mozilla/Preferences.h"
#include "nsThreadUtils.h"
#include "nsIThreadManager.h"
#include "mozilla/dom/mobilemessage/PSms.h"
#include "gfxImageSurface.h"
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

using namespace mozilla;
using namespace mozilla::widget::android;
using namespace mozilla::gfx;

StaticRefPtr<AndroidBridge> AndroidBridge::sBridge;
static unsigned sJavaEnvThreadIndex = 0;
static jobject sGlobalContext = nullptr;
static void JavaThreadDetachFunc(void *arg);

// This is a dummy class that can be used in the template for android::sp
class AndroidRefable {
    void incStrong(void* thing) { }
    void decStrong(void* thing) { }
};

// This isn't in AndroidBridge.h because including StrongPointer.h there is gross
static android::sp<AndroidRefable> (*android_SurfaceTexture_getNativeWindow)(JNIEnv* env, jobject surfaceTexture) = nullptr;

jclass AndroidBridge::GetClassGlobalRef(JNIEnv* env, const char* className)
{
    jobject classLocalRef = env->FindClass(className);
    if (!classLocalRef) {
        ALOG(">>> FATAL JNI ERROR! FindClass(className=\"%s\") failed. Did ProGuard optimize away something it shouldn't have?",
             className);
        env->ExceptionDescribe();
        MOZ_CRASH();
    }
    jobject classGlobalRef = env->NewGlobalRef(classLocalRef);
    if (!classGlobalRef) {
        env->ExceptionDescribe();
        MOZ_CRASH();
    }
    // Local ref no longer necessary because we have a global ref.
    env->DeleteLocalRef(classLocalRef);
    classLocalRef = nullptr;
    return static_cast<jclass>(classGlobalRef);
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
AndroidBridge::ConstructBridge(JNIEnv *jEnv)
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
    if (!bridge->Init(jEnv)) {
        delete bridge;
    }
    sBridge = bridge;
}

bool
AndroidBridge::Init(JNIEnv *jEnv)
{
    ALOG_BRIDGE("AndroidBridge::Init");
    jEnv->GetJavaVM(&mJavaVM);
    if (!mJavaVM) {
        MOZ_CRASH(); // Nothing we can do here
    }

    AutoLocalJNIFrame jniFrame(jEnv);

    mJNIEnv = nullptr;
    mThread = -1;
    mGLControllerObj = nullptr;
    mOpenedGraphicsLibraries = false;
    mHasNativeBitmapAccess = false;
    mHasNativeWindowAccess = false;
    mHasNativeWindowFallback = false;

    initInit();

#ifdef MOZ_WEBSMS_BACKEND
    mAndroidSmsMessageClass = getClassGlobalRef("android/telephony/SmsMessage");
    jCalculateLength = getStaticMethod("calculateLength", "(Ljava/lang/CharSequence;Z)[I");
#endif

    jStringClass = getClassGlobalRef("java/lang/String");

    if (!GetStaticIntField("android/os/Build$VERSION", "SDK_INT", &mAPIVersion, jEnv)) {
        ALOG_BRIDGE("Failed to find API version");
    }

    jSurfaceClass = getClassGlobalRef("android/view/Surface");
    if (mAPIVersion <= 8 /* Froyo */) {
        jSurfacePointerField = getField("mSurface", "I");
    } else if (mAPIVersion > 8 && mAPIVersion < 19 /* KitKat */) {
        jSurfacePointerField = getField("mNativeSurface", "I");
    } else {
        // We don't know how to get this, just set it to 0
        jSurfacePointerField = 0;
    }

    jclass eglClass = getClassGlobalRef("com/google/android/gles_jni/EGLSurfaceImpl");
    if (eglClass) {
        jEGLSurfacePointerField = getField("mEGLSurface", "I");
    } else {
        jEGLSurfacePointerField = 0;
    }

    InitAndroidJavaWrappers(jEnv);

    // jEnv should NOT be cached here by anything -- the jEnv here
    // is not valid for the real gecko main thread, which is set
    // at SetMainThread time.

    return true;
}

bool
AndroidBridge::SetMainThread(pthread_t thr)
{
    ALOG_BRIDGE("AndroidBridge::SetMainThread");
    if (thr) {
        mThread = thr;
        mJavaVM->GetEnv((void**) &mJNIEnv, JNI_VERSION_1_2);
        return (bool) mJNIEnv;
    }

    mJNIEnv = nullptr;
    mThread = -1;
    return true;
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

extern "C" {
    __attribute__ ((visibility("default")))
    JNIEnv * GetJNIForThread()
    {
        JNIEnv *jEnv = static_cast<JNIEnv*>(PR_GetThreadPrivate(sJavaEnvThreadIndex));
        if (jEnv) {
            return jEnv;
        }
        JavaVM *jVm  = mozilla::AndroidBridge::GetVM();
        if (!jVm->GetEnv(reinterpret_cast<void**>(&jEnv), JNI_VERSION_1_2)) {
            MOZ_ASSERT(jEnv);
            return jEnv;
        }
        if (!jVm->AttachCurrentThread(&jEnv, nullptr)) {
            MOZ_ASSERT(jEnv);
            PR_SetThreadPrivate(sJavaEnvThreadIndex, jEnv);
            return jEnv;
        }
        MOZ_CRASH();
        return nullptr; // unreachable
    }
}

void AutoGlobalWrappedJavaObject::Dispose() {
    if (isNull()) {
        return;
    }

    GetJNIForThread()->DeleteGlobalRef(wrapped_obj);
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

    JNIEnv *env = GetJNIEnv();

    AutoLocalJNIFrame jniFrame(env, 1);
    jobjectArray arr = GeckoAppShell::GetHandlersForMimeTypeWrapper(aMimeType, aAction);
    if (!arr)
        return false;

    jsize len = env->GetArrayLength(arr);

    if (!aHandlersArray)
        return len > 0;

    getHandlersFromStringArray(env, arr, len, aHandlersArray,
                               aDefaultApp, aAction,
                               NS_ConvertUTF16toUTF8(aMimeType));
    return true;
}

bool
AndroidBridge::GetHandlersForURL(const nsAString& aURL,
                                 nsIMutableArray* aHandlersArray,
                                 nsIHandlerApp **aDefaultApp,
                                 const nsAString& aAction)
{
    ALOG_BRIDGE("AndroidBridge::GetHandlersForURL");

    JNIEnv *env = GetJNIEnv();

    AutoLocalJNIFrame jniFrame(env, 1);
    jobjectArray arr = GeckoAppShell::GetHandlersForURLWrapper(aURL, aAction);
    if (!arr)
        return false;

    jsize len = env->GetArrayLength(arr);

    if (!aHandlersArray)
        return len > 0;

    getHandlersFromStringArray(env, arr, len, aHandlersArray,
                               aDefaultApp, aAction);
    return true;
}

void
AndroidBridge::GetMimeTypeFromExtensions(const nsACString& aFileExt, nsCString& aMimeType)
{
    ALOG_BRIDGE("AndroidBridge::GetMimeTypeFromExtensions");

    JNIEnv *env = GetJNIEnv();

    AutoLocalJNIFrame jniFrame(env, 1);
    jstring jstrType = GeckoAppShell::GetMimeTypeFromExtensionsWrapper(NS_ConvertUTF8toUTF16(aFileExt));
    if (!jstrType) {
        return;
    }
    nsJNIString jniStr(jstrType, env);
    CopyUTF16toUTF8(jniStr.get(), aMimeType);
}

void
AndroidBridge::GetExtensionFromMimeType(const nsACString& aMimeType, nsACString& aFileExt)
{
    ALOG_BRIDGE("AndroidBridge::GetExtensionFromMimeType");

    JNIEnv *env = GetJNIEnv();

    AutoLocalJNIFrame jniFrame(env, 1);
    jstring jstrExt = GeckoAppShell::GetExtensionFromMimeTypeWrapper(NS_ConvertUTF8toUTF16(aMimeType));
    if (!jstrExt) {
        return;
    }
    nsJNIString jniStr(jstrExt, env);
    CopyUTF16toUTF8(jniStr.get(), aFileExt);
}

bool
AndroidBridge::GetClipboardText(nsAString& aText)
{
    ALOG_BRIDGE("AndroidBridge::GetClipboardText");

    JNIEnv *env = GetJNIEnv();

    AutoLocalJNIFrame jniFrame(env, 1);
    jstring result = Clipboard::GetClipboardTextWrapper();
    if (!result)
        return false;

    nsJNIString jniStr(result, env);
    aText.Assign(jniStr);
    return true;
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

    GeckoAppShell::ShowAlertNotificationWrapper(aImageUrl, aAlertTitle, aAlertText, aAlertCookie, aAlertName);
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

    if (HasEnv()) {
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

    JNIEnv *env = GetJNIEnv();

    AutoLocalJNIFrame jniFrame(env, 1);

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

    GeckoAppShell::VibrateA(array, -1/*don't repeat*/);
}

void
AndroidBridge::GetSystemColors(AndroidSystemColors *aColors)
{

    NS_ASSERTION(aColors != nullptr, "AndroidBridge::GetSystemColors: aColors is null!");
    if (!aColors)
        return;

    JNIEnv *env = GetJNIEnv();

    AutoLocalJNIFrame jniFrame(env, 1);

    jintArray arr = GeckoAppShell::GetSystemColoursWrapper();
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

    AutoLocalJNIFrame jniFrame(env, 1);

    jbyteArray arr = GeckoAppShell::GetIconForExtensionWrapper(NS_ConvertUTF8toUTF16(aFileExt), aIconSize);

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

void
AndroidBridge::SetLayerClient(JNIEnv* env, jobject jobj)
{
    // if resetting is true, that means Android destroyed our GeckoApp activity
    // and we had to recreate it, but all the Gecko-side things were not destroyed.
    // We therefore need to link up the new java objects to Gecko, and that's what
    // we do here.
    bool resetting = (mLayerClient != nullptr);

    if (resetting) {
        // clear out the old layer client
        delete mLayerClient;
        mLayerClient = nullptr;
    }

    mLayerClient = GeckoLayerClient::Wrap(jobj);

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
    if (mGLControllerObj != nullptr && !mGLControllerObj->isNull()) {
        // we already have this set up, no need to do it again
        return;
    }

    jobject glController = LayerView::RegisterCompositorWrapper();
    if (!glController) {
        return;
    }

    mGLControllerObj = GLController::Wrap(glController);
}

EGLSurface
AndroidBridge::CreateEGLSurfaceForCompositor()
{
    if (!jEGLSurfacePointerField) {
        return nullptr;
    }
    MOZ_ASSERT(mGLControllerObj, "AndroidBridge::CreateEGLSurfaceForCompositor called with a null GL controller ref");

    JNIEnv* env = GetJNIForThread(); // called on the compositor thread

    AutoLocalJNIFrame jniFrame(env, 1);
    jobject eglSurface = mGLControllerObj->CreateEGLSurfaceForCompositorWrapper();
    if (!eglSurface)
        return nullptr;

    EGLSurface ret = reinterpret_cast<EGLSurface>(env->GetIntField(eglSurface, jEGLSurfacePointerField));
    return ret;
}

bool
AndroidBridge::GetStaticIntField(const char *className, const char *fieldName, int32_t* aInt, JNIEnv* jEnv /* = nullptr */)
{
    ALOG_BRIDGE("AndroidBridge::GetStaticIntField %s", fieldName);

    if (!jEnv) {
        if (!HasEnv()) {
            return false;
        }
        jEnv = GetJNIEnv();
    }

    initInit();
    getClassGlobalRef(className);
    jfieldID field = getStaticField(fieldName, "I");

    if (!field) {
        jEnv->DeleteGlobalRef(jClass);
        return false;
    }

    *aInt = static_cast<int32_t>(jEnv->GetStaticIntField(jClass, field));

    jEnv->DeleteGlobalRef(jClass);
    return true;
}

bool
AndroidBridge::GetStaticStringField(const char *className, const char *fieldName, nsAString &result, JNIEnv* jEnv /* = nullptr */)
{
    ALOG_BRIDGE("AndroidBridge::GetStaticStringField %s", fieldName);

    if (!jEnv) {
        if (!HasEnv()) {
            return false;
        }
        jEnv = GetJNIEnv();
    }

    AutoLocalJNIFrame jniFrame(jEnv, 1);
    initInit();
    getClassGlobalRef(className);
    jfieldID field = getStaticField(fieldName, "Ljava/lang/String;");

    if (!field) {
        jEnv->DeleteGlobalRef(jClass);
        return false;
    }

    jstring jstr = (jstring) jEnv->GetStaticObjectField(jClass, field);
    jEnv->DeleteGlobalRef(jClass);
    if (!jstr)
        return false;

    result.Assign(nsJNIString(jstr, jEnv));
    return true;
}

// Available for places elsewhere in the code to link to.
bool
mozilla_AndroidBridge_SetMainThread(pthread_t thr)
{
    return AndroidBridge::Bridge()->SetMainThread(thr);
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

    AutoLocalJNIFrame jniFrame(env, 1);
    jintArray arr = GeckoAppShell::InitCameraWrapper(NS_ConvertUTF8toUTF16(contentType), (int32_t) camera, (int32_t) width, (int32_t) height);

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
AndroidBridge::GetCurrentBatteryInformation(hal::BatteryInformation* aBatteryInfo)
{
    ALOG_BRIDGE("AndroidBridge::GetCurrentBatteryInformation");

    JNIEnv *env = GetJNIEnv();

    AutoLocalJNIFrame jniFrame(env, 1);

    // To prevent calling too many methods through JNI, the Java method returns
    // an array of double even if we actually want a double and a boolean.
    jdoubleArray arr = GeckoAppShell::GetCurrentBatteryInformationWrapper();
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
AndroidBridge::HandleGeckoMessage(const nsAString &aMessage)
{
    ALOG_BRIDGE("%s", __PRETTY_FUNCTION__);

    JNIEnv *env = GetJNIEnv();

    AutoLocalJNIFrame jniFrame(env, 1);
    GeckoAppShell::HandleGeckoMessageWrapper(aMessage);
}

nsresult
AndroidBridge::GetSegmentInfoForText(const nsAString& aText,
                                     nsIMobileMessageCallback* aRequest)
{
#ifndef MOZ_WEBSMS_BACKEND
    return NS_ERROR_FAILURE;
#else
    ALOG_BRIDGE("AndroidBridge::GetSegmentInfoForText");

    dom::mobilemessage::SmsSegmentInfoData data;

    data.segments() = 0;
    data.charsPerSegment() = 0;
    data.charsAvailableInLastSegment() = 0;

    JNIEnv *env = GetJNIEnv();

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

    data.segments() = info[0]; // msgCount
    data.charsPerSegment() = info[2]; // codeUnitsRemaining
    // segmentChars = (codeUnitCount + codeUnitsRemaining) / msgCount
    data.charsAvailableInLastSegment() = (info[1] + info[2]) / info[0];

    env->ReleaseIntArrayElements(arr, info, JNI_ABORT);

    // TODO Bug 908598 - Should properly use |QueueSmsRequest(...)| to queue up
    // the nsIMobileMessageCallback just like other functions.
    nsCOMPtr<nsIDOMMozSmsSegmentInfo> info = new SmsSegmentInfo(data);
    return aRequest->NotifySegmentInfoForTextGot(info);
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

    JNIEnv *env = GetJNIEnv();

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

    GeckoAppShell::CreateMessageListWrapper(aFilter.startDate(), aFilter.endDate(),
                             numbers, aFilter.numbers().Length(),
                             aFilter.delivery(), aReverse, requestId);
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

    JNIEnv *env = GetJNIEnv();

    AutoLocalJNIFrame jniFrame(env, 1);

    // To prevent calling too many methods through JNI, the Java method returns
    // an array of double even if we actually want a double, two booleans, and an integer.

    jdoubleArray arr = GeckoAppShell::GetCurrentNetworkInformationWrapper();
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

void *
AndroidBridge::LockBitmap(jobject bitmap)
{
    JNIEnv *env = GetJNIEnv();

    AutoLocalJNIFrame jniFrame(env, 0);

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

    AutoLocalJNIFrame jniFrame(env, 0);

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
    if (sGlobalContext == nullptr) {
        JNIEnv *env = GetJNIForThread();

        AutoLocalJNIFrame jniFrame(env, 4);

        jobject context = GeckoAppShell::GetContext();
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
        jobject appContext = env->CallObjectMethod(context, mid);
        if (!appContext) {
            ALOG_BRIDGE("%s: getApplicationContext failed.", __FUNCTION__);
            return 0;
        }

        sGlobalContext = env->NewGlobalRef(appContext);
        MOZ_ASSERT(sGlobalContext);
    }

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
    GeckoLayerClient *client = mLayerClient;
    if (!client)
        return;

    client->SetFirstPaintViewport((float)aOffset.x, (float)aOffset.y, aZoom.scale,
                                  aCssPageRect.x, aCssPageRect.y, aCssPageRect.XMost(), aCssPageRect.YMost());
}

void
AndroidBridge::SetPageRect(const CSSRect& aCssPageRect)
{
    GeckoLayerClient *client = mLayerClient;
    if (!client)
        return;

    client->SetPageRect(aCssPageRect.x, aCssPageRect.y, aCssPageRect.XMost(), aCssPageRect.YMost());
}

void
AndroidBridge::SyncViewportInfo(const LayerIntRect& aDisplayPort, const CSSToLayerScale& aDisplayResolution,
                                bool aLayersUpdated, ScreenPoint& aScrollOffset, CSSToScreenScale& aScale,
                                LayerMargin& aFixedLayerMargins, ScreenPoint& aOffset)
{
    GeckoLayerClient *client = mLayerClient;
    if (!client) {
        ALOG_BRIDGE("Exceptional Exit: %s", __PRETTY_FUNCTION__);
        return;
    }

    jobject viewTransformJObj = client->SyncViewportInfo(aDisplayPort.x, aDisplayPort.y,
                                aDisplayPort.width, aDisplayPort.height,
                                aDisplayResolution.scale, aLayersUpdated);
    NS_ABORT_IF_FALSE(viewTransformJObj, "No view transform object!");

    if (!viewTransformJObj) {
        return;
    }

    ViewTransform* viewTransform = ViewTransform::Wrap(viewTransformJObj);
    aScrollOffset = ScreenPoint(viewTransform->getx(), viewTransform->gety());
    aScale.scale = viewTransform->getscale();
    aFixedLayerMargins.top = viewTransform->getfixedLayerMarginTop();
    aFixedLayerMargins.right = viewTransform->getfixedLayerMarginRight();
    aFixedLayerMargins.bottom = viewTransform->getfixedLayerMarginBottom();
    aFixedLayerMargins.left = viewTransform->getfixedLayerMarginLeft();
    aOffset.x = viewTransform->getoffsetX();
    aOffset.y = viewTransform->getoffsetY();
    delete viewTransform;
}

void AndroidBridge::SyncFrameMetrics(const ScreenPoint& aScrollOffset, float aZoom, const CSSRect& aCssPageRect,
                                     bool aLayersUpdated, const CSSRect& aDisplayPort, const CSSToLayerScale& aDisplayResolution,
                                     bool aIsFirstPaint, LayerMargin& aFixedLayerMargins, ScreenPoint& aOffset)
{
    GeckoLayerClient *client = mLayerClient;
    if (!client) {
        ALOG_BRIDGE("Exceptional Exit: %s", __PRETTY_FUNCTION__);
        return;
    }

    // convert the displayport rect from scroll-relative CSS pixels to document-relative device pixels
    LayerRect dpUnrounded = aDisplayPort * aDisplayResolution;
    dpUnrounded += LayerPoint::FromUnknownPoint(aScrollOffset.ToUnknownPoint());
    LayerIntRect dp = gfx::RoundedToInt(dpUnrounded);

    jobject viewTransformJObj = client->SyncFrameMetrics(aScrollOffset.x, aScrollOffset.y, aZoom,
                                                         aCssPageRect.x, aCssPageRect.y, aCssPageRect.XMost(), aCssPageRect.YMost(),
                                                         aLayersUpdated, dp.x, dp.y, dp.width, dp.height, aDisplayResolution.scale,
                                                         aIsFirstPaint);

    NS_ABORT_IF_FALSE(viewTransformJObj, "No view transform object!");
    if (!viewTransformJObj) {
        return;
    }
    ViewTransform* viewTransform = ViewTransform::Wrap(viewTransformJObj);

    aFixedLayerMargins.top = viewTransform->getfixedLayerMarginTop();
    aFixedLayerMargins.right = viewTransform->getfixedLayerMarginRight();
    aFixedLayerMargins.bottom = viewTransform->getfixedLayerMarginBottom();
    aFixedLayerMargins.left = viewTransform->getfixedLayerMarginLeft();

    aOffset.x = viewTransform->getoffsetX();
    aOffset.y = viewTransform->getoffsetY();

    delete viewTransform;
}

AndroidBridge::AndroidBridge()
  : mLayerClient(nullptr),
    mNativePanZoomController(nullptr)
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
NS_IMETHODIMP nsAndroidBridge::HandleGeckoMessage(const nsAString & message)
{
    AndroidBridge::Bridge()->HandleGeckoMessage(message);
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

// DO NOT USE THIS unless you need to access JNI from
// non-main threads.  This is probably not what you want.
// Questions, ask blassey or dougt.

static void
JavaThreadDetachFunc(void *arg)
{
    JNIEnv *env = (JNIEnv*) arg;
    MOZ_ASSERT(env, "No JNIEnv on Gecko thread");
    if (!env) {
        return;
    }
    JavaVM *vm = nullptr;
    env->GetJavaVM(&vm);
    MOZ_ASSERT(vm, "No JavaVM on Gecko thread");
    if (!vm) {
        return;
    }
    vm->DetachCurrentThread();
}

uint32_t
AndroidBridge::GetScreenOrientation()
{
    ALOG_BRIDGE("AndroidBridge::GetScreenOrientation");

    int16_t orientation = GeckoAppShell::GetScreenOrientationWrapper();

    if (!orientation)
        return dom::eScreenOrientation_None;

    return static_cast<dom::ScreenOrientation>(orientation);
}

void
AndroidBridge::ScheduleComposite()
{
    nsWindow::ScheduleComposite();
}

nsresult
AndroidBridge::GetProxyForURI(const nsACString & aSpec,
                              const nsACString & aScheme,
                              const nsACString & aHost,
                              const int32_t      aPort,
                              nsACString & aResult)
{
    if (!HasEnv()) {
        return NS_ERROR_FAILURE;
    }
    JNIEnv* env = GetJNIEnv();

    AutoLocalJNIFrame jniFrame(env, 1);
    jstring jstrRet = GeckoAppShell::GetProxyForURIWrapper(NS_ConvertUTF8toUTF16(aSpec),
                                            NS_ConvertUTF8toUTF16(aScheme),
                                            NS_ConvertUTF8toUTF16(aHost),
                                            aPort);

    if (!jstrRet)
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
    nsWindow* win = nsWindow::TopWindow();
    if (!win)
        return;

    CSSRect cssRect = rect / win->GetDefaultScale();
    GeckoAppShell::AddPluginViewWrapper(view, cssRect.x, cssRect.y, cssRect.width, cssRect.height, isFullScreen);
}

extern "C"
__attribute__ ((visibility("default")))
jobject JNICALL
Java_org_mozilla_gecko_GeckoAppShell_allocateDirectBuffer(JNIEnv *env, jclass, jlong size);

bool
AndroidBridge::GetThreadNameJavaProfiling(uint32_t aThreadId, nsCString & aResult)
{
    JNIEnv* env = GetJNIForThread();

    AutoLocalJNIFrame jniFrame(env, 1);

    jstring jstrThreadName = GeckoJavaSampler::GetThreadNameJavaProfilingWrapper(aThreadId);

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

    AutoLocalJNIFrame jniFrame(env, 1);

    jstring jstrSampleName = GeckoJavaSampler::GetFrameNameJavaProfilingWrapper(aThreadId, aSampleId, aFrameId);

    if (!jstrSampleName)
        return false;

    nsJNIString jniStr(jstrSampleName, env);
    CopyUTF16toUTF8(jniStr.get(), aResult);
    return true;
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
                            is24bit ? gfxImageFormat::RGB24 :
                                      gfxImageFormat::RGB16_565);
    if (surf->CairoStatus() != 0) {
        ALOG_BRIDGE("Error creating gfxImageSurface");
        return NS_ERROR_FAILURE;
    }

    nsRefPtr<gfxContext> context;
    if (gfxPlatform::GetPlatform()->SupportsAzureContentForType(BackendType::CAIRO)) {
        RefPtr<DrawTarget> dt =
            gfxPlatform::GetPlatform()->CreateDrawTargetForSurface(surf, IntSize(bufW, bufH));

        if (!dt) {
            ALOG_BRIDGE("Error creating DrawTarget");
            return NS_ERROR_FAILURE;
        }
        context = new gfxContext(dt);
    } else {
        context = new gfxContext(surf);
    }
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

    ALOG_BRIDGE("Enter: %s", __PRETTY_FUNCTION__);
    JNIEnv* env = GetJNIEnv();
    if (!mLayerClient || mLayerClient->isNull()) {

        ALOG_BRIDGE("Exceptional Exit: %s", __PRETTY_FUNCTION__);
        return;
    }
    AutoLocalJNIFrame jniFrame(env, 0);

    float x, y, width, height,
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

    ImmutableViewportMetrics jmetrics = ImmutableViewportMetrics(pageLeft, pageTop, pageRight, pageBottom,
                                                                 cssPageLeft, cssPageTop, cssPageRight, cssPageBottom,
                                                                 x, y, x + width, y + height,
                                                                 zoom);

    jobject jobj = mLayerClient->GetDisplayPort(aPageSizeUpdate, aIsBrowserContentDisplayed, tabId, jmetrics.wrappedObject());
    if (!jobj) {
        ALOG_BRIDGE("Exceptional Exit: %s", __PRETTY_FUNCTION__);
        return;
    }
    DisplayPortMetrics* displayPortMetrics = DisplayPortMetrics::Wrap(jobj);

    AndroidRectF rect(env, displayPortMetrics->getMPosition());
    if (jniFrame.CheckForException()) {
        ALOG_BRIDGE("Exceptional Exit: %s", __PRETTY_FUNCTION__);
        return;
    }

    float resolution = displayPortMetrics->getResolution();
    if (jniFrame.CheckForException()) {
        ALOG_BRIDGE("Exceptional Exit: %s", __PRETTY_FUNCTION__);
        return;
    }

    *displayPort = new nsAndroidDisplayport(rect, resolution);
    (*displayPort)->AddRef();

    delete displayPortMetrics;
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
AndroidBridge::ProgressiveUpdateCallback(bool aHasPendingNewThebesContent, const LayerRect& aDisplayPort, float aDisplayResolution, bool aDrawingCritical, ParentLayerRect& aCompositionBounds, CSSToParentLayerScale& aZoom)
{
    GeckoLayerClient *client = mLayerClient;
    if (!client) {
        ALOG_BRIDGE("Exceptional Exit: %s", __PRETTY_FUNCTION__);
        return false;
    }

    jobject progressiveUpdateDataJObj = client->ProgressiveUpdateCallback(aHasPendingNewThebesContent,
                                                                   (float)aDisplayPort.x,
                                                                   (float)aDisplayPort.y,
                                                                   (float)aDisplayPort.width,
                                                                   (float)aDisplayPort.height,
                                                                          aDisplayResolution,
                                                                         !aDrawingCritical);

    NS_ABORT_IF_FALSE(progressiveUpdateDataJObj, "No progressive update data!");

    ProgressiveUpdateData* progressiveUpdateData = ProgressiveUpdateData::Wrap(progressiveUpdateDataJObj);

    aCompositionBounds.x = progressiveUpdateData->getx();
    aCompositionBounds.y = progressiveUpdateData->gety();
    aCompositionBounds.width = progressiveUpdateData->getwidth();
    aCompositionBounds.height = progressiveUpdateData->getheight();
    aZoom.scale = progressiveUpdateData->getscale();

    bool ret = progressiveUpdateData->getabort();
    delete progressiveUpdateData;

    return ret;
}

NativePanZoomController*
AndroidBridge::SetNativePanZoomController(jobject obj)
{
    NativePanZoomController* old = mNativePanZoomController;
    mNativePanZoomController = NativePanZoomController::Wrap(obj);
    return old;
}

void
AndroidBridge::RequestContentRepaint(const mozilla::layers::FrameMetrics& aFrameMetrics)
{
    ALOG_BRIDGE("AndroidBridge::RequestContentRepaint");

    CSSToScreenScale resolution = aFrameMetrics.mZoom;
    ScreenRect dp = (aFrameMetrics.mDisplayPort + aFrameMetrics.mScrollOffset) * resolution;

    mNativePanZoomController->RequestContentRepaintWrapper(dp.x, dp.y, dp.width, dp.height, resolution.scale);
}

void
AndroidBridge::AcknowledgeScrollUpdate(const mozilla::layers::FrameMetrics::ViewID& aScrollId,
                                       const uint32_t& aScrollGeneration)
{
    // FIXME implement this
}

void
AndroidBridge::HandleDoubleTap(const CSSIntPoint& aPoint,
                               int32_t aModifiers,
                               const mozilla::layers::ScrollableLayerGuid& aGuid)
{
    nsCString data = nsPrintfCString("{ \"x\": %d, \"y\": %d }", aPoint.x, aPoint.y);
    nsAppShell::gAppShell->PostEvent(AndroidGeckoEvent::MakeBroadcastEvent(
            NS_LITERAL_CSTRING("Gesture:DoubleTap"), data));
}

void
AndroidBridge::HandleSingleTap(const CSSIntPoint& aPoint,
                               int32_t aModifiers,
                               const mozilla::layers::ScrollableLayerGuid& aGuid)
{
    // TODO Send the modifier data to Gecko for use in mouse events.
    nsCString data = nsPrintfCString("{ \"x\": %d, \"y\": %d }", aPoint.x, aPoint.y);
    nsAppShell::gAppShell->PostEvent(AndroidGeckoEvent::MakeBroadcastEvent(
            NS_LITERAL_CSTRING("Gesture:SingleTap"), data));
}

void
AndroidBridge::HandleLongTap(const CSSIntPoint& aPoint,
                             int32_t aModifiers,
                             const mozilla::layers::ScrollableLayerGuid& aGuid)
{
    nsCString data = nsPrintfCString("{ \"x\": %d, \"y\": %d }", aPoint.x, aPoint.y);
    nsAppShell::gAppShell->PostEvent(AndroidGeckoEvent::MakeBroadcastEvent(
            NS_LITERAL_CSTRING("Gesture:LongPress"), data));
}

void
AndroidBridge::HandleLongTapUp(const CSSIntPoint& aPoint,
                               int32_t aModifiers,
                               const mozilla::layers::ScrollableLayerGuid& aGuid)
{
}

void
AndroidBridge::SendAsyncScrollDOMEvent(bool aIsRoot,
                                       const CSSRect& aContentRect,
                                       const CSSSize& aScrollableSize)
{
    // FIXME implement this
}

void
AndroidBridge::PostDelayedTask(Task* aTask, int aDelayMs)
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
        mNativePanZoomController->PostDelayedCallbackWrapper((int64_t)aDelayMs);
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
