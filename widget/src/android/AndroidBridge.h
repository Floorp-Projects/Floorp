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

#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "nsIRunnable.h"
#include "nsIObserver.h"

#include "AndroidJavaWrappers.h"

#include "nsIMutableArray.h"
#include "nsIMIMEInfo.h"
#include "nsColor.h"

#include "nsIAndroidBridge.h"

// Some debug #defines
// #define DEBUG_ANDROID_EVENTS
// #define DEBUG_ANDROID_WIDGET

class nsWindow;

namespace mozilla {

namespace hal {
class BatteryInformation;
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

class AndroidBridge
{
public:
    enum {
        NOTIFY_IME_RESETINPUTSTATE = 0,
        NOTIFY_IME_SETOPENSTATE = 1,
        NOTIFY_IME_CANCELCOMPOSITION = 2,
        NOTIFY_IME_FOCUSCHANGE = 3
    };

    static AndroidBridge *ConstructBridge(JNIEnv *jEnv,
                                          jclass jGeckoAppShellClass);

    static AndroidBridge *Bridge() {
        return sBridge;
    }

    static JavaVM *VM() {
        return sBridge->mJavaVM;
    }

    static JNIEnv *JNI() {
        sBridge->EnsureJNIThread();
        return sBridge->mJNIEnv;
    }

    static JNIEnv *JNIForThread() {
        if (NS_LIKELY(sBridge))
          return sBridge->AttachThread();
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

    JNIEnv* AttachThread(bool asDaemon = true);

    /* These are all implemented in Java */
    static void NotifyIME(int aType, int aState);

    static void NotifyIMEEnabled(int aState, const nsAString& aTypeHint,
                                 const nsAString& aActionHint);

    static void NotifyIMEChange(const PRUnichar *aText, PRUint32 aTextLen, int aStart, int aEnd, int aNewEnd);

    void AcknowledgeEventSync();

    void EnableDeviceMotion(bool aEnable);

    void EnableLocation(bool aEnable);

    void ReturnIMEQueryResult(const PRUnichar *aResult, PRUint32 aLen, int aSelStart, int aSelLen);

    void NotifyAppShellReady();

    void NotifyXreExit();

    void ScheduleRestart();

    void SetSoftwareLayerClient(jobject jobj);
    AndroidGeckoSoftwareLayerClient &GetSoftwareLayerClient() { return mSoftwareLayerClient; }

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

    void ShowFilePicker(nsAString& aFilePath, nsAString& aFilters);

    void PerformHapticFeedback(bool aIsLongPress);

    void Vibrate(const nsTArray<PRUint32>& aPattern);
    void CancelVibrate();

    void SetFullScreen(bool aFullScreen);

    void ShowInputMethodPicker();

    void HideProgressDialogOnce();

    bool IsNetworkLinkUp();

    bool IsNetworkLinkKnown();

    void SetSelectedLocale(const nsAString&);

    void GetSystemColors(AndroidSystemColors *aColors);

    void GetIconForExtension(const nsACString& aFileExt, PRUint32 aIconSize, PRUint8 * const aBuf);

    bool GetShowPasswordSetting();

    void FireAndWaitForTracerEvent();

    bool GetAccessibilityEnabled();

    struct AutoLocalJNIFrame {
        AutoLocalJNIFrame(int nEntries = 128) : mEntries(nEntries) {
            // Make sure there is enough space to store a local ref to the
            // exception.  I am not completely sure this is needed, but does
            // not hurt.
            AndroidBridge::Bridge()->JNI()->PushLocalFrame(mEntries + 1);
        }
        // Note! Calling Purge makes all previous local refs created in
        // the AutoLocalJNIFrame's scope INVALID; be sure that you locked down
        // any local refs that you need to keep around in global refs!
        void Purge() {
            AndroidBridge::Bridge()->JNI()->PopLocalFrame(NULL);
            AndroidBridge::Bridge()->JNI()->PushLocalFrame(mEntries);
        }
        ~AutoLocalJNIFrame() {
            jthrowable exception =
                AndroidBridge::Bridge()->JNI()->ExceptionOccurred();
            if (exception) {
                AndroidBridge::Bridge()->JNI()->ExceptionDescribe();
                AndroidBridge::Bridge()->JNI()->ExceptionClear();
            }
            AndroidBridge::Bridge()->JNI()->PopLocalFrame(NULL);
        }
        int mEntries;
    };

    /* See GLHelpers.java as to why this is needed */
    void *CallEglCreateWindowSurface(void *dpy, void *config, AndroidGeckoSurfaceView& surfaceView);

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

    void PostToJavaThread(nsIRunnable* aRunnable, bool aMainThread = false);

    void ExecuteNextRunnable();

    /* Copied from Android's native_window.h in newer (platform 9) NDK */
    enum {
        WINDOW_FORMAT_RGBA_8888          = 1,
        WINDOW_FORMAT_RGBX_8888          = 2,
        WINDOW_FORMAT_RGB_565            = 4,
    };

    bool HasNativeWindowAccess();

    void *AcquireNativeWindow(jobject surface);
    void ReleaseNativeWindow(void *window);
    bool SetNativeWindowFormat(void *window, int format);

    bool LockWindow(void *window, unsigned char **bits, int *width, int *height, int *format, int *stride);
    bool UnlockWindow(void *window);
    
    void HandleGeckoMessage(const nsAString& message, nsAString &aRet);

    void EmitGeckoAccessibilityEvent (PRInt32 eventType, const nsAString& role, const nsAString& text, const nsAString& description, bool enabled, bool checked, bool password);

    void CheckURIVisited(const nsAString& uri);
    void MarkURIVisited(const nsAString& uri);

    bool InitCamera(const nsCString& contentType, PRUint32 camera, PRUint32 *width, PRUint32 *height, PRUint32 *fps);

    void CloseCamera();

    void EnableBatteryNotifications();
    void DisableBatteryNotifications();
    void GetCurrentBatteryInformation(hal::BatteryInformation* aBatteryInfo);

protected:
    static AndroidBridge *sBridge;

    // the global JavaVM
    JavaVM *mJavaVM;

    // the JNIEnv for the main thread
    JNIEnv *mJNIEnv;
    void *mThread;

    // the GeckoSurfaceView
    AndroidGeckoSurfaceView mSurfaceView;
    AndroidGeckoSoftwareLayerClient mSoftwareLayerClient;

    // the GeckoAppShell java class
    jclass mGeckoAppShellClass;

    AndroidBridge() { }
    bool Init(JNIEnv *jEnv, jclass jGeckoApp);

    void EnsureJNIThread();

    bool mOpenedGraphicsLibraries;
    void OpenGraphicsLibraries();

    bool mHasNativeBitmapAccess;
    bool mHasNativeWindowAccess;

    nsCOMArray<nsIRunnable> mRunnableQueue;

    // other things
    jmethodID jNotifyIME;
    jmethodID jNotifyIMEEnabled;
    jmethodID jNotifyIMEChange;
    jmethodID jAcknowledgeEventSync;
    jmethodID jEnableDeviceMotion;
    jmethodID jEnableLocation;
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
    jmethodID jShowFilePicker;
    jmethodID jAlertsProgressListener_OnProgress;
    jmethodID jAlertsProgressListener_OnCancel;
    jmethodID jGetDpi;
    jmethodID jSetFullScreen;
    jmethodID jShowInputMethodPicker;
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
    jmethodID jEnableBatteryNotifications;
    jmethodID jDisableBatteryNotifications;
    jmethodID jGetCurrentBatteryInformation;
    jmethodID jGetAccessibilityEnabled;
    jmethodID jHandleGeckoMessage;
    jmethodID jCheckUriVisited;
    jmethodID jMarkUriVisited;
    jmethodID jEmitGeckoAccessibilityEvent;

    // stuff we need for CallEglCreateWindowSurface
    jclass jEGLSurfaceImplClass;
    jclass jEGLContextImplClass;
    jclass jEGLConfigImplClass;
    jclass jEGLDisplayImplClass;
    jclass jEGLContextClass;
    jclass jEGL10Class;

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

extern "C" JNIEnv * GetJNIForThread();
extern bool mozilla_AndroidBridge_SetMainThread(void *);
extern jclass GetGeckoAppShellClass();

#endif /* AndroidBridge_h__ */
