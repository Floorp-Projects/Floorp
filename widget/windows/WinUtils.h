/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_WinUtils_h__
#define mozilla_widget_WinUtils_h__

#include "nscore.h"
#include <windows.h>
#include <shobjidl.h>
#include "nsAutoPtr.h"
#include "nsString.h"
#include "nsRegion.h"
#include "nsRect.h"

#include "nsThreadUtils.h"
#include "nsICryptoHash.h"
#ifdef MOZ_PLACES
#include "nsIFaviconService.h"
#endif
#include "nsIDownloader.h"
#include "nsIURI.h"

#include "mozilla/Attributes.h"

class nsWindow;

namespace mozilla {
namespace widget {

class myDownloadObserver MOZ_FINAL : public nsIDownloadObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOWNLOADOBSERVER
};

class WinUtils {
public:
  enum WinVersion {
    WINXP_VERSION     = 0x501,
    WIN2K3_VERSION    = 0x502,
    VISTA_VERSION     = 0x600,
    // WIN2K8_VERSION    = VISTA_VERSION,
    WIN7_VERSION      = 0x601,
    // WIN2K8R2_VERSION  = WIN7_VERSION
    WIN8_VERSION      = 0x602
  };
  static WinVersion GetWindowsVersion();

  // Retrieves the Service Pack version number.
  // Returns true on success, false on failure.
  static bool GetWindowsServicePackVersion(UINT& aOutMajor, UINT& aOutMinor);

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
                             const PRUnichar* aKeyName,
                             const PRUnichar* aValueName,
                             PRUnichar* aBuffer,
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
                             const PRUnichar* aKeyName);

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
   * SetNSWindowPtr() associates an nsWindow to aWnd.  If aWindow is NULL,
   * it dissociate any nsWindow pointer from aWnd.
   * GetNSWindowPtr() returns an nsWindow pointer which was associated by
   * SetNSWindowPtr().
   */
  static bool SetNSWindowPtr(HWND aWnd, nsWindow* aWindow);
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
   * top level window, returns NULL.  And note that this is using ::GetParent()
   * for climbing the window hierarchy, therefore, it gives up at an owned top
   * level window except popup window (e.g., dialog).
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
  static MSG InitMSG(UINT aMessage, WPARAM wParam, LPARAM lParam);

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
  nsCOMPtr<nsIURI> mNewURI;
  nsCOMPtr<nsIThread> mIOThread;
  const bool mURLShortcut;
};
#endif

/**
  * Asynchronously tries add the list to the build
  */
class AsyncWriteIconToDisk : public nsIRunnable
{
public:
  const bool mURLShortcut;
  NS_DECL_ISUPPORTS
  NS_DECL_NSIRUNNABLE

  // Warning: AsyncWriteIconToDisk assumes ownership of the aData buffer passed in
  AsyncWriteIconToDisk(const nsAString &aIconPath,
                       const nsACString &aMimeTypeOfInputData,
                       uint8_t *aData, 
                       uint32_t aDataLen,
                       const bool aURLShortcut);
  virtual ~AsyncWriteIconToDisk();

private:
  nsAutoString mIconPath;
  nsAutoCString mMimeTypeOfInputData;
  nsAutoArrayPtr<uint8_t> mBuffer;
  uint32_t mBufferLength;
};

class AsyncDeleteIconFromDisk : public nsIRunnable
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIRUNNABLE

  AsyncDeleteIconFromDisk(const nsAString &aIconPath);
  virtual ~AsyncDeleteIconFromDisk();

private:
  nsAutoString mIconPath;
};

class AsyncDeleteAllFaviconsFromDisk : public nsIRunnable
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIRUNNABLE

  AsyncDeleteAllFaviconsFromDisk();
  virtual ~AsyncDeleteAllFaviconsFromDisk();
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
