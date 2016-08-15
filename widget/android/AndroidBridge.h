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
#include "AndroidJavaWrappers.h"

#include "nsIMutableArray.h"
#include "nsIMIMEInfo.h"
#include "nsColor.h"
#include "gfxRect.h"

#include "nsIAndroidBridge.h"
#include "nsIMobileMessageCallback.h"
#include "nsIMobileMessageCursorCallback.h"
#include "nsIDOMDOMCursor.h"

#include "mozilla/Likely.h"
#include "mozilla/Mutex.h"
#include "mozilla/Types.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/jni/Utils.h"
#include "nsIObserver.h"

// Some debug #defines
// #define DEBUG_ANDROID_EVENTS
// #define DEBUG_ANDROID_WIDGET

class nsIObserver;

namespace base {
class Thread;
} // end namespace base

typedef void* EGLSurface;

namespace mozilla {

class Runnable;

namespace hal {
class BatteryInformation;
class NetworkInformation;
} // namespace hal

namespace dom {
namespace mobilemessage {
class SmsFilterData;
} // namespace mobilemessage
} // namespace dom

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

class ThreadCursorContinueCallback : public nsICursorContinueCallback
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSICURSORCONTINUECALLBACK

    ThreadCursorContinueCallback(int aRequestId)
        : mRequestId(aRequestId)
    {
    }
private:
    virtual ~ThreadCursorContinueCallback()
    {
    }

    int mRequestId;
};

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

    /* These are all implemented in Java */
    bool GetThreadNameJavaProfiling(uint32_t aThreadId, nsCString & aResult);
    bool GetFrameNameJavaProfiling(uint32_t aThreadId, uint32_t aSampleId, uint32_t aFrameId, nsCString & aResult);

    void GetDisplayPort(bool aPageSizeUpdate, bool aIsBrowserContentDisplayed, int32_t tabId, nsIAndroidViewport* metrics, nsIAndroidDisplayport** displayPort);
    void ContentDocumentChanged();
    bool IsContentDocumentDisplayed();

    bool ProgressiveUpdateCallback(bool aHasPendingNewThebesContent, const LayerRect& aDisplayPort, float aDisplayResolution, bool aDrawingCritical,
                                   mozilla::ParentLayerPoint& aScrollOffset, mozilla::CSSToParentLayerScale& aZoom);

    void SetLayerClient(java::GeckoLayerClient::Param jobj);
    const java::GeckoLayerClient::Ref& GetLayerClient() { return mLayerClient; }

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

    void *AcquireNativeWindow(JNIEnv* aEnv, jobject aSurface);
    void ReleaseNativeWindow(void *window);
    mozilla::gfx::IntSize GetNativeWindowSize(void* window);

    void HandleGeckoMessage(JSContext* cx, JS::HandleObject message);

    bool InitCamera(const nsCString& contentType, uint32_t camera, uint32_t *width, uint32_t *height, uint32_t *fps);

    void GetCurrentBatteryInformation(hal::BatteryInformation* aBatteryInfo);

    nsresult GetSegmentInfoForText(const nsAString& aText,
                                   nsIMobileMessageCallback* aRequest);
    void SendMessage(const nsAString& aNumber, const nsAString& aText,
                     nsIMobileMessageCallback* aRequest);
    void GetMessage(int32_t aMessageId, nsIMobileMessageCallback* aRequest);
    void DeleteMessage(int32_t aMessageId, nsIMobileMessageCallback* aRequest);
    void MarkMessageRead(int32_t aMessageId,
                         bool aValue,
                         bool aSendReadReport,
                         nsIMobileMessageCallback* aRequest);
    already_AddRefed<nsICursorContinueCallback>
    CreateMessageCursor(bool aHasStartDate,
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
                        nsIMobileMessageCursorCallback* aRequest);
    already_AddRefed<nsICursorContinueCallback>
    CreateThreadCursor(nsIMobileMessageCursorCallback* aRequest);
    already_AddRefed<nsIMobileMessageCallback> DequeueSmsRequest(uint32_t aRequestId);
    nsCOMPtr<nsIMobileMessageCursorCallback> GetSmsCursorRequest(uint32_t aRequestId);
    already_AddRefed<nsIMobileMessageCursorCallback> DequeueSmsCursorRequest(uint32_t aRequestId);

    void GetCurrentNetworkInformation(hal::NetworkInformation* aNetworkInfo);

    void SetFirstPaintViewport(const LayerIntPoint& aOffset, const CSSToLayerScale& aZoom, const CSSRect& aCssPageRect);
    void SetPageRect(const CSSRect& aCssPageRect);
    void SyncViewportInfo(const LayerIntRect& aDisplayPort, const CSSToLayerScale& aDisplayResolution,
                          bool aLayersUpdated, int32_t aPaintSyncId, ParentLayerRect& aScrollRect, CSSToParentLayerScale& aScale,
                          ScreenMargin& aFixedLayerMargins);
    void SyncFrameMetrics(const ParentLayerPoint& aScrollOffset,
                          const CSSToParentLayerScale& aZoom,
                          const CSSRect& aCssPageRect,
                          const CSSRect& aDisplayPort,
                          const CSSToLayerScale& aPaintedResolution,
                          bool aLayersUpdated, int32_t aPaintSyncId,
                          ScreenMargin& aFixedLayerMargins);

    void AddPluginView(jobject view, const LayoutDeviceRect& rect, bool isFullScreen);

    // These methods don't use a ScreenOrientation because it's an
    // enum and that would require including the header which requires
    // include IPC headers which requires including basictypes.h which
    // requires a lot of changes...
    uint32_t GetScreenOrientation();
    uint16_t GetScreenAngle();

    int GetAPIVersion() { return mAPIVersion; }

    void InvalidateAndScheduleComposite();

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

    static jclass GetClassGlobalRef(JNIEnv* env, const char* className);
    static jfieldID GetFieldID(JNIEnv* env, jclass jClass, const char* fieldName, const char* fieldType);
    static jfieldID GetStaticFieldID(JNIEnv* env, jclass jClass, const char* fieldName, const char* fieldType);
    static jmethodID GetMethodID(JNIEnv* env, jclass jClass, const char* methodName, const char* methodType);
    static jmethodID GetStaticMethodID(JNIEnv* env, jclass jClass, const char* methodName, const char* methodType);

    static jni::Object::LocalRef ChannelCreate(jni::Object::Param);

    static void InputStreamClose(jni::Object::Param obj);
    static uint32_t InputStreamAvailable(jni::Object::Param obj);
    static nsresult InputStreamRead(jni::Object::Param obj, char *aBuf, uint32_t aCount, uint32_t *aRead);

    static nsresult GetExternalPublicDirectory(const nsAString& aType, nsAString& aPath);

protected:
    static nsDataHashtable<nsStringHashKey, nsString> sStoragePaths;

    static AndroidBridge* sBridge;
    nsTArray<nsCOMPtr<nsIMobileMessageCallback>> mSmsRequests;
    nsTArray<nsCOMPtr<nsIMobileMessageCursorCallback>> mSmsCursorRequests;

    java::GeckoLayerClient::GlobalRef mLayerClient;

    // the android.telephony.SmsMessage class
    jclass mAndroidSmsMessageClass;

    AndroidBridge();
    ~AndroidBridge();

    int mAPIVersion;

    bool QueueSmsRequest(nsIMobileMessageCallback* aRequest, uint32_t* aRequestIdOut);
    bool QueueSmsCursorRequest(nsIMobileMessageCursorCallback* aRequest, uint32_t* aRequestIdOut);

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

    jni::Object::GlobalRef mClassLoader;
    jmethodID mClassLoaderLoadClass;

    jni::Object::GlobalRef mMessageQueue;
    jfieldID mMessageQueueMessages;
    jmethodID mMessageQueueNext;

private:
    class DelayedTask;
    nsTArray<DelayedTask> mUiTaskQueue;
    mozilla::Mutex mUiTaskQueueLock;

public:
    void PostTaskToUiThread(already_AddRefed<Runnable> aTask, int aDelayMs);
    int64_t RunDelayedUiThreadTasks();

    void* GetPresentationWindow();
    void SetPresentationWindow(void* aPresentationWindow);

    EGLSurface GetPresentationSurface();
    void SetPresentationSurface(EGLSurface aPresentationSurface);
private:
    void* mPresentationWindow;
    EGLSurface mPresentationSurface;
};

class AutoJNIClass {
private:
    JNIEnv* const mEnv;
    const jclass mClass;

public:
    AutoJNIClass(JNIEnv* jEnv, const char* name)
        : mEnv(jEnv)
        , mClass(AndroidBridge::GetClassGlobalRef(jEnv, name))
    {}

    ~AutoJNIClass() {
        mEnv->DeleteGlobalRef(mClass);
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

  nsAndroidBridge();

private:
  ~nsAndroidBridge();

  void AddObservers();
  void RemoveObservers();

  void UpdateAudioPlayingWindows(nsPIDOMWindowOuter* aWindow, bool aPlaying);

  nsTArray<nsPIDOMWindowOuter*> mAudioPlayingWindows;

protected:
};

#endif /* AndroidBridge_h__ */
