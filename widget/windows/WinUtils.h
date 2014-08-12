/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_WinUtils_h__
#define mozilla_widget_WinUtils_h__

#include "nscore.h"
#include <windows.h>
#include <shobjidl.h>
#include <uxtheme.h>
#include <dwmapi.h>
#include "nsAutoPtr.h"
#include "nsString.h"
#include "nsRegion.h"

#include "nsIRunnable.h"
#include "nsICryptoHash.h"
#ifdef MOZ_PLACES
#include "nsIFaviconService.h"
#endif
#include "nsIDownloader.h"
#include "nsIURI.h"
#include "nsIWidget.h"
#include "nsIThread.h"

#include "mozilla/Attributes.h"

/**
 * NS_INLINE_DECL_IUNKNOWN_REFCOUNTING should be used for defining and
 * implementing AddRef() and Release() of IUnknown interface.
 * This depends on xpcom/glue/nsISupportsImpl.h.
 */

#define NS_INLINE_DECL_IUNKNOWN_REFCOUNTING(_class)                           \
public:                                                                       \
  STDMETHODIMP_(ULONG) AddRef()                                               \
  {                                                                           \
    MOZ_ASSERT_TYPE_OK_FOR_REFCOUNTING(_class)                                \
    MOZ_ASSERT(int32_t(mRefCnt) >= 0, "illegal refcnt");                      \
    NS_ASSERT_OWNINGTHREAD(_class);                                           \
    ++mRefCnt;                                                                \
    NS_LOG_ADDREF(this, mRefCnt, #_class, sizeof(*this));                     \
    return static_cast<ULONG>(mRefCnt.get());                                 \
  }                                                                           \
  STDMETHODIMP_(ULONG) Release()                                              \
  {                                                                           \
    MOZ_ASSERT(int32_t(mRefCnt) > 0,                                          \
      "Release called on object that has already been released!");            \
    NS_ASSERT_OWNINGTHREAD(_class);                                           \
    --mRefCnt;                                                                \
    NS_LOG_RELEASE(this, mRefCnt, #_class);                                   \
    if (mRefCnt == 0) {                                                       \
      NS_ASSERT_OWNINGTHREAD(_class);                                         \
      mRefCnt = 1; /* stabilize */                                            \
      delete this;                                                            \
      return 0;                                                               \
    }                                                                         \
    return static_cast<ULONG>(mRefCnt.get());                                 \
  }                                                                           \
protected:                                                                    \
  nsAutoRefCnt mRefCnt;                                                       \
  NS_DECL_OWNINGTHREAD                                                        \
public:

class nsWindow;
class nsWindowBase;
struct KeyPair;
struct nsIntRect;

namespace mozilla {
namespace widget {

// Windows message debugging data
typedef struct {
  const char * mStr;
  UINT         mId;
} EventMsgInfo;
extern EventMsgInfo gAllEvents[];

// More complete QS definitions for MsgWaitForMultipleObjects() and
// GetQueueStatus() that include newer win8 specific defines.

#ifndef QS_RAWINPUT
#define QS_RAWINPUT 0x0400
#endif

#ifndef QS_TOUCH
#define QS_TOUCH    0x0800
#define QS_POINTER  0x1000
#endif

#define MOZ_QS_ALLEVENT (QS_KEY | QS_MOUSEMOVE | QS_MOUSEBUTTON | \
                         QS_POSTMESSAGE | QS_TIMER | QS_PAINT |   \
                         QS_SENDMESSAGE | QS_HOTKEY |             \
                         QS_ALLPOSTMESSAGE | QS_RAWINPUT |        \
                         QS_TOUCH | QS_POINTER)

// Logging macros
#define LogFunction() mozilla::widget::WinUtils::Log(__FUNCTION__)
#define LogThread() mozilla::widget::WinUtils::Log("%s: IsMainThread:%d ThreadId:%X", __FUNCTION__, NS_IsMainThread(), GetCurrentThreadId())
#define LogThis() mozilla::widget::WinUtils::Log("[%X] %s", this, __FUNCTION__)
#define LogException(e) mozilla::widget::WinUtils::Log("%s Exception:%s", __FUNCTION__, e->ToString()->Data())
#define LogHRESULT(hr) mozilla::widget::WinUtils::Log("%s hr=%X", __FUNCTION__, hr)

#ifdef MOZ_PLACES
class myDownloadObserver MOZ_FINAL : public nsIDownloadObserver
{
  ~myDownloadObserver() {}

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOWNLOADOBSERVER
};
#endif

class WinUtils {
public:
  /**
   * Functions to convert between logical pixels as used by most Windows APIs
   * and physical (device) pixels.
   */
  static double LogToPhysFactor();
  static double PhysToLogFactor();
  static int32_t LogToPhys(double aValue);
  static double PhysToLog(int32_t aValue);

  /**
   * Logging helpers that dump output to prlog module 'Widget', console, and
   * OutputDebugString. Note these output in both debug and release builds.
   */
  static void Log(const char *fmt, ...);
  static void LogW(const wchar_t *fmt, ...);

  /**
   * PeekMessage() and GetMessage() are wrapper methods for PeekMessageW(),
   * GetMessageW(), ITfMessageMgr::PeekMessageW() and
   * ITfMessageMgr::GetMessageW().
   * Don't call the native APIs directly.  You MUST use these methods instead.
   */
  static bool PeekMessage(LPMSG aMsg, HWND aWnd, UINT aFirstMessage,
                          UINT aLastMessage, UINT aOption);
  static bool GetMessage(LPMSG aMsg, HWND aWnd, UINT aFirstMessage,
                         UINT aLastMessage);

  /**
   * Wait until a message is ready to be processed.
   * Prefer using this method to directly calling ::WaitMessage since
   * ::WaitMessage will wait if there is an unread message in the queue.
   * That can cause freezes until another message enters the queue if the
   * message is marked read by a call to PeekMessage which the caller is
   * not aware of (e.g., from a different thread).
   * Note that this method may cause sync dispatch of sent (as opposed to
   * posted) messages.
   */
  static void WaitForMessage();

  /**
   * Gets the value of a string-typed registry value.
   *
   * @param aRoot The registry root to search in.
   * @param aKeyName The name of the registry key to open.
   * @param aValueName The name of the registry value in the specified key whose
   *   value is to be retrieved.  Can be null, to retrieve the key's unnamed/
   *   default value.
   * @param aBuffer The buffer into which to store the string value.  Can be
   *   null, in which case the return value indicates just whether the value
   *   exists.
   * @param aBufferLength The size of aBuffer, in bytes.
   * @return Whether the value exists and is a string.
   */
  static bool GetRegistryKey(HKEY aRoot,
                             char16ptr_t aKeyName,
                             char16ptr_t aValueName,
                             wchar_t* aBuffer,
                             DWORD aBufferLength);

  /**
   * Checks whether the registry key exists in either 32bit or 64bit branch on
   * the environment.
   *
   * @param aRoot The registry root of aName.
   * @param aKeyName The name of the registry key to check.
   * @return TRUE if it exists and is readable.  Otherwise, FALSE.
   */
  static bool HasRegistryKey(HKEY aRoot,
                             char16ptr_t aKeyName);

  /**
   * GetTopLevelHWND() returns a window handle of the top level window which
   * aWnd belongs to.  Note that the result may not be our window, i.e., it
   * may not be managed by nsWindow.
   *
   * See follwing table for the detail of the result window type.
   *
   * +-------------------------+-----------------------------------------------+
   * |                         |                aStopIfNotPopup                |
   * +-------------------------+-----------------------+-----------------------+
   * |                         |         TRUE          |         FALSE         |
   + +-----------------+-------+-----------------------+-----------------------+
   * |                 |       |  * an independent top level window            |
   * |                 | TRUE  |  * a pupup window (WS_POPUP)                  |
   * |                 |       |  * an owned top level window (like dialog)    |
   * | aStopIfNotChild +-------+-----------------------+-----------------------+
   * |                 |       |  * independent window | * only an independent |
   * |                 | FALSE |  * non-popup-owned-   |   top level window    |
   * |                 |       |    window like dialog |                       |
   * +-----------------+-------+-----------------------+-----------------------+
   */
  static HWND GetTopLevelHWND(HWND aWnd, 
                              bool aStopIfNotChild = false, 
                              bool aStopIfNotPopup = true);

  /**
   * SetNSWindowBasePtr() associates an nsWindowBase to aWnd.  If aWidget is
   * nullptr, it dissociate any nsBaseWidget pointer from aWnd.
   * GetNSWindowBasePtr() returns an nsWindowBase pointer which was associated by
   * SetNSWindowBasePtr().
   * GetNSWindowPtr() is a legacy api for win32 nsWindow and should be avoided
   * outside of nsWindow src.
   */
  static bool SetNSWindowBasePtr(HWND aWnd, nsWindowBase* aWidget);
  static nsWindowBase* GetNSWindowBasePtr(HWND aWnd);
  static nsWindow* GetNSWindowPtr(HWND aWnd);

  /**
   * GetMonitorCount() returns count of monitors on the environment.
   */
  static int32_t GetMonitorCount();

  /**
   * IsOurProcessWindow() returns TRUE if aWnd belongs our process.
   * Otherwise, FALSE.
   */
  static bool IsOurProcessWindow(HWND aWnd);

  /**
   * FindOurProcessWindow() returns the nearest ancestor window which
   * belongs to our process.  If it fails to find our process's window by the
   * top level window, returns nullptr.  And note that this is using
   * ::GetParent() for climbing the window hierarchy, therefore, it gives
   * up at an owned top level window except popup window (e.g., dialog).
   */
  static HWND FindOurProcessWindow(HWND aWnd);

  /**
   * FindOurWindowAtPoint() returns the topmost child window which belongs to
   * our process's top level window.
   *
   * NOTE: the topmost child window may NOT be our process's window like a
   *       plugin's window.
   */
  static HWND FindOurWindowAtPoint(const POINT& aPointInScreen);

  /**
   * InitMSG() returns an MSG struct which was initialized by the params.
   * Don't trust the other members in the result.
   */
  static MSG InitMSG(UINT aMessage, WPARAM wParam, LPARAM lParam, HWND aWnd);

  /**
   * GetScanCode() returns a scan code for the LPARAM of WM_KEYDOWN, WM_KEYUP,
   * WM_CHAR and WM_UNICHAR.
   *
   */
  static WORD GetScanCode(LPARAM aLParam)
  {
    return (aLParam >> 16) & 0xFF;
  }

  /**
   * IsExtendedScanCode() returns TRUE if the LPARAM indicates the key message
   * is an extended key event.
   */
  static bool IsExtendedScanCode(LPARAM aLParam)
  {
    return (aLParam & 0x1000000) != 0;
  }

  /**
   * GetInternalMessage() converts a native message to an internal message.
   * If there is no internal message for the given native message, returns
   * the native message itself.
   */
  static UINT GetInternalMessage(UINT aNativeMessage);

  /**
   * GetNativeMessage() converts an internal message to a native message.
   * If aInternalMessage is a native message, returns the native message itself.
   */
  static UINT GetNativeMessage(UINT aInternalMessage);

  /**
   * GetMouseInputSource() returns a pointing device information.  The value is
   * one of nsIDOMMouseEvent::MOZ_SOURCE_*.  This method MUST be called during
   * mouse message handling.
   */
  static uint16_t GetMouseInputSource();

  static bool GetIsMouseFromTouch(uint32_t aEventType);

  /**
   * SHCreateItemFromParsingName() calls native SHCreateItemFromParsingName()
   * API which is available on Vista and up.
   */
  static HRESULT SHCreateItemFromParsingName(PCWSTR pszPath, IBindCtx *pbc,
                                             REFIID riid, void **ppv);

  /**
   * SHGetKnownFolderPath() calls native SHGetKnownFolderPath()
   * API which is available on Vista and up.
   */
  static HRESULT SHGetKnownFolderPath(REFKNOWNFOLDERID rfid,
                                      DWORD dwFlags,
                                      HANDLE hToken,
                                      PWSTR *ppszPath);
  /**
   * GetShellItemPath return the file or directory path of a shell item.
   * Internally calls IShellItem's GetDisplayName.
   *
   * aItem  the shell item containing the path.
   * aResultString  the resulting string path.
   * returns  true if a path was retreived.
   */
  static bool GetShellItemPath(IShellItem* aItem,
                               nsString& aResultString);

  /**
   * ConvertHRGNToRegion converts a Windows HRGN to an nsIntRegion.
   *
   * aRgn the HRGN to convert.
   * returns the nsIntRegion.
   */
  static nsIntRegion ConvertHRGNToRegion(HRGN aRgn);

  /**
   * ToIntRect converts a Windows RECT to a nsIntRect.
   *
   * aRect the RECT to convert.
   * returns the nsIntRect.
   */
  static nsIntRect ToIntRect(const RECT& aRect);

  /**
   * Returns true if the context or IME state is enabled.  Otherwise, false.
   */
  static bool IsIMEEnabled(const InputContext& aInputContext);
  static bool IsIMEEnabled(IMEState::Enabled aIMEState);

  /**
   * Returns modifier key array for aModifiers.  This is for
   * nsIWidget::SynthethizeNative*Event().
   */
  static void SetupKeyModifiersSequence(nsTArray<KeyPair>* aArray,
                                        uint32_t aModifiers);

  // dwmapi.dll function typedefs and declarations
  typedef HRESULT (WINAPI*DwmExtendFrameIntoClientAreaProc)(HWND hWnd, const MARGINS *pMarInset);
  typedef HRESULT (WINAPI*DwmIsCompositionEnabledProc)(BOOL *pfEnabled);
  typedef HRESULT (WINAPI*DwmSetIconicThumbnailProc)(HWND hWnd, HBITMAP hBitmap, DWORD dwSITFlags);
  typedef HRESULT (WINAPI*DwmSetIconicLivePreviewBitmapProc)(HWND hWnd, HBITMAP hBitmap, POINT *pptClient, DWORD dwSITFlags);
  typedef HRESULT (WINAPI*DwmGetWindowAttributeProc)(HWND hWnd, DWORD dwAttribute, LPCVOID pvAttribute, DWORD cbAttribute);
  typedef HRESULT (WINAPI*DwmSetWindowAttributeProc)(HWND hWnd, DWORD dwAttribute, LPCVOID pvAttribute, DWORD cbAttribute);
  typedef HRESULT (WINAPI*DwmInvalidateIconicBitmapsProc)(HWND hWnd);
  typedef HRESULT (WINAPI*DwmDefWindowProcProc)(HWND hWnd, UINT msg, LPARAM lParam, WPARAM wParam, LRESULT *aRetValue);
  typedef HRESULT (WINAPI*DwmGetCompositionTimingInfoProc)(HWND hWnd, DWM_TIMING_INFO *info);

  static DwmExtendFrameIntoClientAreaProc dwmExtendFrameIntoClientAreaPtr;
  static DwmIsCompositionEnabledProc dwmIsCompositionEnabledPtr;
  static DwmSetIconicThumbnailProc dwmSetIconicThumbnailPtr;
  static DwmSetIconicLivePreviewBitmapProc dwmSetIconicLivePreviewBitmapPtr;
  static DwmGetWindowAttributeProc dwmGetWindowAttributePtr;
  static DwmSetWindowAttributeProc dwmSetWindowAttributePtr;
  static DwmInvalidateIconicBitmapsProc dwmInvalidateIconicBitmapsPtr;
  static DwmDefWindowProcProc dwmDwmDefWindowProcPtr;
  static DwmGetCompositionTimingInfoProc dwmGetCompositionTimingInfoPtr;

  static void Initialize();

  static bool ShouldHideScrollbars();

private:
  typedef HRESULT (WINAPI * SHCreateItemFromParsingNamePtr)(PCWSTR pszPath,
                                                            IBindCtx *pbc,
                                                            REFIID riid,
                                                            void **ppv);
  static SHCreateItemFromParsingNamePtr sCreateItemFromParsingName;
  typedef HRESULT (WINAPI * SHGetKnownFolderPathPtr)(REFKNOWNFOLDERID rfid,
                                                     DWORD dwFlags,
                                                     HANDLE hToken,
                                                     PWSTR *ppszPath);
  static SHGetKnownFolderPathPtr sGetKnownFolderPath;
};

#ifdef MOZ_PLACES
class AsyncFaviconDataReady MOZ_FINAL : public nsIFaviconDataCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIFAVICONDATACALLBACK
  
  AsyncFaviconDataReady(nsIURI *aNewURI, 
                        nsCOMPtr<nsIThread> &aIOThread, 
                        const bool aURLShortcut);
  nsresult OnFaviconDataNotAvailable(void);
private:
  ~AsyncFaviconDataReady() {}

  nsCOMPtr<nsIURI> mNewURI;
  nsCOMPtr<nsIThread> mIOThread;
  const bool mURLShortcut;
};
#endif

/**
  * Asynchronously tries add the list to the build
  */
class AsyncEncodeAndWriteIcon : public nsIRunnable
{
public:
  const bool mURLShortcut;
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIRUNNABLE

  // Warning: AsyncEncodeAndWriteIcon assumes ownership of the aData buffer passed in
  AsyncEncodeAndWriteIcon(const nsAString &aIconPath,
                          uint8_t *aData, uint32_t aDataLen, uint32_t aStride,
                          uint32_t aWidth, uint32_t aHeight,
                          const bool aURLShortcut);

private:
  virtual ~AsyncEncodeAndWriteIcon();

  nsAutoString mIconPath;
  nsAutoArrayPtr<uint8_t> mBuffer;
  HMODULE sDwmDLL;
  uint32_t mBufferLength;
  uint32_t mStride;
  uint32_t mWidth;
  uint32_t mHeight;
};


class AsyncDeleteIconFromDisk : public nsIRunnable
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIRUNNABLE

  AsyncDeleteIconFromDisk(const nsAString &aIconPath);

private:
  virtual ~AsyncDeleteIconFromDisk();

  nsAutoString mIconPath;
};

class AsyncDeleteAllFaviconsFromDisk : public nsIRunnable
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIRUNNABLE

  AsyncDeleteAllFaviconsFromDisk(bool aIgnoreRecent = false);

private:
  virtual ~AsyncDeleteAllFaviconsFromDisk();

  int32_t mIcoNoDeleteSeconds;
  bool mIgnoreRecent;
};

class FaviconHelper
{
public:
  static const char kJumpListCacheDir[];
  static const char kShortcutCacheDir[];
  static nsresult ObtainCachedIconFile(nsCOMPtr<nsIURI> aFaviconPageURI,
                                       nsString &aICOFilePath,
                                       nsCOMPtr<nsIThread> &aIOThread,
                                       bool aURLShortcut);

  static nsresult HashURI(nsCOMPtr<nsICryptoHash> &aCryptoHash, 
                          nsIURI *aUri,
                          nsACString& aUriHash);

  static nsresult GetOutputIconPath(nsCOMPtr<nsIURI> aFaviconPageURI,
                                    nsCOMPtr<nsIFile> &aICOFile,
                                    bool aURLShortcut);

  static nsresult 
  CacheIconFileFromFaviconURIAsync(nsCOMPtr<nsIURI> aFaviconPageURI,
                                   nsCOMPtr<nsIFile> aICOFile,
                                   nsCOMPtr<nsIThread> &aIOThread,
                                   bool aURLShortcut);

  static int32_t GetICOCacheSecondsTimeout();
};



} // namespace widget
} // namespace mozilla

#endif // mozilla_widget_WinUtils_h__
