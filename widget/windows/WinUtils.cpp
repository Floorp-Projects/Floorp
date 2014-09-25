/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sts=2 sw=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WinUtils.h"

#include "gfxPlatform.h"
#include "gfxUtils.h"
#include "nsWindow.h"
#include "nsWindowDefs.h"
#include "KeyboardLayout.h"
#include "nsIDOMMouseEvent.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/DataSurfaceHelpers.h"
#include "mozilla/Preferences.h"
#include "mozilla/RefPtr.h"
#include "mozilla/WindowsVersion.h"
#include "nsIContentPolicy.h"
#include "nsContentUtils.h"

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG /* Allow logging in the release build */
#endif // MOZ_LOGGING
#include "prlog.h"

#include "nsString.h"
#include "nsDirectoryServiceUtils.h"
#include "imgIContainer.h"
#include "imgITools.h"
#include "nsStringStream.h"
#include "nsNetUtil.h"
#ifdef MOZ_PLACES
#include "mozIAsyncFavicons.h"
#endif
#include "nsIIconURI.h"
#include "nsIDownloader.h"
#include "nsINetUtil.h"
#include "nsIChannel.h"
#include "nsIObserver.h"
#include "imgIEncoder.h"
#include "nsIThread.h"
#include "MainThreadUtils.h"
#include "gfxColor.h"
#ifdef MOZ_METRO
#include "winrt/MetroInput.h"
#include "winrt/MetroUtils.h"
#endif // MOZ_METRO

#ifdef NS_ENABLE_TSF
#include <textstor.h>
#include "nsTextStore.h"
#endif // #ifdef NS_ENABLE_TSF

#ifdef PR_LOGGING
PRLogModuleInfo* gWindowsLog = nullptr;
#endif

using namespace mozilla::gfx;

namespace mozilla {
namespace widget {

#define ENTRY(_msg) { #_msg, _msg }
EventMsgInfo gAllEvents[] = {
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
  {nullptr, 0x0}
};
#undef ENTRY

#ifdef MOZ_PLACES
NS_IMPL_ISUPPORTS(myDownloadObserver, nsIDownloadObserver)
NS_IMPL_ISUPPORTS(AsyncFaviconDataReady, nsIFaviconDataCallback)
#endif
NS_IMPL_ISUPPORTS(AsyncEncodeAndWriteIcon, nsIRunnable)
NS_IMPL_ISUPPORTS(AsyncDeleteIconFromDisk, nsIRunnable)
NS_IMPL_ISUPPORTS(AsyncDeleteAllFaviconsFromDisk, nsIRunnable)


const char FaviconHelper::kJumpListCacheDir[] = "jumpListCache";
const char FaviconHelper::kShortcutCacheDir[] = "shortcutCache";

// apis available on vista and up.
WinUtils::SHCreateItemFromParsingNamePtr WinUtils::sCreateItemFromParsingName = nullptr;
WinUtils::SHGetKnownFolderPathPtr WinUtils::sGetKnownFolderPath = nullptr;

// We just leak these DLL HMODULEs. There's no point in calling FreeLibrary
// on them during shutdown anyway.
static const wchar_t kShellLibraryName[] =  L"shell32.dll";
static HMODULE sShellDll = nullptr;
static const wchar_t kDwmLibraryName[] = L"dwmapi.dll";
static HMODULE sDwmDll = nullptr;

WinUtils::DwmExtendFrameIntoClientAreaProc WinUtils::dwmExtendFrameIntoClientAreaPtr = nullptr;
WinUtils::DwmIsCompositionEnabledProc WinUtils::dwmIsCompositionEnabledPtr = nullptr;
WinUtils::DwmSetIconicThumbnailProc WinUtils::dwmSetIconicThumbnailPtr = nullptr;
WinUtils::DwmSetIconicLivePreviewBitmapProc WinUtils::dwmSetIconicLivePreviewBitmapPtr = nullptr;
WinUtils::DwmGetWindowAttributeProc WinUtils::dwmGetWindowAttributePtr = nullptr;
WinUtils::DwmSetWindowAttributeProc WinUtils::dwmSetWindowAttributePtr = nullptr;
WinUtils::DwmInvalidateIconicBitmapsProc WinUtils::dwmInvalidateIconicBitmapsPtr = nullptr;
WinUtils::DwmDefWindowProcProc WinUtils::dwmDwmDefWindowProcPtr = nullptr;
WinUtils::DwmGetCompositionTimingInfoProc WinUtils::dwmGetCompositionTimingInfoPtr = nullptr;

/* static */
void
WinUtils::Initialize()
{
#ifdef PR_LOGGING
  if (!gWindowsLog) {
    gWindowsLog = PR_NewLogModule("Widget");
  }
#endif
  if (!sDwmDll && IsVistaOrLater()) {
    sDwmDll = ::LoadLibraryW(kDwmLibraryName);

    if (sDwmDll) {
      dwmExtendFrameIntoClientAreaPtr = (DwmExtendFrameIntoClientAreaProc)::GetProcAddress(sDwmDll, "DwmExtendFrameIntoClientArea");
      dwmIsCompositionEnabledPtr = (DwmIsCompositionEnabledProc)::GetProcAddress(sDwmDll, "DwmIsCompositionEnabled");
      dwmSetIconicThumbnailPtr = (DwmSetIconicThumbnailProc)::GetProcAddress(sDwmDll, "DwmSetIconicThumbnail");
      dwmSetIconicLivePreviewBitmapPtr = (DwmSetIconicLivePreviewBitmapProc)::GetProcAddress(sDwmDll, "DwmSetIconicLivePreviewBitmap");
      dwmGetWindowAttributePtr = (DwmGetWindowAttributeProc)::GetProcAddress(sDwmDll, "DwmGetWindowAttribute");
      dwmSetWindowAttributePtr = (DwmSetWindowAttributeProc)::GetProcAddress(sDwmDll, "DwmSetWindowAttribute");
      dwmInvalidateIconicBitmapsPtr = (DwmInvalidateIconicBitmapsProc)::GetProcAddress(sDwmDll, "DwmInvalidateIconicBitmaps");
      dwmDwmDefWindowProcPtr = (DwmDefWindowProcProc)::GetProcAddress(sDwmDll, "DwmDefWindowProc");
      dwmGetCompositionTimingInfoPtr = (DwmGetCompositionTimingInfoProc)::GetProcAddress(sDwmDll, "DwmGetCompositionTimingInfo");
    }
  }
}

// static
void
WinUtils::LogW(const wchar_t *fmt, ...)
{
  va_list args = nullptr;
  if(!lstrlenW(fmt)) {
    return;
  }
  va_start(args, fmt);
  int buflen = _vscwprintf(fmt, args);
  wchar_t* buffer = new wchar_t[buflen+1];
  if (!buffer) {
    va_end(args);
    return;
  }
  vswprintf(buffer, buflen, fmt, args);
  va_end(args);

  // MSVC, including remote debug sessions
  OutputDebugStringW(buffer);
  OutputDebugStringW(L"\n");

  int len = wcslen(buffer);
  if (len) {
    char* utf8 = new char[len+1];
    memset(utf8, 0, sizeof(utf8));
    if (WideCharToMultiByte(CP_ACP, 0, buffer,
                            -1, utf8, len+1, nullptr,
                            nullptr) > 0) {
      // desktop console
      printf("%s\n", utf8);
#ifdef PR_LOGGING
      NS_ASSERTION(gWindowsLog, "Called WinUtils Log() but Widget "
                                   "log module doesn't exist!");
      PR_LOG(gWindowsLog, PR_LOG_ALWAYS, (utf8));
#endif
    }
    delete[] utf8;
  }
  delete[] buffer;
}

// static
void
WinUtils::Log(const char *fmt, ...)
{
  va_list args = nullptr;
  if(!strlen(fmt)) {
    return;
  }
  va_start(args, fmt);
  int buflen = _vscprintf(fmt, args);
  char* buffer = new char[buflen+1];
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

#ifdef PR_LOGGING
  NS_ASSERTION(gWindowsLog, "Called WinUtils Log() but Widget "
                               "log module doesn't exist!");
  PR_LOG(gWindowsLog, PR_LOG_ALWAYS, (buffer));
#endif
  delete[] buffer;
}

/* static */
double
WinUtils::LogToPhysFactor()
{
  // dpi / 96.0
  if (XRE_GetWindowsEnvironment() == WindowsEnvironmentType_Metro) {
#ifdef MOZ_METRO
    return MetroUtils::LogToPhysFactor();
#else
    return 1.0;
#endif
  } else {
    HDC hdc = ::GetDC(nullptr);
    double result = ::GetDeviceCaps(hdc, LOGPIXELSY) / 96.0;
    ::ReleaseDC(nullptr, hdc);

    if (result == 0) {
      // Bug 1012487 - This can occur when the Screen DC is used off the
      // main thread on windows. For now just assume a 100% DPI for this
      // drawing call.
      // XXX - fixme!
      result = 1.0;
    }
    return result;
  }
}

/* static */
double
WinUtils::PhysToLogFactor()
{
  // 1.0 / (dpi / 96.0)
  return 1.0 / LogToPhysFactor();
}

/* static */
double
WinUtils::PhysToLog(int32_t aValue)
{
  return double(aValue) * PhysToLogFactor();
}

/* static */
int32_t
WinUtils::LogToPhys(double aValue)
{
  return int32_t(NS_round(aValue * LogToPhysFactor()));
}

/* static */
bool
WinUtils::PeekMessage(LPMSG aMsg, HWND aWnd, UINT aFirstMessage,
                      UINT aLastMessage, UINT aOption)
{
#ifdef NS_ENABLE_TSF
  ITfMessagePump* msgPump = nsTextStore::GetMessagePump();
  if (msgPump) {
    BOOL ret = FALSE;
    HRESULT hr = msgPump->PeekMessageW(aMsg, aWnd, aFirstMessage, aLastMessage,
                                       aOption, &ret);
    NS_ENSURE_TRUE(SUCCEEDED(hr), false);
    return ret;
  }
#endif // #ifdef NS_ENABLE_TSF
  return ::PeekMessageW(aMsg, aWnd, aFirstMessage, aLastMessage, aOption);
}

/* static */
bool
WinUtils::GetMessage(LPMSG aMsg, HWND aWnd, UINT aFirstMessage,
                     UINT aLastMessage)
{
#ifdef NS_ENABLE_TSF
  ITfMessagePump* msgPump = nsTextStore::GetMessagePump();
  if (msgPump) {
    BOOL ret = FALSE;
    HRESULT hr = msgPump->GetMessageW(aMsg, aWnd, aFirstMessage, aLastMessage,
                                      &ret);
    NS_ENSURE_TRUE(SUCCEEDED(hr), false);
    return ret;
  }
#endif // #ifdef NS_ENABLE_TSF
  return ::GetMessageW(aMsg, aWnd, aFirstMessage, aLastMessage);
}

/* static */
void
WinUtils::WaitForMessage(DWORD aTimeoutMs)
{
  const DWORD waitStart = ::GetTickCount();
  DWORD elapsed = 0;
  while (true) {
    if (aTimeoutMs != INFINITE) {
      elapsed = ::GetTickCount() - waitStart;
    }
    if (elapsed >= aTimeoutMs) {
      break;
    }
    DWORD result = ::MsgWaitForMultipleObjectsEx(0, NULL, aTimeoutMs - elapsed,
                                                 MOZ_QS_ALLEVENT,
                                                 MWMO_INPUTAVAILABLE);
    NS_WARN_IF_FALSE(result != WAIT_FAILED, "Wait failed");
    if (result == WAIT_TIMEOUT) {
      break;
    }

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
bool
WinUtils::GetRegistryKey(HKEY aRoot,
                         char16ptr_t aKeyName,
                         char16ptr_t aValueName,
                         wchar_t* aBuffer,
                         DWORD aBufferLength)
{
  NS_PRECONDITION(aKeyName, "The key name is NULL");

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
  result =
    ::RegQueryValueExW(key, aValueName, nullptr, &type, (BYTE*) aBuffer,
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
WinUtils::HasRegistryKey(HKEY aRoot, char16ptr_t aKeyName)
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
  HWND topWnd = nullptr;

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

static const wchar_t*
GetNSWindowPropName()
{
  static wchar_t sPropName[40] = L"";
  if (!*sPropName) {
    _snwprintf(sPropName, 39, L"MozillansIWidgetPtr%p",
               ::GetCurrentProcessId());
    sPropName[39] = '\0';
  }
  return sPropName;
}

/* static */
bool
WinUtils::SetNSWindowBasePtr(HWND aWnd, nsWindowBase* aWidget)
{
  if (!aWidget) {
    ::RemovePropW(aWnd, GetNSWindowPropName());
    return true;
  }
  return ::SetPropW(aWnd, GetNSWindowPropName(), (HANDLE)aWidget);
}

/* static */
nsWindowBase*
WinUtils::GetNSWindowBasePtr(HWND aWnd)
{
  return static_cast<nsWindowBase*>(::GetPropW(aWnd, GetNSWindowPropName()));
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
  (*(int32_t*)aParam)++;
  return TRUE;
}

/* static */
int32_t
WinUtils::GetMonitorCount()
{
  int32_t monitorCount = 0;
  EnumDisplayMonitors(nullptr, nullptr, AddMonitor, (LPARAM)&monitorCount);
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
  return nullptr;
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
  info.mOutWnd = nullptr;

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
uint16_t
WinUtils::GetMouseInputSource()
{
  int32_t inputSource = nsIDOMMouseEvent::MOZ_SOURCE_MOUSE;
  LPARAM lParamExtraInfo = ::GetMessageExtraInfo();
  if ((lParamExtraInfo & TABLET_INK_SIGNATURE) == TABLET_INK_CHECK) {
    inputSource = (lParamExtraInfo & TABLET_INK_TOUCH) ?
      nsIDOMMouseEvent::MOZ_SOURCE_TOUCH : nsIDOMMouseEvent::MOZ_SOURCE_PEN;
  }
  return static_cast<uint16_t>(inputSource);
}

bool
WinUtils::GetIsMouseFromTouch(uint32_t aEventType)
{
#define MOUSEEVENTF_FROMTOUCH 0xFF515700
  return (aEventType == NS_MOUSE_BUTTON_DOWN ||
          aEventType == NS_MOUSE_BUTTON_UP ||
          aEventType == NS_MOUSE_MOVE) &&
          (GetMessageExtraInfo() & MOUSEEVENTF_FROMTOUCH);
}

/* static */
MSG
WinUtils::InitMSG(UINT aMessage, WPARAM wParam, LPARAM lParam, HWND aWnd)
{
  MSG msg;
  msg.message = aMessage;
  msg.wParam  = wParam;
  msg.lParam  = lParam;
  msg.hwnd    = aWnd;
  return msg;
}

/* static */
HRESULT
WinUtils::SHCreateItemFromParsingName(PCWSTR pszPath, IBindCtx *pbc,
                                      REFIID riid, void **ppv)
{
  if (sCreateItemFromParsingName) {
    return sCreateItemFromParsingName(pszPath, pbc, riid, ppv);
  }

  if (!sShellDll) {
    sShellDll = ::LoadLibraryW(kShellLibraryName);
    if (!sShellDll) {
      return false;
    }
  }

  sCreateItemFromParsingName = (SHCreateItemFromParsingNamePtr)
    GetProcAddress(sShellDll, "SHCreateItemFromParsingName");
  if (!sCreateItemFromParsingName)
    return E_FAIL;

  return sCreateItemFromParsingName(pszPath, pbc, riid, ppv);
}

/* static */
HRESULT 
WinUtils::SHGetKnownFolderPath(REFKNOWNFOLDERID rfid,
                               DWORD dwFlags,
                               HANDLE hToken,
                               PWSTR *ppszPath)
{
  if (sGetKnownFolderPath) {
    return sGetKnownFolderPath(rfid, dwFlags, hToken, ppszPath);
  }

  if (!sShellDll) {
    sShellDll = ::LoadLibraryW(kShellLibraryName);
    if (!sShellDll) {
      return false;
    }
  }

  sGetKnownFolderPath = (SHGetKnownFolderPathPtr)
    GetProcAddress(sShellDll, "SHGetKnownFolderPath");
  if (!sGetKnownFolderPath)
    return E_FAIL;

  return sGetKnownFolderPath(rfid, dwFlags, hToken, ppszPath);
}

#ifdef MOZ_PLACES
/************************************************************************/
/* Constructs as AsyncFaviconDataReady Object
/* @param aIOThread : the thread which performs the action
/* @param aURLShortcut : Differentiates between (false)Jumplistcache and (true)Shortcutcache
/************************************************************************/

AsyncFaviconDataReady::AsyncFaviconDataReady(nsIURI *aNewURI, 
                                             nsCOMPtr<nsIThread> &aIOThread, 
                                             const bool aURLShortcut):
  mNewURI(aNewURI),
  mIOThread(aIOThread),
  mURLShortcut(aURLShortcut)
{
}

NS_IMETHODIMP
myDownloadObserver::OnDownloadComplete(nsIDownloader *downloader, 
                                     nsIRequest *request, 
                                     nsISupports *ctxt, 
                                     nsresult status, 
                                     nsIFile *result)
{
  return NS_OK;
}

nsresult AsyncFaviconDataReady::OnFaviconDataNotAvailable(void)
{
  if (!mURLShortcut) {
    return NS_OK;
  }

  nsCOMPtr<nsIFile> icoFile;
  nsresult rv = FaviconHelper::GetOutputIconPath(mNewURI, icoFile, mURLShortcut);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIURI> mozIconURI;
  rv = NS_NewURI(getter_AddRefs(mozIconURI), "moz-icon://.html?size=32");
  if (NS_FAILED(rv)) {
    return rv;
  }
 
  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannel(getter_AddRefs(channel),
                     mozIconURI,
                     nsContentUtils::GetSystemPrincipal(),
                     nsILoadInfo::SEC_NORMAL,
                     nsIContentPolicy::TYPE_IMAGE);

  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDownloadObserver> downloadObserver = new myDownloadObserver;
  nsCOMPtr<nsIStreamListener> listener;
  rv = NS_NewDownloader(getter_AddRefs(listener), downloadObserver, icoFile);
  NS_ENSURE_SUCCESS(rv, rv);

  channel->AsyncOpen(listener, nullptr);
  return NS_OK;
}

NS_IMETHODIMP
AsyncFaviconDataReady::OnComplete(nsIURI *aFaviconURI,
                                  uint32_t aDataLen,
                                  const uint8_t *aData, 
                                  const nsACString &aMimeType)
{
  if (!aDataLen || !aData) {
    if (mURLShortcut) {
      OnFaviconDataNotAvailable();
    }
    
    return NS_OK;
  }

  nsCOMPtr<nsIFile> icoFile;
  nsresult rv = FaviconHelper::GetOutputIconPath(mNewURI, icoFile, mURLShortcut);
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsAutoString path;
  rv = icoFile->GetPath(path);
  NS_ENSURE_SUCCESS(rv, rv);

  // Convert the obtained favicon data to an input stream
  nsCOMPtr<nsIInputStream> stream;
  rv = NS_NewByteInputStream(getter_AddRefs(stream),
                             reinterpret_cast<const char*>(aData),
                             aDataLen,
                             NS_ASSIGNMENT_DEPEND);
  NS_ENSURE_SUCCESS(rv, rv);

  // Decode the image from the format it was returned to us in (probably PNG)
  nsCOMPtr<imgIContainer> container;
  nsCOMPtr<imgITools> imgtool = do_CreateInstance("@mozilla.org/image/tools;1");
  rv = imgtool->DecodeImageData(stream, aMimeType,
                                getter_AddRefs(container));
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<SourceSurface> surface =
    container->GetFrame(imgIContainer::FRAME_FIRST, 0);
  NS_ENSURE_TRUE(surface, NS_ERROR_FAILURE);

  RefPtr<DataSourceSurface> dataSurface;
  IntSize size;

  if (mURLShortcut) {
    // Create a 48x48 surface and paint the icon into the central 16x16 rect.
    size.width = 48;
    size.height = 48;
    dataSurface =
      Factory::CreateDataSourceSurface(size, SurfaceFormat::B8G8R8A8);
    NS_ENSURE_TRUE(dataSurface, NS_ERROR_FAILURE);

    DataSourceSurface::MappedSurface map;
    if (!dataSurface->Map(DataSourceSurface::MapType::WRITE, &map)) {
      return NS_ERROR_FAILURE;
    }

    RefPtr<DrawTarget> dt =
      Factory::CreateDrawTargetForData(BackendType::CAIRO,
                                       map.mData,
                                       dataSurface->GetSize(),
                                       map.mStride,
                                       dataSurface->GetFormat());
    dt->FillRect(Rect(0, 0, size.width, size.height),
                 ColorPattern(Color(1.0f, 1.0f, 1.0f, 1.0f)));
    dt->DrawSurface(surface,
                    Rect(16, 16, 16, 16),
                    Rect(Point(0, 0),
                         Size(surface->GetSize().width, surface->GetSize().height)));

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
  uint8_t *data = SurfaceToPackedBGRA(dataSurface);
  if (!data) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  int32_t stride = 4 * size.width;
  int32_t dataLength = stride * size.height;

  // AsyncEncodeAndWriteIcon takes ownership of the heap allocated buffer
  nsCOMPtr<nsIRunnable> event = new AsyncEncodeAndWriteIcon(path, data,
                                                            dataLength,
                                                            stride,
                                                            size.width,
                                                            size.height,
                                                            mURLShortcut);
  mIOThread->Dispatch(event, NS_DISPATCH_NORMAL);

  return NS_OK;
}
#endif

// Warning: AsyncEncodeAndWriteIcon assumes ownership of the aData buffer passed in
AsyncEncodeAndWriteIcon::AsyncEncodeAndWriteIcon(const nsAString &aIconPath,
                                                 uint8_t *aBuffer,
                                                 uint32_t aBufferLength,
                                                 uint32_t aStride,
                                                 uint32_t aWidth,
                                                 uint32_t aHeight,
                                                 const bool aURLShortcut) :
  mURLShortcut(aURLShortcut),
  mIconPath(aIconPath),
  mBuffer(aBuffer),
  mBufferLength(aBufferLength),
  mStride(aStride),
  mWidth(aWidth),
  mHeight(aHeight)
{
}

NS_IMETHODIMP AsyncEncodeAndWriteIcon::Run()
{
  NS_PRECONDITION(!NS_IsMainThread(), "Should not be called on the main thread.");

  RefPtr<DrawTarget> dt =
    gfxPlatform::GetPlatform()->ScreenReferenceDrawTarget();
  RefPtr<SourceSurface> surface =
    dt->CreateSourceSurfaceFromData(mBuffer, IntSize(mWidth, mHeight), mStride,
                                    SurfaceFormat::B8G8R8A8);

  FILE* file = fopen(NS_ConvertUTF16toUTF8(mIconPath).get(), "wb");
  if (!file) {
    // Maybe the directory doesn't exist; try creating it, then fopen again.
    nsresult rv = NS_ERROR_FAILURE;
    nsCOMPtr<nsIFile> comFile = do_CreateInstance("@mozilla.org/file/local;1");
    if (comFile) {
      //NS_ConvertUTF8toUTF16 utf16path(mIconPath);
      rv = comFile->InitWithPath(mIconPath);
      if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsIFile> dirPath;
        comFile->GetParent(getter_AddRefs(dirPath));
        if (dirPath) {
          rv = dirPath->Create(nsIFile::DIRECTORY_TYPE, 0777);
          if (NS_SUCCEEDED(rv) || rv == NS_ERROR_FILE_ALREADY_EXISTS) {
            file = fopen(NS_ConvertUTF16toUTF8(mIconPath).get(), "wb");
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
  nsresult rv =
    gfxUtils::EncodeSourceSurface(surface,
                                  NS_LITERAL_CSTRING("image/vnd.microsoft.icon"),
                                  EmptyString(),
                                  gfxUtils::eBinaryEncode,
                                  file);
  fclose(file);
  NS_ENSURE_SUCCESS(rv, rv);

  // Cleanup
  if (mURLShortcut) {
    SendMessage(HWND_BROADCAST, WM_SETTINGCHANGE, SPI_SETNONCLIENTMETRICS, 0);
  }
  return rv;
}

AsyncEncodeAndWriteIcon::~AsyncEncodeAndWriteIcon()
{
}

AsyncDeleteIconFromDisk::AsyncDeleteIconFromDisk(const nsAString &aIconPath)
  : mIconPath(aIconPath)
{
}

NS_IMETHODIMP AsyncDeleteIconFromDisk::Run()
{
  // Construct the parent path of the passed in path
  nsCOMPtr<nsIFile> icoFile = do_CreateInstance("@mozilla.org/file/local;1");
  NS_ENSURE_TRUE(icoFile, NS_ERROR_FAILURE);
  nsresult rv = icoFile->InitWithPath(mIconPath);
  NS_ENSURE_SUCCESS(rv, rv);

  // Check if the cached ICO file exists
  bool exists;
  rv = icoFile->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  // Check that we aren't deleting some arbitrary file that is not an icon
  if (StringTail(mIconPath, 4).LowerCaseEqualsASCII(".ico")) {
    // Check if the cached ICO file exists
    bool exists;
    if (NS_FAILED(icoFile->Exists(&exists)) || !exists)
      return NS_ERROR_FAILURE;

    // We found an ICO file that exists, so we should remove it
    icoFile->Remove(false);
  }

  return NS_OK;
}

AsyncDeleteIconFromDisk::~AsyncDeleteIconFromDisk()
{
}

AsyncDeleteAllFaviconsFromDisk::
  AsyncDeleteAllFaviconsFromDisk(bool aIgnoreRecent)
  : mIgnoreRecent(aIgnoreRecent)
{
  // We can't call FaviconHelper::GetICOCacheSecondsTimeout() on non-main
  // threads, as it reads a pref, so cache its value here.
  mIcoNoDeleteSeconds = FaviconHelper::GetICOCacheSecondsTimeout() + 600;
}

NS_IMETHODIMP AsyncDeleteAllFaviconsFromDisk::Run()
{
  // Construct the path of our jump list cache
  nsCOMPtr<nsIFile> jumpListCacheDir;
  nsresult rv = NS_GetSpecialDirectory("ProfLDS", 
    getter_AddRefs(jumpListCacheDir));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = jumpListCacheDir->AppendNative(
      nsDependentCString(FaviconHelper::kJumpListCacheDir));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsISimpleEnumerator> entries;
  rv = jumpListCacheDir->GetDirectoryEntries(getter_AddRefs(entries));
  NS_ENSURE_SUCCESS(rv, rv);

  // Loop through each directory entry and remove all ICO files found
  do {
    bool hasMore = false;
    if (NS_FAILED(entries->HasMoreElements(&hasMore)) || !hasMore)
      break;

    nsCOMPtr<nsISupports> supp;
    if (NS_FAILED(entries->GetNext(getter_AddRefs(supp))))
      break;

    nsCOMPtr<nsIFile> currFile(do_QueryInterface(supp));
    nsAutoString path;
    if (NS_FAILED(currFile->GetPath(path)))
      continue;

    if (StringTail(path, 4).LowerCaseEqualsASCII(".ico")) {
      // Check if the cached ICO file exists
      bool exists;
      if (NS_FAILED(currFile->Exists(&exists)) || !exists)
        continue;

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
        if (NS_FAILED(rv) ||
          (nowTime - fileModTime) < mIcoNoDeleteSeconds) {
          continue;
        }
      }

      // We found an ICO file that exists, so we should remove it
      currFile->Remove(false);
    }
  } while(true);

  return NS_OK;
}

AsyncDeleteAllFaviconsFromDisk::~AsyncDeleteAllFaviconsFromDisk()
{
}


/*
 * (static) If the data is available, will return the path on disk where 
 * the favicon for page aFaviconPageURI is stored.  If the favicon does not
 * exist, or its cache is expired, this method will kick off an async request
 * for the icon so that next time the method is called it will be available. 
 * @param aFaviconPageURI The URI of the page to obtain
 * @param aICOFilePath The path of the icon file
 * @param aIOThread The thread to perform the Fetch on
 * @param aURLShortcut to distinguish between jumplistcache(false) and shortcutcache(true)
 */
nsresult FaviconHelper::ObtainCachedIconFile(nsCOMPtr<nsIURI> aFaviconPageURI,
                                             nsString &aICOFilePath,
                                             nsCOMPtr<nsIThread> &aIOThread,
                                             bool aURLShortcut)
{
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
    if (NS_FAILED(rv) ||
        (nowTime - fileModTime) > icoReCacheSecondsTimeout) {
        CacheIconFileFromFaviconURIAsync(aFaviconPageURI, icoFile, aIOThread, aURLShortcut);
        return NS_ERROR_NOT_AVAILABLE;
    }
  } else {

    // The file does not exist yet, obtain it async from the favicon service so that
    // the next time we try to build the jump list it'll be available.
    CacheIconFileFromFaviconURIAsync(aFaviconPageURI, icoFile, aIOThread, aURLShortcut);
    return NS_ERROR_NOT_AVAILABLE;
  }

  // The icoFile is filled with a path that exists, get its path
  rv = icoFile->GetPath(aICOFilePath);
  return rv;
}

nsresult FaviconHelper::HashURI(nsCOMPtr<nsICryptoHash> &aCryptoHash, 
                                nsIURI *aUri, 
                                nsACString& aUriHash)
{
  if (!aUri)
    return NS_ERROR_INVALID_ARG;

  nsAutoCString spec;
  nsresult rv = aUri->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!aCryptoHash) {
    aCryptoHash = do_CreateInstance(NS_CRYPTO_HASH_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = aCryptoHash->Init(nsICryptoHash::MD5);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aCryptoHash->Update(reinterpret_cast<const uint8_t*>(spec.BeginReading()), 
                           spec.Length());
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aCryptoHash->Finish(true, aUriHash);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}



// (static) Obtains the ICO file for the favicon at page aFaviconPageURI
// If successful, the file path on disk is in the format:
// <ProfLDS>\jumpListCache\<hash(aFaviconPageURI)>.ico
nsresult FaviconHelper::GetOutputIconPath(nsCOMPtr<nsIURI> aFaviconPageURI,
  nsCOMPtr<nsIFile> &aICOFile,
  bool aURLShortcut)
{
  // Hash the input URI and replace any / with _
  nsAutoCString inputURIHash;
  nsCOMPtr<nsICryptoHash> cryptoHash;
  nsresult rv = HashURI(cryptoHash, aFaviconPageURI,
                        inputURIHash);
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
nsresult 
  FaviconHelper::CacheIconFileFromFaviconURIAsync(nsCOMPtr<nsIURI> aFaviconPageURI,
                                                  nsCOMPtr<nsIFile> aICOFile,
                                                  nsCOMPtr<nsIThread> &aIOThread,
                                                  bool aURLShortcut)
{
#ifdef MOZ_PLACES
  // Obtain the favicon service and get the favicon for the specified page
  nsCOMPtr<mozIAsyncFavicons> favIconSvc(
    do_GetService("@mozilla.org/browser/favicon-service;1"));
  NS_ENSURE_TRUE(favIconSvc, NS_ERROR_FAILURE);

  nsCOMPtr<nsIFaviconDataCallback> callback = 
    new mozilla::widget::AsyncFaviconDataReady(aFaviconPageURI, 
                                               aIOThread, 
                                               aURLShortcut);

  favIconSvc->GetFaviconDataForPage(aFaviconPageURI, callback);
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
  const char PREF_ICOTIMEOUT[]  = "browser.taskbar.lists.icoTimeoutInSeconds";
  icoReCacheSecondsTimeout = Preferences::GetInt(PREF_ICOTIMEOUT, 
                                                 kSecondsPerDay);
  alreadyObtained = true;
  return icoReCacheSecondsTimeout;
}




/* static */
bool
WinUtils::GetShellItemPath(IShellItem* aItem,
                           nsString& aResultString)
{
  NS_ENSURE_TRUE(aItem, false);
  LPWSTR str = nullptr;
  if (FAILED(aItem->GetDisplayName(SIGDN_FILESYSPATH, &str)))
    return false;
  aResultString.Assign(str);
  CoTaskMemFree(str);
  return !aResultString.IsEmpty();
}

/* static */
nsIntRegion
WinUtils::ConvertHRGNToRegion(HRGN aRgn)
{
  NS_ASSERTION(aRgn, "Don't pass NULL region here");

  nsIntRegion rgn;

  DWORD size = ::GetRegionData(aRgn, 0, nullptr);
  nsAutoTArray<uint8_t,100> buffer;
  buffer.SetLength(size);

  RGNDATA* data = reinterpret_cast<RGNDATA*>(buffer.Elements());
  if (!::GetRegionData(aRgn, size, data))
    return rgn;

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

nsIntRect
WinUtils::ToIntRect(const RECT& aRect)
{
  return nsIntRect(aRect.left, aRect.top,
                   aRect.right - aRect.left,
                   aRect.bottom - aRect.top);
}

/* static */
bool
WinUtils::IsIMEEnabled(const InputContext& aInputContext)
{
  return IsIMEEnabled(aInputContext.mIMEState.mEnabled);
}

/* static */
bool
WinUtils::IsIMEEnabled(IMEState::Enabled aIMEState)
{
  return (aIMEState == IMEState::ENABLED ||
          aIMEState == IMEState::PLUGIN);
}

/* static */
void
WinUtils::SetupKeyModifiersSequence(nsTArray<KeyPair>* aArray,
                                    uint32_t aModifiers)
{
  for (uint32_t i = 0; i < ArrayLength(sModifierKeyMap); ++i) {
    const uint32_t* map = sModifierKeyMap[i];
    if (aModifiers & map[0]) {
      aArray->AppendElement(KeyPair(map[1], map[2]));
    }
  }
}

/* static */
bool
WinUtils::ShouldHideScrollbars()
{
#ifdef MOZ_METRO
  if (XRE_GetWindowsEnvironment() == WindowsEnvironmentType_Metro) {
    return widget::winrt::MetroInput::IsInputModeImprecise();
  }
#endif // MOZ_METRO
  return false;
}

} // namespace widget
} // namespace mozilla
