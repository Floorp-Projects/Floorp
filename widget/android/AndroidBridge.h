/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AndroidBridge_h__
#define AndroidBridge_h__

#include <jni.h>
#include <android/log.h>
#include <cstdlib>
#include <pthread.h>

#include "APKOpen.h"

#include "nsCOMPtr.h"
#include "nsCOMArray.h"

#include "GeneratedJNIWrappers.h"

#include "nsIMutableArray.h"
#include "nsIMIMEInfo.h"
#include "nsColor.h"
#include "gfxRect.h"

#include "nsIAndroidBridge.h"
#include "nsIDOMDOMCursor.h"

#include "mozilla/Likely.h"
#include "mozilla/Mutex.h"
#include "mozilla/Types.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/jni/Utils.h"
#include "nsIObserver.h"
#include "nsDataHashtable.h"

#include "Units.h"

// Some debug #defines
// #define DEBUG_ANDROID_EVENTS
// #define DEBUG_ANDROID_WIDGET

class nsPIDOMWindowOuter;

typedef void* EGLSurface;
class nsIRunnable;

namespace mozilla {

class AutoLocalJNIFrame;

namespace hal {
class BatteryInformation;
class NetworkInformation;
} // namespace hal

// The order and number of the members in this structure must correspond
// to the attrsAppearance array in GeckoAppShell.getSystemColors()
typedef struct AndroidSystemColors {
    nscolor textColorPrimary;
    nscolor textColorPrimaryInverse;
    nscolor textColorSecondary;
    nscolor textColorSecondaryInverse;
    nscolor textColorTertiary;
    nscolor textColorTertiaryInverse;
    nscolor textColorHighlight;
    nscolor colorForeground;
    nscolor colorBackground;
    nscolor panelColorForeground;
    nscolor panelColorBackground;
} AndroidSystemColors;

class MessageCursorContinueCallback : public nsICursorContinueCallback
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSICURSORCONTINUECALLBACK

    MessageCursorContinueCallback(int aRequestId)
        : mRequestId(aRequestId)
    {
    }
private:
    virtual ~MessageCursorContinueCallback()
    {
    }

    int mRequestId;
};

class AndroidBridge final
{
public:
    enum {
        // Values for NotifyIME, in addition to values from the Gecko
        // IMEMessage enum; use negative values here to prevent conflict
        NOTIFY_IME_OPEN_VKB = -2,
        NOTIFY_IME_REPLY_EVENT = -1,
    };

    enum {
        LAYER_CLIENT_TYPE_NONE = 0,
        LAYER_CLIENT_TYPE_GL = 2            // AndroidGeckoGLLayerClient
    };

    static bool IsJavaUiThread() {
        return pthread_equal(pthread_self(), ::getJavaUiThread());
    }

    static void ConstructBridge();
    static void DeconstructBridge();

    static AndroidBridge *Bridge() {
        return sBridge;
    }

    void ContentDocumentChanged(mozIDOMWindowProxy* aDOMWindow);
    bool IsContentDocumentDisplayed(mozIDOMWindowProxy* aDOMWindow);

    bool GetHandlersForURL(const nsAString& aURL,
                           nsIMutableArray* handlersArray = nullptr,
                           nsIHandlerApp **aDefaultApp = nullptr,
                           const nsAString& aAction = EmptyString());

    bool GetHandlersForMimeType(const nsAString& aMimeType,
                                nsIMutableArray* handlersArray = nullptr,
                                nsIHandlerApp **aDefaultApp = nullptr,
                                const nsAString& aAction = EmptyString());

    bool GetHWEncoderCapability();
    bool GetHWDecoderCapability();

    void GetMimeTypeFromExtensions(const nsACString& aFileExt, nsCString& aMimeType);
    void GetExtensionFromMimeType(const nsACString& aMimeType, nsACString& aFileExt);

    bool GetClipboardText(nsAString& aText);

    int GetDPI();
    int GetScreenDepth();

    void Vibrate(const nsTArray<uint32_t>& aPattern);

    void GetSystemColors(AndroidSystemColors *aColors);

    void GetIconForExtension(const nsACString& aFileExt, uint32_t aIconSize, uint8_t * const aBuf);

    bool GetStaticStringField(const char *classID, const char *field, nsAString &result, JNIEnv* env = nullptr);

    bool GetStaticIntField(const char *className, const char *fieldName, int32_t* aInt, JNIEnv* env = nullptr);

    // Returns a global reference to the Context for Fennec's Activity. The
    // caller is responsible for ensuring this doesn't leak by calling
    // DeleteGlobalRef() when the context is no longer needed.
    jobject GetGlobalContextRef(void);

    void GetCurrentBatteryInformation(hal::BatteryInformation* aBatteryInfo);

    void GetCurrentNetworkInformation(hal::NetworkInformation* aNetworkInfo);

    // These methods don't use a ScreenOrientation because it's an
    // enum and that would require including the header which requires
    // include IPC headers which requires including basictypes.h which
    // requires a lot of changes...
    uint32_t GetScreenOrientation();
    uint16_t GetScreenAngle();

    int GetAPIVersion() { return mAPIVersion; }

    nsresult GetProxyForURI(const nsACString & aSpec,
                            const nsACString & aScheme,
                            const nsACString & aHost,
                            const int32_t      aPort,
                            nsACString & aResult);

    bool PumpMessageLoop();

    // Utility methods.
    static jstring NewJavaString(JNIEnv* env, const char16_t* string, uint32_t len);
    static jstring NewJavaString(JNIEnv* env, const nsAString& string);
    static jstring NewJavaString(JNIEnv* env, const char* string);
    static jstring NewJavaString(JNIEnv* env, const nsACString& string);

    static jstring NewJavaString(AutoLocalJNIFrame* frame, const char16_t* string, uint32_t len);
    static jstring NewJavaString(AutoLocalJNIFrame* frame, const nsAString& string);
    static jstring NewJavaString(AutoLocalJNIFrame* frame, const char* string);
    static jstring NewJavaString(AutoLocalJNIFrame* frame, const nsACString& string);

    static jfieldID GetFieldID(JNIEnv* env, jclass jClass, const char* fieldName, const char* fieldType);
    static jfieldID GetStaticFieldID(JNIEnv* env, jclass jClass, const char* fieldName, const char* fieldType);
    static jmethodID GetMethodID(JNIEnv* env, jclass jClass, const char* methodName, const char* methodType);
    static jmethodID GetStaticMethodID(JNIEnv* env, jclass jClass, const char* methodName, const char* methodType);

    static jni::Object::LocalRef ChannelCreate(jni::Object::Param);

    static void InputStreamClose(jni::Object::Param obj);
    static uint32_t InputStreamAvailable(jni::Object::Param obj);
    static nsresult InputStreamRead(jni::Object::Param obj, char *aBuf, uint32_t aCount, uint32_t *aRead);

protected:
    static nsDataHashtable<nsStringHashKey, nsString> sStoragePaths;

    static AndroidBridge* sBridge;

    AndroidBridge();
    ~AndroidBridge();

    int mAPIVersion;

    // intput stream
    jclass jReadableByteChannel;
    jclass jChannels;
    jmethodID jChannelCreate;
    jmethodID jByteBufferRead;

    jclass jInputStream;
    jmethodID jClose;
    jmethodID jAvailable;

    jmethodID jCalculateLength;

    // some convinient types to have around
    jclass jStringClass;

    jni::Object::GlobalRef mMessageQueue;
    jfieldID mMessageQueueMessages;
    jmethodID mMessageQueueNext;

private:
    class DelayedTask;
    nsTArray<DelayedTask> mUiTaskQueue;
    mozilla::Mutex mUiTaskQueueLock;

public:
    void PostTaskToUiThread(already_AddRefed<nsIRunnable> aTask, int aDelayMs);
    int64_t RunDelayedUiThreadTasks();
};

class AutoJNIClass {
private:
    JNIEnv* const mEnv;
    const jclass mClass;

public:
    AutoJNIClass(JNIEnv* jEnv, const char* name)
        : mEnv(jEnv)
        , mClass(jni::GetClassRef(jEnv, name))
    {}

    ~AutoJNIClass() {
        mEnv->DeleteLocalRef(mClass);
    }

    jclass getRawRef() const {
        return mClass;
    }

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
    AutoJObject(JNIEnv* aJNIEnv = nullptr) : mObject(nullptr)
    {
        mJNIEnv = aJNIEnv ? aJNIEnv : jni::GetGeckoThreadEnv();
    }

    AutoJObject(JNIEnv* aJNIEnv, jobject aObject)
    {
        mJNIEnv = aJNIEnv ? aJNIEnv : jni::GetGeckoThreadEnv();
        mObject = aObject;
    }

    ~AutoJObject() {
        if (mObject)
            mJNIEnv->DeleteLocalRef(mObject);
    }

    jobject operator=(jobject aObject)
    {
        if (mObject) {
            mJNIEnv->DeleteLocalRef(mObject);
        }
        return mObject = aObject;
    }

    operator jobject() {
        return mObject;
    }
private:
    JNIEnv* mJNIEnv;
    jobject mObject;
};

class AutoLocalJNIFrame {
public:
    AutoLocalJNIFrame(int nEntries = 15)
        : mEntries(nEntries)
        , mJNIEnv(jni::GetGeckoThreadEnv())
        , mHasFrameBeenPushed(false)
    {
        MOZ_ASSERT(mJNIEnv);
        Push();
    }

    AutoLocalJNIFrame(JNIEnv* aJNIEnv, int nEntries = 15)
        : mEntries(nEntries)
        , mJNIEnv(aJNIEnv ? aJNIEnv : jni::GetGeckoThreadEnv())
        , mHasFrameBeenPushed(false)
    {
        MOZ_ASSERT(mJNIEnv);
        Push();
    }

    ~AutoLocalJNIFrame() {
        if (mHasFrameBeenPushed) {
            Pop();
        }
    }

    JNIEnv* GetEnv() {
        return mJNIEnv;
    }

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

}

#define NS_ANDROIDBRIDGE_CID \
{ 0x0FE2321D, 0xEBD9, 0x467D, \
    { 0xA7, 0x43, 0x03, 0xA6, 0x8D, 0x40, 0x59, 0x9E } }

class nsAndroidBridge final : public nsIAndroidBridge,
                              public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIANDROIDBRIDGE
  NS_DECL_NSIOBSERVER

  NS_FORWARD_SAFE_NSIANDROIDEVENTDISPATCHER(mEventDispatcher)

  nsAndroidBridge();

private:
  ~nsAndroidBridge();

  void AddObservers();
  void RemoveObservers();

  void UpdateAudioPlayingWindows(bool aPlaying);

  int32_t mAudibleWindowsNum;
  nsCOMPtr<nsIAndroidEventDispatcher> mEventDispatcher;

protected:
};

#endif /* AndroidBridge_h__ */
