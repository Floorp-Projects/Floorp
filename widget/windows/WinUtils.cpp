/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sts=2 sw=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WinUtils.h"
#include "nsWindow.h"
#include "nsWindowDefs.h"
#include "nsGUIEvent.h"
#include "nsIDOMMouseEvent.h"
#include "mozilla/Preferences.h"

namespace mozilla {
namespace widget {

// SHCreateItemFromParsingName is only available on vista and up.
WinUtils::SHCreateItemFromParsingNamePtr WinUtils::sCreateItemFromParsingName = nsnull;

/* static */ 
WinUtils::WinVersion
WinUtils::GetWindowsVersion()
{
  static PRInt32 version = 0;

  if (version) {
    return static_cast<WinVersion>(version);
  }

  OSVERSIONINFOEX osInfo;
  osInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
  // This cast is safe and supposed to be here, don't worry
  ::GetVersionEx((OSVERSIONINFO*)&osInfo);
  version =
    (osInfo.dwMajorVersion & 0xff) << 8 | (osInfo.dwMinorVersion & 0xff);
  return static_cast<WinVersion>(version);
}

/* static */
bool
WinUtils::GetRegistryKey(HKEY aRoot,
                         const PRUnichar* aKeyName,
                         const PRUnichar* aValueName,
                         PRUnichar* aBuffer,
                         DWORD aBufferLength)
{
  NS_PRECONDITION(aKeyName, "The key name is NULL");

  HKEY key;
  LONG result =
    ::RegOpenKeyExW(aRoot, aKeyName, NULL, KEY_READ | KEY_WOW64_32KEY, &key);
  if (result != ERROR_SUCCESS) {
    result =
      ::RegOpenKeyExW(aRoot, aKeyName, NULL, KEY_READ | KEY_WOW64_64KEY, &key);
    if (result != ERROR_SUCCESS) {
      return false;
    }
  }

  DWORD type;
  result =
    ::RegQueryValueExW(key, aValueName, NULL, &type, (BYTE*) aBuffer,
                       &aBufferLength);
  ::RegCloseKey(key);
  if (result != ERROR_SUCCESS || type != REG_SZ) {
    return false;
  }
  if (aBuffer) {
    aBuffer[aBufferLength / sizeof(*aBuffer) - 1] = 0;
  }
  return true;
}

/* static */
bool
WinUtils::HasRegistryKey(HKEY aRoot, const PRUnichar* aKeyName)
{
  MOZ_ASSERT(aRoot, "aRoot must not be NULL");
  MOZ_ASSERT(aKeyName, "aKeyName must not be NULL");
  HKEY key;
  LONG result =
    ::RegOpenKeyExW(aRoot, aKeyName, 0, KEY_READ | KEY_WOW64_32KEY, &key);
  if (result != ERROR_SUCCESS) {
    result =
      ::RegOpenKeyExW(aRoot, aKeyName, 0, KEY_READ | KEY_WOW64_64KEY, &key);
    if (result != ERROR_SUCCESS) {
      return false;
    }
  }
  ::RegCloseKey(key);
  return true;
}

/* static */
HWND
WinUtils::GetTopLevelHWND(HWND aWnd,
                          bool aStopIfNotChild,
                          bool aStopIfNotPopup)
{
  HWND curWnd = aWnd;
  HWND topWnd = NULL;

  while (curWnd) {
    topWnd = curWnd;

    if (aStopIfNotChild) {
      DWORD_PTR style = ::GetWindowLongPtrW(curWnd, GWL_STYLE);

      VERIFY_WINDOW_STYLE(style);

      if (!(style & WS_CHILD)) // first top-level window
        break;
    }

    HWND upWnd = ::GetParent(curWnd); // Parent or owner (if has no parent)

    // GetParent will only return the owner if the passed in window 
    // has the WS_POPUP style.
    if (!upWnd && !aStopIfNotPopup) {
      upWnd = ::GetWindow(curWnd, GW_OWNER);
    }
    curWnd = upWnd;
  }

  return topWnd;
}

static PRUnichar*
GetNSWindowPropName()
{
  static PRUnichar sPropName[40] = L"";
  if (!*sPropName) {
    _snwprintf(sPropName, 39, L"MozillansIWidgetPtr%p",
               ::GetCurrentProcessId());
    sPropName[39] = '\0';
  }
  return sPropName;
}

/* static */
bool
WinUtils::SetNSWindowPtr(HWND aWnd, nsWindow* aWindow)
{
  if (!aWindow) {
    ::RemovePropW(aWnd, GetNSWindowPropName());
    return true;
  }
  return ::SetPropW(aWnd, GetNSWindowPropName(), (HANDLE)aWindow);
}

/* static */
nsWindow*
WinUtils::GetNSWindowPtr(HWND aWnd)
{
  return static_cast<nsWindow*>(::GetPropW(aWnd, GetNSWindowPropName()));
}

static BOOL CALLBACK
AddMonitor(HMONITOR, HDC, LPRECT, LPARAM aParam)
{
  (*(PRInt32*)aParam)++;
  return TRUE;
}

/* static */
PRInt32
WinUtils::GetMonitorCount()
{
  PRInt32 monitorCount = 0;
  EnumDisplayMonitors(NULL, NULL, AddMonitor, (LPARAM)&monitorCount);
  return monitorCount;
}

/* static */
bool
WinUtils::IsOurProcessWindow(HWND aWnd)
{
  if (!aWnd) {
    return false;
  }
  DWORD processId = 0;
  ::GetWindowThreadProcessId(aWnd, &processId);
  return (processId == ::GetCurrentProcessId());
}

/* static */
HWND
WinUtils::FindOurProcessWindow(HWND aWnd)
{
  for (HWND wnd = ::GetParent(aWnd); wnd; wnd = ::GetParent(wnd)) {
    if (IsOurProcessWindow(wnd)) {
      return wnd;
    }
  }
  return NULL;
}

static bool
IsPointInWindow(HWND aWnd, const POINT& aPointInScreen)
{
  RECT bounds;
  if (!::GetWindowRect(aWnd, &bounds)) {
    return false;
  }

  return (aPointInScreen.x >= bounds.left && aPointInScreen.x < bounds.right &&
          aPointInScreen.y >= bounds.top && aPointInScreen.y < bounds.bottom);
}

/**
 * FindTopmostWindowAtPoint() returns the topmost child window (topmost means
 * forground in this context) of aWnd.
 */

static HWND
FindTopmostWindowAtPoint(HWND aWnd, const POINT& aPointInScreen)
{
  if (!::IsWindowVisible(aWnd) || !IsPointInWindow(aWnd, aPointInScreen)) {
    return NULL;
  }

  HWND childWnd = ::GetTopWindow(aWnd);
  while (childWnd) {
    HWND topmostWnd = FindTopmostWindowAtPoint(childWnd, aPointInScreen);
    if (topmostWnd) {
      return topmostWnd;
    }
    childWnd = ::GetNextWindow(childWnd, GW_HWNDNEXT);
  }

  return aWnd;
}

struct FindOurWindowAtPointInfo
{
  POINT mInPointInScreen;
  HWND mOutWnd;
};

static BOOL CALLBACK
FindOurWindowAtPointCallback(HWND aWnd, LPARAM aLPARAM)
{
  if (!WinUtils::IsOurProcessWindow(aWnd)) {
    // This isn't one of our top-level windows; continue enumerating.
    return TRUE;
  }

  // Get the top-most child window under the point.  If there's no child
  // window, and the point is within the top-level window, then the top-level
  // window will be returned.  (This is the usual case.  A child window
  // would be returned for plugins.)
  FindOurWindowAtPointInfo* info =
    reinterpret_cast<FindOurWindowAtPointInfo*>(aLPARAM);
  HWND childWnd = FindTopmostWindowAtPoint(aWnd, info->mInPointInScreen);
  if (!childWnd) {
    // This window doesn't contain the point; continue enumerating.
    return TRUE;
  }

  // Return the HWND and stop enumerating.
  info->mOutWnd = childWnd;
  return FALSE;
}

/* static */
HWND
WinUtils::FindOurWindowAtPoint(const POINT& aPointInScreen)
{
  FindOurWindowAtPointInfo info;
  info.mInPointInScreen = aPointInScreen;
  info.mOutWnd = NULL;

  // This will enumerate all top-level windows in order from top to bottom.
  EnumWindows(FindOurWindowAtPointCallback, reinterpret_cast<LPARAM>(&info));
  return info.mOutWnd;
}

/* static */
UINT
WinUtils::GetInternalMessage(UINT aNativeMessage)
{
  switch (aNativeMessage) {
    case WM_MOUSEWHEEL:
      return MOZ_WM_MOUSEVWHEEL;
    case WM_MOUSEHWHEEL:
      return MOZ_WM_MOUSEHWHEEL;
    case WM_VSCROLL:
      return MOZ_WM_VSCROLL;
    case WM_HSCROLL:
      return MOZ_WM_HSCROLL;
    default:
      return aNativeMessage;
  }
}

/* static */
UINT
WinUtils::GetNativeMessage(UINT aInternalMessage)
{
  switch (aInternalMessage) {
    case MOZ_WM_MOUSEVWHEEL:
      return WM_MOUSEWHEEL;
    case MOZ_WM_MOUSEHWHEEL:
      return WM_MOUSEHWHEEL;
    case MOZ_WM_VSCROLL:
      return WM_VSCROLL;
    case MOZ_WM_HSCROLL:
      return WM_HSCROLL;
    default:
      return aInternalMessage;
  }
}

/* static */
PRUint16
WinUtils::GetMouseInputSource()
{
  PRInt32 inputSource = nsIDOMMouseEvent::MOZ_SOURCE_MOUSE;
  LPARAM lParamExtraInfo = ::GetMessageExtraInfo();
  if ((lParamExtraInfo & TABLET_INK_SIGNATURE) == TABLET_INK_CHECK) {
    inputSource = (lParamExtraInfo & TABLET_INK_TOUCH) ?
      nsIDOMMouseEvent::MOZ_SOURCE_TOUCH : nsIDOMMouseEvent::MOZ_SOURCE_PEN;
  }
  return static_cast<PRUint16>(inputSource);
}

/* static */
MSG
WinUtils::InitMSG(UINT aMessage, WPARAM wParam, LPARAM lParam)
{
  MSG msg;
  msg.message = aMessage;
  msg.wParam  = wParam;
  msg.lParam  = lParam;
  return msg;
}

/* static */
bool
WinUtils::VistaCreateItemFromParsingNameInit()
{
  // Load and store Vista+ SHCreateItemFromParsingName
  if (sCreateItemFromParsingName) {
    return true;
  }
  static HMODULE sShellDll = nsnull;
  if (sShellDll) {
    return false;
  }
  static const PRUnichar kSehllLibraryName[] =  L"shell32.dll";
  sShellDll = ::LoadLibraryW(kSehllLibraryName);
  if (!sShellDll) {
    return false;
  }
  sCreateItemFromParsingName = (SHCreateItemFromParsingNamePtr)
    GetProcAddress(sShellDll, "SHCreateItemFromParsingName");
  return sCreateItemFromParsingName != nsnull;
}

/* static */
HRESULT
WinUtils::SHCreateItemFromParsingName(PCWSTR pszPath, IBindCtx *pbc,
                                      REFIID riid, void **ppv)
{
  if (!VistaCreateItemFromParsingNameInit())
    return E_FAIL;
  return sCreateItemFromParsingName(pszPath, pbc, riid, ppv);
}

/* static */
bool
WinUtils::GetShellItemPath(IShellItem* aItem,
                           nsString& aResultString)
{
  NS_ENSURE_TRUE(aItem, false);
  LPWSTR str = NULL;
  if (FAILED(aItem->GetDisplayName(SIGDN_FILESYSPATH, &str)))
    return false;
  aResultString.Assign(str);
  CoTaskMemFree(str);
  return !aResultString.IsEmpty();
}

} // namespace widget
} // namespace mozilla
