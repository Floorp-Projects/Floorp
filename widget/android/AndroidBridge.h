/* -*- Mode: c++; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AndroidBridge_h__
#define AndroidBridge_h__

#include <unistd.h>  // for gettid

#include "nsCOMPtr.h"

#include "mozilla/jni/Refs.h"

#include "nsIMutableArray.h"
#include "nsIMIMEInfo.h"

#include "nsIAndroidBridge.h"

#include "mozilla/jni/Utils.h"
#include "nsTHashMap.h"

// Some debug #defines
// #define DEBUG_ANDROID_EVENTS
// #define DEBUG_ANDROID_WIDGET

namespace mozilla {

class AutoLocalJNIFrame;

namespace hal {
class BatteryInformation;
class NetworkInformation;
enum class ScreenOrientation : uint32_t;
}  // namespace hal

class AndroidBridge final {
 public:
  static bool IsJavaUiThread() {
    return mozilla::jni::GetUIThreadId() == gettid();
  }

  static void ConstructBridge();
  static void DeconstructBridge();

  static AndroidBridge* Bridge() { return sBridge; }

  bool GetHandlersForURL(const nsAString& aURL,
                         nsIMutableArray* handlersArray = nullptr,
                         nsIHandlerApp** aDefaultApp = nullptr,
                         const nsAString& aAction = u""_ns);

  bool GetHandlersForMimeType(const nsAString& aMimeType,
                              nsIMutableArray* handlersArray = nullptr,
                              nsIHandlerApp** aDefaultApp = nullptr,
                              const nsAString& aAction = u""_ns);

  void GetMimeTypeFromExtensions(const nsACString& aFileExt,
                                 nsCString& aMimeType);
  void GetExtensionFromMimeType(const nsACString& aMimeType,
                                nsACString& aFileExt);

  void Vibrate(const nsTArray<uint32_t>& aPattern);

  void GetIconForExtension(const nsACString& aFileExt, uint32_t aIconSize,
                           uint8_t* const aBuf);

  bool GetStaticStringField(const char* classID, const char* field,
                            nsAString& result, JNIEnv* env = nullptr);

  bool GetStaticIntField(const char* className, const char* fieldName,
                         int32_t* aInt, JNIEnv* env = nullptr);

  // Returns a global reference to the Context for Fennec's Activity. The
  // caller is responsible for ensuring this doesn't leak by calling
  // DeleteGlobalRef() when the context is no longer needed.
  jobject GetGlobalContextRef(void);

  void GetCurrentBatteryInformation(hal::BatteryInformation* aBatteryInfo);

  void GetCurrentNetworkInformation(hal::NetworkInformation* aNetworkInfo);

  hal::ScreenOrientation GetScreenOrientation();
  uint16_t GetScreenAngle();

  nsresult GetProxyForURI(const nsACString& aSpec, const nsACString& aScheme,
                          const nsACString& aHost, const int32_t aPort,
                          nsACString& aResult);

  bool PumpMessageLoop();

  // Utility methods.
  static jfieldID GetFieldID(JNIEnv* env, jclass jClass, const char* fieldName,
                             const char* fieldType);
  static jfieldID GetStaticFieldID(JNIEnv* env, jclass jClass,
                                   const char* fieldName,
                                   const char* fieldType);
  static jmethodID GetMethodID(JNIEnv* env, jclass jClass,
                               const char* methodName, const char* methodType);
  static jmethodID GetStaticMethodID(JNIEnv* env, jclass jClass,
                                     const char* methodName,
                                     const char* methodType);

  static jni::Object::LocalRef ChannelCreate(jni::Object::Param);

  static void InputStreamClose(jni::Object::Param obj);
  static uint32_t InputStreamAvailable(jni::Object::Param obj);
  static nsresult InputStreamRead(jni::Object::Param obj, char* aBuf,
                                  uint32_t aCount, uint32_t* aRead);

 protected:
  static nsTHashMap<nsStringHashKey, nsString> sStoragePaths;

  static AndroidBridge* sBridge;

  AndroidBridge();
  ~AndroidBridge();

  jni::Object::GlobalRef mMessageQueue;
  jfieldID mMessageQueueMessages;
  jmethodID mMessageQueueNext;
};

class AutoJNIClass {
 private:
  JNIEnv* const mEnv;
  const jclass mClass;

 public:
  AutoJNIClass(JNIEnv* jEnv, const char* name)
      : mEnv(jEnv), mClass(jni::GetClassRef(jEnv, name)) {}

  ~AutoJNIClass() { mEnv->DeleteLocalRef(mClass); }

  jclass getRawRef() const { return mClass; }

  jclass getGlobalRef() const {
    return static_cast<jclass>(mEnv->NewGlobalRef(mClass));
  }

  jfieldID getField(const char* name, const char* type) const {
    return AndroidBridge::GetFieldID(mEnv, mClass, name, type);
  }

  jfieldID getStaticField(const char* name, const char* type) const {
    return AndroidBridge::GetStaticFieldID(mEnv, mClass, name, type);
  }

  jmethodID getMethod(const char* name, const char* type) const {
    return AndroidBridge::GetMethodID(mEnv, mClass, name, type);
  }

  jmethodID getStaticMethod(const char* name, const char* type) const {
    return AndroidBridge::GetStaticMethodID(mEnv, mClass, name, type);
  }
};

class AutoJObject {
 public:
  explicit AutoJObject(JNIEnv* aJNIEnv = nullptr) : mObject(nullptr) {
    mJNIEnv = aJNIEnv ? aJNIEnv : jni::GetGeckoThreadEnv();
  }

  AutoJObject(JNIEnv* aJNIEnv, jobject aObject) {
    mJNIEnv = aJNIEnv ? aJNIEnv : jni::GetGeckoThreadEnv();
    mObject = aObject;
  }

  ~AutoJObject() {
    if (mObject) mJNIEnv->DeleteLocalRef(mObject);
  }

  jobject operator=(jobject aObject) {
    if (mObject) {
      mJNIEnv->DeleteLocalRef(mObject);
    }
    return mObject = aObject;
  }

  operator jobject() { return mObject; }

 private:
  JNIEnv* mJNIEnv;
  jobject mObject;
};

class AutoLocalJNIFrame {
 public:
  explicit AutoLocalJNIFrame(int nEntries = 15)
      : mEntries(nEntries),
        mJNIEnv(jni::GetGeckoThreadEnv()),
        mHasFrameBeenPushed(false) {
    MOZ_ASSERT(mJNIEnv);
    Push();
  }

  explicit AutoLocalJNIFrame(JNIEnv* aJNIEnv, int nEntries = 15)
      : mEntries(nEntries),
        mJNIEnv(aJNIEnv ? aJNIEnv : jni::GetGeckoThreadEnv()),
        mHasFrameBeenPushed(false) {
    MOZ_ASSERT(mJNIEnv);
    Push();
  }

  ~AutoLocalJNIFrame() {
    if (mHasFrameBeenPushed) {
      Pop();
    }
  }

  JNIEnv* GetEnv() { return mJNIEnv; }

  bool CheckForException() {
    if (mJNIEnv->ExceptionCheck()) {
      MOZ_CATCH_JNI_EXCEPTION(mJNIEnv);
      return true;
    }
    return false;
  }

  // Note! Calling Purge makes all previous local refs created in
  // the AutoLocalJNIFrame's scope INVALID; be sure that you locked down
  // any local refs that you need to keep around in global refs!
  void Purge() {
    Pop();
    Push();
  }

  template <typename ReturnType = jobject>
  ReturnType Pop(ReturnType aResult = nullptr) {
    MOZ_ASSERT(mHasFrameBeenPushed);
    mHasFrameBeenPushed = false;
    return static_cast<ReturnType>(
        mJNIEnv->PopLocalFrame(static_cast<jobject>(aResult)));
  }

 private:
  void Push() {
    MOZ_ASSERT(!mHasFrameBeenPushed);
    // Make sure there is enough space to store a local ref to the
    // exception.  I am not completely sure this is needed, but does
    // not hurt.
    if (mJNIEnv->PushLocalFrame(mEntries + 1) != 0) {
      CheckForException();
      return;
    }
    mHasFrameBeenPushed = true;
  }

  const int mEntries;
  JNIEnv* const mJNIEnv;
  bool mHasFrameBeenPushed;
};

}  // namespace mozilla

#define NS_ANDROIDBRIDGE_CID                         \
  {                                                  \
    0x0FE2321D, 0xEBD9, 0x467D, {                    \
      0xA7, 0x43, 0x03, 0xA6, 0x8D, 0x40, 0x59, 0x9E \
    }                                                \
  }

class nsAndroidBridge final : public nsIAndroidBridge {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIANDROIDBRIDGE

  NS_FORWARD_SAFE_NSIANDROIDEVENTDISPATCHER(mEventDispatcher)

  nsAndroidBridge();

 private:
  ~nsAndroidBridge();

  nsCOMPtr<nsIAndroidEventDispatcher> mEventDispatcher;

 protected:
};

#endif /* AndroidBridge_h__ */
