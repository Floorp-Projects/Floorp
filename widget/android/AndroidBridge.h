/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Android code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef AndroidBridge_h__
#define AndroidBridge_h__

#include <jni.h>
#include <android/log.h>
#include <cstdlib>
#include <pthread.h>

#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "nsIRunnable.h"
#include "nsIObserver.h"
#include "nsThreadUtils.h"

#include "AndroidFlexViewWrapper.h"
#include "AndroidJavaWrappers.h"

#include "nsIMutableArray.h"
#include "nsIMIMEInfo.h"
#include "nsColor.h"
#include "BasicLayers.h"
#include "gfxRect.h"

#include "nsIAndroidBridge.h"

// Some debug #defines
// #define DEBUG_ANDROID_EVENTS
// #define DEBUG_ANDROID_WIDGET

class nsWindow;
class nsIDOMMozSmsMessage;

/* See the comment in AndroidBridge about this function before using it */
extern "C" JNIEnv * GetJNIForThread();

extern bool mozilla_AndroidBridge_SetMainThread(void *);
extern jclass GetGeckoAppShellClass();

namespace base {
class Thread;
} // end namespace base

namespace mozilla {

namespace hal {
class BatteryInformation;
class NetworkInformation;
} // namespace hal

namespace dom {
namespace sms {
struct SmsFilterData;
} // namespace sms
} // namespace dom

namespace layers {
class CompositorParent;
} // namespace layers

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

class AndroidBridge
{
public:
    enum {
        NOTIFY_IME_RESETINPUTSTATE = 0,
        NOTIFY_IME_SETOPENSTATE = 1,
        NOTIFY_IME_CANCELCOMPOSITION = 2,
        NOTIFY_IME_FOCUSCHANGE = 3
    };

    enum {
        LAYER_CLIENT_TYPE_NONE = 0,
        LAYER_CLIENT_TYPE_GL = 2            // AndroidGeckoGLLayerClient
    };

    static AndroidBridge *ConstructBridge(JNIEnv *jEnv,
                                          jclass jGeckoAppShellClass);

    static AndroidBridge *Bridge() {
        return sBridge;
    }

    static JavaVM *GetVM() {
        if (NS_LIKELY(sBridge))
            return sBridge->mJavaVM;
        return nsnull;
    }

    static JNIEnv *GetJNIEnv() {
        if (NS_LIKELY(sBridge)) {
            if ((void*)pthread_self() != sBridge->mThread) {
                __android_log_print(ANDROID_LOG_INFO, "AndroidBridge",
                                    "###!!!!!!! Something's grabbing the JNIEnv from the wrong thread! (thr %p should be %p)",
                                    (void*)pthread_self(), (void*)sBridge->mThread);
                return nsnull;
            }
            return sBridge->mJNIEnv;

        }
        return nsnull;
    }
    
    static jclass GetGeckoAppShellClass() {
        return sBridge->mGeckoAppShellClass;
    }

    // The bridge needs to be constructed via ConstructBridge first,
    // and then once the Gecko main thread is spun up (Gecko side),
    // SetMainThread should be called which will create the JNIEnv for
    // us to use.  toolkit/xre/nsAndroidStartup.cpp calls
    // SetMainThread.
    bool SetMainThread(void *thr);

    /* These are all implemented in Java */
    static void NotifyIME(int aType, int aState);

    static void NotifyIMEEnabled(int aState, const nsAString& aTypeHint,
                                 const nsAString& aActionHint);

    static void NotifyIMEChange(const PRUnichar *aText, PRUint32 aTextLen, int aStart, int aEnd, int aNewEnd);

    nsresult TakeScreenshot(nsIDOMWindow *window, PRInt32 srcX, PRInt32 srcY, PRInt32 srcW, PRInt32 srcH, PRInt32 dstW, PRInt32 dstH, PRInt32 tabId, float scale);

    void AcknowledgeEventSync();

    void EnableDeviceMotion(bool aEnable);

    void EnableLocation(bool aEnable);

    void EnableSensor(int aSensorType);

    void DisableSensor(int aSensorType);

    void ReturnIMEQueryResult(const PRUnichar *aResult, PRUint32 aLen, int aSelStart, int aSelLen);

    void NotifyXreExit();

    void ScheduleRestart();

    void SetLayerClient(jobject jobj);
    AndroidGeckoLayerClient &GetLayerClient() { return *mLayerClient; }

    void SetSurfaceView(jobject jobj);
    AndroidGeckoSurfaceView& SurfaceView() { return mSurfaceView; }

    bool GetHandlersForURL(const char *aURL, 
                             nsIMutableArray* handlersArray = nsnull,
                             nsIHandlerApp **aDefaultApp = nsnull,
                             const nsAString& aAction = EmptyString());

    bool GetHandlersForMimeType(const char *aMimeType,
                                  nsIMutableArray* handlersArray = nsnull,
                                  nsIHandlerApp **aDefaultApp = nsnull,
                                  const nsAString& aAction = EmptyString());

    bool OpenUriExternal(const nsACString& aUriSpec, const nsACString& aMimeType,
                           const nsAString& aPackageName = EmptyString(),
                           const nsAString& aClassName = EmptyString(),
                           const nsAString& aAction = EmptyString(),
                           const nsAString& aTitle = EmptyString());

    void GetMimeTypeFromExtensions(const nsACString& aFileExt, nsCString& aMimeType);
    void GetExtensionFromMimeType(const nsACString& aMimeType, nsACString& aFileExt);

    void MoveTaskToBack();

    bool GetClipboardText(nsAString& aText);

    void SetClipboardText(const nsAString& aText);
    
    void EmptyClipboard();

    bool ClipboardHasText();

    void ShowAlertNotification(const nsAString& aImageUrl,
                               const nsAString& aAlertTitle,
                               const nsAString& aAlertText,
                               const nsAString& aAlertData,
                               nsIObserver *aAlertListener,
                               const nsAString& aAlertName);

    void AlertsProgressListener_OnProgress(const nsAString& aAlertName,
                                           PRInt64 aProgress,
                                           PRInt64 aProgressMax,
                                           const nsAString& aAlertText);

    void AlertsProgressListener_OnCancel(const nsAString& aAlertName);

    int GetDPI();

    void ShowFilePickerForExtensions(nsAString& aFilePath, const nsAString& aExtensions);
    void ShowFilePickerForMimeType(nsAString& aFilePath, const nsAString& aMimeType);

    void PerformHapticFeedback(bool aIsLongPress);

    void Vibrate(const nsTArray<PRUint32>& aPattern);
    void CancelVibrate();

    void SetFullScreen(bool aFullScreen);

    void ShowInputMethodPicker();

    void SetPreventPanning(bool aPreventPanning);

    void HideProgressDialogOnce();

    bool IsNetworkLinkUp();

    bool IsNetworkLinkKnown();

    void SetSelectedLocale(const nsAString&);

    void GetSystemColors(AndroidSystemColors *aColors);

    void GetIconForExtension(const nsACString& aFileExt, PRUint32 aIconSize, PRUint8 * const aBuf);

    bool GetShowPasswordSetting();

    void FireAndWaitForTracerEvent();

    bool GetAccessibilityEnabled();

    class AutoLocalJNIFrame {
    public:
        AutoLocalJNIFrame(int nEntries = 128)
            : mEntries(nEntries)
        {
            mJNIEnv = AndroidBridge::GetJNIEnv();
            Push();
        }

        AutoLocalJNIFrame(JNIEnv* aJNIEnv, int nEntries = 128)
            : mEntries(nEntries)
        {
            mJNIEnv = aJNIEnv ? aJNIEnv : AndroidBridge::GetJNIEnv();

            Push();
        }

        // Note! Calling Purge makes all previous local refs created in
        // the AutoLocalJNIFrame's scope INVALID; be sure that you locked down
        // any local refs that you need to keep around in global refs!
        void Purge() {
            if (mJNIEnv) {
                mJNIEnv->PopLocalFrame(NULL);
                Push();
            }
        }

        ~AutoLocalJNIFrame() {
            if (!mJNIEnv)
                return;

            jthrowable exception = mJNIEnv->ExceptionOccurred();
            if (exception) {
                mJNIEnv->ExceptionDescribe();
                mJNIEnv->ExceptionClear();
            }

            mJNIEnv->PopLocalFrame(NULL);
        }

    private:
        void Push() {
            if (!mJNIEnv)
                return;

            // Make sure there is enough space to store a local ref to the
            // exception.  I am not completely sure this is needed, but does
            // not hurt.
            mJNIEnv->PushLocalFrame(mEntries + 1);
        }

        int mEntries;
        JNIEnv* mJNIEnv;
    };

    /* See GLHelpers.java as to why this is needed */
    void *CallEglCreateWindowSurface(void *dpy, void *config, AndroidGeckoSurfaceView& surfaceView);

    // Switch Java to composite with the Gecko Compositor thread
    void RegisterCompositor();
    EGLSurface ProvideEGLSurface();

    bool GetStaticStringField(const char *classID, const char *field, nsAString &result);

    bool GetStaticIntField(const char *className, const char *fieldName, PRInt32* aInt);

    void SetKeepScreenOn(bool on);

    void ScanMedia(const nsAString& aFile, const nsACString& aMimeType);

    void CreateShortcut(const nsAString& aTitle, const nsAString& aURI, const nsAString& aIconData, const nsAString& aIntent);

    // These next four functions are for native Bitmap access in Android 2.2+
    bool HasNativeBitmapAccess();

    bool ValidateBitmap(jobject bitmap, int width, int height);

    void *LockBitmap(jobject bitmap);

    void UnlockBitmap(jobject bitmap);

    void PostToJavaThread(JNIEnv *aEnv, nsIRunnable* aRunnable, bool aMainThread = false);

    void ExecuteNextRunnable(JNIEnv *aEnv);

    /* Copied from Android's native_window.h in newer (platform 9) NDK */
    enum {
        WINDOW_FORMAT_RGBA_8888          = 1,
        WINDOW_FORMAT_RGBX_8888          = 2,
        WINDOW_FORMAT_RGB_565            = 4,
    };

    bool HasNativeWindowAccess();

    void *AcquireNativeWindow(jobject surface);
    void ReleaseNativeWindow(void *window);
    bool SetNativeWindowFormat(void *window, int width, int height, int format);

    bool LockWindow(void *window, unsigned char **bits, int *width, int *height, int *format, int *stride);
    bool UnlockWindow(void *window);
    
    void HandleGeckoMessage(const nsAString& message, nsAString &aRet);

    nsCOMPtr<nsIAndroidDrawMetadataProvider> GetDrawMetadataProvider();

    void EmitGeckoAccessibilityEvent (PRInt32 eventType, const nsTArray<nsString>& text, const nsAString& description, bool enabled, bool checked, bool password);

    void CheckURIVisited(const nsAString& uri);
    void MarkURIVisited(const nsAString& uri);

    bool InitCamera(const nsCString& contentType, PRUint32 camera, PRUint32 *width, PRUint32 *height, PRUint32 *fps);

    void CloseCamera();

    void EnableBatteryNotifications();
    void DisableBatteryNotifications();
    void GetCurrentBatteryInformation(hal::BatteryInformation* aBatteryInfo);

    PRUint16 GetNumberOfMessagesForText(const nsAString& aText);
    void SendMessage(const nsAString& aNumber, const nsAString& aText, PRInt32 aRequestId, PRUint64 aProcessId);
    PRInt32 SaveSentMessage(const nsAString& aRecipient, const nsAString& aBody, PRUint64 aDate);
    void GetMessage(PRInt32 aMessageId, PRInt32 aRequestId, PRUint64 aProcessId);
    void DeleteMessage(PRInt32 aMessageId, PRInt32 aRequestId, PRUint64 aProcessId);
    void CreateMessageList(const dom::sms::SmsFilterData& aFilter, bool aReverse, PRInt32 aRequestId, PRUint64 aProcessId);
    void GetNextMessageInList(PRInt32 aListId, PRInt32 aRequestId, PRUint64 aProcessId);
    void ClearMessageList(PRInt32 aListId);

    bool IsTablet();

    void GetCurrentNetworkInformation(hal::NetworkInformation* aNetworkInfo);
    void EnableNetworkNotifications();
    void DisableNetworkNotifications();

    void SetCompositorParent(mozilla::layers::CompositorParent* aCompositorParent,
                             base::Thread* aCompositorThread);
    void SetFirstPaintViewport(float aOffsetX, float aOffsetY, float aZoom, float aPageWidth, float aPageHeight);
    void SetPageSize(float aZoom, float aPageWidth, float aPageHeight);
    void SyncViewportInfo(const nsIntRect& aDisplayPort, nsIntPoint& aScrollOffset, float& aScaleX, float& aScaleY);

    jobject CreateSurface();
    void DestroySurface(jobject surface);
    void ShowSurface(jobject surface, const gfxRect& aRect, bool aInverted, bool aBlend);
    void HideSurface(jobject surface);

protected:
    static AndroidBridge *sBridge;

    // the global JavaVM
    JavaVM *mJavaVM;

    // the JNIEnv for the main thread
    JNIEnv *mJNIEnv;
    void *mThread;

    // the GeckoSurfaceView
    AndroidGeckoSurfaceView mSurfaceView;

    AndroidGeckoLayerClient *mLayerClient;

    // the GeckoAppShell java class
    jclass mGeckoAppShellClass;

    AndroidBridge();
    ~AndroidBridge();

    bool Init(JNIEnv *jEnv, jclass jGeckoApp);

    bool mOpenedGraphicsLibraries;
    void OpenGraphicsLibraries();

    bool mHasNativeBitmapAccess;
    bool mHasNativeWindowAccess;

    nsCOMArray<nsIRunnable> mRunnableQueue;

    // other things
    jmethodID jNotifyIME;
    jmethodID jNotifyIMEEnabled;
    jmethodID jNotifyIMEChange;
    jmethodID jNotifyScreenShot;
    jmethodID jAcknowledgeEventSync;
    jmethodID jEnableLocation;
    jmethodID jEnableSensor;
    jmethodID jDisableSensor;
    jmethodID jReturnIMEQueryResult;
    jmethodID jNotifyAppShellReady;
    jmethodID jNotifyXreExit;
    jmethodID jScheduleRestart;
    jmethodID jGetOutstandingDrawEvents;
    jmethodID jGetHandlersForMimeType;
    jmethodID jGetHandlersForURL;
    jmethodID jOpenUriExternal;
    jmethodID jGetMimeTypeFromExtensions;
    jmethodID jGetExtensionFromMimeType;
    jmethodID jMoveTaskToBack;
    jmethodID jGetClipboardText;
    jmethodID jSetClipboardText;
    jmethodID jShowAlertNotification;
    jmethodID jShowFilePickerForExtensions;
    jmethodID jShowFilePickerForMimeType;
    jmethodID jAlertsProgressListener_OnProgress;
    jmethodID jAlertsProgressListener_OnCancel;
    jmethodID jGetDpi;
    jmethodID jSetFullScreen;
    jmethodID jShowInputMethodPicker;
    jmethodID jSetPreventPanning;
    jmethodID jHideProgressDialog;
    jmethodID jPerformHapticFeedback;
    jmethodID jVibrate1;
    jmethodID jVibrateA;
    jmethodID jCancelVibrate;
    jmethodID jSetKeepScreenOn;
    jmethodID jIsNetworkLinkUp;
    jmethodID jIsNetworkLinkKnown;
    jmethodID jSetSelectedLocale;
    jmethodID jScanMedia;
    jmethodID jGetSystemColors;
    jmethodID jGetIconForExtension;
    jmethodID jFireAndWaitForTracerEvent;
    jmethodID jCreateShortcut;
    jmethodID jGetShowPasswordSetting;
    jmethodID jPostToJavaThread;
    jmethodID jInitCamera;
    jmethodID jCloseCamera;
    jmethodID jIsTablet;
    jmethodID jEnableBatteryNotifications;
    jmethodID jDisableBatteryNotifications;
    jmethodID jGetCurrentBatteryInformation;
    jmethodID jGetAccessibilityEnabled;
    jmethodID jHandleGeckoMessage;
    jmethodID jCheckUriVisited;
    jmethodID jMarkUriVisited;
    jmethodID jEmitGeckoAccessibilityEvent;

    jmethodID jNumberOfMessages;
    jmethodID jSendMessage;
    jmethodID jSaveSentMessage;
    jmethodID jGetMessage;
    jmethodID jDeleteMessage;
    jmethodID jCreateMessageList;
    jmethodID jGetNextMessageinList;
    jmethodID jClearMessageList;

    jmethodID jGetCurrentNetworkInformation;
    jmethodID jEnableNetworkNotifications;
    jmethodID jDisableNetworkNotifications;

    // stuff we need for CallEglCreateWindowSurface
    jclass jEGLSurfaceImplClass;
    jclass jEGLContextImplClass;
    jclass jEGLConfigImplClass;
    jclass jEGLDisplayImplClass;
    jclass jEGLContextClass;
    jclass jEGL10Class;

    jclass jFlexSurfaceView;
    jmethodID jRegisterCompositorMethod;

    // some convinient types to have around
    jclass jStringClass;

    // calls we've dlopened from libjnigraphics.so
    int (* AndroidBitmap_getInfo)(JNIEnv *env, jobject bitmap, void *info);
    int (* AndroidBitmap_lockPixels)(JNIEnv *env, jobject bitmap, void **buffer);
    int (* AndroidBitmap_unlockPixels)(JNIEnv *env, jobject bitmap);

    void* (*ANativeWindow_fromSurface)(JNIEnv *env, jobject surface);
    void (*ANativeWindow_release)(void *window);
    int (*ANativeWindow_setBuffersGeometry)(void *window, int width, int height, int format);

    int (* ANativeWindow_lock)(void *window, void *outBuffer, void *inOutDirtyBounds);
    int (* ANativeWindow_unlockAndPost)(void *window);
};

}

#define NS_ANDROIDBRIDGE_CID \
{ 0x0FE2321D, 0xEBD9, 0x467D, \
    { 0xA7, 0x43, 0x03, 0xA6, 0x8D, 0x40, 0x59, 0x9E } }

class nsAndroidBridge : public nsIAndroidBridge
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIANDROIDBRIDGE

  nsAndroidBridge();

private:
  ~nsAndroidBridge();

protected:
};


#endif /* AndroidBridge_h__ */
