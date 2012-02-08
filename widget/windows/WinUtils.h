/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Masayuki Nakano <masayuki@d-toybox.com>
 *
 * Original nsWindow.cpp Contributor(s):
 *   Robert O'Callahan <roc+moz@cs.cmu.edu>
 *   Dean Tessman <dean_tessman@hotmail.com>
 *   Makoto Kato  <m_kato@ga2.so-net.ne.jp>
 *   Dainis Jonitis <Dainis_Jonitis@swh-t.lv>
 *   Masayuki Nakano <masayuki@d-toybox.com>
 *   Ningjie Chen <chenn@email.uc.edu>
 *   Jim Mathies <jmathies@mozilla.com>.
 *   Mats Palmgren <matspal@gmail.com>
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

#ifndef mozilla_widget_WinUtils_h__
#define mozilla_widget_WinUtils_h__

#include "nscore.h"
#include <windows.h>
#include <shobjidl.h>

class nsWindow;

namespace mozilla {
namespace widget {

class WinUtils {
public:
  enum WinVersion {
    WIN2K_VERSION     = 0x500,
    WINXP_VERSION     = 0x501,
    WIN2K3_VERSION    = 0x502,
    VISTA_VERSION     = 0x600,
    // WIN2K8_VERSION    = VISTA_VERSION,
    WIN7_VERSION      = 0x601
    // WIN2K8R2_VERSION  = WIN7_VERSION
    // WIN8_VERSION      = 0x602
  };
  static WinVersion GetWindowsVersion();

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
  static PRInt32 GetMonitorCount();

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
  static PRUint16 GetMouseInputSource();

  /**
   * VistaCreateItemFromParsingNameInit() initializes the static pointer for
   * SHCreateItemFromParsingName() API which is usable only on Vista and later.
   * This returns TRUE if the API is available.  Otherwise, FALSE.
   */
  static bool VistaCreateItemFromParsingNameInit();

  /**
   * SHCreateItemFromParsingName() calls native SHCreateItemFromParsingName()
   * API.  Note that you must call VistaCreateItemFromParsingNameInit() before
   * calling this.  And the result must be TRUE.  Otherwise, returns E_FAIL.
   */
  static HRESULT SHCreateItemFromParsingName(PCWSTR pszPath, IBindCtx *pbc,
                                             REFIID riid, void **ppv);

private:
  typedef HRESULT (WINAPI * SHCreateItemFromParsingNamePtr)(PCWSTR pszPath,
                                                            IBindCtx *pbc,
                                                            REFIID riid,
                                                            void **ppv);
  static SHCreateItemFromParsingNamePtr sCreateItemFromParsingName;
};

} // namespace widget
} // namespace mozilla

#endif // mozilla_widget_WinUtils_h__
