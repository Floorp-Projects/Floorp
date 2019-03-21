/* -*- Mode: c++; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*-
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
#include "AndroidBridge.h"
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
#include "nsIDOMWindowUtils.h"
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
#include "nsIObserverService.h"
#include "nsISupportsPrimitives.h"
#include "WidgetUtils.h"

#include "FennecJNIWrappers.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::java;

AndroidBridge* AndroidBridge::sBridge = nullptr;
static jobject sGlobalContext = nullptr;
nsDataHashtable<nsStringHashKey, nsString> AndroidBridge::sStoragePaths;

jmethodID AndroidBridge::GetMethodID(JNIEnv* env, jclass jClass,
                                     const char* methodName,
                                     const char* methodType) {
  jmethodID methodID = env->GetMethodID(jClass, methodName, methodType);
  if (!methodID) {
    ALOG(
        ">>> FATAL JNI ERROR! GetMethodID(methodName=\"%s\", "
        "methodType=\"%s\") failed. Did ProGuard optimize away something it "
        "shouldn't have?",
        methodName, methodType);
    env->ExceptionDescribe();
    MOZ_CRASH();
  }
  return methodID;
}

jmethodID AndroidBridge::GetStaticMethodID(JNIEnv* env, jclass jClass,
                                           const char* methodName,
                                           const char* methodType) {
  jmethodID methodID = env->GetStaticMethodID(jClass, methodName, methodType);
  if (!methodID) {
    ALOG(
        ">>> FATAL JNI ERROR! GetStaticMethodID(methodName=\"%s\", "
        "methodType=\"%s\") failed. Did ProGuard optimize away something it "
        "shouldn't have?",
        methodName, methodType);
    env->ExceptionDescribe();
    MOZ_CRASH();
  }
  return methodID;
}

jfieldID AndroidBridge::GetFieldID(JNIEnv* env, jclass jClass,
                                   const char* fieldName,
                                   const char* fieldType) {
  jfieldID fieldID = env->GetFieldID(jClass, fieldName, fieldType);
  if (!fieldID) {
    ALOG(
        ">>> FATAL JNI ERROR! GetFieldID(fieldName=\"%s\", "
        "fieldType=\"%s\") failed. Did ProGuard optimize away something it "
        "shouldn't have?",
        fieldName, fieldType);
    env->ExceptionDescribe();
    MOZ_CRASH();
  }
  return fieldID;
}

jfieldID AndroidBridge::GetStaticFieldID(JNIEnv* env, jclass jClass,
                                         const char* fieldName,
                                         const char* fieldType) {
  jfieldID fieldID = env->GetStaticFieldID(jClass, fieldName, fieldType);
  if (!fieldID) {
    ALOG(
        ">>> FATAL JNI ERROR! GetStaticFieldID(fieldName=\"%s\", "
        "fieldType=\"%s\") failed. Did ProGuard optimize away something it "
        "shouldn't have?",
        fieldName, fieldType);
    env->ExceptionDescribe();
    MOZ_CRASH();
  }
  return fieldID;
}

void AndroidBridge::ConstructBridge() {
  /* NSS hack -- bionic doesn't handle recursive unloads correctly,
   * because library finalizer functions are called with the dynamic
   * linker lock still held.  This results in a deadlock when trying
   * to call dlclose() while we're already inside dlclose().
   * Conveniently, NSS has an env var that can prevent it from unloading.
   */
  putenv(const_cast<char*>("NSS_DISABLE_UNLOAD=1"));

  MOZ_ASSERT(!sBridge);
  sBridge = new AndroidBridge();
}

void AndroidBridge::DeconstructBridge() {
  if (sBridge) {
    delete sBridge;
    // AndroidBridge destruction requires sBridge to still be valid,
    // so we set sBridge to nullptr after deleting it.
    sBridge = nullptr;
  }
}

AndroidBridge::~AndroidBridge() {}

AndroidBridge::AndroidBridge() {
  ALOG_BRIDGE("AndroidBridge::Init");

  JNIEnv* const jEnv = jni::GetGeckoThreadEnv();
  AutoLocalJNIFrame jniFrame(jEnv);

  mMessageQueue = java::GeckoThread::MsgQueue();
  auto msgQueueClass = jni::Class::LocalRef::Adopt(
      jEnv, jEnv->GetObjectClass(mMessageQueue.Get()));
  // mMessageQueueNext must not be null
  mMessageQueueNext =
      GetMethodID(jEnv, msgQueueClass.Get(), "next", "()Landroid/os/Message;");
  // mMessageQueueMessages may be null (e.g. due to proguard optimization)
  mMessageQueueMessages = jEnv->GetFieldID(msgQueueClass.Get(), "mMessages",
                                           "Landroid/os/Message;");

  AutoJNIClass string(jEnv, "java/lang/String");
  jStringClass = string.getGlobalRef();

  mAPIVersion = jni::GetAPIVersion();

  AutoJNIClass channels(jEnv, "java/nio/channels/Channels");
  jChannels = channels.getGlobalRef();
  jChannelCreate = channels.getStaticMethod(
      "newChannel",
      "(Ljava/io/InputStream;)Ljava/nio/channels/ReadableByteChannel;");

  AutoJNIClass readableByteChannel(jEnv,
                                   "java/nio/channels/ReadableByteChannel");
  jReadableByteChannel = readableByteChannel.getGlobalRef();
  jByteBufferRead =
      readableByteChannel.getMethod("read", "(Ljava/nio/ByteBuffer;)I");

  AutoJNIClass inputStream(jEnv, "java/io/InputStream");
  jInputStream = inputStream.getGlobalRef();
  jClose = inputStream.getMethod("close", "()V");
  jAvailable = inputStream.getMethod("available", "()I");
}

static void getHandlersFromStringArray(
    JNIEnv* aJNIEnv, jni::ObjectArray::Param aArr, size_t aLen,
    nsIMutableArray* aHandlersArray, nsIHandlerApp** aDefaultApp,
    const nsAString& aAction = EmptyString(),
    const nsACString& aMimeType = EmptyCString()) {
  nsString empty = EmptyString();

  auto getNormalizedString = [](jni::Object::Param obj) -> nsString {
    nsString out;
    if (!obj) {
      out.SetIsVoid(true);
    } else {
      out.Assign(jni::String::Ref::From(obj)->ToString());
    }
    return out;
  };

  for (size_t i = 0; i < aLen; i += 4) {
    nsString name(getNormalizedString(aArr->GetElement(i)));
    nsString isDefault(getNormalizedString(aArr->GetElement(i + 1)));
    nsString packageName(getNormalizedString(aArr->GetElement(i + 2)));
    nsString className(getNormalizedString(aArr->GetElement(i + 3)));

    nsIHandlerApp* app = nsOSHelperAppService::CreateAndroidHandlerApp(
        name, className, packageName, className, aMimeType, aAction);

    aHandlersArray->AppendElement(app);
    if (aDefaultApp && isDefault.Length() > 0) *aDefaultApp = app;
  }
}

bool AndroidBridge::GetHandlersForMimeType(const nsAString& aMimeType,
                                           nsIMutableArray* aHandlersArray,
                                           nsIHandlerApp** aDefaultApp,
                                           const nsAString& aAction) {
  ALOG_BRIDGE("AndroidBridge::GetHandlersForMimeType");

  auto arr = GeckoAppShell::GetHandlersForMimeType(aMimeType, aAction);
  if (!arr) return false;

  JNIEnv* const env = arr.Env();
  size_t len = arr->Length();

  if (!aHandlersArray) return len > 0;

  getHandlersFromStringArray(env, arr, len, aHandlersArray, aDefaultApp,
                             aAction, NS_ConvertUTF16toUTF8(aMimeType));
  return true;
}

bool AndroidBridge::GetHWEncoderCapability() {
  ALOG_BRIDGE("AndroidBridge::GetHWEncoderCapability");

  bool value = GeckoAppShell::GetHWEncoderCapability();

  return value;
}

bool AndroidBridge::GetHWDecoderCapability() {
  ALOG_BRIDGE("AndroidBridge::GetHWDecoderCapability");

  bool value = GeckoAppShell::GetHWDecoderCapability();

  return value;
}

bool AndroidBridge::GetHandlersForURL(const nsAString& aURL,
                                      nsIMutableArray* aHandlersArray,
                                      nsIHandlerApp** aDefaultApp,
                                      const nsAString& aAction) {
  ALOG_BRIDGE("AndroidBridge::GetHandlersForURL");

  auto arr = GeckoAppShell::GetHandlersForURL(aURL, aAction);
  if (!arr) return false;

  JNIEnv* const env = arr.Env();
  size_t len = arr->Length();

  if (!aHandlersArray) return len > 0;

  getHandlersFromStringArray(env, arr, len, aHandlersArray, aDefaultApp,
                             aAction);
  return true;
}

void AndroidBridge::GetMimeTypeFromExtensions(const nsACString& aFileExt,
                                              nsCString& aMimeType) {
  ALOG_BRIDGE("AndroidBridge::GetMimeTypeFromExtensions");

  auto jstrType = GeckoAppShell::GetMimeTypeFromExtensions(aFileExt);

  if (jstrType) {
    aMimeType = jstrType->ToCString();
  }
}

void AndroidBridge::GetExtensionFromMimeType(const nsACString& aMimeType,
                                             nsACString& aFileExt) {
  ALOG_BRIDGE("AndroidBridge::GetExtensionFromMimeType");

  auto jstrExt = GeckoAppShell::GetExtensionFromMimeType(aMimeType);

  if (jstrExt) {
    aFileExt = jstrExt->ToCString();
  }
}

int AndroidBridge::GetScreenDepth() {
  ALOG_BRIDGE("%s", __PRETTY_FUNCTION__);

  static int sDepth = 0;
  if (sDepth) return sDepth;

  const int DEFAULT_DEPTH = 16;

  if (jni::IsAvailable()) {
    sDepth = GeckoAppShell::GetScreenDepth();
  }
  if (!sDepth) return DEFAULT_DEPTH;

  return sDepth;
}

void AndroidBridge::Vibrate(const nsTArray<uint32_t>& aPattern) {
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

  GeckoAppShell::Vibrate(jni::LongArray::Ref::From(array),
                         -1 /* don't repeat */);
}

void AndroidBridge::GetSystemColors(AndroidSystemColors* aColors) {
  NS_ASSERTION(aColors != nullptr,
               "AndroidBridge::GetSystemColors: aColors is null!");
  if (!aColors) return;

  auto arr = GeckoAppShell::GetSystemColors();
  if (!arr) return;

  JNIEnv* const env = arr.Env();
  uint32_t len = static_cast<uint32_t>(env->GetArrayLength(arr.Get()));
  jint* elements = env->GetIntArrayElements(arr.Get(), 0);

  uint32_t colorsCount = sizeof(AndroidSystemColors) / sizeof(nscolor);
  if (len < colorsCount) colorsCount = len;

  // Convert Android colors to nscolor by switching R and B in the ARGB 32 bit
  // value
  nscolor* colors = (nscolor*)aColors;

  for (uint32_t i = 0; i < colorsCount; i++) {
    uint32_t androidColor = static_cast<uint32_t>(elements[i]);
    uint8_t r = (androidColor & 0x00ff0000) >> 16;
    uint8_t b = (androidColor & 0x000000ff);
    colors[i] = (androidColor & 0xff00ff00) | (b << 16) | r;
  }

  env->ReleaseIntArrayElements(arr.Get(), elements, 0);
}

void AndroidBridge::GetIconForExtension(const nsACString& aFileExt,
                                        uint32_t aIconSize,
                                        uint8_t* const aBuf) {
  ALOG_BRIDGE("AndroidBridge::GetIconForExtension");
  NS_ASSERTION(aBuf != nullptr,
               "AndroidBridge::GetIconForExtension: aBuf is null!");
  if (!aBuf) return;

  auto arr = GeckoAppShell::GetIconForExtension(NS_ConvertUTF8toUTF16(aFileExt),
                                                aIconSize);

  NS_ASSERTION(
      arr != nullptr,
      "AndroidBridge::GetIconForExtension: Returned pixels array is null!");
  if (!arr) return;

  JNIEnv* const env = arr.Env();
  uint32_t len = static_cast<uint32_t>(env->GetArrayLength(arr.Get()));
  jbyte* elements = env->GetByteArrayElements(arr.Get(), 0);

  uint32_t bufSize = aIconSize * aIconSize * 4;
  NS_ASSERTION(
      len == bufSize,
      "AndroidBridge::GetIconForExtension: Pixels array is incomplete!");
  if (len == bufSize) memcpy(aBuf, elements, bufSize);

  env->ReleaseByteArrayElements(arr.Get(), elements, 0);
}

bool AndroidBridge::GetStaticIntField(const char* className,
                                      const char* fieldName, int32_t* aInt,
                                      JNIEnv* jEnv /* = nullptr */) {
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

bool AndroidBridge::GetStaticStringField(const char* className,
                                         const char* fieldName,
                                         nsAString& result,
                                         JNIEnv* jEnv /* = nullptr */) {
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

  jstring jstr = (jstring)jEnv->GetStaticObjectField(cls.getRawRef(), field);
  if (!jstr) return false;

  result.Assign(jni::String::Ref::From(jstr)->ToString());
  return true;
}

namespace mozilla {
class TracerRunnable : public Runnable {
 public:
  TracerRunnable() : Runnable("TracerRunnable") {
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
    if (!AndroidBridge::Bridge()) return NS_OK;

    mHasRun = true;
    mTracerCondVar->Notify();
    return NS_OK;
  }

  bool Fire() {
    if (!mTracerLock || !mTracerCondVar) return false;
    MutexAutoLock lock(*mTracerLock);
    mHasRun = false;
    mMainThread->Dispatch(this, NS_DISPATCH_NORMAL);
    while (!mHasRun) mTracerCondVar->Wait();
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
  if (!sTracerRunnable) sTracerRunnable = new TracerRunnable();
  return true;
}

void CleanUpWidgetTracing() { sTracerRunnable = nullptr; }

bool FireAndWaitForTracerEvent() {
  if (sTracerRunnable) return sTracerRunnable->Fire();
  return false;
}

void SignalTracerThread() {
  if (sTracerRunnable) return sTracerRunnable->Signal();
}

}  // namespace mozilla

void AndroidBridge::GetCurrentBatteryInformation(
    hal::BatteryInformation* aBatteryInfo) {
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

void AndroidBridge::GetCurrentNetworkInformation(
    hal::NetworkInformation* aNetworkInfo) {
  ALOG_BRIDGE("AndroidBridge::GetCurrentNetworkInformation");

  // To prevent calling too many methods through JNI, the Java method returns
  // an array of double even if we actually want an integer, a boolean, and an
  // integer.

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

jobject AndroidBridge::GetGlobalContextRef() {
  // The context object can change, so get a fresh copy every time.
  auto context = GeckoAppShell::GetApplicationContext();
  sGlobalContext = jni::Object::GlobalRef(context).Forget();
  MOZ_ASSERT(sGlobalContext);
  return sGlobalContext;
}

/* Implementation file */
NS_IMPL_ISUPPORTS(nsAndroidBridge, nsIAndroidEventDispatcher, nsIAndroidBridge,
                  nsIObserver)

nsAndroidBridge::nsAndroidBridge() : mAudibleWindowsNum(0) {
  if (jni::IsAvailable()) {
    RefPtr<widget::EventDispatcher> dispatcher = new widget::EventDispatcher();
    dispatcher->Attach(java::EventDispatcher::GetInstance(),
                       /* window */ nullptr);
    mEventDispatcher = dispatcher;
  }

  AddObservers();
}

nsAndroidBridge::~nsAndroidBridge() {}

NS_IMETHODIMP nsAndroidBridge::ContentDocumentChanged(
    mozIDOMWindowProxy* aWindow) {
  AndroidBridge::Bridge()->ContentDocumentChanged(aWindow);
  return NS_OK;
}

NS_IMETHODIMP nsAndroidBridge::IsContentDocumentDisplayed(
    mozIDOMWindowProxy* aWindow, bool* aRet) {
  *aRet = AndroidBridge::Bridge()->IsContentDocumentDisplayed(aWindow);
  return NS_OK;
}

NS_IMETHODIMP
nsAndroidBridge::Observe(nsISupports* aSubject, const char* aTopic,
                         const char16_t* aData) {
  if (!strcmp(aTopic, "xpcom-shutdown")) {
    RemoveObservers();
  } else if (!strcmp(aTopic, "audio-playback")) {
    ALOG_BRIDGE("nsAndroidBridge::Observe, get audio-playback event.");

    nsAutoString activeStr(aData);
    bool isPlaying = activeStr.EqualsLiteral("active");
    UpdateAudioPlayingWindows(isPlaying);
  }
  return NS_OK;
}

void nsAndroidBridge::UpdateAudioPlayingWindows(bool aPlaying) {
  // Request audio focus for the first audio playing window and abandon focus
  // for the last audio playing window.
  MOZ_ASSERT(mAudibleWindowsNum >= 0);
  if (aPlaying && mAudibleWindowsNum++ == 0) {
    ALOG_BRIDGE("nsAndroidBridge, request audio focus.");
    AudioFocusAgent::NotifyStartedPlaying();
  } else if (!aPlaying && --mAudibleWindowsNum == 0) {
    ALOG_BRIDGE("nsAndroidBridge, abandon audio focus.");
    AudioFocusAgent::NotifyStoppedPlaying();
  }
}

void nsAndroidBridge::AddObservers() {
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->AddObserver(this, "xpcom-shutdown", false);
    if (jni::IsFennec()) {  // No AudioFocusAgent in non-Fennec environment.
      obs->AddObserver(this, "audio-playback", false);
    }
  }
}

void nsAndroidBridge::RemoveObservers() {
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->RemoveObserver(this, "xpcom-shutdown");
    if (jni::IsFennec()) {  // No AudioFocusAgent in non-Fennec environment.
      obs->RemoveObserver(this, "audio-playback");
    }
  }
}

uint32_t AndroidBridge::GetScreenOrientation() {
  ALOG_BRIDGE("AndroidBridge::GetScreenOrientation");

  int16_t orientation = GeckoAppShell::GetScreenOrientation();

  if (!orientation) return hal::eScreenOrientation_None;

  return static_cast<hal::ScreenOrientation>(orientation);
}

uint16_t AndroidBridge::GetScreenAngle() {
  return GeckoAppShell::GetScreenAngle();
}

nsresult AndroidBridge::GetProxyForURI(const nsACString& aSpec,
                                       const nsACString& aScheme,
                                       const nsACString& aHost,
                                       const int32_t aPort,
                                       nsACString& aResult) {
  if (!jni::IsAvailable()) {
    return NS_ERROR_FAILURE;
  }

  auto jstrRet = GeckoAppShell::GetProxyForURI(aSpec, aScheme, aHost, aPort);

  if (!jstrRet) return NS_ERROR_FAILURE;

  aResult = jstrRet->ToCString();
  return NS_OK;
}

bool AndroidBridge::PumpMessageLoop() {
  JNIEnv* const env = jni::GetGeckoThreadEnv();

  if (mMessageQueueMessages) {
    auto msg = jni::Object::LocalRef::Adopt(
        env, env->GetObjectField(mMessageQueue.Get(), mMessageQueueMessages));
    // if queue.mMessages is null, queue.next() will block, which we don't
    // want. It turns out to be an order of magnitude more performant to do
    // this extra check here and block less vs. one fewer checks here and
    // more blocking.
    if (!msg) {
      return false;
    }
  }

  auto msg = jni::Object::LocalRef::Adopt(
      env, env->CallObjectMethod(mMessageQueue.Get(), mMessageQueueNext));
  if (!msg) {
    return false;
  }

  return GeckoThread::PumpMessageLoop(msg);
}

NS_IMETHODIMP nsAndroidBridge::GetIsFennec(bool* aIsFennec) {
  *aIsFennec = jni::IsFennec();
  return NS_OK;
}

NS_IMETHODIMP nsAndroidBridge::GetBrowserApp(
    nsIAndroidBrowserApp** aBrowserApp) {
  nsAppShell* const appShell = nsAppShell::Get();
  if (appShell) NS_IF_ADDREF(*aBrowserApp = appShell->GetBrowserApp());
  return NS_OK;
}

NS_IMETHODIMP nsAndroidBridge::SetBrowserApp(
    nsIAndroidBrowserApp* aBrowserApp) {
  nsAppShell* const appShell = nsAppShell::Get();
  if (appShell) appShell->SetBrowserApp(aBrowserApp);
  return NS_OK;
}

extern "C" __attribute__((visibility("default"))) jobject JNICALL
Java_org_mozilla_gecko_GeckoAppShell_allocateDirectBuffer(JNIEnv* env, jclass,
                                                          jlong size);

void AndroidBridge::ContentDocumentChanged(mozIDOMWindowProxy* aWindow) {
  if (RefPtr<nsWindow> widget =
          nsWindow::From(nsPIDOMWindowOuter::From(aWindow))) {
    widget->SetContentDocumentDisplayed(false);
  }
}

bool AndroidBridge::IsContentDocumentDisplayed(mozIDOMWindowProxy* aWindow) {
  if (RefPtr<nsWindow> widget =
          nsWindow::From(nsPIDOMWindowOuter::From(aWindow))) {
    return widget->IsContentDocumentDisplayed();
  }
  return false;
}

jni::Object::LocalRef AndroidBridge::ChannelCreate(jni::Object::Param stream) {
  JNIEnv* const env = jni::GetEnvForThread();
  auto rv = jni::Object::LocalRef::Adopt(
      env, env->CallStaticObjectMethod(sBridge->jChannels,
                                       sBridge->jChannelCreate, stream.Get()));
  MOZ_CATCH_JNI_EXCEPTION(env);
  return rv;
}

void AndroidBridge::InputStreamClose(jni::Object::Param obj) {
  JNIEnv* const env = jni::GetEnvForThread();
  env->CallVoidMethod(obj.Get(), sBridge->jClose);
  MOZ_CATCH_JNI_EXCEPTION(env);
}

uint32_t AndroidBridge::InputStreamAvailable(jni::Object::Param obj) {
  JNIEnv* const env = jni::GetEnvForThread();
  auto rv = env->CallIntMethod(obj.Get(), sBridge->jAvailable);
  MOZ_CATCH_JNI_EXCEPTION(env);
  return rv;
}

nsresult AndroidBridge::InputStreamRead(jni::Object::Param obj, char* aBuf,
                                        uint32_t aCount, uint32_t* aRead) {
  JNIEnv* const env = jni::GetEnvForThread();
  auto arr = jni::ByteBuffer::New(aBuf, aCount);
  jint read =
      env->CallIntMethod(obj.Get(), sBridge->jByteBufferRead, arr.Get());

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
