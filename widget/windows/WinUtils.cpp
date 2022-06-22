/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sts=2 sw=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WinUtils.h"

#include <knownfolders.h>
#include <winioctl.h>

#include "gfxPlatform.h"
#include "gfxUtils.h"
#include "nsWindow.h"
#include "nsWindowDefs.h"
#include "InputDeviceUtils.h"
#include "KeyboardLayout.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/BackgroundHangMonitor.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/MouseEventBinding.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/DataSurfaceHelpers.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/Preferences.h"
#include "mozilla/ProfilerThreadSleep.h"
#include "mozilla/RefPtr.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/WinHeaderOnlyUtils.h"
#include "mozilla/WindowsVersion.h"
#include "mozilla/Unused.h"
#include "nsIContentPolicy.h"
#include "WindowsUIUtils.h"
#include "nsContentUtils.h"

#include "mozilla/Logging.h"

#include "nsString.h"
#include "nsDirectoryServiceUtils.h"
#include "imgIContainer.h"
#include "imgITools.h"
#include "nsNetUtil.h"
#include "nsIOutputStream.h"
#include "nsNetCID.h"
#include "prtime.h"
#ifdef MOZ_PLACES
#  include "nsIFaviconService.h"
#endif
#include "nsIDownloader.h"
#include "nsIChannel.h"
#include "nsIThread.h"
#include "MainThreadUtils.h"
#include "nsLookAndFeel.h"
#include "nsUnicharUtils.h"
#include "nsWindowsHelpers.h"
#include "WinContentSystemParameters.h"
#include "WinWindowOcclusionTracker.h"

#include <textstor.h>
#include "TSFTextStore.h"

#include <shlobj.h>
#include <shlwapi.h>

mozilla::LazyLogModule gWindowsLog("Widget");

#define LOG_E(...) MOZ_LOG(gWindowsLog, LogLevel::Error, (__VA_ARGS__))
#define LOG_D(...) MOZ_LOG(gWindowsLog, LogLevel::Debug, (__VA_ARGS__))

using namespace mozilla::gfx;

namespace mozilla {
namespace widget {

#define ENTRY(_msg)       \
  {                       \
    _msg, { #_msg, _msg } \
  }
std::unordered_map<UINT, EventMsgInfo> gAllEvents = {
    ENTRY(WM_NULL),
    ENTRY(WM_CREATE),
    ENTRY(WM_DESTROY),
    ENTRY(WM_MOVE),
    ENTRY(WM_SIZE),
    ENTRY(WM_ACTIVATE),
    ENTRY(WM_SETFOCUS),
    ENTRY(WM_KILLFOCUS),
    ENTRY(WM_ENABLE),
    ENTRY(WM_SETREDRAW),
    ENTRY(WM_SETTEXT),
    ENTRY(WM_GETTEXT),
    ENTRY(WM_GETTEXTLENGTH),
    ENTRY(WM_PAINT),
    ENTRY(WM_CLOSE),
    ENTRY(WM_QUERYENDSESSION),
    ENTRY(WM_QUIT),
    ENTRY(WM_QUERYOPEN),
    ENTRY(WM_ERASEBKGND),
    ENTRY(WM_SYSCOLORCHANGE),
    ENTRY(WM_ENDSESSION),
    ENTRY(WM_SHOWWINDOW),
    ENTRY(WM_SETTINGCHANGE),
    ENTRY(WM_DEVMODECHANGE),
    ENTRY(WM_ACTIVATEAPP),
    ENTRY(WM_FONTCHANGE),
    ENTRY(WM_TIMECHANGE),
    ENTRY(WM_CANCELMODE),
    ENTRY(WM_SETCURSOR),
    ENTRY(WM_MOUSEACTIVATE),
    ENTRY(WM_CHILDACTIVATE),
    ENTRY(WM_QUEUESYNC),
    ENTRY(WM_GETMINMAXINFO),
    ENTRY(WM_PAINTICON),
    ENTRY(WM_ICONERASEBKGND),
    ENTRY(WM_NEXTDLGCTL),
    ENTRY(WM_SPOOLERSTATUS),
    ENTRY(WM_DRAWITEM),
    ENTRY(WM_MEASUREITEM),
    ENTRY(WM_DELETEITEM),
    ENTRY(WM_VKEYTOITEM),
    ENTRY(WM_CHARTOITEM),
    ENTRY(WM_SETFONT),
    ENTRY(WM_GETFONT),
    ENTRY(WM_SETHOTKEY),
    ENTRY(WM_GETHOTKEY),
    ENTRY(WM_QUERYDRAGICON),
    ENTRY(WM_COMPAREITEM),
    ENTRY(WM_GETOBJECT),
    ENTRY(WM_COMPACTING),
    ENTRY(WM_COMMNOTIFY),
    ENTRY(WM_WINDOWPOSCHANGING),
    ENTRY(WM_WINDOWPOSCHANGED),
    ENTRY(WM_POWER),
    ENTRY(WM_COPYDATA),
    ENTRY(WM_CANCELJOURNAL),
    ENTRY(WM_NOTIFY),
    ENTRY(WM_INPUTLANGCHANGEREQUEST),
    ENTRY(WM_INPUTLANGCHANGE),
    ENTRY(WM_TCARD),
    ENTRY(WM_HELP),
    ENTRY(WM_USERCHANGED),
    ENTRY(WM_NOTIFYFORMAT),
    ENTRY(WM_CONTEXTMENU),
    ENTRY(WM_STYLECHANGING),
    ENTRY(WM_STYLECHANGED),
    ENTRY(WM_DISPLAYCHANGE),
    ENTRY(WM_GETICON),
    ENTRY(WM_SETICON),
    ENTRY(WM_NCCREATE),
    ENTRY(WM_NCDESTROY),
    ENTRY(WM_NCCALCSIZE),
    ENTRY(WM_NCHITTEST),
    ENTRY(WM_NCPAINT),
    ENTRY(WM_NCACTIVATE),
    ENTRY(WM_GETDLGCODE),
    ENTRY(WM_SYNCPAINT),
    ENTRY(WM_NCMOUSEMOVE),
    ENTRY(WM_NCLBUTTONDOWN),
    ENTRY(WM_NCLBUTTONUP),
    ENTRY(WM_NCLBUTTONDBLCLK),
    ENTRY(WM_NCRBUTTONDOWN),
    ENTRY(WM_NCRBUTTONUP),
    ENTRY(WM_NCRBUTTONDBLCLK),
    ENTRY(WM_NCMBUTTONDOWN),
    ENTRY(WM_NCMBUTTONUP),
    ENTRY(WM_NCMBUTTONDBLCLK),
    ENTRY(EM_GETSEL),
    ENTRY(EM_SETSEL),
    ENTRY(EM_GETRECT),
    ENTRY(EM_SETRECT),
    ENTRY(EM_SETRECTNP),
    ENTRY(EM_SCROLL),
    ENTRY(EM_LINESCROLL),
    ENTRY(EM_SCROLLCARET),
    ENTRY(EM_GETMODIFY),
    ENTRY(EM_SETMODIFY),
    ENTRY(EM_GETLINECOUNT),
    ENTRY(EM_LINEINDEX),
    ENTRY(EM_SETHANDLE),
    ENTRY(EM_GETHANDLE),
    ENTRY(EM_GETTHUMB),
    ENTRY(EM_LINELENGTH),
    ENTRY(EM_REPLACESEL),
    ENTRY(EM_GETLINE),
    ENTRY(EM_LIMITTEXT),
    ENTRY(EM_CANUNDO),
    ENTRY(EM_UNDO),
    ENTRY(EM_FMTLINES),
    ENTRY(EM_LINEFROMCHAR),
    ENTRY(EM_SETTABSTOPS),
    ENTRY(EM_SETPASSWORDCHAR),
    ENTRY(EM_EMPTYUNDOBUFFER),
    ENTRY(EM_GETFIRSTVISIBLELINE),
    ENTRY(EM_SETREADONLY),
    ENTRY(EM_SETWORDBREAKPROC),
    ENTRY(EM_GETWORDBREAKPROC),
    ENTRY(EM_GETPASSWORDCHAR),
    ENTRY(EM_SETMARGINS),
    ENTRY(EM_GETMARGINS),
    ENTRY(EM_GETLIMITTEXT),
    ENTRY(EM_POSFROMCHAR),
    ENTRY(EM_CHARFROMPOS),
    ENTRY(EM_SETIMESTATUS),
    ENTRY(EM_GETIMESTATUS),
    ENTRY(SBM_SETPOS),
    ENTRY(SBM_GETPOS),
    ENTRY(SBM_SETRANGE),
    ENTRY(SBM_SETRANGEREDRAW),
    ENTRY(SBM_GETRANGE),
    ENTRY(SBM_ENABLE_ARROWS),
    ENTRY(SBM_SETSCROLLINFO),
    ENTRY(SBM_GETSCROLLINFO),
    ENTRY(WM_KEYDOWN),
    ENTRY(WM_KEYUP),
    ENTRY(WM_CHAR),
    ENTRY(WM_DEADCHAR),
    ENTRY(WM_SYSKEYDOWN),
    ENTRY(WM_SYSKEYUP),
    ENTRY(WM_SYSCHAR),
    ENTRY(WM_SYSDEADCHAR),
    ENTRY(WM_KEYLAST),
    ENTRY(WM_IME_STARTCOMPOSITION),
    ENTRY(WM_IME_ENDCOMPOSITION),
    ENTRY(WM_IME_COMPOSITION),
    ENTRY(WM_INITDIALOG),
    ENTRY(WM_COMMAND),
    ENTRY(WM_SYSCOMMAND),
    ENTRY(WM_TIMER),
    ENTRY(WM_HSCROLL),
    ENTRY(WM_VSCROLL),
    ENTRY(WM_INITMENU),
    ENTRY(WM_INITMENUPOPUP),
    ENTRY(WM_MENUSELECT),
    ENTRY(WM_MENUCHAR),
    ENTRY(WM_ENTERIDLE),
    ENTRY(WM_MENURBUTTONUP),
    ENTRY(WM_MENUDRAG),
    ENTRY(WM_MENUGETOBJECT),
    ENTRY(WM_UNINITMENUPOPUP),
    ENTRY(WM_MENUCOMMAND),
    ENTRY(WM_CHANGEUISTATE),
    ENTRY(WM_UPDATEUISTATE),
    ENTRY(WM_CTLCOLORMSGBOX),
    ENTRY(WM_CTLCOLOREDIT),
    ENTRY(WM_CTLCOLORLISTBOX),
    ENTRY(WM_CTLCOLORBTN),
    ENTRY(WM_CTLCOLORDLG),
    ENTRY(WM_CTLCOLORSCROLLBAR),
    ENTRY(WM_CTLCOLORSTATIC),
    ENTRY(CB_GETEDITSEL),
    ENTRY(CB_LIMITTEXT),
    ENTRY(CB_SETEDITSEL),
    ENTRY(CB_ADDSTRING),
    ENTRY(CB_DELETESTRING),
    ENTRY(CB_DIR),
    ENTRY(CB_GETCOUNT),
    ENTRY(CB_GETCURSEL),
    ENTRY(CB_GETLBTEXT),
    ENTRY(CB_GETLBTEXTLEN),
    ENTRY(CB_INSERTSTRING),
    ENTRY(CB_RESETCONTENT),
    ENTRY(CB_FINDSTRING),
    ENTRY(CB_SELECTSTRING),
    ENTRY(CB_SETCURSEL),
    ENTRY(CB_SHOWDROPDOWN),
    ENTRY(CB_GETITEMDATA),
    ENTRY(CB_SETITEMDATA),
    ENTRY(CB_GETDROPPEDCONTROLRECT),
    ENTRY(CB_SETITEMHEIGHT),
    ENTRY(CB_GETITEMHEIGHT),
    ENTRY(CB_SETEXTENDEDUI),
    ENTRY(CB_GETEXTENDEDUI),
    ENTRY(CB_GETDROPPEDSTATE),
    ENTRY(CB_FINDSTRINGEXACT),
    ENTRY(CB_SETLOCALE),
    ENTRY(CB_GETLOCALE),
    ENTRY(CB_GETTOPINDEX),
    ENTRY(CB_SETTOPINDEX),
    ENTRY(CB_GETHORIZONTALEXTENT),
    ENTRY(CB_SETHORIZONTALEXTENT),
    ENTRY(CB_GETDROPPEDWIDTH),
    ENTRY(CB_SETDROPPEDWIDTH),
    ENTRY(CB_INITSTORAGE),
    ENTRY(CB_MSGMAX),
    ENTRY(LB_ADDSTRING),
    ENTRY(LB_INSERTSTRING),
    ENTRY(LB_DELETESTRING),
    ENTRY(LB_SELITEMRANGEEX),
    ENTRY(LB_RESETCONTENT),
    ENTRY(LB_SETSEL),
    ENTRY(LB_SETCURSEL),
    ENTRY(LB_GETSEL),
    ENTRY(LB_GETCURSEL),
    ENTRY(LB_GETTEXT),
    ENTRY(LB_GETTEXTLEN),
    ENTRY(LB_GETCOUNT),
    ENTRY(LB_SELECTSTRING),
    ENTRY(LB_DIR),
    ENTRY(LB_GETTOPINDEX),
    ENTRY(LB_FINDSTRING),
    ENTRY(LB_GETSELCOUNT),
    ENTRY(LB_GETSELITEMS),
    ENTRY(LB_SETTABSTOPS),
    ENTRY(LB_GETHORIZONTALEXTENT),
    ENTRY(LB_SETHORIZONTALEXTENT),
    ENTRY(LB_SETCOLUMNWIDTH),
    ENTRY(LB_ADDFILE),
    ENTRY(LB_SETTOPINDEX),
    ENTRY(LB_GETITEMRECT),
    ENTRY(LB_GETITEMDATA),
    ENTRY(LB_SETITEMDATA),
    ENTRY(LB_SELITEMRANGE),
    ENTRY(LB_SETANCHORINDEX),
    ENTRY(LB_GETANCHORINDEX),
    ENTRY(LB_SETCARETINDEX),
    ENTRY(LB_GETCARETINDEX),
    ENTRY(LB_SETITEMHEIGHT),
    ENTRY(LB_GETITEMHEIGHT),
    ENTRY(LB_FINDSTRINGEXACT),
    ENTRY(LB_SETLOCALE),
    ENTRY(LB_GETLOCALE),
    ENTRY(LB_SETCOUNT),
    ENTRY(LB_INITSTORAGE),
    ENTRY(LB_ITEMFROMPOINT),
    ENTRY(LB_MSGMAX),
    ENTRY(WM_MOUSEMOVE),
    ENTRY(WM_LBUTTONDOWN),
    ENTRY(WM_LBUTTONUP),
    ENTRY(WM_LBUTTONDBLCLK),
    ENTRY(WM_RBUTTONDOWN),
    ENTRY(WM_RBUTTONUP),
    ENTRY(WM_RBUTTONDBLCLK),
    ENTRY(WM_MBUTTONDOWN),
    ENTRY(WM_MBUTTONUP),
    ENTRY(WM_MBUTTONDBLCLK),
    ENTRY(WM_MOUSEWHEEL),
    ENTRY(WM_MOUSEHWHEEL),
    ENTRY(WM_PARENTNOTIFY),
    ENTRY(WM_ENTERMENULOOP),
    ENTRY(WM_EXITMENULOOP),
    ENTRY(WM_NEXTMENU),
    ENTRY(WM_SIZING),
    ENTRY(WM_CAPTURECHANGED),
    ENTRY(WM_MOVING),
    ENTRY(WM_POWERBROADCAST),
    ENTRY(WM_DEVICECHANGE),
    ENTRY(WM_MDICREATE),
    ENTRY(WM_MDIDESTROY),
    ENTRY(WM_MDIACTIVATE),
    ENTRY(WM_MDIRESTORE),
    ENTRY(WM_MDINEXT),
    ENTRY(WM_MDIMAXIMIZE),
    ENTRY(WM_MDITILE),
    ENTRY(WM_MDICASCADE),
    ENTRY(WM_MDIICONARRANGE),
    ENTRY(WM_MDIGETACTIVE),
    ENTRY(WM_MDISETMENU),
    ENTRY(WM_ENTERSIZEMOVE),
    ENTRY(WM_EXITSIZEMOVE),
    ENTRY(WM_DROPFILES),
    ENTRY(WM_MDIREFRESHMENU),
    ENTRY(WM_IME_SETCONTEXT),
    ENTRY(WM_IME_NOTIFY),
    ENTRY(WM_IME_CONTROL),
    ENTRY(WM_IME_COMPOSITIONFULL),
    ENTRY(WM_IME_SELECT),
    ENTRY(WM_IME_CHAR),
    ENTRY(WM_IME_REQUEST),
    ENTRY(WM_IME_KEYDOWN),
    ENTRY(WM_IME_KEYUP),
    ENTRY(WM_NCMOUSEHOVER),
    ENTRY(WM_MOUSEHOVER),
    ENTRY(WM_MOUSELEAVE),
    ENTRY(WM_CUT),
    ENTRY(WM_COPY),
    ENTRY(WM_PASTE),
    ENTRY(WM_CLEAR),
    ENTRY(WM_UNDO),
    ENTRY(WM_RENDERFORMAT),
    ENTRY(WM_RENDERALLFORMATS),
    ENTRY(WM_DESTROYCLIPBOARD),
    ENTRY(WM_DRAWCLIPBOARD),
    ENTRY(WM_PAINTCLIPBOARD),
    ENTRY(WM_VSCROLLCLIPBOARD),
    ENTRY(WM_SIZECLIPBOARD),
    ENTRY(WM_ASKCBFORMATNAME),
    ENTRY(WM_CHANGECBCHAIN),
    ENTRY(WM_HSCROLLCLIPBOARD),
    ENTRY(WM_QUERYNEWPALETTE),
    ENTRY(WM_PALETTEISCHANGING),
    ENTRY(WM_PALETTECHANGED),
    ENTRY(WM_HOTKEY),
    ENTRY(WM_PRINT),
    ENTRY(WM_PRINTCLIENT),
    ENTRY(WM_THEMECHANGED),
    ENTRY(WM_HANDHELDFIRST),
    ENTRY(WM_HANDHELDLAST),
    ENTRY(WM_AFXFIRST),
    ENTRY(WM_AFXLAST),
    ENTRY(WM_PENWINFIRST),
    ENTRY(WM_PENWINLAST),
    ENTRY(WM_APP),
    ENTRY(WM_DWMCOMPOSITIONCHANGED),
    ENTRY(WM_DWMNCRENDERINGCHANGED),
    ENTRY(WM_DWMCOLORIZATIONCOLORCHANGED),
    ENTRY(WM_DWMWINDOWMAXIMIZEDCHANGE),
    ENTRY(WM_DWMSENDICONICTHUMBNAIL),
    ENTRY(WM_DWMSENDICONICLIVEPREVIEWBITMAP),
    ENTRY(WM_TABLET_QUERYSYSTEMGESTURESTATUS),
    ENTRY(WM_GESTURE),
    ENTRY(WM_GESTURENOTIFY),
    ENTRY(WM_GETTITLEBARINFOEX),
    {0x0, {nullptr, 0x0}}};
#undef ENTRY

#ifdef MOZ_PLACES
NS_IMPL_ISUPPORTS(myDownloadObserver, nsIDownloadObserver)
NS_IMPL_ISUPPORTS(AsyncFaviconDataReady, nsIFaviconDataCallback)
#endif
NS_IMPL_ISUPPORTS(AsyncEncodeAndWriteIcon, nsIRunnable)
NS_IMPL_ISUPPORTS(AsyncDeleteAllFaviconsFromDisk, nsIRunnable)

const char FaviconHelper::kJumpListCacheDir[] = "jumpListCache";
const char FaviconHelper::kShortcutCacheDir[] = "shortcutCache";

// Prefix for path used by NT calls.
const wchar_t kNTPrefix[] = L"\\??\\";
const size_t kNTPrefixLen = ArrayLength(kNTPrefix) - 1;

struct CoTaskMemFreePolicy {
  void operator()(void* aPtr) { ::CoTaskMemFree(aPtr); }
};

SetThreadDpiAwarenessContextProc WinUtils::sSetThreadDpiAwarenessContext = NULL;
EnableNonClientDpiScalingProc WinUtils::sEnableNonClientDpiScaling = NULL;
GetSystemMetricsForDpiProc WinUtils::sGetSystemMetricsForDpi = NULL;
bool WinUtils::sHasPackageIdentity = false;

using GetDpiForWindowProc = UINT(WINAPI*)(HWND);
static GetDpiForWindowProc sGetDpiForWindow = NULL;

/* static */
void WinUtils::Initialize() {
  // Dpi-Awareness is not supported with Win32k Lockdown enabled, so we don't
  // initialize DPI-related members and assert later that nothing accidently
  // uses these static members
  if (IsWin10OrLater() && !IsWin32kLockedDown()) {
    HMODULE user32Dll = ::GetModuleHandleW(L"user32");
    if (user32Dll) {
      auto getThreadDpiAwarenessContext =
          (decltype(GetThreadDpiAwarenessContext)*)::GetProcAddress(
              user32Dll, "GetThreadDpiAwarenessContext");
      auto areDpiAwarenessContextsEqual =
          (decltype(AreDpiAwarenessContextsEqual)*)::GetProcAddress(
              user32Dll, "AreDpiAwarenessContextsEqual");
      if (getThreadDpiAwarenessContext && areDpiAwarenessContextsEqual &&
          areDpiAwarenessContextsEqual(
              getThreadDpiAwarenessContext(),
              DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE)) {
        // Only per-monitor v1 requires these workarounds.
        sEnableNonClientDpiScaling =
            (EnableNonClientDpiScalingProc)::GetProcAddress(
                user32Dll, "EnableNonClientDpiScaling");
        sSetThreadDpiAwarenessContext =
            (SetThreadDpiAwarenessContextProc)::GetProcAddress(
                user32Dll, "SetThreadDpiAwarenessContext");
      }

      sGetSystemMetricsForDpi = (GetSystemMetricsForDpiProc)::GetProcAddress(
          user32Dll, "GetSystemMetricsForDpi");
      sGetDpiForWindow =
          (GetDpiForWindowProc)::GetProcAddress(user32Dll, "GetDpiForWindow");
    }
  }

  if (IsWin8OrLater()) {
    sHasPackageIdentity = mozilla::HasPackageIdentity();
  }
}

// static
LRESULT WINAPI WinUtils::NonClientDpiScalingDefWindowProcW(HWND hWnd, UINT msg,
                                                           WPARAM wParam,
                                                           LPARAM lParam) {
  MOZ_DIAGNOSTIC_ASSERT(!IsWin32kLockedDown());

  // NOTE: this function was copied out into the body of the pre-XUL skeleton
  // UI window proc (PreXULSkeletonUI.cpp). If this function changes at any
  // point, we should probably factor this out and use it from both locations.
  if (msg == WM_NCCREATE && sEnableNonClientDpiScaling) {
    sEnableNonClientDpiScaling(hWnd);
  }
  return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

// static
void WinUtils::LogW(const wchar_t* fmt, ...) {
  va_list args = nullptr;
  if (!lstrlenW(fmt)) {
    return;
  }
  va_start(args, fmt);
  int buflen = _vscwprintf(fmt, args);
  wchar_t* buffer = new wchar_t[buflen + 1];
  if (!buffer) {
    va_end(args);
    return;
  }
  vswprintf(buffer, buflen, fmt, args);
  va_end(args);

  // MSVC, including remote debug sessions
  OutputDebugStringW(buffer);
  OutputDebugStringW(L"\n");

  int len =
      WideCharToMultiByte(CP_ACP, 0, buffer, -1, nullptr, 0, nullptr, nullptr);
  if (len) {
    char* utf8 = new char[len];
    if (WideCharToMultiByte(CP_ACP, 0, buffer, -1, utf8, len, nullptr,
                            nullptr) > 0) {
      // desktop console
      printf("%s\n", utf8);
      NS_ASSERTION(gWindowsLog,
                   "Called WinUtils Log() but Widget "
                   "log module doesn't exist!");
      MOZ_LOG(gWindowsLog, LogLevel::Error, ("%s", utf8));
    }
    delete[] utf8;
  }
  delete[] buffer;
}

// static
void WinUtils::Log(const char* fmt, ...) {
  va_list args = nullptr;
  if (!strlen(fmt)) {
    return;
  }
  va_start(args, fmt);
  int buflen = _vscprintf(fmt, args);
  char* buffer = new char[buflen + 1];
  if (!buffer) {
    va_end(args);
    return;
  }
  vsprintf(buffer, fmt, args);
  va_end(args);

  // MSVC, including remote debug sessions
  OutputDebugStringA(buffer);
  OutputDebugStringW(L"\n");

  // desktop console
  printf("%s\n", buffer);

  NS_ASSERTION(gWindowsLog,
               "Called WinUtils Log() but Widget "
               "log module doesn't exist!");
  MOZ_LOG(gWindowsLog, LogLevel::Error, ("%s", buffer));
  delete[] buffer;
}

// static
float WinUtils::SystemDPI() {
  if (XRE_IsContentProcess()) {
    return WinContentSystemParameters::GetSingleton()->SystemDPI();
  }
  // The result of GetDeviceCaps won't change dynamically, as it predates
  // per-monitor DPI and support for on-the-fly resolution changes.
  // Therefore, we only need to look it up once.
  static float dpi = 0;
  if (dpi <= 0) {
    HDC screenDC = GetDC(nullptr);
    dpi = GetDeviceCaps(screenDC, LOGPIXELSY);
    ReleaseDC(nullptr, screenDC);
  }

  // Bug 1012487 - dpi can be 0 when the Screen DC is used off the
  // main thread on windows. For now just assume a 100% DPI for this
  // drawing call.
  // XXX - fixme!
  return dpi > 0 ? dpi : 96;
}

// static
double WinUtils::SystemScaleFactor() { return SystemDPI() / 96.0; }

#if WINVER < 0x603
typedef enum {
  MDT_EFFECTIVE_DPI = 0,
  MDT_ANGULAR_DPI = 1,
  MDT_RAW_DPI = 2,
  MDT_DEFAULT = MDT_EFFECTIVE_DPI
} MONITOR_DPI_TYPE;

typedef enum {
  PROCESS_DPI_UNAWARE = 0,
  PROCESS_SYSTEM_DPI_AWARE = 1,
  PROCESS_PER_MONITOR_DPI_AWARE = 2
} PROCESS_DPI_AWARENESS;
#endif

typedef HRESULT(WINAPI* GETDPIFORMONITORPROC)(HMONITOR, MONITOR_DPI_TYPE, UINT*,
                                              UINT*);

typedef HRESULT(WINAPI* GETPROCESSDPIAWARENESSPROC)(HANDLE,
                                                    PROCESS_DPI_AWARENESS*);

GETDPIFORMONITORPROC sGetDpiForMonitor;
GETPROCESSDPIAWARENESSPROC sGetProcessDpiAwareness;

static bool SlowIsPerMonitorDPIAware() {
  // Intentionally leak the handle.
  HMODULE shcore = LoadLibraryEx(L"shcore", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
  if (shcore) {
    sGetDpiForMonitor =
        (GETDPIFORMONITORPROC)GetProcAddress(shcore, "GetDpiForMonitor");
    sGetProcessDpiAwareness = (GETPROCESSDPIAWARENESSPROC)GetProcAddress(
        shcore, "GetProcessDpiAwareness");
  }
  PROCESS_DPI_AWARENESS dpiAwareness;
  return sGetDpiForMonitor && sGetProcessDpiAwareness &&
         SUCCEEDED(
             sGetProcessDpiAwareness(GetCurrentProcess(), &dpiAwareness)) &&
         dpiAwareness == PROCESS_PER_MONITOR_DPI_AWARE;
}

/* static */
bool WinUtils::IsPerMonitorDPIAware() {
  if (XRE_IsContentProcess()) {
    return WinContentSystemParameters::GetSingleton()->IsPerMonitorDPIAware();
  }
  static bool perMonitorDPIAware = SlowIsPerMonitorDPIAware();
  return perMonitorDPIAware;
}

/* static */
float WinUtils::MonitorDPI(HMONITOR aMonitor) {
  if (IsPerMonitorDPIAware()) {
    UINT dpiX, dpiY = 96;
    sGetDpiForMonitor(aMonitor ? aMonitor : GetPrimaryMonitor(),
                      MDT_EFFECTIVE_DPI, &dpiX, &dpiY);
    return dpiY;
  }

  // We're not per-monitor aware, use system DPI instead.
  return SystemDPI();
}

/* static */
double WinUtils::LogToPhysFactor(HMONITOR aMonitor) {
  return MonitorDPI(aMonitor) / 96.0;
}

/* static */
int32_t WinUtils::LogToPhys(HMONITOR aMonitor, double aValue) {
  return int32_t(NS_round(aValue * LogToPhysFactor(aMonitor)));
}

/* static */
double WinUtils::LogToPhysFactor(HWND aWnd) {
  // if there's an ancestor window, we want to share its DPI setting
  HWND ancestor = ::GetAncestor(aWnd, GA_ROOTOWNER);

  // The GetDpiForWindow api is not available everywhere where we run as
  // per-monitor, but if it is available rely on it to tell us the scale
  // factor of the window.  See bug 1722085.
  if (sGetDpiForWindow) {
    UINT dpi = sGetDpiForWindow(ancestor ? ancestor : aWnd);
    if (dpi > 0) {
      return static_cast<double>(dpi) / 96.0;
    }
  }
  return LogToPhysFactor(::MonitorFromWindow(ancestor ? ancestor : aWnd,
                                             MONITOR_DEFAULTTOPRIMARY));
}

/* static */
HMONITOR
WinUtils::GetPrimaryMonitor() {
  const POINT pt = {0, 0};
  return ::MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY);
}

/* static */
HMONITOR
WinUtils::MonitorFromRect(const gfx::Rect& rect) {
  // convert coordinates from desktop to device pixels for MonitorFromRect
  double dpiScale =
      IsPerMonitorDPIAware() ? 1.0 : LogToPhysFactor(GetPrimaryMonitor());

  RECT globalWindowBounds = {NSToIntRound(dpiScale * rect.X()),
                             NSToIntRound(dpiScale * rect.Y()),
                             NSToIntRound(dpiScale * (rect.XMost())),
                             NSToIntRound(dpiScale * (rect.YMost()))};

  return ::MonitorFromRect(&globalWindowBounds, MONITOR_DEFAULTTONEAREST);
}

/* static */
bool WinUtils::HasSystemMetricsForDpi() {
  MOZ_DIAGNOSTIC_ASSERT(!IsWin32kLockedDown());
  return (sGetSystemMetricsForDpi != NULL);
}

/* static */
int WinUtils::GetSystemMetricsForDpi(int nIndex, UINT dpi) {
  MOZ_DIAGNOSTIC_ASSERT(!IsWin32kLockedDown());
  if (HasSystemMetricsForDpi()) {
    return sGetSystemMetricsForDpi(nIndex, dpi);
  } else {
    double scale = IsPerMonitorDPIAware() ? dpi / SystemDPI() : 1.0;
    return NSToIntRound(::GetSystemMetrics(nIndex) * scale);
  }
}

/* static */
gfx::MarginDouble WinUtils::GetUnwriteableMarginsForDeviceInInches(HDC aHdc) {
  if (!aHdc) {
    return gfx::MarginDouble();
  }

  int pixelsPerInchY = ::GetDeviceCaps(aHdc, LOGPIXELSY);
  int marginTop = ::GetDeviceCaps(aHdc, PHYSICALOFFSETY);
  int printableAreaHeight = ::GetDeviceCaps(aHdc, VERTRES);
  int physicalHeight = ::GetDeviceCaps(aHdc, PHYSICALHEIGHT);

  double marginTopInch = double(marginTop) / pixelsPerInchY;

  double printableAreaHeightInch = double(printableAreaHeight) / pixelsPerInchY;
  double physicalHeightInch = double(physicalHeight) / pixelsPerInchY;
  double marginBottomInch =
      physicalHeightInch - printableAreaHeightInch - marginTopInch;

  int pixelsPerInchX = ::GetDeviceCaps(aHdc, LOGPIXELSX);
  int marginLeft = ::GetDeviceCaps(aHdc, PHYSICALOFFSETX);
  int printableAreaWidth = ::GetDeviceCaps(aHdc, HORZRES);
  int physicalWidth = ::GetDeviceCaps(aHdc, PHYSICALWIDTH);

  double marginLeftInch = double(marginLeft) / pixelsPerInchX;

  double printableAreaWidthInch = double(printableAreaWidth) / pixelsPerInchX;
  double physicalWidthInch = double(physicalWidth) / pixelsPerInchX;
  double marginRightInch =
      physicalWidthInch - printableAreaWidthInch - marginLeftInch;

  return gfx::MarginDouble(marginTopInch, marginRightInch, marginBottomInch,
                           marginLeftInch);
}

#ifdef ACCESSIBILITY
/* static */
a11y::LocalAccessible* WinUtils::GetRootAccessibleForHWND(HWND aHwnd) {
  nsWindow* window = GetNSWindowPtr(aHwnd);
  if (!window) {
    return nullptr;
  }

  return window->GetAccessible();
}
#endif  // ACCESSIBILITY

/* static */
bool WinUtils::PeekMessage(LPMSG aMsg, HWND aWnd, UINT aFirstMessage,
                           UINT aLastMessage, UINT aOption) {
  RefPtr<ITfMessagePump> msgPump = TSFTextStore::GetMessagePump();
  if (msgPump) {
    BOOL ret = FALSE;
    HRESULT hr = msgPump->PeekMessageW(aMsg, aWnd, aFirstMessage, aLastMessage,
                                       aOption, &ret);
    NS_ENSURE_TRUE(SUCCEEDED(hr), false);
    return ret;
  }
  return ::PeekMessageW(aMsg, aWnd, aFirstMessage, aLastMessage, aOption);
}

/* static */
bool WinUtils::GetMessage(LPMSG aMsg, HWND aWnd, UINT aFirstMessage,
                          UINT aLastMessage) {
  RefPtr<ITfMessagePump> msgPump = TSFTextStore::GetMessagePump();
  if (msgPump) {
    BOOL ret = FALSE;
    HRESULT hr =
        msgPump->GetMessageW(aMsg, aWnd, aFirstMessage, aLastMessage, &ret);
    NS_ENSURE_TRUE(SUCCEEDED(hr), false);
    return ret;
  }
  return ::GetMessageW(aMsg, aWnd, aFirstMessage, aLastMessage);
}

#if defined(ACCESSIBILITY)
static DWORD GetWaitFlags() {
  DWORD result = MWMO_INPUTAVAILABLE;
  if (XRE_IsContentProcess()) {
    result |= MWMO_ALERTABLE;
  }
  return result;
}
#endif

/* static */
void WinUtils::WaitForMessage(DWORD aTimeoutMs) {
#if defined(ACCESSIBILITY)
  static const DWORD waitFlags = GetWaitFlags();
#else
  const DWORD waitFlags = MWMO_INPUTAVAILABLE;
#endif

  const DWORD waitStart = ::GetTickCount();
  DWORD elapsed = 0;
  while (true) {
    if (aTimeoutMs != INFINITE) {
      elapsed = ::GetTickCount() - waitStart;
    }
    if (elapsed >= aTimeoutMs) {
      break;
    }
    DWORD result;
    {
      AUTO_PROFILER_THREAD_SLEEP;
      result = ::MsgWaitForMultipleObjectsEx(0, NULL, aTimeoutMs - elapsed,
                                             MOZ_QS_ALLEVENT, waitFlags);
    }
    NS_WARNING_ASSERTION(result != WAIT_FAILED, "Wait failed");
    if (result == WAIT_TIMEOUT) {
      break;
    }
#if defined(ACCESSIBILITY)
    if (result == WAIT_IO_COMPLETION) {
      if (NS_IsMainThread()) {
        // We executed an APC that would have woken up the hang monitor. Since
        // there are no more APCs pending and we are now going to sleep again,
        // we should notify the hang monitor.
        mozilla::BackgroundHangMonitor().NotifyWait();
      }
      continue;
    }
#endif  // defined(ACCESSIBILITY)

    // Sent messages (via SendMessage and friends) are processed differently
    // than queued messages (via PostMessage); the destination window procedure
    // of the sent message is called during (Get|Peek)Message. Since PeekMessage
    // does not tell us whether it processed any sent messages, we need to query
    // this ahead of time.
    bool haveSentMessagesPending =
        (HIWORD(::GetQueueStatus(QS_SENDMESSAGE)) & QS_SENDMESSAGE) != 0;

    MSG msg = {0};
    if (haveSentMessagesPending ||
        ::PeekMessageW(&msg, nullptr, 0, 0, PM_NOREMOVE)) {
      break;
    }
    // The message is intended for another thread that has been synchronized
    // with our input queue; yield to give other threads an opportunity to
    // process the message. This should prevent busy waiting if resumed due
    // to another thread's message.
    ::SwitchToThread();
  }
}

/* static */
bool WinUtils::GetRegistryKey(HKEY aRoot, char16ptr_t aKeyName,
                              char16ptr_t aValueName, wchar_t* aBuffer,
                              DWORD aBufferLength) {
  MOZ_ASSERT(aKeyName, "The key name is NULL");

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

  DWORD type;
  result = ::RegQueryValueExW(key, aValueName, nullptr, &type, (BYTE*)aBuffer,
                              &aBufferLength);
  ::RegCloseKey(key);
  if (result != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ)) {
    return false;
  }
  if (aBuffer) {
    aBuffer[aBufferLength / sizeof(*aBuffer) - 1] = 0;
  }
  return true;
}

/* static */
bool WinUtils::HasRegistryKey(HKEY aRoot, char16ptr_t aKeyName) {
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
HWND WinUtils::GetTopLevelHWND(HWND aWnd, bool aStopIfNotChild,
                               bool aStopIfNotPopup) {
  HWND curWnd = aWnd;
  HWND topWnd = nullptr;

  while (curWnd) {
    topWnd = curWnd;

    if (aStopIfNotChild) {
      DWORD_PTR style = ::GetWindowLongPtrW(curWnd, GWL_STYLE);

      VERIFY_WINDOW_STYLE(style);

      if (!(style & WS_CHILD))  // first top-level window
        break;
    }

    HWND upWnd = ::GetParent(curWnd);  // Parent or owner (if has no parent)

    // GetParent will only return the owner if the passed in window
    // has the WS_POPUP style.
    if (!upWnd && !aStopIfNotPopup) {
      upWnd = ::GetWindow(curWnd, GW_OWNER);
    }
    curWnd = upWnd;
  }

  return topWnd;
}

// Map from native window handles to nsWindow structures. Does not AddRef.
// Inherently unsafe to access outside the main thread.
static nsTHashMap<HWND, nsWindow*> sExtantNSWindows;

/* static */
void WinUtils::SetNSWindowPtr(HWND aWnd, nsWindow* aWindow) {
  MOZ_ASSERT(NS_IsMainThread());
  if (!aWindow) {
    sExtantNSWindows.Remove(aWnd);
  } else {
    sExtantNSWindows.InsertOrUpdate(aWnd, aWindow);
  }
}

/* static */
nsWindow* WinUtils::GetNSWindowPtr(HWND aWnd) {
  MOZ_ASSERT(NS_IsMainThread());
  return sExtantNSWindows.Get(aWnd);  // or nullptr
}

static BOOL CALLBACK AddMonitor(HMONITOR, HDC, LPRECT, LPARAM aParam) {
  (*(int32_t*)aParam)++;
  return TRUE;
}

/* static */
int32_t WinUtils::GetMonitorCount() {
  int32_t monitorCount = 0;
  EnumDisplayMonitors(nullptr, nullptr, AddMonitor, (LPARAM)&monitorCount);
  return monitorCount;
}

/* static */
bool WinUtils::IsOurProcessWindow(HWND aWnd) {
  if (!aWnd) {
    return false;
  }
  DWORD processId = 0;
  ::GetWindowThreadProcessId(aWnd, &processId);
  return (processId == ::GetCurrentProcessId());
}

/* static */
HWND WinUtils::FindOurProcessWindow(HWND aWnd) {
  for (HWND wnd = ::GetParent(aWnd); wnd; wnd = ::GetParent(wnd)) {
    if (IsOurProcessWindow(wnd)) {
      return wnd;
    }
  }
  return nullptr;
}

static bool IsPointInWindow(HWND aWnd, const POINT& aPointInScreen) {
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

static HWND FindTopmostWindowAtPoint(HWND aWnd, const POINT& aPointInScreen) {
  if (!::IsWindowVisible(aWnd) || !IsPointInWindow(aWnd, aPointInScreen)) {
    return nullptr;
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

struct FindOurWindowAtPointInfo {
  POINT mInPointInScreen;
  HWND mOutWnd;
};

static BOOL CALLBACK FindOurWindowAtPointCallback(HWND aWnd, LPARAM aLPARAM) {
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
HWND WinUtils::FindOurWindowAtPoint(const POINT& aPointInScreen) {
  FindOurWindowAtPointInfo info;
  info.mInPointInScreen = aPointInScreen;
  info.mOutWnd = nullptr;

  // This will enumerate all top-level windows in order from top to bottom.
  EnumWindows(FindOurWindowAtPointCallback, reinterpret_cast<LPARAM>(&info));
  return info.mOutWnd;
}

/* static */
UINT WinUtils::GetInternalMessage(UINT aNativeMessage) {
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
UINT WinUtils::GetNativeMessage(UINT aInternalMessage) {
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
uint16_t WinUtils::GetMouseInputSource() {
  int32_t inputSource = dom::MouseEvent_Binding::MOZ_SOURCE_MOUSE;
  LPARAM lParamExtraInfo = ::GetMessageExtraInfo();
  if ((lParamExtraInfo & TABLET_INK_SIGNATURE) == TABLET_INK_CHECK) {
    inputSource = (lParamExtraInfo & TABLET_INK_TOUCH)
                      ? dom::MouseEvent_Binding::MOZ_SOURCE_TOUCH
                      : dom::MouseEvent_Binding::MOZ_SOURCE_PEN;
  }
  return static_cast<uint16_t>(inputSource);
}

/* static */
uint16_t WinUtils::GetMousePointerID() {
  LPARAM lParamExtraInfo = ::GetMessageExtraInfo();
  return lParamExtraInfo & TABLET_INK_ID_MASK;
}

/* static */
bool WinUtils::GetIsMouseFromTouch(EventMessage aEventMessage) {
  const uint32_t MOZ_T_I_SIGNATURE = TABLET_INK_TOUCH | TABLET_INK_SIGNATURE;
  const uint32_t MOZ_T_I_CHECK_TCH = TABLET_INK_TOUCH | TABLET_INK_CHECK;
  return ((aEventMessage == eMouseMove || aEventMessage == eMouseDown ||
           aEventMessage == eMouseUp || aEventMessage == eMouseAuxClick ||
           aEventMessage == eMouseDoubleClick) &&
          (GetMessageExtraInfo() & MOZ_T_I_SIGNATURE) == MOZ_T_I_CHECK_TCH);
}

/* static */
MSG WinUtils::InitMSG(UINT aMessage, WPARAM wParam, LPARAM lParam, HWND aWnd) {
  MSG msg;
  msg.message = aMessage;
  msg.wParam = wParam;
  msg.lParam = lParam;
  msg.hwnd = aWnd;
  return msg;
}

static BOOL WINAPI EnumFirstChild(HWND hwnd, LPARAM lParam) {
  *((HWND*)lParam) = hwnd;
  return FALSE;
}

/* static */
void WinUtils::InvalidatePluginAsWorkaround(nsIWidget* aWidget,
                                            const LayoutDeviceIntRect& aRect) {
  aWidget->Invalidate(aRect);

  // XXX - Even more evil workaround!! See bug 762948, flash's bottom
  // level sandboxed window doesn't seem to get our invalidate. We send
  // an invalidate to it manually. This is totally specialized for this
  // bug, for other child window structures this will just be a more or
  // less bogus invalidate but since that should not have any bad
  // side-effects this will have to do for now.
  HWND current = (HWND)aWidget->GetNativeData(NS_NATIVE_WINDOW);

  RECT windowRect;
  RECT parentRect;

  ::GetWindowRect(current, &parentRect);

  HWND next = current;
  do {
    current = next;
    ::EnumChildWindows(current, &EnumFirstChild, (LPARAM)&next);
    ::GetWindowRect(next, &windowRect);
    // This is relative to the screen, adjust it to be relative to the
    // window we're reconfiguring.
    windowRect.left -= parentRect.left;
    windowRect.top -= parentRect.top;
  } while (next != current && windowRect.top == 0 && windowRect.left == 0);

  if (windowRect.top == 0 && windowRect.left == 0) {
    RECT rect;
    rect.left = aRect.X();
    rect.top = aRect.Y();
    rect.right = aRect.XMost();
    rect.bottom = aRect.YMost();

    ::InvalidateRect(next, &rect, FALSE);
  }
}

#ifdef MOZ_PLACES
/************************************************************************
 * Constructs as AsyncFaviconDataReady Object
 * @param aIOThread : the thread which performs the action
 * @param aURLShortcut : Differentiates between (false)Jumplistcache and
 *                       (true)Shortcutcache
 * @param aRunnable : Executed in the aIOThread when the favicon cache is
 *                    avaiable
 ************************************************************************/

AsyncFaviconDataReady::AsyncFaviconDataReady(
    nsIURI* aNewURI, nsCOMPtr<nsIThread>& aIOThread, const bool aURLShortcut,
    already_AddRefed<nsIRunnable> aRunnable)
    : mNewURI(aNewURI),
      mIOThread(aIOThread),
      mRunnable(aRunnable),
      mURLShortcut(aURLShortcut) {}

NS_IMETHODIMP
myDownloadObserver::OnDownloadComplete(nsIDownloader* downloader,
                                       nsIRequest* request, nsresult status,
                                       nsIFile* result) {
  return NS_OK;
}

nsresult AsyncFaviconDataReady::OnFaviconDataNotAvailable(void) {
  if (!mURLShortcut) {
    return NS_OK;
  }

  nsCOMPtr<nsIFile> icoFile;
  nsresult rv =
      FaviconHelper::GetOutputIconPath(mNewURI, icoFile, mURLShortcut);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIURI> mozIconURI;
  rv = NS_NewURI(getter_AddRefs(mozIconURI), "moz-icon://.html?size=32");
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannel(getter_AddRefs(channel), mozIconURI,
                     nsContentUtils::GetSystemPrincipal(),
                     nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
                     nsIContentPolicy::TYPE_INTERNAL_IMAGE);

  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDownloadObserver> downloadObserver = new myDownloadObserver;
  nsCOMPtr<nsIStreamListener> listener;
  rv = NS_NewDownloader(getter_AddRefs(listener), downloadObserver, icoFile);
  NS_ENSURE_SUCCESS(rv, rv);

  return channel->AsyncOpen(listener);
}

NS_IMETHODIMP
AsyncFaviconDataReady::OnComplete(nsIURI* aFaviconURI, uint32_t aDataLen,
                                  const uint8_t* aData,
                                  const nsACString& aMimeType,
                                  uint16_t aWidth) {
  if (!aDataLen || !aData) {
    if (mURLShortcut) {
      OnFaviconDataNotAvailable();
    }

    return NS_OK;
  }

  nsCOMPtr<nsIFile> icoFile;
  nsresult rv =
      FaviconHelper::GetOutputIconPath(mNewURI, icoFile, mURLShortcut);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString path;
  rv = icoFile->GetPath(path);
  NS_ENSURE_SUCCESS(rv, rv);

  // Decode the image from the format it was returned to us in (probably PNG)
  nsCOMPtr<imgIContainer> container;
  nsCOMPtr<imgITools> imgtool = do_CreateInstance("@mozilla.org/image/tools;1");
  rv = imgtool->DecodeImageFromBuffer(reinterpret_cast<const char*>(aData),
                                      aDataLen, aMimeType,
                                      getter_AddRefs(container));
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<SourceSurface> surface = container->GetFrame(
      imgIContainer::FRAME_FIRST,
      imgIContainer::FLAG_SYNC_DECODE | imgIContainer::FLAG_ASYNC_NOTIFY);
  NS_ENSURE_TRUE(surface, NS_ERROR_FAILURE);

  RefPtr<DataSourceSurface> dataSurface;
  IntSize size;

  if (mURLShortcut &&
      (surface->GetSize().width < 48 || surface->GetSize().height < 48)) {
    // Create a 48x48 surface and paint the icon into the central rect.
    size.width = std::max(surface->GetSize().width, 48);
    size.height = std::max(surface->GetSize().height, 48);
    dataSurface =
        Factory::CreateDataSourceSurface(size, SurfaceFormat::B8G8R8A8);
    NS_ENSURE_TRUE(dataSurface, NS_ERROR_FAILURE);

    DataSourceSurface::MappedSurface map;
    if (!dataSurface->Map(DataSourceSurface::MapType::WRITE, &map)) {
      return NS_ERROR_FAILURE;
    }

    RefPtr<DrawTarget> dt = Factory::CreateDrawTargetForData(
        BackendType::CAIRO, map.mData, dataSurface->GetSize(), map.mStride,
        dataSurface->GetFormat());
    if (!dt) {
      gfxWarning() << "AsyncFaviconDataReady::OnComplete failed in "
                      "CreateDrawTargetForData";
      return NS_ERROR_OUT_OF_MEMORY;
    }
    dt->FillRect(Rect(0, 0, size.width, size.height),
                 ColorPattern(ToDeviceColor(sRGBColor::OpaqueWhite())));
    IntPoint point;
    point.x = (size.width - surface->GetSize().width) / 2;
    point.y = (size.height - surface->GetSize().height) / 2;
    dt->DrawSurface(surface,
                    Rect(point.x, point.y, surface->GetSize().width,
                         surface->GetSize().height),
                    Rect(Point(0, 0), Size(surface->GetSize().width,
                                           surface->GetSize().height)));

    dataSurface->Unmap();
  } else {
    // By using the input image surface's size, we may end up encoding
    // to a different size than a 16x16 (or bigger for higher DPI) ICO, but
    // Windows will resize appropriately for us. If we want to encode ourselves
    // one day because we like our resizing better, we'd have to manually
    // resize the image here and use GetSystemMetrics w/ SM_CXSMICON and
    // SM_CYSMICON. We don't support resizing images asynchronously at the
    // moment anyway so getting the DPI aware icon size won't help.
    size.width = surface->GetSize().width;
    size.height = surface->GetSize().height;
    dataSurface = surface->GetDataSurface();
    NS_ENSURE_TRUE(dataSurface, NS_ERROR_FAILURE);
  }

  // Allocate a new buffer that we own and can use out of line in
  // another thread.
  UniquePtr<uint8_t[]> data = SurfaceToPackedBGRA(dataSurface);
  if (!data) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  int32_t stride = 4 * size.width;

  // AsyncEncodeAndWriteIcon takes ownership of the heap allocated buffer
  nsCOMPtr<nsIRunnable> event =
      new AsyncEncodeAndWriteIcon(path, std::move(data), stride, size.width,
                                  size.height, mRunnable.forget());
  mIOThread->Dispatch(event, NS_DISPATCH_NORMAL);

  return NS_OK;
}
#endif

// Warning: AsyncEncodeAndWriteIcon assumes ownership of the aData buffer passed
// in
AsyncEncodeAndWriteIcon::AsyncEncodeAndWriteIcon(
    const nsAString& aIconPath, UniquePtr<uint8_t[]> aBuffer, uint32_t aStride,
    uint32_t aWidth, uint32_t aHeight, already_AddRefed<nsIRunnable> aRunnable)
    : mIconPath(aIconPath),
      mBuffer(std::move(aBuffer)),
      mRunnable(aRunnable),
      mStride(aStride),
      mWidth(aWidth),
      mHeight(aHeight) {}

NS_IMETHODIMP AsyncEncodeAndWriteIcon::Run() {
  MOZ_ASSERT(!NS_IsMainThread(), "Should not be called on the main thread.");

  // Note that since we're off the main thread we can't use
  // gfxPlatform::GetPlatform()->ScreenReferenceDrawTarget()
  RefPtr<DataSourceSurface> surface = Factory::CreateWrappingDataSourceSurface(
      mBuffer.get(), mStride, IntSize(mWidth, mHeight),
      SurfaceFormat::B8G8R8A8);

  FILE* file = _wfopen(mIconPath.get(), L"wb");
  if (!file) {
    // Maybe the directory doesn't exist; try creating it, then fopen again.
    nsresult rv = NS_ERROR_FAILURE;
    nsCOMPtr<nsIFile> comFile = do_CreateInstance("@mozilla.org/file/local;1");
    if (comFile) {
      rv = comFile->InitWithPath(mIconPath);
      if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsIFile> dirPath;
        comFile->GetParent(getter_AddRefs(dirPath));
        if (dirPath) {
          rv = dirPath->Create(nsIFile::DIRECTORY_TYPE, 0777);
          if (NS_SUCCEEDED(rv) || rv == NS_ERROR_FILE_ALREADY_EXISTS) {
            file = _wfopen(mIconPath.get(), L"wb");
            if (!file) {
              rv = NS_ERROR_FAILURE;
            }
          }
        }
      }
    }
    if (!file) {
      return rv;
    }
  }
  nsresult rv = gfxUtils::EncodeSourceSurface(surface, ImageType::ICO, u""_ns,
                                              gfxUtils::eBinaryEncode, file);
  fclose(file);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mRunnable) {
    mRunnable->Run();
  }
  return rv;
}

AsyncEncodeAndWriteIcon::~AsyncEncodeAndWriteIcon() {}

AsyncDeleteAllFaviconsFromDisk::AsyncDeleteAllFaviconsFromDisk(
    bool aIgnoreRecent)
    : mIgnoreRecent(aIgnoreRecent) {
  // We can't call FaviconHelper::GetICOCacheSecondsTimeout() on non-main
  // threads, as it reads a pref, so cache its value here.
  mIcoNoDeleteSeconds = FaviconHelper::GetICOCacheSecondsTimeout() + 600;

  // Prepare the profile directory cache on the main thread, to ensure we wont
  // do this on non-main threads.
  Unused << NS_GetSpecialDirectory("ProfLDS",
                                   getter_AddRefs(mJumpListCacheDir));
}

NS_IMETHODIMP AsyncDeleteAllFaviconsFromDisk::Run() {
  if (!mJumpListCacheDir) {
    return NS_ERROR_FAILURE;
  }
  // Construct the path of our jump list cache
  nsresult rv = mJumpListCacheDir->AppendNative(
      nsDependentCString(FaviconHelper::kJumpListCacheDir));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDirectoryEnumerator> entries;
  rv = mJumpListCacheDir->GetDirectoryEntries(getter_AddRefs(entries));
  NS_ENSURE_SUCCESS(rv, rv);

  // Loop through each directory entry and remove all ICO files found
  do {
    nsCOMPtr<nsIFile> currFile;
    if (NS_FAILED(entries->GetNextFile(getter_AddRefs(currFile))) || !currFile)
      break;

    nsAutoString path;
    if (NS_FAILED(currFile->GetPath(path))) continue;

    if (StringTail(path, 4).LowerCaseEqualsASCII(".ico")) {
      // Check if the cached ICO file exists
      bool exists;
      if (NS_FAILED(currFile->Exists(&exists)) || !exists) continue;

      if (mIgnoreRecent) {
        // Check to make sure the icon wasn't just recently created.
        // If it was created recently, don't delete it yet.
        int64_t fileModTime = 0;
        rv = currFile->GetLastModifiedTime(&fileModTime);
        fileModTime /= PR_MSEC_PER_SEC;
        // If the icon is older than the regeneration time (+ 10 min to be
        // safe), then it's old and we can get rid of it.
        // This code is only hit directly after a regeneration.
        int64_t nowTime = PR_Now() / int64_t(PR_USEC_PER_SEC);
        if (NS_FAILED(rv) || (nowTime - fileModTime) < mIcoNoDeleteSeconds) {
          continue;
        }
      }

      // We found an ICO file that exists, so we should remove it
      currFile->Remove(false);
    }
  } while (true);

  return NS_OK;
}

AsyncDeleteAllFaviconsFromDisk::~AsyncDeleteAllFaviconsFromDisk() {}

/*
 * (static) If the data is available, will return the path on disk where
 * the favicon for page aFaviconPageURI is stored.  If the favicon does not
 * exist, or its cache is expired, this method will kick off an async request
 * for the icon so that next time the method is called it will be available.
 * @param aFaviconPageURI The URI of the page to obtain
 * @param aICOFilePath The path of the icon file
 * @param aIOThread The thread to perform the Fetch on
 * @param aURLShortcut to distinguish between jumplistcache(false) and
 *                     shortcutcache(true)
 * @param aRunnable Executed in the aIOThread when the favicon cache is
 *                  avaiable
 */
nsresult FaviconHelper::ObtainCachedIconFile(
    nsCOMPtr<nsIURI> aFaviconPageURI, nsString& aICOFilePath,
    nsCOMPtr<nsIThread>& aIOThread, bool aURLShortcut,
    already_AddRefed<nsIRunnable> aRunnable) {
  nsCOMPtr<nsIRunnable> runnable = aRunnable;
  // Obtain the ICO file path
  nsCOMPtr<nsIFile> icoFile;
  nsresult rv = GetOutputIconPath(aFaviconPageURI, icoFile, aURLShortcut);
  NS_ENSURE_SUCCESS(rv, rv);

  // Check if the cached ICO file already exists
  bool exists;
  rv = icoFile->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (exists) {
    // Obtain the file's last modification date in seconds
    int64_t fileModTime = 0;
    rv = icoFile->GetLastModifiedTime(&fileModTime);
    fileModTime /= PR_MSEC_PER_SEC;
    int32_t icoReCacheSecondsTimeout = GetICOCacheSecondsTimeout();
    int64_t nowTime = PR_Now() / int64_t(PR_USEC_PER_SEC);

    // If the last mod call failed or the icon is old then re-cache it
    // This check is in case the favicon of a page changes
    // the next time we try to build the jump list, the data will be available.
    if (NS_FAILED(rv) || (nowTime - fileModTime) > icoReCacheSecondsTimeout) {
      CacheIconFileFromFaviconURIAsync(aFaviconPageURI, icoFile, aIOThread,
                                       aURLShortcut, runnable.forget());
      return NS_ERROR_NOT_AVAILABLE;
    }
  } else {
    // The file does not exist yet, obtain it async from the favicon service so
    // that the next time we try to build the jump list it'll be available.
    CacheIconFileFromFaviconURIAsync(aFaviconPageURI, icoFile, aIOThread,
                                     aURLShortcut, runnable.forget());
    return NS_ERROR_NOT_AVAILABLE;
  }

  // The icoFile is filled with a path that exists, get its path
  rv = icoFile->GetPath(aICOFilePath);
  return rv;
}

nsresult FaviconHelper::HashURI(nsCOMPtr<nsICryptoHash>& aCryptoHash,
                                nsIURI* aUri, nsACString& aUriHash) {
  if (!aUri) return NS_ERROR_INVALID_ARG;

  nsAutoCString spec;
  nsresult rv = aUri->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!aCryptoHash) {
    aCryptoHash = do_CreateInstance(NS_CRYPTO_HASH_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = aCryptoHash->Init(nsICryptoHash::MD5);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aCryptoHash->Update(
      reinterpret_cast<const uint8_t*>(spec.BeginReading()), spec.Length());
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aCryptoHash->Finish(true, aUriHash);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

// (static) Obtains the ICO file for the favicon at page aFaviconPageURI
// If successful, the file path on disk is in the format:
// <ProfLDS>\jumpListCache\<hash(aFaviconPageURI)>.ico
nsresult FaviconHelper::GetOutputIconPath(nsCOMPtr<nsIURI> aFaviconPageURI,
                                          nsCOMPtr<nsIFile>& aICOFile,
                                          bool aURLShortcut) {
  // Hash the input URI and replace any / with _
  nsAutoCString inputURIHash;
  nsCOMPtr<nsICryptoHash> cryptoHash;
  nsresult rv = HashURI(cryptoHash, aFaviconPageURI, inputURIHash);
  NS_ENSURE_SUCCESS(rv, rv);
  char* cur = inputURIHash.BeginWriting();
  char* end = inputURIHash.EndWriting();
  for (; cur < end; ++cur) {
    if ('/' == *cur) {
      *cur = '_';
    }
  }

  // Obtain the local profile directory and construct the output icon file path
  rv = NS_GetSpecialDirectory("ProfLDS", getter_AddRefs(aICOFile));
  NS_ENSURE_SUCCESS(rv, rv);
  if (!aURLShortcut)
    rv = aICOFile->AppendNative(nsDependentCString(kJumpListCacheDir));
  else
    rv = aICOFile->AppendNative(nsDependentCString(kShortcutCacheDir));
  NS_ENSURE_SUCCESS(rv, rv);

  // Append the icon extension
  inputURIHash.AppendLiteral(".ico");
  rv = aICOFile->AppendNative(inputURIHash);

  return rv;
}

// (static) Asynchronously creates a cached ICO file on disk for the favicon of
// page aFaviconPageURI and stores it to disk at the path of aICOFile.
nsresult FaviconHelper::CacheIconFileFromFaviconURIAsync(
    nsCOMPtr<nsIURI> aFaviconPageURI, nsCOMPtr<nsIFile> aICOFile,
    nsCOMPtr<nsIThread>& aIOThread, bool aURLShortcut,
    already_AddRefed<nsIRunnable> aRunnable) {
  nsCOMPtr<nsIRunnable> runnable = aRunnable;
#ifdef MOZ_PLACES
  // Obtain the favicon service and get the favicon for the specified page
  nsCOMPtr<nsIFaviconService> favIconSvc(
      do_GetService("@mozilla.org/browser/favicon-service;1"));
  NS_ENSURE_TRUE(favIconSvc, NS_ERROR_FAILURE);

  nsCOMPtr<nsIFaviconDataCallback> callback =
      new mozilla::widget::AsyncFaviconDataReady(
          aFaviconPageURI, aIOThread, aURLShortcut, runnable.forget());

  favIconSvc->GetFaviconDataForPage(aFaviconPageURI, callback, 0);
#endif
  return NS_OK;
}

// Obtains the jump list 'ICO cache timeout in seconds' pref
int32_t FaviconHelper::GetICOCacheSecondsTimeout() {
  // Only obtain the setting at most once from the pref service.
  // In the rare case that 2 threads call this at the same
  // time it is no harm and we will simply obtain the pref twice.
  // None of the taskbar list prefs are currently updated via a
  // pref observer so I think this should suffice.
  const int32_t kSecondsPerDay = 86400;
  static bool alreadyObtained = false;
  static int32_t icoReCacheSecondsTimeout = kSecondsPerDay;
  if (alreadyObtained) {
    return icoReCacheSecondsTimeout;
  }

  // Obtain the pref
  const char PREF_ICOTIMEOUT[] = "browser.taskbar.lists.icoTimeoutInSeconds";
  icoReCacheSecondsTimeout =
      Preferences::GetInt(PREF_ICOTIMEOUT, kSecondsPerDay);
  alreadyObtained = true;
  return icoReCacheSecondsTimeout;
}

/* static */
bool WinUtils::GetShellItemPath(IShellItem* aItem, nsString& aResultString) {
  NS_ENSURE_TRUE(aItem, false);
  LPWSTR str = nullptr;
  if (FAILED(aItem->GetDisplayName(SIGDN_FILESYSPATH, &str))) return false;
  aResultString.Assign(str);
  CoTaskMemFree(str);
  return !aResultString.IsEmpty();
}

/* static */
LayoutDeviceIntRegion WinUtils::ConvertHRGNToRegion(HRGN aRgn) {
  NS_ASSERTION(aRgn, "Don't pass NULL region here");

  LayoutDeviceIntRegion rgn;

  DWORD size = ::GetRegionData(aRgn, 0, nullptr);
  AutoTArray<uint8_t, 100> buffer;
  buffer.SetLength(size);

  RGNDATA* data = reinterpret_cast<RGNDATA*>(buffer.Elements());
  if (!::GetRegionData(aRgn, size, data)) return rgn;

  if (data->rdh.nCount > MAX_RECTS_IN_REGION) {
    rgn = ToIntRect(data->rdh.rcBound);
    return rgn;
  }

  RECT* rects = reinterpret_cast<RECT*>(data->Buffer);
  for (uint32_t i = 0; i < data->rdh.nCount; ++i) {
    RECT* r = rects + i;
    rgn.Or(rgn, ToIntRect(*r));
  }

  return rgn;
}

LayoutDeviceIntRect WinUtils::ToIntRect(const RECT& aRect) {
  return LayoutDeviceIntRect(aRect.left, aRect.top, aRect.right - aRect.left,
                             aRect.bottom - aRect.top);
}

/* static */
bool WinUtils::IsIMEEnabled(const InputContext& aInputContext) {
  return IsIMEEnabled(aInputContext.mIMEState.mEnabled);
}

/* static */
bool WinUtils::IsIMEEnabled(IMEEnabled aIMEState) {
  return aIMEState == IMEEnabled::Enabled;
}

/* static */
void WinUtils::SetupKeyModifiersSequence(nsTArray<KeyPair>* aArray,
                                         uint32_t aModifiers, UINT aMessage) {
  MOZ_ASSERT(!(aModifiers & nsIWidget::ALTGRAPH) ||
             !(aModifiers & (nsIWidget::CTRL_L | nsIWidget::ALT_R)));
  if (aMessage == WM_KEYUP) {
    // If AltGr is released, ControlLeft key is released first, then,
    // AltRight key is released.
    if (aModifiers & nsIWidget::ALTGRAPH) {
      aArray->AppendElement(
          KeyPair(VK_CONTROL, VK_LCONTROL, ScanCode::eControlLeft));
      aArray->AppendElement(KeyPair(VK_MENU, VK_RMENU, ScanCode::eAltRight));
    }
    for (uint32_t i = ArrayLength(sModifierKeyMap); i; --i) {
      const uint32_t* map = sModifierKeyMap[i - 1];
      if (aModifiers & map[0]) {
        aArray->AppendElement(KeyPair(map[1], map[2], map[3]));
      }
    }
  } else {
    for (uint32_t i = 0; i < ArrayLength(sModifierKeyMap); ++i) {
      const uint32_t* map = sModifierKeyMap[i];
      if (aModifiers & map[0]) {
        aArray->AppendElement(KeyPair(map[1], map[2], map[3]));
      }
    }
    // If AltGr is pressed, ControlLeft key is pressed first, then,
    // AltRight key is pressed.
    if (aModifiers & nsIWidget::ALTGRAPH) {
      aArray->AppendElement(
          KeyPair(VK_CONTROL, VK_LCONTROL, ScanCode::eControlLeft));
      aArray->AppendElement(KeyPair(VK_MENU, VK_RMENU, ScanCode::eAltRight));
    }
  }
}

/* static */
nsresult WinUtils::WriteBitmap(nsIFile* aFile, imgIContainer* aImage) {
  RefPtr<SourceSurface> surface = aImage->GetFrame(
      imgIContainer::FRAME_FIRST,
      imgIContainer::FLAG_SYNC_DECODE | imgIContainer::FLAG_ASYNC_NOTIFY);
  NS_ENSURE_TRUE(surface, NS_ERROR_FAILURE);

  return WriteBitmap(aFile, surface);
}

/* static */
nsresult WinUtils::WriteBitmap(nsIFile* aFile, SourceSurface* surface) {
  nsresult rv;

  // For either of the following formats we want to set the biBitCount member
  // of the BITMAPINFOHEADER struct to 32, below. For that value the bitmap
  // format defines that the A8/X8 WORDs in the bitmap byte stream be ignored
  // for the BI_RGB value we use for the biCompression member.
  MOZ_ASSERT(surface->GetFormat() == SurfaceFormat::B8G8R8A8 ||
             surface->GetFormat() == SurfaceFormat::B8G8R8X8);

  RefPtr<DataSourceSurface> dataSurface = surface->GetDataSurface();
  NS_ENSURE_TRUE(dataSurface, NS_ERROR_FAILURE);

  int32_t width = dataSurface->GetSize().width;
  int32_t height = dataSurface->GetSize().height;
  int32_t bytesPerPixel = 4 * sizeof(uint8_t);
  uint32_t bytesPerRow = bytesPerPixel * width;
  bool hasAlpha = surface->GetFormat() == SurfaceFormat::B8G8R8A8;

  // initialize these bitmap structs which we will later
  // serialize directly to the head of the bitmap file
  BITMAPV4HEADER bmi;
  memset(&bmi, 0, sizeof(BITMAPV4HEADER));
  bmi.bV4Size = sizeof(BITMAPV4HEADER);
  bmi.bV4Width = width;
  bmi.bV4Height = height;
  bmi.bV4Planes = 1;
  bmi.bV4BitCount = (WORD)bytesPerPixel * 8;
  bmi.bV4V4Compression = hasAlpha ? BI_BITFIELDS : BI_RGB;
  bmi.bV4SizeImage = bytesPerRow * height;
  bmi.bV4CSType = LCS_sRGB;
  if (hasAlpha) {
    bmi.bV4RedMask = 0x00FF0000;
    bmi.bV4GreenMask = 0x0000FF00;
    bmi.bV4BlueMask = 0x000000FF;
    bmi.bV4AlphaMask = 0xFF000000;
  }

  BITMAPFILEHEADER bf;
  DWORD colormask[3];
  bf.bfType = 0x4D42;  // 'BM'
  bf.bfReserved1 = 0;
  bf.bfReserved2 = 0;
  bf.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPV4HEADER) +
                 (hasAlpha ? sizeof(colormask) : 0);
  bf.bfSize = bf.bfOffBits + bmi.bV4SizeImage;

  // get a file output stream
  nsCOMPtr<nsIOutputStream> stream;
  rv = NS_NewLocalFileOutputStream(getter_AddRefs(stream), aFile);
  NS_ENSURE_SUCCESS(rv, rv);

  DataSourceSurface::MappedSurface map;
  if (!dataSurface->Map(DataSourceSurface::MapType::READ, &map)) {
    return NS_ERROR_FAILURE;
  }

  // write the bitmap headers and rgb pixel data to the file
  rv = NS_ERROR_FAILURE;
  if (stream) {
    uint32_t written;
    stream->Write((const char*)&bf, sizeof(BITMAPFILEHEADER), &written);
    if (written == sizeof(BITMAPFILEHEADER)) {
      stream->Write((const char*)&bmi, sizeof(BITMAPV4HEADER), &written);
      if (written == sizeof(BITMAPV4HEADER)) {
        if (hasAlpha) {
          // color mask
          colormask[0] = 0x00FF0000;
          colormask[1] = 0x0000FF00;
          colormask[2] = 0x000000FF;

          stream->Write((const char*)colormask, sizeof(colormask), &written);
        }
        if (!hasAlpha || written == sizeof(colormask)) {
          // write out the image data backwards because the desktop won't
          // show bitmaps with negative heights for top-to-bottom
          uint32_t i = map.mStride * height;
          do {
            i -= map.mStride;
            stream->Write(((const char*)map.mData) + i, bytesPerRow, &written);
            if (written == bytesPerRow) {
              rv = NS_OK;
            } else {
              rv = NS_ERROR_FAILURE;
              break;
            }
          } while (i != 0);
        }
      }
    }

    stream->Close();
  }

  dataSurface->Unmap();

  return rv;
}

// This is in use here and in dom/events/TouchEvent.cpp
/* static */
uint32_t WinUtils::IsTouchDeviceSupportPresent() {
  int32_t touchCapabilities = ::GetSystemMetrics(SM_DIGITIZER);
  return (touchCapabilities & NID_READY) &&
         (touchCapabilities & (NID_EXTERNAL_TOUCH | NID_INTEGRATED_TOUCH));
}

/* static */
uint32_t WinUtils::GetMaxTouchPoints() {
  if (IsTouchDeviceSupportPresent()) {
    return GetSystemMetrics(SM_MAXIMUMTOUCHES);
  }
  return 0;
}

// Starting with version 10.0.22621.0 of the Windows SDK the AR_STATE enum and
// types are only defined when building for Windows 8 instead of Windows 7.
#if (WDK_NTDDI_VERSION >= 0x0A00000C) && (WINVER < 0x0602)

enum AR_STATE {
  AR_ENABLED = 0x0,
  AR_DISABLED = 0x1,
  AR_SUPPRESSED = 0x2,
  AR_REMOTESESSION = 0x4,
  AR_MULTIMON = 0x8,
  AR_NOSENSOR = 0x10,
  AR_NOT_SUPPORTED = 0x20,
  AR_DOCKED = 0x40,
  AR_LAPTOP = 0x80
};

using PAR_STATE = enum AR_STATE*;

#endif  // (WDK_NTDDI_VERSION >= 0x0A00000C) && (WINVER < 0x0602)

/* static */
POWER_PLATFORM_ROLE
WinUtils::GetPowerPlatformRole() {
  typedef POWER_PLATFORM_ROLE(WINAPI *
                              PowerDeterminePlatformRoleEx)(ULONG Version);
  static PowerDeterminePlatformRoleEx power_determine_platform_role =
      reinterpret_cast<PowerDeterminePlatformRoleEx>(::GetProcAddress(
          ::LoadLibraryW(L"PowrProf.dll"), "PowerDeterminePlatformRoleEx"));

  POWER_PLATFORM_ROLE powerPlatformRole = PlatformRoleUnspecified;
  if (!power_determine_platform_role) {
    return powerPlatformRole;
  }

  return power_determine_platform_role(POWER_PLATFORM_ROLE_V2);
}

static bool CallGetAutoRotationState(AR_STATE* aRotationState) {
  typedef BOOL(WINAPI * GetAutoRotationStateFunc)(PAR_STATE pState);
  static GetAutoRotationStateFunc get_auto_rotation_state_func =
      reinterpret_cast<GetAutoRotationStateFunc>(::GetProcAddress(
          GetModuleHandleW(L"user32.dll"), "GetAutoRotationState"));
  if (get_auto_rotation_state_func) {
    ZeroMemory(aRotationState, sizeof(AR_STATE));
    return get_auto_rotation_state_func(aRotationState);
  }
  return false;
}

static bool IsTabletDevice() {
  // Guarantees that:
  // - The device has a touch screen.
  // - It is used as a tablet which means that it has no keyboard connected.
  // On Windows 10 it means that it is verifying with ConvertibleSlateMode.

  if (!IsWin8OrLater()) {
    return false;
  }

  if (WindowsUIUtils::GetInTabletMode()) {
    return true;
  }

  if (!GetSystemMetrics(SM_MAXIMUMTOUCHES)) {
    return false;
  }

  // If the device is docked, the user is treating the device as a PC.
  if (GetSystemMetrics(SM_SYSTEMDOCKED)) {
    return false;
  }

  // If the device is not supporting rotation, it's unlikely to be a tablet,
  // a convertible or a detachable. See:
  // https://msdn.microsoft.com/en-us/library/windows/desktop/dn629263(v=vs.85).aspx
  AR_STATE rotation_state;
  if (CallGetAutoRotationState(&rotation_state) &&
      (rotation_state & (AR_NOT_SUPPORTED | AR_LAPTOP | AR_NOSENSOR))) {
    return false;
  }

  // PlatformRoleSlate was added in Windows 8+.
  POWER_PLATFORM_ROLE role = WinUtils::GetPowerPlatformRole();
  if (role == PlatformRoleMobile || role == PlatformRoleSlate) {
    return !GetSystemMetrics(SM_CONVERTIBLESLATEMODE);
  }
  return false;
}

static bool IsMousePresent() {
  if (!::GetSystemMetrics(SM_MOUSEPRESENT)) {
    return false;
  }

  DWORD count = InputDeviceUtils::CountMouseDevices();
  if (!count) {
    return false;
  }

  // If there is a mouse device and if this machine is a tablet or has a
  // digitizer, that's counted as the mouse device.
  // FIXME: Bug 1495938:  We should drop this heuristic way once we find out a
  // reliable way to tell there is no mouse or not.
  if (count == 1 &&
      (WinUtils::IsTouchDeviceSupportPresent() || IsTabletDevice())) {
    return false;
  }

  return true;
}

/* static */
PointerCapabilities WinUtils::GetPrimaryPointerCapabilities() {
  if (IsTabletDevice()) {
    return PointerCapabilities::Coarse;
  }

  if (IsMousePresent()) {
    return PointerCapabilities::Fine | PointerCapabilities::Hover;
  }

  if (IsTouchDeviceSupportPresent()) {
    return PointerCapabilities::Coarse;
  }

  return PointerCapabilities::None;
}

/* static */
PointerCapabilities WinUtils::GetAllPointerCapabilities() {
  PointerCapabilities result = PointerCapabilities::None;

  if (IsTabletDevice() || IsTouchDeviceSupportPresent()) {
    result |= PointerCapabilities::Coarse;
  }

  if (IsMousePresent()) {
    result |= PointerCapabilities::Fine | PointerCapabilities::Hover;
  }

  return result;
}

/* static */
bool WinUtils::ResolveJunctionPointsAndSymLinks(std::wstring& aPath) {
  LOG_D("ResolveJunctionPointsAndSymLinks: Resolving path: %S", aPath.c_str());

  wchar_t path[MAX_PATH] = {0};

  nsAutoHandle handle(::CreateFileW(
      aPath.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
      nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr));

  if (handle == INVALID_HANDLE_VALUE) {
    LOG_E("Failed to open file handle to resolve path. GetLastError=%lu",
          GetLastError());
    return false;
  }

  DWORD pathLen = GetFinalPathNameByHandleW(
      handle, path, MAX_PATH, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
  if (pathLen == 0 || pathLen >= MAX_PATH) {
    LOG_E("GetFinalPathNameByHandleW failed. GetLastError=%lu", GetLastError());
    return false;
  }
  aPath = path;

  // GetFinalPathNameByHandle sticks a '\\?\' in front of the path,
  // but that confuses some APIs so strip it off. It will also put
  // '\\?\UNC\' in front of network paths, we convert that to '\\'.
  if (aPath.compare(0, 7, L"\\\\?\\UNC") == 0) {
    aPath.erase(2, 6);
  } else if (aPath.compare(0, 4, L"\\\\?\\") == 0) {
    aPath.erase(0, 4);
  }

  LOG_D("ResolveJunctionPointsAndSymLinks: Resolved path to: %S",
        aPath.c_str());
  return true;
}

/* static */
bool WinUtils::ResolveJunctionPointsAndSymLinks(nsIFile* aPath) {
  MOZ_ASSERT(aPath);

  nsAutoString filePath;
  nsresult rv = aPath->GetPath(filePath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  std::wstring resolvedPath(filePath.get());
  if (!ResolveJunctionPointsAndSymLinks(resolvedPath)) {
    return false;
  }

  rv = aPath->InitWithPath(nsDependentString(resolvedPath.c_str()));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  return true;
}

/* static */
bool WinUtils::RunningFromANetworkDrive() {
  wchar_t exePath[MAX_PATH];
  if (!::GetModuleFileNameW(nullptr, exePath, MAX_PATH)) {
    return false;
  }

  std::wstring exeString(exePath);
  if (!widget::WinUtils::ResolveJunctionPointsAndSymLinks(exeString)) {
    return false;
  }

  wchar_t volPath[MAX_PATH];
  if (!::GetVolumePathNameW(exeString.c_str(), volPath, MAX_PATH)) {
    return false;
  }

  return (::GetDriveTypeW(volPath) == DRIVE_REMOTE);
}

/* static */
bool WinUtils::CanonicalizePath(nsAString& aPath) {
  wchar_t tempPath[MAX_PATH + 1];
  if (!PathCanonicalizeW(tempPath,
                         (char16ptr_t)PromiseFlatString(aPath).get())) {
    return false;
  }
  aPath = tempPath;
  MOZ_ASSERT(aPath.Length() <= MAX_PATH);
  return true;
}

/* static */
bool WinUtils::MakeLongPath(nsAString& aPath) {
  wchar_t tempPath[MAX_PATH + 1];
  DWORD longResult =
      GetLongPathNameW((char16ptr_t)PromiseFlatString(aPath).get(), tempPath,
                       ArrayLength(tempPath));
  if (longResult > ArrayLength(tempPath)) {
    // Our buffer is too short, and we're guaranteeing <= MAX_PATH results.
    return false;
  } else if (longResult) {
    // Success.
    aPath = tempPath;
    MOZ_ASSERT(aPath.Length() <= MAX_PATH);
  }
  // GetLongPathNameW returns 0 if the path is not found or is not rooted,
  // but we shouldn't consider that a failure condition.
  return true;
}

/* static */
bool WinUtils::UnexpandEnvVars(nsAString& aPath) {
  wchar_t tempPath[MAX_PATH + 1];
  // PathUnExpandEnvStringsW returns false if it doesn't make any
  // substitutions. Silently continue using the unaltered path.
  if (PathUnExpandEnvStringsW((char16ptr_t)PromiseFlatString(aPath).get(),
                              tempPath, ArrayLength(tempPath))) {
    aPath = tempPath;
    MOZ_ASSERT(aPath.Length() <= MAX_PATH);
  }
  return true;
}

/* static */
WinUtils::WhitelistVec WinUtils::BuildWhitelist() {
  WhitelistVec result;

  Unused << result.emplaceBack(
      std::make_pair(nsString(u"%ProgramFiles%"_ns), nsDependentString()));

  // When no substitution is required, set the void flag
  result.back().second.SetIsVoid(true);

  Unused << result.emplaceBack(
      std::make_pair(nsString(u"%SystemRoot%"_ns), nsDependentString()));
  result.back().second.SetIsVoid(true);

  wchar_t tmpPath[MAX_PATH + 1] = {};
  if (GetTempPath(MAX_PATH, tmpPath)) {
    // GetTempPath's result always ends with a backslash, which we don't want
    uint32_t tmpPathLen = wcslen(tmpPath);
    if (tmpPathLen) {
      tmpPath[tmpPathLen - 1] = 0;
    }

    nsAutoString cleanTmpPath(tmpPath);
    if (UnexpandEnvVars(cleanTmpPath)) {
      constexpr auto tempVar = u"%TEMP%"_ns;
      Unused << result.emplaceBack(std::make_pair(
          nsString(cleanTmpPath), nsDependentString(tempVar, 0)));
    }
  }

  // If we add more items to the whitelist, ensure we still don't invoke an
  // unnecessary heap allocation.
  MOZ_ASSERT(result.length() <= kMaxWhitelistedItems);

  return result;
}

/**
 * This function provides an array of (system path, substitution) pairs that are
 * considered to be acceptable with respect to privacy, for the purposes of
 * submitting within telemetry or crash reports.
 *
 * The substitution string's void flag may be set. If it is, no subsitution is
 * necessary. Otherwise, the consumer should replace the system path with the
 * substitution.
 *
 * @see PreparePathForTelemetry for an example of its usage.
 */
/* static */
const WinUtils::WhitelistVec& WinUtils::GetWhitelistedPaths() {
  static WhitelistVec sWhitelist([]() -> WhitelistVec {
    auto setClearFn = [ptr = &sWhitelist]() -> void {
      RunOnShutdown([ptr]() -> void { ptr->clear(); },
                    ShutdownPhase::XPCOMShutdownFinal);
    };

    if (NS_IsMainThread()) {
      setClearFn();
    } else {
      SchedulerGroup::Dispatch(
          TaskCategory::Other,
          NS_NewRunnableFunction("WinUtils::GetWhitelistedPaths",
                                 std::move(setClearFn)));
    }

    return BuildWhitelist();
  }());
  return sWhitelist;
}

/**
 * This function is located here (as opposed to nsSystemInfo or elsewhere)
 * because we need to gather this information as early as possible during
 * startup.
 */
/* static */
bool WinUtils::GetAppInitDLLs(nsAString& aOutput) {
  aOutput.Truncate();
  HKEY hkey = NULL;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                    L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Windows",
                    0, KEY_QUERY_VALUE, &hkey)) {
    return false;
  }
  nsAutoRegKey key(hkey);
  LONG status;
  const wchar_t kLoadAppInitDLLs[] = L"LoadAppInit_DLLs";
  DWORD loadAppInitDLLs = 0;
  DWORD loadAppInitDLLsLen = sizeof(loadAppInitDLLs);
  status = RegQueryValueExW(hkey, kLoadAppInitDLLs, nullptr, nullptr,
                            (LPBYTE)&loadAppInitDLLs, &loadAppInitDLLsLen);
  if (status != ERROR_SUCCESS) {
    return false;
  }
  if (!loadAppInitDLLs) {
    // If loadAppInitDLLs is zero then AppInit_DLLs is disabled.
    // In this case we'll return true along with an empty output string.
    return true;
  }
  DWORD numBytes = 0;
  const wchar_t kAppInitDLLs[] = L"AppInit_DLLs";
  // Query for required buffer size
  status = RegQueryValueExW(hkey, kAppInitDLLs, nullptr, nullptr, nullptr,
                            &numBytes);
  if (status != ERROR_SUCCESS) {
    return false;
  }
  // Allocate the buffer and query for the actual data
  mozilla::UniquePtr<wchar_t[]> data =
      mozilla::MakeUnique<wchar_t[]>(numBytes / sizeof(wchar_t));
  status = RegQueryValueExW(hkey, kAppInitDLLs, nullptr, nullptr,
                            (LPBYTE)data.get(), &numBytes);
  if (status != ERROR_SUCCESS) {
    return false;
  }
  // For each token, split up the filename components and then check the
  // name of the file.
  const wchar_t kDelimiters[] = L", ";
  wchar_t* tokenContext = nullptr;
  wchar_t* token = wcstok_s(data.get(), kDelimiters, &tokenContext);
  while (token) {
    nsAutoString cleanPath(token);
    // Since these paths are short paths originating from the registry, we need
    // to canonicalize them, lengthen them, and sanitize them before we can
    // check them against the whitelist
    if (PreparePathForTelemetry(cleanPath)) {
      if (!aOutput.IsEmpty()) {
        aOutput += L";";
      }
      aOutput += cleanPath;
    }
    token = wcstok_s(nullptr, kDelimiters, &tokenContext);
  }
  return true;
}

/* static */
bool WinUtils::PreparePathForTelemetry(nsAString& aPath,
                                       PathTransformFlags aFlags) {
  if (aFlags & PathTransformFlags::Canonicalize) {
    if (!CanonicalizePath(aPath)) {
      return false;
    }
  }
  if (aFlags & PathTransformFlags::Lengthen) {
    if (!MakeLongPath(aPath)) {
      return false;
    }
  }
  if (aFlags & PathTransformFlags::UnexpandEnvVars) {
    if (!UnexpandEnvVars(aPath)) {
      return false;
    }
  }

  const WhitelistVec& whitelistedPaths = GetWhitelistedPaths();

  for (uint32_t i = 0; i < whitelistedPaths.length(); ++i) {
    const nsString& testPath = whitelistedPaths[i].first;
    const nsDependentString& substitution = whitelistedPaths[i].second;
    if (StringBeginsWith(aPath, testPath, nsCaseInsensitiveStringComparator)) {
      if (!substitution.IsVoid()) {
        aPath.Replace(0, testPath.Length(), substitution);
      }
      return true;
    }
  }

  // For non-whitelisted paths, we strip the path component and just leave
  // the filename. We can't use nsLocalFile to do this because these paths may
  // begin with environment variables, and nsLocalFile doesn't like
  // non-absolute paths.
  const nsString& flatPath = PromiseFlatString(aPath);
  LPCWSTR leafStart = ::PathFindFileNameW(flatPath.get());
  ptrdiff_t cutLen = leafStart - flatPath.get();
  if (cutLen) {
    aPath.Cut(0, cutLen);
  } else if (aFlags & PathTransformFlags::RequireFilePath) {
    return false;
  }

  return true;
}

nsString WinUtils::GetPackageFamilyName() {
  nsString rv;

  UniquePtr<wchar_t[]> packageIdentity = mozilla::GetPackageFamilyName();
  if (packageIdentity) {
    rv = packageIdentity.get();
  }

  return rv;
}

bool WinUtils::GetClassName(HWND aHwnd, nsAString& aClassName) {
  const int bufferLength = 256;
  aClassName.SetLength(bufferLength);

  int length = ::GetClassNameW(aHwnd, (char16ptr_t)aClassName.BeginWriting(),
                               bufferLength);
  if (length == 0) {
    return false;
  }
  MOZ_RELEASE_ASSERT(length <= (bufferLength - 1));
  aClassName.Truncate(length);
  return true;
}

static BOOL CALLBACK EnumUpdateWindowOcclusionProc(HWND aHwnd, LPARAM aLParam) {
  const bool* const enable = reinterpret_cast<bool*>(aLParam);
  nsWindow* window = WinUtils::GetNSWindowPtr(aHwnd);
  if (window) {
    window->MaybeEnableWindowOcclusion(*enable);
  }
  return TRUE;
}

void WinUtils::EnableWindowOcclusion(const bool aEnable) {
  if (aEnable) {
    WinWindowOcclusionTracker::Ensure();
  }
  ::EnumWindows(EnumUpdateWindowOcclusionProc,
                reinterpret_cast<LPARAM>(&aEnable));
}

bool WinUtils::GetTimezoneName(wchar_t* aBuffer) {
  DYNAMIC_TIME_ZONE_INFORMATION tzInfo;
  DWORD tzid = GetDynamicTimeZoneInformation(&tzInfo);

  if (tzid == TIME_ZONE_ID_INVALID) {
    return false;
  }

  wcscpy_s(aBuffer, 128, tzInfo.TimeZoneKeyName);

  return true;
}

}  // namespace widget
}  // namespace mozilla
