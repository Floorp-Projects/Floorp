/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sts=2 sw=2 et cin: */
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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dean Tessman <dean_tessman@hotmail.com>
 *   Ere Maijala <ere@atp.fi>
 *   Mark Hammond <markh@activestate.com>
 *   Michael Lowe <michael.lowe@bigfoot.com>
 *   Peter Bajusz <hyp-x@inf.bme.hu>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Robert O'Callahan <roc+moz@cs.cmu.edu>
 *   Roy Yokoyama <yokoyama@netscape.com>
 *   Makoto Kato  <m_kato@ga2.so-net.ne.jp>
 *   Masayuki Nakano <masayuki@d-toybox.com>
 *   Dainis Jonitis <Dainis_Jonitis@swh-t.lv>
 *   Christian Biesinger <cbiesinger@web.de>
 *   Mats Palmgren <mats.palmgren@bredband.net>
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

#if defined(DEBUG_ftang)
//#define KE_DEBUG
//#define DEBUG_IME
//#define DEBUG_IME2
//#define DEBUG_KBSTATE
#endif

#include "nsWindow.h"
#include "nsIAppShell.h"
#include "nsIFontMetrics.h"
#include "nsIFontEnumerator.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsFont.h"
#include "nsGUIEvent.h"
#include "nsIRenderingContext.h"
#include "nsIDeviceContext.h"
#include "nsIScreenManager.h"
#include "nsRect.h"
#include "nsTransform2D.h"
#include "nsThreadUtils.h"
#include "nsIObserverService.h"
#include "imgIContainer.h"
#include "gfxIImageFrame.h"
#include "nsNativeCharsetUtils.h"
#include "nsKeyboardLayout.h"
#include "nsWidgetAtoms.h"
#include <windows.h>
#include <process.h>

#ifdef WINCE
#include "aygshell.h"
#include "imm.h"
#include "tpcshell.h"
#endif


// unknwn.h is needed to build with WIN32_LEAN_AND_MEAN
#include <unknwn.h>

//#include <winuser.h>
#include <zmouse.h>
//#include "sysmets.h"
#include "nsGfxCIID.h"
#include "resource.h"
#include <commctrl.h>
#include "prtime.h"
#ifdef MOZ_CAIRO_GFX
#include "nsIThebesRenderingContext.h"
#include "gfxContext.h"
#include "gfxWindowsSurface.h"
#else
#include "nsIRenderingContextWin.h"
#endif
#include "nsIImage.h"

#ifdef ACCESSIBILITY
#include "OLEIDL.H"
#include "winable.h"
#include "nsIAccessible.h"
#include "nsIAccessibleDocument.h"
#include "nsIAccessNode.h"
#ifndef WM_GETOBJECT
#define WM_GETOBJECT 0x03d
#endif
#endif

#ifndef WINCE
#include <pbt.h>
#ifndef PBT_APMRESUMEAUTOMATIC
#define PBT_APMRESUMEAUTOMATIC 0x0012
#endif
#endif

#include "nsNativeDragTarget.h"
#include "nsIRollupListener.h"
#include "nsIMenuRollup.h"
#include "nsIRegion.h"

//~~~ windowless plugin support
#include "nsplugindefs.h"

// For clipboard support
#include "nsIServiceManager.h"
#include "nsIClipboard.h"
#include "nsWidgetsCID.h"

#include "nsITimer.h"

#include "nsITheme.h"

// For SetIcon
#include "nsILocalFile.h"
#include "nsCRT.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsXPIDLString.h"
#include "nsIFile.h"

#include "prprf.h"
#include "prmem.h"

static const char kMozHeapDumpMessageString[] = "MOZ_HeapDump";

#define kWindowPositionSlop 20

#ifndef SPI_GETWHEELSCROLLLINES
#define SPI_GETWHEELSCROLLLINES 104
#endif

#ifndef WM_MOUSEHWHEEL
#define WM_MOUSEHWHEEL 0x020E
#endif

#ifndef SPI_GETWHEELSCROLLCHARS
#define SPI_GETWHEELSCROLLCHARS 0x006C
#endif

#ifndef MAPVK_VSC_TO_VK
#define MAPVK_VSC_TO_VK  1
#define MAPVK_VK_TO_CHAR 2
#endif

#ifdef MOZ_XUL

#ifndef AC_SRC_ALPHA
#define AC_SRC_ALPHA            0x01
#endif

#ifndef WS_EX_LAYERED
#define WS_EX_LAYERED           0x00080000
#endif

#ifndef ULW_ALPHA
#define ULW_ALPHA               0x00000002

extern "C"
WINUSERAPI
BOOL WINAPI UpdateLayeredWindow(HWND hWnd, HDC hdcDst, POINT *pptDst,
                                SIZE *psize, HDC hdcSrc, POINT *pptSrc,
                                COLORREF crKey, BLENDFUNCTION *pblend,
                                DWORD dwFlags);
#endif

#endif


#ifdef WINCE
static PRBool gSoftKeyMenuBar = PR_FALSE;
static PRBool gOverrideHWKeys = PR_TRUE;

typedef BOOL (__stdcall *UnregisterFunc1Proc)( UINT, UINT );
static UnregisterFunc1Proc gProcUnregisterFunc = NULL;
static HINSTANCE gCoreDll = NULL;

UINT gHardwareKeys[][2] =
  {
    { 0xc1, MOD_WIN },
    { 0xc2, MOD_WIN },
    { 0xc3, MOD_WIN },
    { 0xc4, MOD_WIN },
    { 0xc5, MOD_WIN },
    { 0xc6, MOD_WIN },

    { 0x72, 0 },// Answer - 0x72 Modifier - 0  
    { 0x73, 0 },// Hangup - 0x73 Modifier - 0 
    { 0x74, 0 },// 
    { 0x75, 0 },// Volume Up   - 0x75 Modifier - 0
    { 0x76, 0 },// Volume Down - 0x76 Modifier - 0
    { 0, 0 },
  };

static void MapHardwareButtons(HWND window)
{
  if (!window)
    return;

  // handle hardware buttons so that they broadcast into our
  // application. the following code is based on an article
  // on the Pocket PC Developer Network:
  //
  // http://www.pocketpcdn.com/articles/handle_hardware_keys.html
  
  if (gOverrideHWKeys)
  {
    if (!gProcUnregisterFunc)
    {
      gCoreDll = LoadLibrary(_T("coredll.dll")); // leak
      
      if (gCoreDll)
        gProcUnregisterFunc = (UnregisterFunc1Proc)GetProcAddress( gCoreDll, _T("UnregisterFunc1"));
    }
    
    if (gProcUnregisterFunc)
    {    
      for (int i=0; gHardwareKeys[i][0]; i++)
      {
        UINT mod = gHardwareKeys[i][1];
        UINT kc = gHardwareKeys[i][0];
        
        gProcUnregisterFunc(mod, kc);
        RegisterHotKey(window, kc, mod, kc);
      }
    }
  }
}

static void UnmapHardwareButtons()
{
  if (!gProcUnregisterFunc)
    return;

  for (int i=0; gHardwareKeys[i][0]; i++)
  {
    UINT mod = gHardwareKeys[i][1];
    UINT kc = gHardwareKeys[i][0];

    gProcUnregisterFunc(mod, kc);
  }
}

void CreateSoftKeyMenuBar(HWND wnd)
{
  if (!wnd)
    return;

  static HWND gSoftKeyMenuBar = nsnull;

  if (gSoftKeyMenuBar != nsnull)
    return;

  SHMENUBARINFO mbi;
  ZeroMemory(&mbi, sizeof(SHMENUBARINFO));
  mbi.cbSize = sizeof(SHMENUBARINFO);
  mbi.hwndParent = wnd;

  //  On windows ce smartphone, events never occur if the
  //  menubar is empty.  This doesn't work: 
  //  mbi.dwFlags = SHCMBF_EMPTYBAR;

  mbi.nToolBarId = IDC_DUMMY_CE_MENUBAR;
  mbi.hInstRes   = GetModuleHandle(NULL);
  
  if (!SHCreateMenuBar(&mbi))
    return;

  SetWindowPos(mbi.hwndMB, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOACTIVATE);

  SendMessage(mbi.hwndMB, SHCMBM_OVERRIDEKEY, VK_TBACK,
              MAKELPARAM(SHMBOF_NODEFAULT | SHMBOF_NOTIFY,
                         SHMBOF_NODEFAULT | SHMBOF_NOTIFY));
  
  SendMessage(mbi.hwndMB, SHCMBM_OVERRIDEKEY, VK_TSOFT1, 
              MAKELPARAM (SHMBOF_NODEFAULT | SHMBOF_NOTIFY, 
                          SHMBOF_NODEFAULT | SHMBOF_NOTIFY));
  
  
  SendMessage(mbi.hwndMB, SHCMBM_OVERRIDEKEY, VK_TSOFT2, 
              MAKELPARAM (SHMBOF_NODEFAULT | SHMBOF_NOTIFY, 
                          SHMBOF_NODEFAULT | SHMBOF_NOTIFY));

  gSoftKeyMenuBar = mbi.hwndMB;
}

#endif

static PRBool IsCursorTranslucencySupported() {
  static PRBool didCheck = PR_FALSE;
  static PRBool isSupported = PR_FALSE;
  if (!didCheck) {
    didCheck = PR_TRUE;
    // Cursor translucency is supported on Windows XP and newer
    OSVERSIONINFO osversion;
    memset(&osversion, 0, sizeof(OSVERSIONINFO));
    osversion.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    if (GetVersionEx(&osversion))
      isSupported = osversion.dwMajorVersion > 5 || // Newer Windows versions
                    osversion.dwMajorVersion == 5 &&
                       osversion.dwMinorVersion >= 1; // WinXP, Server 2003
  }

  return isSupported;
}


static PRBool IsWin2k()
{
  static PRBool didCheck = PR_FALSE;
  static PRBool isWin2k = PR_FALSE;

  if (!didCheck) {
    didCheck = PR_TRUE;
    OSVERSIONINFO versionInfo;
  
    versionInfo.dwOSVersionInfoSize = sizeof(versionInfo);
    if (::GetVersionEx(&versionInfo))
      isWin2k = versionInfo.dwMajorVersion == 5 &&
                versionInfo.dwMinorVersion == 0;
  }

  return isWin2k;
}


// Pick some random timer ID.  Is there a better way?
#define NS_FLASH_TIMER_ID 0x011231984

static NS_DEFINE_CID(kCClipboardCID,       NS_CLIPBOARD_CID);
static NS_DEFINE_IID(kRenderingContextCID, NS_RENDERING_CONTEXT_CID);

// When we build we are currently (11/27/01) setting the WINVER to 0x0400
// Which means we do not compile in the system resource for the HAND cursor
// this enables us still define the resource and if it isn't there then we will
// get our own hand cursor.
// 32649 is the resource number as defined by WINUSER.H for this cursor
// if the resource is defined by the build env. then it will find it when asked
// if not, then we get our own cursor.
#ifndef IDC_HAND
#define IDC_HAND MAKEINTRESOURCE(32649)
#endif

static const char *sScreenManagerContractID = "@mozilla.org/gfx/screenmanager;1";

////////////////////////////////////////////////////
// Manager for Registering and unregistering OLE
// This is needed for drag & drop & Clipboard support
////////////////////////////////////////////////////
class OleRegisterMgr {
public:
  ~OleRegisterMgr();
protected:
  OleRegisterMgr();

  static OleRegisterMgr mSingleton;
};
OleRegisterMgr OleRegisterMgr::mSingleton;

OleRegisterMgr::OleRegisterMgr()
{
  //DWORD dwVer = ::OleBuildVersion();

  if (FAILED(::OleInitialize(NULL))) {
    NS_ASSERTION(0, "***** OLE has not been initialized!\n");
  } else {
#ifdef DEBUG
    //printf("***** OLE has been initialized!\n");
#endif
  }
}

OleRegisterMgr::~OleRegisterMgr()
{
#ifdef DEBUG
  //printf("***** OLE has been Uninitialized!\n");
#endif
  ::OleUninitialize();
}

////////////////////////////////////////////////////
// nsWindow Class static variable definitions
////////////////////////////////////////////////////
PRUint32   nsWindow::sInstanceCount            = 0;

PRBool     nsWindow::sIMEIsComposing           = PR_FALSE;
PRBool     nsWindow::sIMEIsStatusChanged       = PR_FALSE;

DWORD      nsWindow::sIMEProperty              = 0;
nsString*  nsWindow::sIMECompUnicode           = NULL;
PRUint8*   nsWindow::sIMEAttributeArray        = NULL;
PRInt32    nsWindow::sIMEAttributeArrayLength  = 0;
PRInt32    nsWindow::sIMEAttributeArraySize    = 0;
PRUint32*  nsWindow::sIMECompClauseArray       = NULL;
PRInt32    nsWindow::sIMECompClauseArrayLength = 0;
PRInt32    nsWindow::sIMECompClauseArraySize   = 0;
long       nsWindow::sIMECursorPosition        = 0;
PRUnichar* nsWindow::sIMEReconvertUnicode      = NULL;

RECT*      nsWindow::sIMECompCharPos           = nsnull;
PRInt32    nsWindow::sIMECaretHeight           = 0;

BOOL nsWindow::sIsRegistered       = FALSE;
BOOL nsWindow::sIsPopupClassRegistered = FALSE;
UINT nsWindow::uWM_MSIME_MOUSE     = 0; // mouse message for MSIME
UINT nsWindow::uWM_HEAP_DUMP       = 0; // Heap Dump to a file

HCURSOR        nsWindow::gHCursor            = NULL;
imgIContainer* nsWindow::gCursorImgContainer = nsnull;


#ifdef ACCESSIBILITY
BOOL nsWindow::gIsAccessibilityOn = FALSE;
#endif
nsWindow* nsWindow::gCurrentWindow = nsnull;
////////////////////////////////////////////////////

////////////////////////////////////////////////////
// Rollup Listener - static variable definitions
////////////////////////////////////////////////////
static nsIRollupListener * gRollupListener           = nsnull;
static nsIWidget         * gRollupWidget             = nsnull;
static PRBool              gRollupConsumeRollupEvent = PR_FALSE;

// Hook Data Memebers for Dropdowns
//
// gProcessHook Tells the hook methods whether they should be
//              processing the hook messages
//
static HHOOK        gMsgFilterHook = NULL;
static HHOOK        gCallProcHook  = NULL;
static HHOOK        gCallMouseHook = NULL;
static PRPackedBool gProcessHook   = PR_FALSE;
static UINT         gRollupMsgId   = 0;
static HWND         gRollupMsgWnd  = NULL;
static UINT         gHookTimerId   = 0;
////////////////////////////////////////////////////


////////////////////////////////////////////////////
// Mouse Clicks - static variable definitions
// for figuring out 1 - 3 Clicks
////////////////////////////////////////////////////
static POINT gLastMousePoint;
static POINT gLastMouseMovePoint;
static LONG  gLastMouseDownTime = 0L;
static LONG  gLastClickCount    = 0L;
static BYTE  gLastMouseButton = 0;
////////////////////////////////////////////////////

// The last user input event time in microseconds. If there are any pending
// native toolkit input events it returns the current time. The value is
// compatible with PR_IntervalToMicroseconds(PR_IntervalNow()).
static PRUint32 gLastInputEventTime = 0;

static int gTrimOnMinimize = 2; // uninitialized, but still true

#if 0
static PRBool is_vk_down(int vk)
{
   SHORT st = GetKeyState(vk);
#ifdef DEBUG
   printf("is_vk_down vk=%x st=%x\n",vk, st);
#endif
   return (st < 0);
}
#define IS_VK_DOWN is_vk_down
#else
#define IS_VK_DOWN(a) (GetKeyState(a) < 0)
#endif


//
// input method offsets
//
#define IME_X_OFFSET  0
#define IME_Y_OFFSET  0



#define IS_IME_CODEPAGE(cp) ((932==(cp))||(936==(cp))||(949==(cp))||(950==(cp)))

//
// App Command messages for IntelliMouse and Natural Keyboard Pro
//
// These messages are not included in Visual C++ 6.0, but are in 7.0
//
#ifndef WM_APPCOMMAND
#define WM_APPCOMMAND  0x0319
#endif

#ifndef APPCOMMAND_BROWSER_BACKWARD
#define APPCOMMAND_BROWSER_BACKWARD       1
#define APPCOMMAND_BROWSER_FORWARD        2
#define APPCOMMAND_BROWSER_REFRESH        3
#define APPCOMMAND_BROWSER_STOP           4
#define APPCOMMAND_BROWSER_SEARCH         5
#define APPCOMMAND_BROWSER_FAVORITES      6
#define APPCOMMAND_BROWSER_HOME           7
// keep these around in case we want them later
//#define APPCOMMAND_VOLUME_MUTE            8
//#define APPCOMMAND_VOLUME_DOWN            9
//#define APPCOMMAND_VOLUME_UP              10
//#define APPCOMMAND_MEDIA_NEXTTRACK        11
//#define APPCOMMAND_MEDIA_PREVIOUSTRACK    12
//#define APPCOMMAND_MEDIA_STOP             13
//#define APPCOMMAND_MEDIA_PLAY_PAUSE       14
//#define APPCOMMAND_LAUNCH_MAIL            15
//#define APPCOMMAND_LAUNCH_MEDIA_SELECT    16
//#define APPCOMMAND_LAUNCH_APP1            17
//#define APPCOMMAND_LAUNCH_APP2            18
//#define APPCOMMAND_BASS_DOWN              19
//#define APPCOMMAND_BASS_BOOST             20
//#define APPCOMMAND_BASS_UP                21
//#define APPCOMMAND_TREBLE_DOWN            22
//#define APPCOMMAND_TREBLE_UP              23

//#define FAPPCOMMAND_MOUSE 0x8000
//#define FAPPCOMMAND_KEY   0
//#define FAPPCOMMAND_OEM   0x1000
#define FAPPCOMMAND_MASK  0xF000

#define GET_APPCOMMAND_LPARAM(lParam) ((short)(HIWORD(lParam) & ~FAPPCOMMAND_MASK))
//#define GET_DEVICE_LPARAM(lParam)     ((WORD)(HIWORD(lParam) & FAPPCOMMAND_MASK))
//#define GET_MOUSEORKEY_LPARAM         GET_DEVICE_LPARAM
//#define GET_FLAGS_LPARAM(lParam)      (LOWORD(lParam))
//#define GET_KEYSTATE_LPARAM(lParam)   GET_FLAGS_LPARAM(lParam)

#endif  // #ifndef APPCOMMAND_BROWSER_BACKWARD

/* This object maintains a correlation between attention timers and the
   windows to which they belong. It's lighter than a hashtable (expected usage
   is really just one at a time) and allows nsWindow::GetNSWindowPtr
   to remain private. */
class nsAttentionTimerMonitor {
public:
  nsAttentionTimerMonitor() : mHeadTimer(0) { }
  ~nsAttentionTimerMonitor() {
    TimerInfo *current, *next;
    for (current = mHeadTimer; current; current = next) {
      next = current->next;
      delete current;
    }
  }
  void AddTimer(HWND timerWindow, HWND flashWindow, PRInt32 maxFlashCount, UINT timerID) {
    TimerInfo *info;
    PRBool    newInfo = PR_FALSE;
    info = FindInfo(timerWindow);
    if (!info) {
      info = new TimerInfo;
      newInfo = PR_TRUE;
    }
    if (info) {
      info->timerWindow = timerWindow;
      info->flashWindow = flashWindow;
      info->maxFlashCount = maxFlashCount;
      info->flashCount = 0;
      info->timerID = timerID;
      info->hasFlashed = PR_FALSE;
      info->next = 0;
      if (newInfo)
        AppendTimer(info);
    }
  }
  HWND GetFlashWindowFor(HWND timerWindow) {
    TimerInfo *info = FindInfo(timerWindow);
    return info ? info->flashWindow : 0;
  }
  PRInt32 GetMaxFlashCount(HWND timerWindow) {
    TimerInfo *info = FindInfo(timerWindow);
    return info ? info->maxFlashCount : -1;
  }
  PRInt32 GetFlashCount(HWND timerWindow) {
    TimerInfo *info = FindInfo(timerWindow);
    return info ? info->flashCount : -1;
  }
  void IncrementFlashCount(HWND timerWindow) {
    TimerInfo *info = FindInfo(timerWindow);
    ++(info->flashCount);
  }
  void KillTimer(HWND timerWindow) {
    TimerInfo *info = FindInfo(timerWindow);
    if (info) {
      // make sure it's unflashed and kill the timer

      if (info->hasFlashed)
        ::FlashWindow(info->flashWindow, FALSE);

      ::KillTimer(info->timerWindow, info->timerID);
      RemoveTimer(info);
      delete info;
    }
  }
  void SetFlashed(HWND timerWindow) {
    TimerInfo *info = FindInfo(timerWindow);
    if (info)
      info->hasFlashed = PR_TRUE;
  }

private:
  struct TimerInfo {
    HWND       timerWindow,
               flashWindow;
    UINT       timerID;
    PRInt32    maxFlashCount;
    PRInt32    flashCount;
    PRBool     hasFlashed;
    TimerInfo *next;
  };
  TimerInfo *FindInfo(HWND timerWindow) {
    TimerInfo *scan;
    for (scan = mHeadTimer; scan; scan = scan->next)
      if (scan->timerWindow == timerWindow)
        break;
    return scan;
  }
  void AppendTimer(TimerInfo *info) {
    if (!mHeadTimer)
      mHeadTimer = info;
    else {
      TimerInfo *scan, *last;
      for (scan = mHeadTimer; scan; scan = scan->next)
        last = scan;
      last->next = info;
    }
  }
  void RemoveTimer(TimerInfo *info) {
    TimerInfo *scan, *last = 0;
    for (scan = mHeadTimer; scan && scan != info; scan = scan->next)
      last = scan;
    if (scan) {
      if (last)
        last->next = scan->next;
      else
        mHeadTimer = scan->next;
    }
  }

  TimerInfo *mHeadTimer;
};

static nsAttentionTimerMonitor *gAttentionTimerMonitor = 0;

HWND nsWindow::GetTopLevelHWND(HWND aWnd, PRBool aStopOnFirstTopLevel)
{
  HWND curWnd = aWnd;
  HWND topWnd = NULL;

  while (curWnd)
  {
    topWnd = curWnd;

#ifndef WINCE
    if (aStopOnFirstTopLevel)
    {
      DWORD style = ::GetWindowLongW(curWnd, GWL_STYLE);

      if (!(style & WS_CHILDWINDOW))    // first top-level window
        break;
    }
#endif

    curWnd = ::GetParent(curWnd);       // Parent or owner (if has no parent)
  }

  return topWnd;
}

// Code to dispatch WM_SYSCOLORCHANGE message to all child windows.
// WM_SYSCOLORCHANGE is only sent to top-level windows, but the
// cross platform API requires that NS_SYSCOLORCHANGE message be sent to
// all child windows as well. When running in an embedded application
// we may not receive a WM_SYSCOLORCHANGE message because the top
// level window is owned by the embeddor. Note: this code can be used to
// dispatch other global messages (i.e messages that must be sent to all
// nsIWidget instances.

// Enumerate all child windows sending aMsg to each of them

BOOL CALLBACK nsWindow::BroadcastMsgToChildren(HWND aWnd, LPARAM aMsg)
{
  WNDPROC winProc = (WNDPROC)::GetWindowLongW(aWnd, GWL_WNDPROC);
  if (winProc == &nsWindow::WindowProc) {
    // it's one of our windows so go ahead and send a message to it
    ::CallWindowProcW(winProc, aWnd, aMsg, 0, 0);
  }
  return TRUE;
}

// Enumerate all top level windows specifying that the children of each
// top level window should be enumerated. Do *not* send the message to
// each top level window since it is assumed that the toolkit will send
// aMsg to them directly.

BOOL CALLBACK nsWindow::BroadcastMsg(HWND aTopWindow, LPARAM aMsg)
{
  // Iterate each of aTopWindows child windows sending the aMsg
  // to each of them.
  EnumChildWindows(aTopWindow, nsWindow::BroadcastMsgToChildren, aMsg);
  return TRUE;
}

// This method is called from nsToolkit::WindowProc to forward global
// messages which need to be dispatched to all child windows.

void nsWindow::GlobalMsgWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch (msg) {
    case WM_SYSCOLORCHANGE:
      // System color changes are posted to top-level windows only.
      // The NS_SYSCOLORCHANGE must be dispatched to all child
      // windows as well.
     ::EnumThreadWindows(GetCurrentThreadId(), nsWindow::BroadcastMsg, msg);
    break;
  }
}

// End of the methods to dispatch global messages

//-------------------------------------------------------------------------
//
// nsISupport stuff
//
//-------------------------------------------------------------------------
NS_IMPL_ADDREF(nsWindow)
NS_IMPL_RELEASE(nsWindow)
NS_IMETHODIMP nsWindow::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  if (aIID.Equals(NS_GET_IID(nsIKBStateControl))) {
    *aInstancePtr = (void*) ((nsIKBStateControl*)this);
    NS_ADDREF((nsBaseWidget*)this);
    return NS_OK;
  }

  return nsBaseWidget::QueryInterface(aIID,aInstancePtr);
}
//-------------------------------------------------------------------------
//
// nsWindow constructor
//
//-------------------------------------------------------------------------
#ifdef ACCESSIBILITY
nsWindow::nsWindow() : nsBaseWidget()
#else
nsWindow::nsWindow() : nsBaseWidget()
#endif
{
  mWnd                = 0;
  mPaintDC                 = 0;
  mPrevWndProc        = NULL;
  mBackground         = ::GetSysColor(COLOR_BTNFACE);
  mBrush              = ::CreateSolidBrush(NSRGB_2_COLOREF(mBackground));
  mForeground         = ::GetSysColor(COLOR_WINDOWTEXT);
  mIsShiftDown        = PR_FALSE;
  mIsControlDown      = PR_FALSE;
  mIsAltDown          = PR_FALSE;
  mIsDestroying       = PR_FALSE;
  mOnDestroyCalled    = PR_FALSE;
  mDeferredPositioner = NULL;
  mLastPoint.x        = 0;
  mLastPoint.y        = 0;
  mPreferredWidth     = 0;
  mPreferredHeight    = 0;
  mFont               = nsnull;
  mIsVisible          = PR_FALSE;
  mHas3DBorder        = PR_FALSE;
#ifdef MOZ_XUL
  mIsTranslucent      = PR_FALSE;
  mIsTopTranslucent   = PR_FALSE;
  mMemoryDC           = NULL;
  mMemoryBitmap       = NULL;
  mMemoryBits         = NULL;
  mAlphaMask          = nsnull;
#endif
  mWindowType         = eWindowType_child;
  mBorderStyle        = eBorderStyle_default;
  mUnicodeWidget      = PR_TRUE;
  mIsInMouseCapture   = PR_FALSE;
  mIsInMouseWheelProcessing = PR_FALSE;
  mLastSize.width     = 0;
  mLastSize.height    = 0;
  mOldStyle           = 0;
  mOldExStyle         = 0;
  mPainting           = 0;
  mOldIMC             = NULL;

  mLeadByte = '\0';
  mBlurEventSuppressionLevel = 0;

  static BOOL gbInitGlobalValue = FALSE;
  if (! gbInitGlobalValue) {
    gbInitGlobalValue = TRUE;
    gKeyboardLayout = GetKeyboardLayout(0);

    // mouse message of MSIME98/2000
    nsWindow::uWM_MSIME_MOUSE     = ::RegisterWindowMessage(RWM_MOUSE);

    // Heap dump
#ifndef WINCE
    nsWindow::uWM_HEAP_DUMP = ::RegisterWindowMessage(kMozHeapDumpMessageString);
#endif
  }

  mNativeDragTarget = nsnull;
  mIsTopWidgetWindow = PR_FALSE;
  mLastKeyboardLayout = 0;

  sInstanceCount++;

#ifdef WINCE
  nsCOMPtr<nsIPrefService> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefs) {
    nsCOMPtr<nsIPrefBranch> prefBranch;
    prefs->GetBranch(0, getter_AddRefs(prefBranch));
    if (prefBranch)
    {
      prefBranch->GetBoolPref("config.wince.overrideHWKeys", &gOverrideHWKeys);
    }
  }
#endif
}


HKL nsWindow::gKeyboardLayout = 0;
PRBool nsWindow::gSwitchKeyboardLayout = PR_FALSE;
static KeyboardLayout gKbdLayout;

//-------------------------------------------------------------------------
//
// nsWindow destructor
//
//-------------------------------------------------------------------------
nsWindow::~nsWindow()
{
  mIsDestroying = PR_TRUE;
  if (gCurrentWindow == this) {
    gCurrentWindow = nsnull;
  }

  MouseTrailer* mtrailer = nsToolkit::gMouseTrailer;
  if (mtrailer) {
    if (mtrailer->GetMouseTrailerWindow() == mWnd)
      mtrailer->DestroyTimer();

    if (mtrailer->GetCaptureWindow() == mWnd)
      mtrailer->SetCaptureWindow(nsnull);
  }

  // If the widget was released without calling Destroy() then the native
  // window still exists, and we need to destroy it
  if (NULL != mWnd) {
    Destroy();
  }

  //XXX Temporary: Should not be caching the font
  delete mFont;

  if (mCursor == -1) {
    // A successfull SetCursor call will destroy the custom cursor, if it's ours
    SetCursor(eCursor_standard);
  }

  //
  // delete any of the IME structures that we allocated
  //
  sInstanceCount--;
  if (sInstanceCount == 0) {
    if (sIMECompUnicode) 
      delete sIMECompUnicode;
    if (sIMEAttributeArray) 
      delete [] sIMEAttributeArray;
    if (sIMECompClauseArray) 
      delete [] sIMECompClauseArray;
    if (sIMEReconvertUnicode)
      nsMemory::Free(sIMEReconvertUnicode);

    NS_IF_RELEASE(gCursorImgContainer);
  }

  NS_IF_RELEASE(mNativeDragTarget);

}


NS_METHOD nsWindow::CaptureMouse(PRBool aCapture)
{
  if (!nsToolkit::gMouseTrailer) {
    NS_ERROR("nsWindow::CaptureMouse called after nsToolkit destroyed");
    return NS_OK;
  }

  if (aCapture) {
    nsToolkit::gMouseTrailer->SetCaptureWindow(mWnd);
    ::SetCapture(mWnd);
  } else {
    nsToolkit::gMouseTrailer->SetCaptureWindow(NULL);
    ::ReleaseCapture();
  }
  mIsInMouseCapture = aCapture;
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Default for height modification is to do nothing
//
//-------------------------------------------------------------------------

PRInt32 nsWindow::GetHeight(PRInt32 aProposedHeight)
{
  return(aProposedHeight);
}

//-------------------------------------------------------------------------
//
// Deferred Window positioning
//
//-------------------------------------------------------------------------

NS_METHOD nsWindow::BeginResizingChildren(void)
{
  if (NULL == mDeferredPositioner)
    mDeferredPositioner = ::BeginDeferWindowPos(1);
  return NS_OK;
}

NS_METHOD nsWindow::EndResizingChildren(void)
{
  if (NULL != mDeferredPositioner) {
    ::EndDeferWindowPos(mDeferredPositioner);
    mDeferredPositioner = NULL;
  }
  return NS_OK;
}

NS_METHOD nsWindow::WidgetToScreen(const nsRect& aOldRect, nsRect& aNewRect)
{
  POINT point;
  point.x = aOldRect.x;
  point.y = aOldRect.y;
  ::ClientToScreen(mWnd, &point);
  aNewRect.x = point.x;
  aNewRect.y = point.y;
  aNewRect.width = aOldRect.width;
  aNewRect.height = aOldRect.height;
  return NS_OK;
}

NS_METHOD nsWindow::ScreenToWidget(const nsRect& aOldRect, nsRect& aNewRect)
{
  POINT point;
  point.x = aOldRect.x;
  point.y = aOldRect.y;
  ::ScreenToClient(mWnd, &point);
  aNewRect.x = point.x;
  aNewRect.y = point.y;
  aNewRect.width = aOldRect.width;
  aNewRect.height = aOldRect.height;
  return NS_OK;
}

LPARAM nsWindow::lParamToScreen(LPARAM lParam)
{
  POINT pt;
  pt.x = GET_X_LPARAM(lParam);
  pt.y = GET_Y_LPARAM(lParam);
  ::ClientToScreen(mWnd, &pt);
  return MAKELPARAM(pt.x, pt.y);
}

LPARAM nsWindow::lParamToClient(LPARAM lParam)
{
  POINT pt;
  pt.x = GET_X_LPARAM(lParam);
  pt.y = GET_Y_LPARAM(lParam);
  ::ScreenToClient(mWnd, &pt);
  return MAKELPARAM(pt.x, pt.y);
}

//-------------------------------------------------------------------------
//
// Initialize an event to dispatch
//
//-------------------------------------------------------------------------
void nsWindow::InitEvent(nsGUIEvent& event, nsPoint* aPoint)
{
  NS_ADDREF(event.widget);

  if (nsnull == aPoint) {     // use the point from the event
    // get the message position in client coordinates and in twips
    if (mWnd != NULL) {

      DWORD pos = ::GetMessagePos();
      POINT cpos;
      
      cpos.x = GET_X_LPARAM(pos);
      cpos.y = GET_Y_LPARAM(pos);

      ::ScreenToClient(mWnd, &cpos);
      event.refPoint.x = cpos.x;
      event.refPoint.y = cpos.y;
    } else {
      event.refPoint.x = 0;
      event.refPoint.y = 0;
    }
  }
  else {                      // use the point override if provided
    event.refPoint.x = aPoint->x;
    event.refPoint.y = aPoint->y;
  }

  event.time = ::GetMessageTime();

  mLastPoint = event.refPoint;
}

/* In some circumstances (opening dependent windows) it makes more sense
   (and fixes a crash bug) to not blur the parent window. */
void nsWindow::SuppressBlurEvents(PRBool aSuppress)
{
  if (aSuppress)
    ++mBlurEventSuppressionLevel; // for this widget
  else {
    NS_ASSERTION(mBlurEventSuppressionLevel > 0, "unbalanced blur event suppression");
    if (mBlurEventSuppressionLevel > 0)
      --mBlurEventSuppressionLevel;
  }
}

PRBool nsWindow::BlurEventsSuppressed()
{
  // are they suppressed in this window?
  if (mBlurEventSuppressionLevel > 0)
    return PR_TRUE;

  // are they suppressed by any container widget?
  HWND parentWnd = ::GetParent(mWnd);
  if (parentWnd) {
    nsWindow *parent = GetNSWindowPtr(parentWnd);
    if (parent)
      return parent->BlurEventsSuppressed();
  }
  return PR_FALSE;
}

//-------------------------------------------------------------------------
//
// Invokes callback and ProcessEvent method on Event Listener object
//
//-------------------------------------------------------------------------

NS_IMETHODIMP nsWindow::DispatchEvent(nsGUIEvent* event, nsEventStatus & aStatus)
{
#ifdef NS_DEBUG
  debug_DumpEvent(stdout,
                  event->widget,
                  event,
                  nsCAutoString("something"),
                  (PRInt32) mWnd);
#endif // NS_DEBUG

  aStatus = nsEventStatus_eIgnore;

  // skip processing of suppressed blur events
  if ((event->message == NS_DEACTIVATE || event->message == NS_LOSTFOCUS) &&
      BlurEventsSuppressed())
    return NS_OK;

  if (nsnull != mEventCallback) {
    aStatus = (*mEventCallback)(event);
  }

  // Dispatch to event listener if event was not consumed
  if ((aStatus != nsEventStatus_eIgnore) && (nsnull != mEventListener)) {
    aStatus = mEventListener->ProcessEvent(*event);
  }

  // the window can be destroyed during processing of seemingly innocuous events like, say,
  // mousedowns due to the magic of scripting. mousedowns will return nsEventStatus_eIgnore,
  // which causes problems with the deleted window. therefore:
  if (mOnDestroyCalled)
    aStatus = nsEventStatus_eConsumeNoDefault;
  return NS_OK;
}

//-------------------------------------------------------------------------
PRBool nsWindow::DispatchWindowEvent(nsGUIEvent* event)
{
  nsEventStatus status;
  DispatchEvent(event, status);
  return ConvertStatus(status);
}

PRBool nsWindow::DispatchWindowEvent(nsGUIEvent* event, nsEventStatus &aStatus) {
  DispatchEvent(event, aStatus);
  return ConvertStatus(aStatus);
}

//-------------------------------------------------------------------------
//
// Dispatch standard event
//
//-------------------------------------------------------------------------

PRBool nsWindow::DispatchStandardEvent(PRUint32 aMsg)
{
  nsGUIEvent event(PR_TRUE, aMsg, this);
  InitEvent(event);

  PRBool result = DispatchWindowEvent(&event);
  NS_RELEASE(event.widget);
  return result;
}

//-------------------------------------------------------------------------
//
// Dispatch app command event
//
//-------------------------------------------------------------------------
PRBool nsWindow::DispatchCommandEvent(PRUint32 aEventCommand)
{
  nsCOMPtr<nsIAtom> command;
  switch (aEventCommand) {
    case APPCOMMAND_BROWSER_BACKWARD:
      command = nsWidgetAtoms::Back;
      break;
    case APPCOMMAND_BROWSER_FORWARD:
      command = nsWidgetAtoms::Forward;
      break;
    case APPCOMMAND_BROWSER_REFRESH:
      command = nsWidgetAtoms::Reload;
      break;
    case APPCOMMAND_BROWSER_STOP:
      command = nsWidgetAtoms::Stop;
      break;
    case APPCOMMAND_BROWSER_SEARCH:
      command = nsWidgetAtoms::Search;
      break;
    case APPCOMMAND_BROWSER_FAVORITES:
      command = nsWidgetAtoms::Bookmarks;
      break;
    case APPCOMMAND_BROWSER_HOME:
      command = nsWidgetAtoms::Home;
      break;
    default:
      return PR_FALSE;
  }
  nsCommandEvent event(PR_TRUE, nsWidgetAtoms::onAppCommand, command, this);

  InitEvent(event);
  DispatchWindowEvent(&event);
  NS_RELEASE(event.widget);

  return PR_TRUE;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::CaptureRollupEvents(nsIRollupListener * aListener,
                                            PRBool aDoCapture,
                                            PRBool aConsumeRollupEvent)
{
  if (aDoCapture) {
    /* we haven't bothered carrying a weak reference to gRollupWidget because
       we believe lifespan is properly scoped. this next assertion helps
       assure that remains true. */
    NS_ASSERTION(!gRollupWidget, "rollup widget reassigned before release");
    gRollupConsumeRollupEvent = aConsumeRollupEvent;
    NS_IF_RELEASE(gRollupListener);
    NS_IF_RELEASE(gRollupWidget);
    gRollupListener = aListener;
    NS_ADDREF(aListener);
    gRollupWidget = this;
    NS_ADDREF(this);

#ifndef WINCE
    if (!gMsgFilterHook && !gCallProcHook && !gCallMouseHook) {
      RegisterSpecialDropdownHooks();
    }
    gProcessHook = PR_TRUE;
#endif
    
  } else {
    NS_IF_RELEASE(gRollupListener);
    NS_IF_RELEASE(gRollupWidget);
    
#ifndef WINCE
    gProcessHook = PR_FALSE;
    UnregisterSpecialDropdownHooks();
#endif
  }

  return NS_OK;
}

PRBool
nsWindow::EventIsInsideWindow(UINT Msg, nsWindow* aWindow)
{
  RECT r;

  if (Msg == WM_ACTIVATE)
#ifndef WINCE
    // don't care about activation/deactivation
    return PR_FALSE;
#else
    // but on Windows CE we do care about
    // activation/deactivation because there doesn't exist
    // cancelable Mouse Activation events
    return TRUE;
#endif

  ::GetWindowRect(aWindow->mWnd, &r);
  DWORD pos = ::GetMessagePos();
  POINT mp;
  mp.x = GET_X_LPARAM(pos);
  mp.y = GET_Y_LPARAM(pos);

  // was the event inside this window?
  return (PRBool) PtInRect(&r, mp);
}

static char sPropName[40] = "";
static char* GetNSWindowPropName() {
  if (!*sPropName)
  {
    _snprintf(sPropName, 39, "MozillansIWidgetPtr%p", _getpid());
    sPropName[39] = '\0';
  }
  return sPropName;
}

nsWindow * nsWindow::GetNSWindowPtr(HWND aWnd) {
  return (nsWindow *) ::GetPropA(aWnd, GetNSWindowPropName());
}

BOOL nsWindow::SetNSWindowPtr(HWND aWnd, nsWindow * ptr) {
  if (ptr == NULL) {
    ::RemovePropA(aWnd, GetNSWindowPropName());
    return TRUE;
  } else {
    return ::SetPropA(aWnd, GetNSWindowPropName(), (HANDLE)ptr);
  }
}

//-------------------------------------------------------------------------
//
// the nsWindow procedure for all nsWindows in this toolkit
//
//-------------------------------------------------------------------------
LRESULT CALLBACK nsWindow::WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  LRESULT popupHandlingResult;
  if ( DealWithPopups(hWnd, msg, wParam, lParam, &popupHandlingResult) )
    return popupHandlingResult;

  // Get the window which caused the event and ask it to process the message
  nsWindow *someWindow = GetNSWindowPtr(hWnd);

  // XXX This fixes 50208 and we are leaving 51174 open to further investigate
  // why we are hitting this assert
  if (nsnull == someWindow) {
    NS_ASSERTION(someWindow, "someWindow is null, cannot call any CallWindowProc");
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
  }

  // hold on to the window for the life of this method, in case it gets
  // deleted during processing. yes, it's a double hack, since someWindow
  // is not really an interface.
  nsCOMPtr<nsISupports> kungFuDeathGrip;
  if (!someWindow->mIsDestroying) // not if we're in the destructor!
    kungFuDeathGrip = do_QueryInterface((nsBaseWidget*)someWindow);

  // Re-direct a tab change message destined for its parent window to the
  // the actual window which generated the event.
  if (msg == WM_NOTIFY) {
    LPNMHDR pnmh = (LPNMHDR) lParam;
    if (pnmh->code == TCN_SELCHANGE) {
      someWindow = GetNSWindowPtr(pnmh->hwndFrom);
    }
  }

  if (nsnull != someWindow) {
    LRESULT retValue;
    if (PR_TRUE == someWindow->ProcessMessage(msg, wParam, lParam, &retValue)) {
      return retValue;
    }
  }

#if defined(STRICT)
  return ::CallWindowProcW((WNDPROC)someWindow->GetPrevWindowProc(), hWnd,
                                    msg, wParam, lParam);
#else
  return ::CallWindowProcW((FARPROC)someWindow->GetPrevWindowProc(), hWnd,
                                    msg, wParam, lParam);
#endif
}

//
// Default Window procedure for AIMM support.
//
LRESULT CALLBACK nsWindow::DefaultWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  //XXX nsWindow::DefaultWindowProc still ever required?
  return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

static BOOL CALLBACK DummyDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  return FALSE;
}

//WINOLEAPI oleStatus;
//-------------------------------------------------------------------------
//
// Utility method for implementing both Create(nsIWidget ...) and
// Create(nsNativeWidget...)
//-------------------------------------------------------------------------

nsresult
nsWindow::StandardWindowCreate(nsIWidget *aParent,
                               const nsRect &aRect,
                               EVENT_CALLBACK aHandleEventFunction,
                               nsIDeviceContext *aContext,
                               nsIAppShell *aAppShell,
                               nsIToolkit *aToolkit,
                               nsWidgetInitData *aInitData,
                               nsNativeWidget aNativeParent)
{
  nsIWidget *baseParent = aInitData &&
                         (aInitData->mWindowType == eWindowType_dialog ||
                          aInitData->mWindowType == eWindowType_toplevel ||
                          aInitData->mWindowType == eWindowType_invisible) ?
                         nsnull : aParent;

  mIsTopWidgetWindow = (nsnull == baseParent);
  mBounds.width = aRect.width;
  mBounds.height = aRect.height;

  BaseCreate(baseParent, aRect, aHandleEventFunction, aContext,
             aAppShell, aToolkit, aInitData);

  // Switch to the "main gui thread" if necessary... This method must
  // be executed on the "gui thread"...

  nsToolkit* toolkit = (nsToolkit *)mToolkit;
  if (toolkit && !toolkit->IsGuiThread()) {
    DWORD args[7];
    args[0] = (DWORD)aParent;
    args[1] = (DWORD)&aRect;
    args[2] = (DWORD)aHandleEventFunction;
    args[3] = (DWORD)aContext;
    args[4] = (DWORD)aAppShell;
    args[5] = (DWORD)aToolkit;
    args[6] = (DWORD)aInitData;

    if (nsnull != aParent) {
      // nsIWidget parent dispatch
      MethodInfo info(this, nsWindow::CREATE, 7, args);
      toolkit->CallMethod(&info);
      return NS_OK;
    }
    else {
      // Native parent dispatch
      MethodInfo info(this, nsWindow::CREATE_NATIVE, 5, args);
      toolkit->CallMethod(&info);
      return NS_OK;
    }
  }

  HWND parent;
  if (nsnull != aParent) { // has a nsIWidget parent
    parent = ((aParent) ? (HWND)aParent->GetNativeData(NS_NATIVE_WINDOW) : nsnull);
  } else { // has a nsNative parent
    parent = (HWND)aNativeParent;
  }

  if (nsnull != aInitData) {
    SetWindowType(aInitData->mWindowType);
    SetBorderStyle(aInitData->mBorderStyle);
  }

  mContentType = aInitData ? aInitData->mContentType : eContentTypeInherit;

  DWORD style = WindowStyle();
  DWORD extendedStyle = WindowExStyle();

  if (mWindowType == eWindowType_popup) {
    NS_ASSERTION(!aParent, "Popups should not be hooked into the nsIWidget hierarchy");
    // Don't set the parent of a popup window.
    parent = NULL;
  } else if (nsnull != aInitData) {
    // See if the caller wants to explictly set clip children and clip siblings
    if (aInitData->clipChildren) {
      style |= WS_CLIPCHILDREN;
    } else {
      style &= ~WS_CLIPCHILDREN;
    }
    if (aInitData->clipSiblings) {
      style |= WS_CLIPSIBLINGS;
    }
  }

  mHas3DBorder = (extendedStyle & WS_EX_CLIENTEDGE) > 0;

  mWnd = ::CreateWindowExW(extendedStyle,
                           aInitData && aInitData->mDropShadow ?
                           WindowPopupClassW() : WindowClassW(),
                           L"",
                           style,
                           aRect.x,
                           aRect.y,
                           aRect.width,
                           GetHeight(aRect.height),
                           parent,
                           NULL,
                           nsToolkit::mDllInstance,
                           NULL);

  if (!mWnd)
    return NS_ERROR_FAILURE;

  /*mNativeDragTarget = new nsNativeDragTarget(this);
  if (NULL != mNativeDragTarget) {
    mNativeDragTarget->AddRef();
    if (S_OK == ::CoLockObjectExternal((LPUNKNOWN)mNativeDragTarget,TRUE,FALSE)) {
      if (S_OK == ::RegisterDragDrop(mWnd, (LPDROPTARGET)mNativeDragTarget)) {
      }
    }
  }*/

  // call the event callback to notify about creation

  DispatchStandardEvent(NS_CREATE);
  SubclassWindow(TRUE);

  if (gTrimOnMinimize == 2 && mWindowType == eWindowType_invisible) {
    /* The internal variable set by the config.trim_on_minimize pref
       has not yet been initialized, and this is the hidden window
       (conveniently created before any visible windows, and after
       the profile has been initialized).
       
       Default config.trim_on_minimize to false, to fix bug 76831
       for good.  If anyone complains about this new default, saying
       that a Mozilla app hogs too much memory while minimized, they
       will have that entire bug tattooed on their backside. */

    gTrimOnMinimize = 0;
    nsCOMPtr<nsIPrefService> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (prefs) {
      nsCOMPtr<nsIPrefBranch> prefBranch;
      prefs->GetBranch(0, getter_AddRefs(prefBranch));
      if (prefBranch) {
        PRBool trimOnMinimize;
        if (NS_SUCCEEDED(prefBranch->GetBoolPref("config.trim_on_minimize",
                                                 &trimOnMinimize))
            && trimOnMinimize)
          gTrimOnMinimize = 1;

        PRBool switchKeyboardLayout;
        if (NS_SUCCEEDED(prefBranch->GetBoolPref("intl.keyboard.per_window_layout",
                                                 &switchKeyboardLayout)))
          gSwitchKeyboardLayout = switchKeyboardLayout;
      }
    }
  }
#ifdef WINCE
  
  if (mWindowType == eWindowType_dialog || mWindowType == eWindowType_toplevel )
    CreateSoftKeyMenuBar(mWnd);
  
  MapHardwareButtons(mWnd);
#endif

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Create the proper widget
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Create(nsIWidget *aParent,
                           const nsRect &aRect,
                           EVENT_CALLBACK aHandleEventFunction,
                           nsIDeviceContext *aContext,
                           nsIAppShell *aAppShell,
                           nsIToolkit *aToolkit,
                           nsWidgetInitData *aInitData)
{
  if (aInitData)
    mUnicodeWidget = aInitData->mUnicode;
  return(StandardWindowCreate(aParent, aRect, aHandleEventFunction,
                              aContext, aAppShell, aToolkit, aInitData,
                              nsnull));
}


//-------------------------------------------------------------------------
//
// create with a native parent
//
//-------------------------------------------------------------------------

NS_METHOD nsWindow::Create(nsNativeWidget aParent,
                           const nsRect &aRect,
                           EVENT_CALLBACK aHandleEventFunction,
                           nsIDeviceContext *aContext,
                           nsIAppShell *aAppShell,
                           nsIToolkit *aToolkit,
                           nsWidgetInitData *aInitData)
{
  if (aInitData)
    mUnicodeWidget = aInitData->mUnicode;
  return(StandardWindowCreate(nsnull, aRect, aHandleEventFunction,
                              aContext, aAppShell, aToolkit, aInitData,
                              aParent));
}


//-------------------------------------------------------------------------
//
// Close this nsWindow
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Destroy()
{
  // Switch to the "main gui thread" if necessary... This method must
  // be executed on the "gui thread"...
  nsToolkit* toolkit = (nsToolkit *)mToolkit;
  if (toolkit != nsnull && !toolkit->IsGuiThread()) {
    MethodInfo info(this, nsWindow::DESTROY);
    toolkit->CallMethod(&info);
    return NS_ERROR_FAILURE;
  }

  // disconnect from the parent
  if (!mIsDestroying) {
    nsBaseWidget::Destroy();
  }

  // just to be safe. If we're going away and for some reason we're still
  // the rollup widget, rollup and turn off capture.
  if ( this == gRollupWidget ) {
    if ( gRollupListener )
      gRollupListener->Rollup();
    CaptureRollupEvents(nsnull, PR_FALSE, PR_TRUE);
  }

  EnableDragDrop(PR_FALSE);

  // destroy the HWND
  if (mWnd) {
    // prevent the widget from causing additional events
    mEventCallback = nsnull;
    if (gAttentionTimerMonitor)
      gAttentionTimerMonitor->KillTimer(mWnd);

    // if IME is disabled, restore it.
    if (mOldIMC) {
      mOldIMC = ::ImmAssociateContext(mWnd, mOldIMC);
      NS_ASSERTION(!mOldIMC, "Another IMC was associated");
    }

    HICON icon;
    icon = (HICON) ::SendMessageW(mWnd, WM_SETICON, (WPARAM)ICON_BIG, (LPARAM) 0);
    if (icon)
      ::DestroyIcon(icon);

    icon = (HICON) ::SendMessageW(mWnd, WM_SETICON, (WPARAM)ICON_SMALL, (LPARAM) 0);
    if (icon)
      ::DestroyIcon(icon);

#ifdef MOZ_XUL
    if (mIsTranslucent)
    {
      SetupTranslucentWindowMemoryBitmap(PR_FALSE);

      delete [] mAlphaMask;
      mAlphaMask = nsnull;
    }
#endif

    VERIFY(::DestroyWindow(mWnd));

    mWnd = NULL;
    //our windows can be subclassed by
    //others and these nameless, faceless others
    //may not let us know about WM_DESTROY. so,
    //if OnDestroy() didn't get called, just call
    //it now. MMP
    if (PR_FALSE == mOnDestroyCalled)
      OnDestroy();
  }

  return NS_OK;
}

NS_IMETHODIMP nsWindow::SetParent(nsIWidget *aNewParent)
{
  if (aNewParent) {
    HWND newParent = (HWND)aNewParent->GetNativeData(NS_NATIVE_WINDOW);
    NS_ASSERTION(newParent, "Parent widget has a null native window handle");
    ::SetParent(mWnd, newParent);

    return NS_OK;
  }
  NS_WARNING("Null aNewParent passed to SetParent");
  return NS_ERROR_FAILURE;
}


//-------------------------------------------------------------------------
//
// Get this nsWindow parent
//
//-------------------------------------------------------------------------
nsIWidget* nsWindow::GetParent(void)
{
  return GetParent(PR_TRUE);
}

// XXX does anyone pass false for aStopOnFirstTopLevel?
nsWindow* nsWindow::GetParent(PRBool aStopOnFirstTopLevel)
{
  if (mIsTopWidgetWindow && aStopOnFirstTopLevel) {
    // Must use a flag instead of mWindowType to tell if the window is the
    // owned by the topmost widget, because a child window can be embedded inside
    // a HWND which is not associated with a nsIWidget.
    return nsnull;
  }
  // If this widget has already been destroyed, pretend we have no parent.
  // This corresponds to code in Destroy which removes the destroyed
  // widget from its parent's child list.
  if (mIsDestroying || mOnDestroyCalled)
    return nsnull;

  nsWindow* widget = nsnull;
  if (mWnd) {
    HWND parent = ::GetParent(mWnd);
    if (parent) {
      widget = GetNSWindowPtr(parent);
      if (widget) {
        // If the widget is in the process of being destroyed then
        // do NOT return it
        if (widget->mIsDestroying) {
          widget = nsnull;
        }
      }
    }
  }

  return widget;
}


//-------------------------------------------------------------------------
//
// Hide or show this component
//
//-------------------------------------------------------------------------
#ifndef WINCE
PRBool gWindowsVisible;

static BOOL CALLBACK gEnumWindowsProc(HWND hwnd, LPARAM lParam)
{
  DWORD pid;
  ::GetWindowThreadProcessId(hwnd, &pid);
  if (pid == _getpid() && ::IsWindowVisible(hwnd))
  {
    gWindowsVisible = PR_TRUE;
    return FALSE;
  }
  return TRUE;
}
#endif

PRBool nsWindow::CanTakeFocus()
{
#ifndef WINCE
  gWindowsVisible = PR_FALSE;
  EnumWindows(gEnumWindowsProc, 0);
  if (!gWindowsVisible) {
    return PR_TRUE;
  } else {
    HWND fgWnd = ::GetForegroundWindow();
    if (!fgWnd) {
      return PR_TRUE;
    }
    DWORD pid;
    GetWindowThreadProcessId(fgWnd, &pid);
    if (pid == _getpid()) {
      return PR_TRUE;
    }
  }
  return PR_FALSE;
#else
  return PR_TRUE;
#endif
}

NS_METHOD nsWindow::Show(PRBool bState)
{
  if (mWnd) {
    if (bState) {
      if (!mIsVisible && mWindowType == eWindowType_toplevel) {
        switch (mSizeMode) {
          case nsSizeMode_Maximized :
            ::ShowWindow(mWnd, SW_SHOWMAXIMIZED);
            break;
          case nsSizeMode_Minimized :
#ifndef WINCE
            ::ShowWindow(mWnd, SW_SHOWMINIMIZED);
#endif
            break;
          default:
            if (CanTakeFocus()) {
              ::ShowWindow(mWnd, SW_SHOWNORMAL);
            } else {
              // Place the window behind the foreground window
              // (as long as it is not topmost)
              HWND wndAfter = ::GetForegroundWindow();
              if (!wndAfter)
                wndAfter = HWND_BOTTOM;
              else if (GetWindowLong(wndAfter, GWL_EXSTYLE) & WS_EX_TOPMOST)
                wndAfter = HWND_TOP;
              ::SetWindowPos(mWnd, wndAfter, 0, 0, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE | 
                             SWP_NOMOVE | SWP_NOACTIVATE);
              GetAttention(2);
            }
        }
      } else {
        DWORD flags = SWP_NOSIZE | SWP_NOMOVE | SWP_SHOWWINDOW;
        if (mIsVisible)
          flags |= SWP_NOZORDER;

        if (mWindowType == eWindowType_popup) {
#ifndef WINCE
          // ensure popups are the topmost of the TOPMOST
          // layer. Remember not to set the SWP_NOZORDER
          // flag as that might allow the taskbar to overlap
          // the popup.  However on windows ce, we need to
          // activate the popup or clicks will not be sent.
          flags |= SWP_NOACTIVATE;
#endif
          ::SetWindowPos(mWnd, HWND_TOPMOST, 0, 0, 0, 0, flags);
        } else {
          ::SetWindowPos(mWnd, HWND_TOP, 0, 0, 0, 0, flags);
        }
      }
    } else {
      if (mWindowType != eWindowType_dialog) {
        ::ShowWindow(mWnd, SW_HIDE);
      } else {
        ::SetWindowPos(mWnd, 0, 0, 0, 0, 0, SWP_HIDEWINDOW | SWP_NOSIZE | SWP_NOMOVE |
                       SWP_NOZORDER | SWP_NOACTIVATE);
      }
    }
  }
  
#ifdef MOZ_XUL
  if (!mIsVisible && bState && mIsTopTranslucent)
    Invalidate(PR_FALSE);
#endif

  mIsVisible = bState;

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Return PR_TRUE if the whether the component is visible, PR_FALSE otherwise
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::IsVisible(PRBool & bState)
{
  bState = mIsVisible;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Position the window behind the given window
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::PlaceBehind(nsTopLevelWidgetZPlacement aPlacement,
                                nsIWidget *aWidget, PRBool aActivate)
{
  HWND behind = HWND_TOP;
  if (aPlacement == eZPlacementBottom)
    behind = HWND_BOTTOM;
  else if (aPlacement == eZPlacementBelow && aWidget)
    behind = (HWND)aWidget->GetNativeData(NS_NATIVE_WINDOW);
  UINT flags = SWP_NOMOVE | SWP_NOREPOSITION | SWP_NOSIZE;
  if (!aActivate)
    flags |= SWP_NOACTIVATE;

  if (!CanTakeFocus() && behind == HWND_TOP)
  {
    // Can't place the window to top so place it behind the foreground window
    // (as long as it is not topmost)
    HWND wndAfter = ::GetForegroundWindow();
    if (!wndAfter)
      behind = HWND_BOTTOM;
    else if (!(GetWindowLong(wndAfter, GWL_EXSTYLE) & WS_EX_TOPMOST))
      behind = wndAfter;
    flags |= SWP_NOACTIVATE;
  }

  ::SetWindowPos(mWnd, behind, 0, 0, 0, 0, flags);
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Maximize, minimize or restore the window.
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::SetSizeMode(PRInt32 aMode) {

  nsresult rv;

  // Let's not try and do anything if we're already in that state.
  // (This is needed to prevent problems when calling window.minimize(), which
  // calls us directly, and then the OS triggers another call to us.)
  if (aMode == mSizeMode)
    return NS_OK;

  // save the requested state
  rv = nsBaseWidget::SetSizeMode(aMode);
  if (NS_SUCCEEDED(rv) && mIsVisible) {
    int mode;

    switch (aMode) {
      case nsSizeMode_Maximized :
        mode = SW_MAXIMIZE;
        break;
      case nsSizeMode_Minimized :
#ifndef WINCE
        mode = gTrimOnMinimize ? SW_MINIMIZE : SW_SHOWMINIMIZED;
        if (!gTrimOnMinimize) {
          // Find the next window that is visible and not minimized.
          HWND hwndBelow = ::GetNextWindow(mWnd, GW_HWNDNEXT);
          while (hwndBelow && (!::IsWindowVisible(hwndBelow) ||
                               ::IsIconic(hwndBelow))) {
            hwndBelow = ::GetNextWindow(hwndBelow, GW_HWNDNEXT);
          }

          // Push ourselves to the bottom of the stack, then activate the
          // next window.
          ::SetWindowPos(mWnd, HWND_BOTTOM, 0, 0, 0, 0,
                         SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
          if (hwndBelow)
            ::SetForegroundWindow(hwndBelow);
        }
#endif
        break;
      default :
        mode = SW_RESTORE;
    }
    ::ShowWindow(mWnd, mode);
  }
  return rv;
}

//-------------------------------------------------------------------------
// Return PR_TRUE in aForWindow if the given event should be processed
// assuming this is a modal window.
//-------------------------------------------------------------------------
NS_METHOD nsWindow::ModalEventFilter(PRBool aRealEvent, void *aEvent,
                                     PRBool *aForWindow)
{
  if (!aRealEvent) {
    *aForWindow = PR_FALSE;
    return NS_OK;
  }
#if 0
  // this version actually works, but turns out to be unnecessary
  // if we use the OS properly.
  MSG *msg = (MSG *) aEvent;

  switch (msg->message) {
    case WM_MOUSEMOVE:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_LBUTTONDBLCLK:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_MBUTTONDBLCLK:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_RBUTTONDBLCLK:
    {
      PRBool acceptEvent;

      // is the event within our window?
      HWND rollupWindow = NULL;
      HWND msgWindow = GetTopLevelHWND(msg->hwnd);
      if (gRollupWidget)
        rollupWindow = (HWND)gRollupWidget->GetNativeData(NS_NATIVE_WINDOW);
      acceptEvent = msgWindow && (msgWindow == mWnd || msgWindow == rollupWindow) ?
                    PR_TRUE : PR_FALSE;

      // if not, accept events for any window that hasn't been disabled.
      if (!acceptEvent) {
        LONG proc = ::GetWindowLongW(msgWindow, GWL_WNDPROC);
        if (proc == (LONG)&nsWindow::WindowProc) {
          nsWindow *msgWin = GetNSWindowPtr(msgWindow);
          msgWin->IsEnabled(&acceptEvent);
        }
      }
    }
    break;

    default:
      *aForWindow = PR_TRUE;
  }
#else
  *aForWindow = PR_TRUE;
#endif

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Constrain a potential move to fit onscreen
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::ConstrainPosition(PRBool aAllowSlop,
                                      PRInt32 *aX, PRInt32 *aY)
{
  if (!mIsTopWidgetWindow) // only a problem for top-level windows
    return NS_OK;

  PRBool doConstrain = PR_FALSE; // whether we have enough info to do anything

  /* get our playing field. use the current screen, or failing that
    for any reason, use device caps for the default screen. */
  RECT screenRect;

  nsCOMPtr<nsIScreenManager> screenmgr = do_GetService(sScreenManagerContractID);
  if (screenmgr) {
    nsCOMPtr<nsIScreen> screen;
    PRInt32 left, top, width, height;

    // zero size rects confuse the screen manager
    width = mBounds.width > 0 ? mBounds.width : 1;
    height = mBounds.height > 0 ? mBounds.height : 1;
    screenmgr->ScreenForRect(*aX, *aY, width, height,
                             getter_AddRefs(screen));
    if (screen) {
      screen->GetAvailRect(&left, &top, &width, &height);
      screenRect.left = left;
      screenRect.right = left+width;
      screenRect.top = top;
      screenRect.bottom = top+height;
      doConstrain = PR_TRUE;
    }
  } else {
    if (mWnd) {
      HDC dc = ::GetDC(mWnd);
      if (dc) {
        if (::GetDeviceCaps(dc, TECHNOLOGY) == DT_RASDISPLAY) {
          ::SystemParametersInfo(SPI_GETWORKAREA, 0, &screenRect, 0);
          doConstrain = PR_TRUE;
        }
        ::ReleaseDC(mWnd, dc);
      }
    }
  }

  if (aAllowSlop) {
    if (*aX < screenRect.left - mBounds.width + kWindowPositionSlop)
      *aX = screenRect.left - mBounds.width + kWindowPositionSlop;
    else if (*aX >= screenRect.right - kWindowPositionSlop)
      *aX = screenRect.right - kWindowPositionSlop;

    if (*aY < screenRect.top - mBounds.height + kWindowPositionSlop)
      *aY = screenRect.top - mBounds.height + kWindowPositionSlop;
    else if (*aY >= screenRect.bottom - kWindowPositionSlop)
      *aY = screenRect.bottom - kWindowPositionSlop;

  } else {

    if (*aX < screenRect.left)
      *aX = screenRect.left;
    else if (*aX >= screenRect.right - mBounds.width)
      *aX = screenRect.right - mBounds.width;

    if (*aY < screenRect.top)
      *aY = screenRect.top;
    else if (*aY >= screenRect.bottom - mBounds.height)
      *aY = screenRect.bottom - mBounds.height;
  }

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Move this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Move(PRInt32 aX, PRInt32 aY)
{
  // Check to see if window needs to be moved first
  // to avoid a costly call to SetWindowPos. This check
  // can not be moved to the calling code in nsView, because
  // some platforms do not position child windows correctly

  // Only perform this check for non-popup windows, since the positioning can
  // in fact change even when the x/y do not.  We always need to perform the
  // check. See bug #97805 for details.
  if (mWindowType != eWindowType_popup && (mBounds.x == aX) && (mBounds.y == aY))
  {
    // Nothing to do, since it is already positioned correctly.
    return NS_OK;
  }

  mBounds.x = aX;
  mBounds.y = aY;

  if (mWnd) {
#ifdef DEBUG
    // complain if a window is moved offscreen (legal, but potentially worrisome)
    if (mIsTopWidgetWindow) { // only a problem for top-level windows
      // Make sure this window is actually on the screen before we move it
      // XXX: Needs multiple monitor support
      HDC dc = ::GetDC(mWnd);
      if (dc) {
        if (::GetDeviceCaps(dc, TECHNOLOGY) == DT_RASDISPLAY) {
          RECT workArea;
          ::SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
          // no annoying assertions. just mention the issue.
          if (aX < 0 || aX >= workArea.right || aY < 0 || aY >= workArea.bottom)
            printf("window moved to offscreen position\n");
        }
      ::ReleaseDC(mWnd, dc);
      }
    }
#endif

    nsIWidget *par = GetParent();
    HDWP      deferrer = NULL;

    if (nsnull != par) {
      deferrer = ((nsWindow *)par)->mDeferredPositioner;
    }

    if (NULL != deferrer) {
      VERIFY(((nsWindow *)par)->mDeferredPositioner = ::DeferWindowPos(deferrer,
                            mWnd, NULL, aX, aY, 0, 0,
                            SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSIZE));
    }
    else {
      VERIFY(::SetWindowPos(mWnd, NULL, aX, aY, 0, 0,
                            SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSIZE));
    }
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Resize this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Resize(PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint)
{
  NS_ASSERTION((aWidth >=0 ) , "Negative width passed to nsWindow::Resize");
  NS_ASSERTION((aHeight >=0 ), "Negative height passed to nsWindow::Resize");

#ifdef MOZ_XUL
  if (mIsTranslucent)
    ResizeTranslucentWindow(aWidth, aHeight);
#endif

  // Set cached value for lightweight and printing
  mBounds.width  = aWidth;
  mBounds.height = aHeight;

  if (mWnd) {
    nsIWidget *par = GetParent();
    HDWP      deferrer = NULL;

    if (nsnull != par) {
      deferrer = ((nsWindow *)par)->mDeferredPositioner;
    }

    UINT  flags = SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE;
#ifndef WINCE
    if (!aRepaint) {
      flags |= SWP_NOREDRAW;
    }
#endif

    if (NULL != deferrer) {
      VERIFY(((nsWindow *)par)->mDeferredPositioner = ::DeferWindowPos(deferrer,
                            mWnd, NULL, 0, 0, aWidth, GetHeight(aHeight), flags));
    }
    else {
      VERIFY(::SetWindowPos(mWnd, NULL, 0, 0, aWidth, GetHeight(aHeight), flags));
    }
  }

  if (aRepaint)
    Invalidate(PR_FALSE);

  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Resize this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Resize(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint)
{
  NS_ASSERTION((aWidth >=0 ),  "Negative width passed to nsWindow::Resize");
  NS_ASSERTION((aHeight >=0 ), "Negative height passed to nsWindow::Resize");

#ifdef MOZ_XUL
  if (mIsTranslucent)
    ResizeTranslucentWindow(aWidth, aHeight);
#endif

  // Set cached value for lightweight and printing
  mBounds.x      = aX;
  mBounds.y      = aY;
  mBounds.width  = aWidth;
  mBounds.height = aHeight;

  if (mWnd) {
    nsIWidget *par = GetParent();
    HDWP      deferrer = NULL;

    if (nsnull != par) {
      deferrer = ((nsWindow *)par)->mDeferredPositioner;
    }

    UINT  flags = SWP_NOZORDER | SWP_NOACTIVATE;
#ifndef WINCE
    if (!aRepaint) {
      flags |= SWP_NOREDRAW;
    }
#endif
    if (NULL != deferrer) {
      VERIFY(((nsWindow *)par)->mDeferredPositioner = ::DeferWindowPos(deferrer,
                            mWnd, NULL, aX, aY, aWidth, GetHeight(aHeight), flags));
    }
    else {
      VERIFY(::SetWindowPos(mWnd, NULL, aX, aY, aWidth, GetHeight(aHeight), flags));
    }
  }

  if (aRepaint)
    Invalidate(PR_FALSE);

  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Enable/disable this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Enable(PRBool bState)
{
  if (mWnd) {
    ::EnableWindow(mWnd, bState);
  }
  return NS_OK;
}


NS_METHOD nsWindow::IsEnabled(PRBool *aState)
{
  NS_ENSURE_ARG_POINTER(aState);
  *aState = !mWnd || ::IsWindowEnabled(mWnd);
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Give the focus to this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::SetFocus(PRBool aRaise)
{
  //
  // Switch to the "main gui thread" if necessary... This method must
  // be executed on the "gui thread"...
  //
  nsToolkit* toolkit = (nsToolkit *)mToolkit;
  NS_ASSERTION(toolkit != nsnull, "This should never be null!"); // Bug 57044
  if (toolkit != nsnull && !toolkit->IsGuiThread()) {
    MethodInfo info(this, nsWindow::SET_FOCUS);
    toolkit->CallMethod(&info);
    return NS_ERROR_FAILURE;
  }

  if (mWnd) {
    // Uniconify, if necessary
    HWND toplevelWnd = GetTopLevelHWND(mWnd);
    if (::IsIconic(toplevelWnd))
      ::ShowWindow(toplevelWnd, SW_RESTORE);
    ::SetFocus(mWnd);

#ifdef WINCE
    MapHardwareButtons(mWnd);
#endif

  }
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Get this component dimension
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::GetBounds(nsRect &aRect)
{
  if (mWnd) {
    RECT r;
    VERIFY(::GetWindowRect(mWnd, &r));

    // assign size
    aRect.width  = r.right - r.left;
    aRect.height = r.bottom - r.top;

    // convert coordinates if parent exists
    HWND parent = ::GetParent(mWnd);
    if (parent) {
      RECT pr;
      VERIFY(::GetWindowRect(parent, &pr));
      r.left -= pr.left;
      r.top  -= pr.top;
    }
    aRect.x = r.left;
    aRect.y = r.top;
  } else {
    aRect = mBounds;
  }

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Get this component dimension
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::GetClientBounds(nsRect &aRect)
{
  if (mWnd) {
    RECT r;
    VERIFY(::GetClientRect(mWnd, &r));

    // assign size
    aRect.x = 0;
    aRect.y = 0;
    aRect.width  = r.right - r.left;
    aRect.height = r.bottom - r.top;

  } else {
    aRect.SetRect(0,0,0,0);
  }
  return NS_OK;
}

//get the bounds, but don't take into account the client size

void nsWindow::GetNonClientBounds(nsRect &aRect)
{
  if (mWnd) {
    RECT r;
    VERIFY(::GetWindowRect(mWnd, &r));

    // assign size
    aRect.width = r.right - r.left;
    aRect.height = r.bottom - r.top;

    // convert coordinates if parent exists
    HWND parent = ::GetParent(mWnd);
    if (parent) {
      RECT pr;
      VERIFY(::GetWindowRect(parent, &pr));
      r.left -= pr.left;
      r.top -= pr.top;
    }
    aRect.x = r.left;
    aRect.y = r.top;
  } else {
    aRect.SetRect(0,0,0,0);
  }
}

// like GetBounds, but don't offset by the parent
NS_METHOD nsWindow::GetScreenBounds(nsRect &aRect)
{
  if (mWnd) {
    RECT r;
    VERIFY(::GetWindowRect(mWnd, &r));

    aRect.width  = r.right - r.left;
    aRect.height = r.bottom - r.top;
    aRect.x = r.left;
    aRect.y = r.top;
  } else
    aRect = mBounds;

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Set the background color
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::SetBackgroundColor(const nscolor &aColor)
{
  nsBaseWidget::SetBackgroundColor(aColor);

  if (mBrush)
    ::DeleteObject(mBrush);

  mBrush = ::CreateSolidBrush(NSRGB_2_COLOREF(mBackground));
#ifndef WINCE
  if (mWnd != NULL) {
    SetClassLong(mWnd, GCL_HBRBACKGROUND, (LONG)mBrush);
  }
#endif
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Get this component font
//
//-------------------------------------------------------------------------
nsIFontMetrics* nsWindow::GetFont(void)
{
  NS_NOTYETIMPLEMENTED("GetFont not yet implemented"); // to be implemented
  return NULL;
}


//-------------------------------------------------------------------------
//
// Set this component font
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::SetFont(const nsFont &aFont)
{
  // Cache Font for owner draw
  if (mFont == nsnull) {
    mFont = new nsFont(aFont);
  } else {
    *mFont  = aFont;
  }

  // Bail out if there is no context
  if (nsnull == mContext) {
    return NS_ERROR_FAILURE;
  }

  nsIFontMetrics* metrics;
  mContext->GetMetricsFor(aFont, metrics);
  nsFontHandle  fontHandle;
  metrics->GetFontHandle(fontHandle);
  HFONT hfont = (HFONT)fontHandle;

  // Draw in the new font
  ::SendMessageW(mWnd, WM_SETFONT, (WPARAM)hfont, (LPARAM)0);
  NS_RELEASE(metrics);

  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Set this component cursor
//
//-------------------------------------------------------------------------

NS_METHOD nsWindow::SetCursor(nsCursor aCursor)
{
  // Only change cursor if it's changing

  //XXX mCursor isn't always right.  Scrollbars and others change it, too.
  //XXX If we want this optimization we need a better way to do it.
  //if (aCursor != mCursor) {
  HCURSOR newCursor = NULL;

  switch (aCursor) {
    case eCursor_select:
      newCursor = ::LoadCursor(NULL, IDC_IBEAM);
      break;

    case eCursor_wait:
      newCursor = ::LoadCursor(NULL, IDC_WAIT);
      break;

    case eCursor_hyperlink:
    {
      newCursor = ::LoadCursor(NULL, IDC_HAND);
      break;
    }

    case eCursor_standard:
      newCursor = ::LoadCursor(NULL, IDC_ARROW);
      break;

    case eCursor_n_resize:
    case eCursor_s_resize:
      newCursor = ::LoadCursor(NULL, IDC_SIZENS);
      break;

    case eCursor_w_resize:
    case eCursor_e_resize:
      newCursor = ::LoadCursor(NULL, IDC_SIZEWE);
      break;

    case eCursor_nw_resize:
    case eCursor_se_resize:
      newCursor = ::LoadCursor(NULL, IDC_SIZENWSE);
      break;

    case eCursor_ne_resize:
    case eCursor_sw_resize:
      newCursor = ::LoadCursor(NULL, IDC_SIZENESW);
      break;

    case eCursor_crosshair:
      newCursor = ::LoadCursor(NULL, IDC_CROSS);
      break;

    case eCursor_move:
      newCursor = ::LoadCursor(NULL, IDC_SIZEALL);
      break;

    case eCursor_help:
      newCursor = ::LoadCursor(NULL, IDC_HELP);
      break;

    case eCursor_copy: // CSS3
      newCursor = ::LoadCursor(nsToolkit::mDllInstance, MAKEINTRESOURCE(IDC_COPY));
      break;

    case eCursor_alias:
      newCursor = ::LoadCursor(nsToolkit::mDllInstance, MAKEINTRESOURCE(IDC_ALIAS));
      break;

    case eCursor_cell:
      newCursor = ::LoadCursor(nsToolkit::mDllInstance, MAKEINTRESOURCE(IDC_CELL));
      break;

    case eCursor_grab:
      newCursor = ::LoadCursor(nsToolkit::mDllInstance, MAKEINTRESOURCE(IDC_GRAB));
      break;

    case eCursor_grabbing:
      newCursor = ::LoadCursor(nsToolkit::mDllInstance, MAKEINTRESOURCE(IDC_GRABBING));
      break;

    case eCursor_spinning:
      newCursor = ::LoadCursor(NULL, IDC_APPSTARTING);
      break;

    case eCursor_context_menu:
      // XXX this CSS3 cursor needs to be implemented
      break;

    case eCursor_zoom_in:
      newCursor = ::LoadCursor(nsToolkit::mDllInstance, MAKEINTRESOURCE(IDC_ZOOMIN));
      break;

    case eCursor_zoom_out:
      newCursor = ::LoadCursor(nsToolkit::mDllInstance, MAKEINTRESOURCE(IDC_ZOOMOUT));
      break;

    case eCursor_not_allowed:
    case eCursor_no_drop:
      newCursor = ::LoadCursor(NULL, IDC_NO);
      break;

    case eCursor_col_resize:
      newCursor = ::LoadCursor(nsToolkit::mDllInstance, MAKEINTRESOURCE(IDC_COLRESIZE));
      break;

    case eCursor_row_resize:
      newCursor = ::LoadCursor(nsToolkit::mDllInstance, MAKEINTRESOURCE(IDC_ROWRESIZE));
      break;

    case eCursor_vertical_text:
      newCursor = ::LoadCursor(nsToolkit::mDllInstance, MAKEINTRESOURCE(IDC_VERTICALTEXT));
      break;

    case eCursor_all_scroll:
      // XXX not 100% appropriate perhaps
      newCursor = ::LoadCursor(NULL, IDC_SIZEALL);
      break;

    case eCursor_nesw_resize:
      newCursor = ::LoadCursor(NULL, IDC_SIZENESW);
      break;

    case eCursor_nwse_resize:
      newCursor = ::LoadCursor(NULL, IDC_SIZENWSE);
      break;

    case eCursor_ns_resize:
      newCursor = ::LoadCursor(NULL, IDC_SIZENS);
      break;

    case eCursor_ew_resize:
      newCursor = ::LoadCursor(NULL, IDC_SIZEWE);
      break;

    default:
      NS_ERROR("Invalid cursor type");
      break;
  }

  if (NULL != newCursor) {
    mCursor = aCursor;
    HCURSOR oldCursor = ::SetCursor(newCursor);
    
    if (gHCursor == oldCursor) {
      NS_IF_RELEASE(gCursorImgContainer);
      if (gHCursor != NULL)
        ::DestroyIcon(gHCursor);
      gHCursor = NULL;
    }
  }
  //}
  return NS_OK;
}

// static
PRUint8* nsWindow::Data8BitTo1Bit(PRUint8* aAlphaData,
                                  PRUint32 aAlphaBytesPerRow,
                                  PRUint32 aWidth, PRUint32 aHeight)
{
  // We need (aWidth + 7) / 8 bytes plus zero-padding up to a multiple of
  // 4 bytes for each row (HBITMAP requirement). Bug 353553.
  PRUint32 outBpr = ((aWidth + 31) / 8) & ~3;
  
  PRUint8* outData = new PRUint8[outBpr * aHeight];
  if (!outData)
    return NULL;

  PRUint8 *outRow = outData,
          *alphaRow = aAlphaData;

  for (PRUint32 curRow = 0; curRow < aHeight; curRow++) {
    PRUint8 *arow = alphaRow;
    PRUint8 *nextOutRow = outRow + outBpr;
    PRUint8 alphaPixels = 0;
    PRUint8 offset = 7;

    for (PRUint32 curCol = 0; curCol < aWidth; curCol++) {
      if (*alphaRow++ > 0)
        alphaPixels |= (1 << offset);
        
      if (offset == 0) {
        *outRow++ = alphaPixels;
        offset = 7;
        alphaPixels = 0;
      } else {
        offset--;
      }
    }
    if (offset != 7)
      *outRow++ = alphaPixels;

    alphaRow = arow + aAlphaBytesPerRow;
    while (outRow != nextOutRow)
      *outRow++ = 0; // padding
  }

  return outData;
}

// static
PRUint8* nsWindow::DataToAData(PRUint8* aImageData, PRUint32 aImageBytesPerRow,
                               PRUint8* aAlphaData, PRUint32 aAlphaBytesPerRow,
                               PRUint32 aWidth, PRUint32 aHeight)
{
  // We will have 32 bpp, so bytes per row will be 4 * w
  PRUint32 outBpr = aWidth * 4;

  // Avoid overflows
  if (aWidth > 0xfff || aHeight > 0xfff)
    return NULL;

  PRUint8* outData = new PRUint8[outBpr * aHeight];
  if (!outData)
    return NULL;

  PRUint8 *outRow = outData,
          *imageRow = aImageData,
          *alphaRow = aAlphaData;
  for (PRUint32 curRow = 0; curRow < aHeight; curRow++) {
    PRUint8 *irow = imageRow, *arow = alphaRow;
    for (PRUint32 curCol = 0; curCol < aWidth; curCol++) {
      *outRow++ = *imageRow++; // B
      *outRow++ = *imageRow++; // G
      *outRow++ = *imageRow++; // R
      *outRow++ = *alphaRow++; // A
    }
    imageRow = irow + aImageBytesPerRow;
    alphaRow = arow + aAlphaBytesPerRow;
  }
  return outData;
}

// static
HBITMAP nsWindow::DataToBitmap(PRUint8* aImageData,
                               PRUint32 aWidth,
                               PRUint32 aHeight,
                               PRUint32 aDepth)
{
  if (aDepth == 8 || aDepth == 4) {
    NS_WARNING("nsWindow::DataToBitmap can't handle 4 or 8 bit images");
    return NULL;
  }

  // dc must be a CreateCompatibleDC.
  // GetDC, cursors, 1 bit masks, and Win9x do not mix for some reason.
  HDC dc = ::CreateCompatibleDC(NULL);
  
  // force dc into color/bw mode
  int planes = ::GetDeviceCaps(dc, PLANES);
  int bpp = (aDepth == 1) ? 1 : ::GetDeviceCaps(dc, BITSPIXEL);

  HBITMAP tBitmap = ::CreateBitmap(1, 1, planes, bpp, NULL);
  HBITMAP oldbits = (HBITMAP)::SelectObject(dc, tBitmap);

#ifndef WINCE
  if (aDepth == 32 && IsCursorTranslucencySupported()) {
    // Alpha channel. We need the new header.
    BITMAPV4HEADER head = { 0 };
    head.bV4Size = sizeof(head);
    head.bV4Width = aWidth;
    head.bV4Height = aHeight;
    head.bV4Planes = 1;
    head.bV4BitCount = aDepth;
    head.bV4V4Compression = BI_BITFIELDS;
    head.bV4SizeImage = 0; // Uncompressed
    head.bV4XPelsPerMeter = 0;
    head.bV4YPelsPerMeter = 0;
    head.bV4ClrUsed = 0;
    head.bV4ClrImportant = 0;

    head.bV4RedMask   = 0x00FF0000;
    head.bV4GreenMask = 0x0000FF00;
    head.bV4BlueMask  = 0x000000FF;
    head.bV4AlphaMask = 0xFF000000;

    HBITMAP bmp = ::CreateDIBitmap(dc,
                                   NS_REINTERPRET_CAST(CONST BITMAPINFOHEADER*, &head),
                                   CBM_INIT,
                                   aImageData,
                                   NS_REINTERPRET_CAST(CONST BITMAPINFO*, &head),
                                   DIB_RGB_COLORS);

    ::SelectObject(dc, oldbits);
    ::DeleteObject(tBitmap);
    ::DeleteDC(dc);
    return bmp;
  }
#endif

  BITMAPINFOHEADER head = { 0 };

  head.biSize = sizeof(BITMAPINFOHEADER);
  head.biWidth = aWidth;
  head.biHeight = aHeight;
  head.biPlanes = 1;
  head.biBitCount = (WORD)aDepth;
  head.biCompression = BI_RGB;
  head.biSizeImage = 0; // Uncompressed
  head.biXPelsPerMeter = 0;
  head.biYPelsPerMeter = 0;
  head.biClrUsed = 0;
  head.biClrImportant = 0;
  
  char reserved_space[sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * 2];
  BITMAPINFO& bi = *(BITMAPINFO*)reserved_space;

  bi.bmiHeader = head;

  if (aDepth == 1) {
    RGBQUAD black = { 0, 0, 0, 0 };
    RGBQUAD white = { 255, 255, 255, 0 };

    bi.bmiColors[0] = white;
    bi.bmiColors[1] = black;
  }

  HBITMAP bmp = ::CreateDIBitmap(dc, &head, CBM_INIT, aImageData, &bi, DIB_RGB_COLORS);

  ::SelectObject(dc, oldbits);
  ::DeleteObject(tBitmap);
  ::DeleteDC(dc);
  return bmp;
}

// static
HBITMAP nsWindow::CreateOpaqueAlphaChannel(PRUint32 aWidth, PRUint32 aHeight)
{
  // Make up an opaque alpha channel.
  // We need (aWidth + 7) / 8 bytes plus zero-padding up to a multiple of
  // 4 bytes for each row (HBITMAP requirement). Bug 353553.
  PRUint32 nonPaddedBytesPerRow = (aWidth + 7) / 8;
  PRUint32 abpr = (nonPaddedBytesPerRow + 3) & ~3;
  PRUint32 bufferSize = abpr * aHeight;
  PRUint8* opaque = (PRUint8*)malloc(bufferSize);
  if (!opaque)
    return NULL;

  memset(opaque, 0xff, bufferSize);

  // If we have row padding, set it to zero.
  if (nonPaddedBytesPerRow != abpr) {
    PRUint8* p = opaque;
    PRUint8* end = opaque + bufferSize;
    while (p != end) {
      PRUint8* nextRow = p + abpr;
      p += nonPaddedBytesPerRow;
      while (p != nextRow)
        *p++ = 0; // padding
    }
  }

  HBITMAP hAlpha = DataToBitmap(opaque, aWidth, aHeight, 1);
  free(opaque);
  return hAlpha;
}

NS_IMETHODIMP nsWindow::SetCursor(imgIContainer* aCursor,
                                  PRUint32 aHotspotX, PRUint32 aHotspotY)
{
  if (gCursorImgContainer == aCursor && gHCursor) {
    ::SetCursor(gHCursor);
    return NS_OK;
  }

  // Get the image data
  nsCOMPtr<gfxIImageFrame> frame;
  aCursor->GetFrameAt(0, getter_AddRefs(frame));
  if (!frame)
    return NS_ERROR_NOT_AVAILABLE;

  PRInt32 width, height;
  frame->GetWidth(&width);
  frame->GetHeight(&height);

  // Reject cursors greater than 128 pixels in some direction, to prevent
  // spoofing.
  // XXX ideally we should rescale. Also, we could modify the API to
  // allow trusted content to set larger cursors.
  if (width > 128 || height > 128)
    return NS_ERROR_NOT_AVAILABLE;

#ifdef MOZ_CAIRO_GFX
  PRUint32 bpr;
  gfx_format format;
  frame->GetImageBytesPerRow(&bpr);
  frame->GetFormat(&format);

  frame->LockImageData();

  PRUint32 dataLen;
  PRUint8 *data;
  nsresult rv = frame->GetImageData(&data, &dataLen);
  if (NS_FAILED(rv)) {
    frame->UnlockImageData();
    return rv;
  }

  /* flip the image so that it is stored bottom-up */
  PRUint8 *bottomUpData = (PRUint8*)malloc(dataLen);
  if (!bottomUpData) {
    frame->UnlockImageData();
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (format == gfxIFormats::RGB_A8 || format == gfxIFormats::BGR_A8) {
    for (PRInt32 i = 0; i < height; ++i) {
      PRUint32 srcOffset = i * bpr;
      PRUint32 dstOffset = dataLen - (bpr * (i + 1));
      PRUint32 *srcRow = (PRUint32*)(data + srcOffset);
      PRUint32 *dstRow = (PRUint32*)(bottomUpData + dstOffset);
      memcpy(dstRow, srcRow, bpr);
    }
  } else {
    for (PRInt32 i = 0; i < height; ++i) {
      PRUint32 srcOffset = i * bpr;
      PRUint32 dstOffset = dataLen - (bpr * (i + 1));
      PRUint32 *srcRow = (PRUint32*)(data + srcOffset);
      PRUint32 *dstRow = (PRUint32*)(bottomUpData + dstOffset);
      for (PRInt32 x = 0; x < width; ++x) {
        dstRow[x] = (srcRow[x] & 0xFFFFFF) | (0xFF << 24);
      }
    }
  }
  HBITMAP bmp = DataToBitmap(bottomUpData, width, height, 32);

  free(bottomUpData);

  frame->UnlockImageData();

  ICONINFO info = {0};
  info.fIcon = FALSE;
  info.xHotspot = aHotspotX;
  info.yHotspot = aHotspotY;
  info.hbmMask = bmp;
  info.hbmColor = bmp;
  
  HCURSOR cursor = ::CreateIconIndirect(&info);
  ::DeleteObject(bmp);
  if (cursor == NULL) {
    return NS_ERROR_FAILURE;
  }

  mCursor = nsCursor(-1);
  ::SetCursor(cursor);

  NS_IF_RELEASE(gCursorImgContainer);
  gCursorImgContainer = aCursor;
  NS_ADDREF(gCursorImgContainer);

  if (gHCursor != NULL)
    ::DestroyIcon(gHCursor);
  gHCursor = cursor;

#else

  gfx_format format;
  nsresult rv = frame->GetFormat(&format);
  if (NS_FAILED(rv))
    return rv;

  if (format != gfxIFormats::BGR_A1 && format != gfxIFormats::BGR_A8 &&
      format != gfxIFormats::BGR)
    return NS_ERROR_UNEXPECTED;

  // On Win2k with nVidia video drivers 71.84 at 32 bit color, cursors that 
  // have 8 bit alpha are truncated to 64x64.  Skip cursors larger than that.
  // This is redundant with checks above, but we'll leave it in as a reminder
  // in case we start accepting larger cursors again
  if (IsWin2k() && (format == gfxIFormats::BGR_A8) &&
      (width > 64 || height > 64))
    return NS_ERROR_FAILURE;

  PRUint32 bpr;
  rv = frame->GetImageBytesPerRow(&bpr);
  if (NS_FAILED(rv))
    return rv;

  frame->LockImageData();
  PRUint32 dataLen;
  PRUint8* data;
  rv = frame->GetImageData(&data, &dataLen);
  if (NS_FAILED(rv)) {
    frame->UnlockImageData();
    return rv;
  }

  HBITMAP hBMP = NULL;
  if (format != gfxIFormats::BGR_A8) {
    hBMP = DataToBitmap(data, width, height, 24);
    if (hBMP == NULL) {
      frame->UnlockImageData();
      return NS_ERROR_FAILURE;
    }
  }

  HBITMAP hAlpha = NULL;
  if (format == gfxIFormats::BGR) {
    hAlpha = CreateOpaqueAlphaChannel(width, height);
  } else {
    PRUint32 abpr;
    rv = frame->GetAlphaBytesPerRow(&abpr);
    if (NS_FAILED(rv)) {
      frame->UnlockImageData();
      if (hBMP != NULL)
        ::DeleteObject(hBMP);
      return rv;
    }

    PRUint8* adata;
    frame->LockAlphaData();
    rv = frame->GetAlphaData(&adata, &dataLen);
    if (NS_FAILED(rv)) {
      if (hBMP != NULL)
        ::DeleteObject(hBMP);
      frame->UnlockImageData();
      frame->UnlockAlphaData();
      return rv;
    }

    if (format == gfxIFormats::BGR_A8) {
      // Convert BGR_A8 to BGRA.  
      // Some platforms (or video cards?) on 32bit color mode will ignore
      // hAlpha. For them, we could speed up things by creating an opaque alpha
      // channel, but since we don't know how to determine whether hAlpha is
      // ignored, create a proper 1 bit alpha channel to supplement the RGBA.
      // Plus, on non-32bit color and possibly other platforms, the alpha
      // of RGBA is ignored.
      PRUint8* bgra8data = DataToAData(data, bpr, adata, abpr, width, height);
      if (bgra8data) {
        hBMP = DataToBitmap(bgra8data, width, height, 32);
        if (hBMP != NULL) {
          PRUint8* a1data = Data8BitTo1Bit(adata, abpr, width, height);
          if (a1data) {
            hAlpha = DataToBitmap(a1data, width, height, 1);
            delete [] a1data;
          }
        }
        delete [] bgra8data;
      }
    } else {
      hAlpha = DataToBitmap(adata, width, height, 1);
    }

    frame->UnlockAlphaData();
  }
  frame->UnlockImageData();
  if (hBMP == NULL) {
    return NS_ERROR_FAILURE;
  }
  if (hAlpha == NULL) {
    ::DeleteObject(hBMP);
    return NS_ERROR_FAILURE;
  }

  ICONINFO info = {0};
  info.fIcon = FALSE;
  info.xHotspot = aHotspotX;
  info.yHotspot = aHotspotY;
  info.hbmMask = hAlpha;
  info.hbmColor = hBMP;
  
  HCURSOR cursor = ::CreateIconIndirect(&info);
  ::DeleteObject(hBMP);
  ::DeleteObject(hAlpha);
  if (cursor == NULL) {
    return NS_ERROR_FAILURE;
  }

  mCursor = nsCursor(-1);
  ::SetCursor(cursor);

  NS_IF_RELEASE(gCursorImgContainer);
  gCursorImgContainer = aCursor;
  NS_ADDREF(gCursorImgContainer);

  if (gHCursor != NULL)
    ::DestroyIcon(gHCursor);
  gHCursor = cursor;

#endif

  return NS_OK;
}

NS_IMETHODIMP nsWindow::HideWindowChrome(PRBool aShouldHide)
{
  HWND hwnd = GetTopLevelHWND(mWnd, PR_TRUE);
  if (!GetNSWindowPtr(hwnd))
  {
    NS_WARNING("Trying to hide window decorations in an embedded context");
    return NS_ERROR_FAILURE;
  }

  DWORD style, exStyle;
  if (aShouldHide) {
    DWORD tempStyle = ::GetWindowLongW(hwnd, GWL_STYLE);
    DWORD tempExStyle = ::GetWindowLongW(hwnd, GWL_EXSTYLE);

    style = tempStyle & ~(WS_CAPTION | WS_THICKFRAME);
    exStyle = tempExStyle & ~(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE |
                              WS_EX_CLIENTEDGE | WS_EX_STATICEDGE);

    mOldStyle = tempStyle;
    mOldExStyle = tempExStyle;
  }
  else {
    if (!mOldStyle || !mOldExStyle) {
      mOldStyle = ::GetWindowLongW(hwnd, GWL_STYLE);
      mOldExStyle = ::GetWindowLongW(hwnd, GWL_EXSTYLE);
    }

    style = mOldStyle;
    exStyle = mOldExStyle;
  }

  ::SetWindowLongW(hwnd, GWL_STYLE, style);
  ::SetWindowLongW(hwnd, GWL_EXSTYLE, exStyle);

  return NS_OK;
}

// ------------------------------------------------------------------------
//
// Validate a visible area of a widget.
//
// ------------------------------------------------------------------------

NS_METHOD nsWindow::Validate()
{
  if (mWnd)
    VERIFY(::ValidateRect(mWnd, NULL));
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Invalidate this component visible area
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Invalidate(PRBool aIsSynchronous)
{
  if (mWnd)
  {
#ifdef NS_DEBUG
    debug_DumpInvalidate(stdout,
                         this,
                         nsnull,
                         aIsSynchronous,
                         nsCAutoString("noname"),
                         (PRInt32) mWnd);
#endif // NS_DEBUG

    VERIFY(::InvalidateRect(mWnd, NULL, FALSE));

    if (aIsSynchronous) {
      VERIFY(::UpdateWindow(mWnd));
    }
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Invalidate this component visible area
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Invalidate(const nsRect & aRect, PRBool aIsSynchronous)
{
  if (mWnd)
  {
#ifdef NS_DEBUG
    debug_DumpInvalidate(stdout,
                         this,
                         &aRect,
                         aIsSynchronous,
                         nsCAutoString("noname"),
                         (PRInt32) mWnd);
#endif // NS_DEBUG

    RECT rect;

    rect.left   = aRect.x;
    rect.top    = aRect.y;
    rect.right  = aRect.x + aRect.width;
    rect.bottom = aRect.y + aRect.height;

    VERIFY(::InvalidateRect(mWnd, &rect, FALSE));

    if (aIsSynchronous) {
      VERIFY(::UpdateWindow(mWnd));
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWindow::InvalidateRegion(const nsIRegion *aRegion, PRBool aIsSynchronous)
{
  nsresult rv = NS_OK;
  if (mWnd) {
    HRGN nativeRegion;
    rv = aRegion->GetNativeRegion((void *&)nativeRegion);
    if (nativeRegion) {
      if (NS_SUCCEEDED(rv)) {
        VERIFY(::InvalidateRgn(mWnd, nativeRegion, FALSE));

        if (aIsSynchronous) {
          VERIFY(::UpdateWindow(mWnd));
        }
      }
    } else {
      rv = NS_ERROR_FAILURE;
    }
  }
  return rv;
}

//-------------------------------------------------------------------------
//
// Force a synchronous repaint of the window
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::Update()
{
  nsresult rv = NS_OK;

  // updates can come through for windows no longer holding an mWnd during
  // deletes triggered by JavaScript in buttons with mouse feedback
  if (mWnd)
    VERIFY(::UpdateWindow(mWnd));

  return rv;
}

//-------------------------------------------------------------------------
//
// Return some native data according to aDataType
//
//-------------------------------------------------------------------------
void* nsWindow::GetNativeData(PRUint32 aDataType)
{
  switch (aDataType) {
    case NS_NATIVE_WIDGET:
    case NS_NATIVE_WINDOW:
    case NS_NATIVE_PLUGIN_PORT:
      return (void*)mWnd;
    case NS_NATIVE_GRAPHIC:
      // XXX:  This is sleezy!!  Remember to Release the DC after using it!
#ifdef MOZ_XUL
      return (void*)(mIsTranslucent) ?
        mMemoryDC : ::GetDC(mWnd);
#else
      return (void*)::GetDC(mWnd);
#endif
    case NS_NATIVE_COLORMAP:
    default:
      break;
  }

  return NULL;
}

//~~~
void nsWindow::FreeNativeData(void * data, PRUint32 aDataType)
{
  switch (aDataType)
  {
    case NS_NATIVE_GRAPHIC:
#ifdef MOZ_XUL
      if (!mIsTranslucent)
        ::ReleaseDC(mWnd, (HDC)data);
#else
      ::ReleaseDC(mWnd, (HDC)data);
#endif
      break;
    case NS_NATIVE_WIDGET:
    case NS_NATIVE_WINDOW:
    case NS_NATIVE_PLUGIN_PORT:
    case NS_NATIVE_COLORMAP:
      break;
    default:
      break;
  }
}

//-------------------------------------------------------------------------
//
// Set the colormap of the window
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::SetColorMap(nsColorMap *aColorMap)
{
#if 0
  if (mPalette != NULL) {
    ::DeleteObject(mPalette);
  }

  PRUint8 *map = aColorMap->Index;
  LPLOGPALETTE pLogPal = (LPLOGPALETTE) new char[2 * sizeof(WORD) +
                                                 aColorMap->NumColors * sizeof(PALETTEENTRY)];
  pLogPal->palVersion = 0x300;
  pLogPal->palNumEntries = aColorMap->NumColors;
  for(int i = 0; i < aColorMap->NumColors; i++)
  {
    pLogPal->palPalEntry[i].peRed = *map++;
    pLogPal->palPalEntry[i].peGreen = *map++;
    pLogPal->palPalEntry[i].peBlue = *map++;
    pLogPal->palPalEntry[i].peFlags = 0;
  }
  mPalette = ::CreatePalette(pLogPal);
  delete pLogPal;

  NS_ASSERTION(mPalette != NULL, "Null palette");
  if (mPalette != NULL) {
    HDC hDC = ::GetDC(mWnd);
    HPALETTE hOldPalette = ::SelectPalette(hDC, mPalette, TRUE);
    ::RealizePalette(hDC);
    ::SelectPalette(hDC, hOldPalette, TRUE);
    ::ReleaseDC(mWnd, hDC);
  }
#endif
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Scroll the bits of a window
//
//-------------------------------------------------------------------------
//XXX Scroll is obsolete and should go away soon
NS_METHOD nsWindow::Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect)
{
  RECT  trect;

  if (nsnull != aClipRect)
  {
    trect.left = aClipRect->x;
    trect.top = aClipRect->y;
    trect.right = aClipRect->XMost();
    trect.bottom = aClipRect->YMost();
  }

  ::ScrollWindowEx(mWnd, aDx, aDy, (nsnull != aClipRect) ? &trect : NULL, NULL,
                   NULL, NULL, SW_INVALIDATE | SW_SCROLLCHILDREN);
  ::UpdateWindow(mWnd);
  return NS_OK;
}

NS_IMETHODIMP nsWindow::ScrollWidgets(PRInt32 aDx, PRInt32 aDy)
{
  // Scroll the entire contents of the window + change the offset of any child windows
  ::ScrollWindowEx(mWnd, aDx, aDy, NULL, NULL, NULL,
                   NULL, SW_INVALIDATE | SW_SCROLLCHILDREN);
  ::UpdateWindow(mWnd); // Force synchronous generation of NS_PAINT
  return NS_OK;
}

NS_IMETHODIMP nsWindow::ScrollRect(nsRect &aRect, PRInt32 aDx, PRInt32 aDy)
{
  RECT  trect;

  trect.left = aRect.x;
  trect.top = aRect.y;
  trect.right = aRect.XMost();
  trect.bottom = aRect.YMost();

  // Scroll the bits in the window defined by trect.
  // Child windows are not scrolled.
  ::ScrollWindowEx(mWnd, aDx, aDy, &trect, NULL, NULL,
                   NULL, SW_INVALIDATE);
  ::UpdateWindow(mWnd); // Force synchronous generation of NS_PAINT
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Every function that needs a thread switch goes through this function
// by calling SendMessage (..WM_CALLMETHOD..) in nsToolkit::CallMethod.
//
//-------------------------------------------------------------------------
BOOL nsWindow::CallMethod(MethodInfo *info)
{
  BOOL bRet = TRUE;

  switch (info->methodId) {
    case nsWindow::CREATE:
      NS_ASSERTION(info->nArgs == 7, "Wrong number of arguments to CallMethod");
      Create((nsIWidget*)(info->args[0]),
             (nsRect&)*(nsRect*)(info->args[1]),
             (EVENT_CALLBACK)(info->args[2]),
             (nsIDeviceContext*)(info->args[3]),
             (nsIAppShell *)(info->args[4]),
             (nsIToolkit*)(info->args[5]),
             (nsWidgetInitData*)(info->args[6]));
      break;

    case nsWindow::CREATE_NATIVE:
      NS_ASSERTION(info->nArgs == 7, "Wrong number of arguments to CallMethod");
      Create((nsNativeWidget)(info->args[0]),
             (nsRect&)*(nsRect*)(info->args[1]),
             (EVENT_CALLBACK)(info->args[2]),
             (nsIDeviceContext*)(info->args[3]),
             (nsIAppShell *)(info->args[4]),
             (nsIToolkit*)(info->args[5]),
             (nsWidgetInitData*)(info->args[6]));
      return TRUE;

    case nsWindow::DESTROY:
      NS_ASSERTION(info->nArgs == 0, "Wrong number of arguments to CallMethod");
      Destroy();
      break;

    case nsWindow::SET_FOCUS:
      NS_ASSERTION(info->nArgs == 0, "Wrong number of arguments to CallMethod");
      SetFocus(PR_FALSE);
      break;

    default:
      bRet = FALSE;
      break;
  }

  return bRet;
}

//-------------------------------------------------------------------------
void nsWindow::SetUpForPaint(HDC aHDC)
{
  ::SetBkColor (aHDC, NSRGB_2_COLOREF(mBackground));
  ::SetTextColor(aHDC, NSRGB_2_COLOREF(mForeground));
  ::SetBkMode (aHDC, TRANSPARENT);
}

//---------------------------------------------------------
NS_METHOD nsWindow::EnableDragDrop(PRBool aEnable)
{
  nsresult rv = NS_ERROR_FAILURE;
#ifndef WINCE
  if (aEnable) {
    if (nsnull == mNativeDragTarget) {
       mNativeDragTarget = new nsNativeDragTarget(this);
       if (NULL != mNativeDragTarget) {
         mNativeDragTarget->AddRef();
         if (S_OK == ::CoLockObjectExternal((LPUNKNOWN)mNativeDragTarget,TRUE,FALSE)) {
           if (S_OK == ::RegisterDragDrop(mWnd, (LPDROPTARGET)mNativeDragTarget)) {
             rv = NS_OK;
           }
         }
       }
    }
  } else {
    if (nsnull != mWnd && NULL != mNativeDragTarget) {
      ::RevokeDragDrop(mWnd);
      if (S_OK == ::CoLockObjectExternal((LPUNKNOWN)mNativeDragTarget, FALSE, TRUE)) {
        rv = NS_OK;
      }
      NS_RELEASE(mNativeDragTarget);
    }
  }
#endif
  return rv;
}

//-------------------------------------------------------------------------
UINT nsWindow::MapFromNativeToDOM(UINT aNativeKeyCode)
{
  switch (aNativeKeyCode) {
    case VK_OEM_1:     return NS_VK_SEMICOLON;     // 0xBA, For the US standard keyboard, the ';:' key
    case VK_OEM_PLUS:  return NS_VK_ADD;           // 0xBB, For any country/region, the '+' key
    case VK_OEM_MINUS: return NS_VK_SUBTRACT;      // 0xBD, For any country/region, the '-' key
  }

  return aNativeKeyCode;
}

//-------------------------------------------------------------------------
//
// OnKey
//
//-------------------------------------------------------------------------
PRBool nsWindow::DispatchKeyEvent(PRUint32 aEventType, WORD aCharCode, UINT aVirtualCharCode, 
                                  LPARAM aKeyData, PRUint32 aFlags)
{
  nsKeyEvent event(PR_TRUE, aEventType, this);
  nsPoint point(0, 0);

  InitEvent(event, &point); // this add ref's event.widget

  event.flags |= aFlags;
  event.charCode = aCharCode;
  event.keyCode  = aVirtualCharCode;

#ifdef KE_DEBUG
  static cnt=0;
  printf("%d DispatchKE Type: %s charCode %d  keyCode %d ", cnt++,
        (NS_KEY_PRESS == aEventType) ? "PRESS" : (aEventType == NS_KEY_UP ? "Up" : "Down"),
         event.charCode, event.keyCode);
  printf("Shift: %s Control %s Alt: %s \n", 
         (mIsShiftDown ? "D" : "U"), (mIsControlDown ? "D" : "U"), (mIsAltDown ? "D" : "U"));
  printf("[%c][%c][%c] <==   [%c][%c][%c][ space bar ][%c][%c][%c]\n",
         IS_VK_DOWN(NS_VK_SHIFT) ? 'S' : ' ',
         IS_VK_DOWN(NS_VK_CONTROL) ? 'C' : ' ',
         IS_VK_DOWN(NS_VK_ALT) ? 'A' : ' ',
         IS_VK_DOWN(VK_LSHIFT) ? 'S' : ' ',
         IS_VK_DOWN(VK_LCONTROL) ? 'C' : ' ',
         IS_VK_DOWN(VK_LMENU) ? 'A' : ' ',
         IS_VK_DOWN(VK_RMENU) ? 'A' : ' ',
         IS_VK_DOWN(VK_RCONTROL) ? 'C' : ' ',
         IS_VK_DOWN(VK_RSHIFT) ? 'S' : ' ');
#endif

  event.isShift   = mIsShiftDown;
  event.isControl = mIsControlDown;
  event.isMeta    = PR_FALSE;
  event.isAlt     = mIsAltDown;

  nsPluginEvent pluginEvent;

  switch (aEventType)
  {
    case NS_KEY_UP:
      pluginEvent.event = WM_KEYUP;
      break;
    case NS_KEY_DOWN:
      pluginEvent.event = WM_KEYDOWN;
      break;
    default:
      break;
  }

  pluginEvent.wParam = aVirtualCharCode;
  pluginEvent.lParam = aKeyData;

  event.nativeMsg = (void *)&pluginEvent;

  PRBool result = DispatchWindowEvent(&event);
  NS_RELEASE(event.widget);

  return result;
}



//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
BOOL nsWindow::OnKeyDown(UINT aVirtualKeyCode, UINT aScanCode, LPARAM aKeyData)
{
#ifdef VK_BROWSER_BACK
  // VK_BROWSER_BACK and VK_BROWSER_FORWARD are converted to nsCommandEvents
  if (aVirtualKeyCode == VK_BROWSER_BACK) 
  {
    DispatchCommandEvent(APPCOMMAND_BROWSER_BACKWARD);
    return TRUE;
  } 
  else if (aVirtualKeyCode == VK_BROWSER_FORWARD) 
  {
    DispatchCommandEvent(APPCOMMAND_BROWSER_FORWARD);
    return TRUE;
  }
#endif

  gKbdLayout.OnKeyDown (aVirtualKeyCode);

  // Use only DOMKeyCode for XP processing.
  // Use aVirtualKeyCode for gKbdLayout and native processing.
  UINT DOMKeyCode = sIMEIsComposing ?
                      aVirtualKeyCode : MapFromNativeToDOM(aVirtualKeyCode);

#ifdef DEBUG
  //printf("In OnKeyDown virt: %d  scan: %d\n", DOMKeyCode, aScanCode);
#endif

  BOOL noDefault = DispatchKeyEvent(NS_KEY_DOWN, 0, DOMKeyCode, aKeyData);

  // If we won't be getting a WM_CHAR, WM_SYSCHAR or WM_DEADCHAR, synthesize a keypress
  // for almost all keys
  switch (DOMKeyCode) {
    case NS_VK_SHIFT:
    case NS_VK_CONTROL:
    case NS_VK_ALT:
    case NS_VK_CAPS_LOCK:
    case NS_VK_NUM_LOCK:
    case NS_VK_SCROLL_LOCK: return noDefault;
  }

  PRUint32 extraFlags = (noDefault ? NS_EVENT_FLAG_NO_DEFAULT : 0);
  MSG msg;
  BOOL gotMsg = ::PeekMessageW(&msg, mWnd, WM_KEYFIRST, WM_KEYLAST, PM_NOREMOVE | PM_NOYIELD);
  PRBool anyCharMessagesRemoved = PR_FALSE;
  // Enter and backspace are always handled here to avoid for example the
  // confusion between ctrl-enter and ctrl-J.
  if (DOMKeyCode == NS_VK_RETURN || DOMKeyCode == NS_VK_BACK ||
      ((mIsControlDown || mIsAltDown) &&
       KeyboardLayout::IsPrintableCharKey(aVirtualKeyCode)))
  {
    // Remove a possible WM_CHAR or WM_SYSCHAR messages from the message queue.
    // They can be more than one because of:
    //  * Dead-keys not pairing with base character
    //  * Some keyboard layouts may map up to 4 characters to the single key


    while (gotMsg && (msg.message == WM_CHAR || msg.message == WM_SYSCHAR))
    {
      ::GetMessageW(&msg, mWnd, WM_KEYFIRST, WM_KEYLAST);
      anyCharMessagesRemoved = PR_TRUE;

      gotMsg = ::PeekMessageW (&msg, mWnd, WM_KEYFIRST, WM_KEYLAST, PM_NOREMOVE | PM_NOYIELD);
    }

    if (!anyCharMessagesRemoved && DOMKeyCode == NS_VK_BACK) {
      MSG imeStartCompositionMsg, imeCompositionMsg;
      if (::PeekMessageW(&imeStartCompositionMsg, mWnd, WM_IME_STARTCOMPOSITION, WM_IME_STARTCOMPOSITION, PM_NOREMOVE | PM_NOYIELD)
       && ::PeekMessageW(&imeCompositionMsg, mWnd, WM_IME_COMPOSITION, WM_IME_COMPOSITION, PM_NOREMOVE | PM_NOYIELD)
       && ::PeekMessageW(&msg, mWnd, WM_CHAR, WM_CHAR, PM_NOREMOVE | PM_NOYIELD)
       && imeStartCompositionMsg.wParam == 0x0 && imeStartCompositionMsg.lParam == 0x0
       && imeCompositionMsg.wParam == 0x0 && imeCompositionMsg.lParam == 0x1BF
       && msg.wParam == NS_VK_BACK && msg.lParam == 0x1
       && imeStartCompositionMsg.time <= imeCompositionMsg.time
       && imeCompositionMsg.time <= msg.time) {
        // This message pattern is "Kakutei-Undo" on ATOK and WXG.
        // (ATOK and WXG are popular IMEs in Japan)
        // In this case, the message queue has following messages:
        // ------------------------------------------------------------------------------------------
        // WM_KEYDOWN              * n (wParam = VK_BACK, lParam = 0x1)
        // WM_KEYUP                * 1 (wParam = VK_BACK, lParam = 0xC0000001) #this is ATOK only
        // WM_IME_STARTCOMPOSITION * 1 (wParam = 0x0, lParam = 0x0)
        // WM_IME_COMPOSITION      * 1 (wParam = 0x0, lParam = 0x1BF)
        // WM_CHAR                 * n (wParam = VK_BACK, lParam = 0x1)
        // WM_KEYUP                * 1 (wParam = VK_BACK, lParam = 0xC00E0001)
        // ------------------------------------------------------------------------------------------
        // This message pattern does not match to the above case;
        // i.e.,WM_KEYDOWN -> WM_CHAR -> WM_KEYDOWN -> WM_CHAR.
        // For more information of this problem:
        // http://bugzilla.mozilla.gr.jp/show_bug.cgi?id=2885 (written in Japanese)
        // http://bugzilla.mozilla.org/show_bug.cgi?id=194559 (written in English)

        ::GetMessageW(&msg, mWnd, WM_CHAR, WM_CHAR);
      }
    }
  }
  else if (gotMsg &&
           (msg.message == WM_CHAR || msg.message == WM_SYSCHAR || msg.message == WM_DEADCHAR)) {
    // If prevent default set for keydown, do same for keypress
    ::GetMessageW(&msg, mWnd, msg.message, msg.message);

    if (msg.message == WM_DEADCHAR)
      return PR_FALSE;

#ifdef KE_DEBUG
    printf("%s\tchar=%c\twp=%4x\tlp=%8x\n",
           (msg.message == WM_SYSCHAR) ? "WM_SYSCHAR" : "WM_CHAR",
           msg.wParam, msg.wParam, msg.lParam);
#endif
    return OnChar(msg.wParam, msg.lParam, extraFlags);
  } else if (!mIsControlDown && !mIsAltDown &&
             (KeyboardLayout::IsPrintableCharKey(aVirtualKeyCode) ||
              KeyboardLayout::IsNumpadKey(aVirtualKeyCode)))
  {
    // If this is simple KeyDown event but next message is not WM_CHAR,
    // this event may not input text, so we should ignore this event.
    // See bug 314130.
    return PR_FALSE;
  }

  if (gKbdLayout.IsDeadKey ())
    return PR_FALSE;

  PRUint8 shiftStates [5];
  PRUint16 uniChars [5];
  PRUint32 numOfUniChars = 0;
  PRUint32 numOfShiftStates = 0;

  switch (aVirtualKeyCode) {
    // keys to be sent as characters
    case VK_ADD:       uniChars [0] = '+';  numOfUniChars = 1;  break;
    case VK_SUBTRACT:  uniChars [0] = '-';  numOfUniChars = 1;  break;
    case VK_DIVIDE:    uniChars [0] = '/';  numOfUniChars = 1;  break;
    case VK_MULTIPLY:  uniChars [0] = '*';  numOfUniChars = 1;  break;
    case VK_NUMPAD0:
    case VK_NUMPAD1:
    case VK_NUMPAD2:
    case VK_NUMPAD3:
    case VK_NUMPAD4:
    case VK_NUMPAD5:
    case VK_NUMPAD6:
    case VK_NUMPAD7:
    case VK_NUMPAD8:
    case VK_NUMPAD9:
      uniChars [0] = aVirtualKeyCode - VK_NUMPAD0 + '0';
      numOfUniChars = 1;
      break;
    default:
      if (KeyboardLayout::IsPrintableCharKey (aVirtualKeyCode))
        numOfUniChars = numOfShiftStates = gKbdLayout.GetUniChars (uniChars, shiftStates, NS_ARRAY_LENGTH (uniChars));

      if (mIsControlDown ^ mIsAltDown)
      {
        // XXX
        // For both Alt+key and Ctrl+key combinations we return the latin characters A..Z and
        // numbers 0..9, ignoring the real characters returned by active keyboard layout.
        // This is required to make sure that all shortcut keys (e.g. Ctrl+c, Ctrl+1, Alt+f)
        // work the same way no matter what keyboard layout you are using.

        if ((NS_VK_0 <= DOMKeyCode && DOMKeyCode <= NS_VK_9) ||
            (NS_VK_A <= DOMKeyCode && DOMKeyCode <= NS_VK_Z))
        {
          uniChars [0] = DOMKeyCode;
          numOfUniChars = 1;
          numOfShiftStates = 0;

          // For letters take the Shift state into account
          if (!mIsShiftDown &&
              NS_VK_A <= DOMKeyCode && DOMKeyCode <= NS_VK_Z)
            uniChars [0] += 0x20;
        }
        else if (!anyCharMessagesRemoved && DOMKeyCode != aVirtualKeyCode) {
          switch (DOMKeyCode) {
            case NS_VK_ADD:
              uniChars [0] = '+'; numOfUniChars = 1; break;
            case NS_VK_SUBTRACT:
              uniChars [0] = '-'; numOfUniChars = 1; break;
            case NS_VK_SEMICOLON:
              // XXXmnakano I don't know whether this is correct.
              uniChars [0] = ';';
              uniChars [1] = ':';
              numOfUniChars = 2;
              break;
            default:
              NS_ERROR("implement me!");
          }
        }
      }
  }

  if (numOfUniChars)
  {
    for (PRUint32 cnt = 0; cnt < numOfUniChars; cnt++)
    {
      if (cnt < numOfShiftStates)
      {
        // If key in combination with Alt and/or Ctrl produces a different character than without them
        // then do not report these flags because it is separate keyboard layout shift state.
        // If dead-key and base character does not produce a valid composite character then both produced
        // dead-key character and following base character may have different modifier flags, too.
        mIsShiftDown   = (shiftStates [cnt] & eShift) != 0;
        mIsControlDown = (shiftStates [cnt] & eCtrl) != 0;
        mIsAltDown     = (shiftStates [cnt] & eAlt) != 0;
      }

      DispatchKeyEvent(NS_KEY_PRESS, uniChars [cnt], 0, aKeyData, extraFlags);
    }
  } else
    DispatchKeyEvent(NS_KEY_PRESS, 0, DOMKeyCode, aKeyData, extraFlags);

  return noDefault;
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
BOOL nsWindow::OnKeyUp( UINT aVirtualKeyCode, UINT aScanCode, LPARAM aKeyData)
{
#ifdef VK_BROWSER_BACK
  if (aVirtualKeyCode == VK_BROWSER_BACK || aVirtualKeyCode == VK_BROWSER_FORWARD) 
    return TRUE;
#endif

  aVirtualKeyCode = sIMEIsComposing ? aVirtualKeyCode : MapFromNativeToDOM(aVirtualKeyCode);
  BOOL result = DispatchKeyEvent(NS_KEY_UP, 0, aVirtualKeyCode, aKeyData);
  return result;
}


//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
BOOL nsWindow::OnChar(UINT charCode, LPARAM keyData, PRUint32 aFlags)
{
  // These must be checked here too as a lone WM_CHAR could be received
  // if a child window didn't handle it (for example Alt+Space in a content window)
  mIsShiftDown   = IS_VK_DOWN(NS_VK_SHIFT);
  mIsControlDown = IS_VK_DOWN(NS_VK_CONTROL);
  mIsAltDown     = IS_VK_DOWN(NS_VK_ALT);

  // ignore [shift+]alt+space so the OS can handle it
  if (mIsAltDown && !mIsControlDown && IS_VK_DOWN(NS_VK_SPACE)) {
    return FALSE;
  }

  // WM_CHAR with Control and Alt (== AltGr) down really means a normal character
  PRBool saveIsAltDown = mIsAltDown;
  PRBool saveIsControlDown = mIsControlDown;
  if (mIsAltDown && mIsControlDown)
    mIsAltDown = mIsControlDown = PR_FALSE;

  wchar_t uniChar;

  if (sIMEIsComposing) {
    HandleEndComposition();
  }

  if (mIsControlDown && charCode <= 0x1A) { // Ctrl+A Ctrl+Z, see Programming Windows 3.1 page 110 for details
    // need to account for shift here.  bug 16486
    if (mIsShiftDown)
      uniChar = charCode - 1 + 'A';
    else
      uniChar = charCode - 1 + 'a';
    charCode = 0;
  }
  else if (mIsControlDown && charCode <= 0x1F) {
    // Fix for 50255 - <ctrl><[> and <ctrl><]> are not being processed.
    // also fixes ctrl+\ (x1c), ctrl+^ (x1e) and ctrl+_ (x1f)
    // for some reason the keypress handler need to have the uniChar code set
    // with the addition of a upper case A not the lower case.
    uniChar = charCode - 1 + 'A';
    charCode = 0;
  } else { // 0x20 - SPACE, 0x3D - EQUALS
    if (charCode < 0x20 || (charCode == 0x3D && mIsControlDown)) {
      uniChar = 0;
    } else {
      uniChar = charCode;
      charCode = 0;
    }
  }

  // Keep the characters unshifted for shortcuts and accesskeys and make sure
  // that numbers are always passed as such (among others: bugs 50255 and 351310)
  if (uniChar && (mIsControlDown || mIsAltDown)) {
    UINT virtualKeyCode = ::MapVirtualKey(HIWORD(keyData) & 0xFF, MAPVK_VSC_TO_VK);
    UINT unshiftedCharCode =
      virtualKeyCode >= '0' && virtualKeyCode <= '9' ? virtualKeyCode :
      mIsShiftDown ? ::MapVirtualKey(virtualKeyCode, MAPVK_VK_TO_CHAR) : 0;
    // ignore diacritics (top bit set) and key mapping errors (char code 0)
    if ((INT)unshiftedCharCode > 0)
      uniChar = unshiftedCharCode;
  }

  // Fix for bug 285161 (and 295095) which was caused by the initial fix for bug 178110.
  // When pressing (alt|ctrl)+char, the char must be lowercase unless shift is 
  // pressed too.
  if (!mIsShiftDown && (saveIsAltDown || saveIsControlDown)) {
    uniChar = towlower(uniChar);
  }

  PRBool result = DispatchKeyEvent(NS_KEY_PRESS, uniChar, charCode, 0, aFlags);
  mIsAltDown = saveIsAltDown;
  mIsControlDown = saveIsControlDown;
  return result;
}


void nsWindow::ConstrainZLevel(HWND *aAfter)
{
  nsZLevelEvent  event(PR_TRUE, NS_SETZLEVEL, this);
  nsWindow      *aboveWindow = 0;

  InitEvent(event);

  if (*aAfter == HWND_BOTTOM)
    event.mPlacement = nsWindowZBottom;
  else if (*aAfter == HWND_TOP || *aAfter == HWND_TOPMOST || *aAfter == HWND_NOTOPMOST)
    event.mPlacement = nsWindowZTop;
  else {
    event.mPlacement = nsWindowZRelative;
    aboveWindow = GetNSWindowPtr(*aAfter);
  }
  event.mReqBelow = aboveWindow;
  event.mActualBelow = nsnull;

  event.mImmediate = PR_FALSE;
  event.mAdjusted = PR_FALSE;
  DispatchWindowEvent(&event);

  if (event.mAdjusted) {
    if (event.mPlacement == nsWindowZBottom)
      *aAfter = HWND_BOTTOM;
    else if (event.mPlacement == nsWindowZTop)
      *aAfter = HWND_TOP;
    else {
      *aAfter = (HWND)event.mActualBelow->GetNativeData(NS_NATIVE_WINDOW);
    }
  }
  NS_IF_RELEASE(event.mActualBelow);
  NS_RELEASE(event.widget);
}

//-------------------------------------------------------------------------
//
// Process all nsWindows messages
//
//-------------------------------------------------------------------------
static PRBool gJustGotDeactivate = PR_FALSE;
static PRBool gJustGotActivate = PR_FALSE;

#ifdef NS_DEBUG

typedef struct {
  char * mStr;
  long   mId;
} EventMsgInfo;

EventMsgInfo gAllEvents[] = {
  {"WM_NULL",                   0x0000},
  {"WM_CREATE",                 0x0001},
  {"WM_DESTROY",                0x0002},
  {"WM_MOVE",                   0x0003},
  {"WM_SIZE",                   0x0005},
  {"WM_ACTIVATE",               0x0006},
  {"WM_SETFOCUS",               0x0007},
  {"WM_KILLFOCUS",              0x0008},
  {"WM_ENABLE",                 0x000A},
  {"WM_SETREDRAW",              0x000B},
  {"WM_SETTEXT",                0x000C},
  {"WM_GETTEXT",                0x000D},
  {"WM_GETTEXTLENGTH",          0x000E},
  {"WM_PAINT",                  0x000F},
  {"WM_CLOSE",                  0x0010},
  {"WM_QUERYENDSESSION",        0x0011},
  {"WM_QUIT",                   0x0012},
  {"WM_QUERYOPEN",              0x0013},
  {"WM_ERASEBKGND",             0x0014},
  {"WM_SYSCOLORCHANGE",         0x0015},
  {"WM_ENDSESSION",             0x0016},
  {"WM_SHOWWINDOW",             0x0018},
  {"WM_SETTINGCHANGE",          0x001A},
  {"WM_DEVMODECHANGE",          0x001B},
  {"WM_ACTIVATEAPP",            0x001C},
  {"WM_FONTCHANGE",             0x001D},
  {"WM_TIMECHANGE",             0x001E},
  {"WM_CANCELMODE",             0x001F},
  {"WM_SETCURSOR",              0x0020},
  {"WM_MOUSEACTIVATE",          0x0021},
  {"WM_CHILDACTIVATE",          0x0022},
  {"WM_QUEUESYNC",              0x0023},
  {"WM_GETMINMAXINFO",          0x0024},
  {"WM_PAINTICON",              0x0026},
  {"WM_ICONERASEBKGND",         0x0027},
  {"WM_NEXTDLGCTL",             0x0028},
  {"WM_SPOOLERSTATUS",          0x002A},
  {"WM_DRAWITEM",               0x002B},
  {"WM_MEASUREITEM",            0x002C},
  {"WM_DELETEITEM",             0x002D},
  {"WM_VKEYTOITEM",             0x002E},
  {"WM_CHARTOITEM",             0x002F},
  {"WM_SETFONT",                0x0030},
  {"WM_GETFONT",                0x0031},
  {"WM_SETHOTKEY",              0x0032},
  {"WM_GETHOTKEY",              0x0033},
  {"WM_QUERYDRAGICON",          0x0037},
  {"WM_COMPAREITEM",            0x0039},
  {"WM_GETOBJECT",              0x003D},
  {"WM_COMPACTING",             0x0041},
  {"WM_COMMNOTIFY",             0x0044},
  {"WM_WINDOWPOSCHANGING",      0x0046},
  {"WM_WINDOWPOSCHANGED",       0x0047},
  {"WM_POWER",                  0x0048},
  {"WM_COPYDATA",               0x004A},
  {"WM_CANCELJOURNAL",          0x004B},
  {"WM_NOTIFY",                 0x004E},
  {"WM_INPUTLANGCHANGEREQUEST", 0x0050},
  {"WM_INPUTLANGCHANGE",        0x0051},
  {"WM_TCARD",                  0x0052},
  {"WM_HELP",                   0x0053},
  {"WM_USERCHANGED",            0x0054},
  {"WM_NOTIFYFORMAT",           0x0055},
  {"WM_CONTEXTMENU",            0x007B},
  {"WM_STYLECHANGING",          0x007C},
  {"WM_STYLECHANGED",           0x007D},
  {"WM_DISPLAYCHANGE",          0x007E},
  {"WM_GETICON",                0x007F},
  {"WM_SETICON",                0x0080},
  {"WM_NCCREATE",               0x0081},
  {"WM_NCDESTROY",              0x0082},
  {"WM_NCCALCSIZE",             0x0083},
  {"WM_NCHITTEST",              0x0084},
  {"WM_NCPAINT",                0x0085},
  {"WM_NCACTIVATE",             0x0086},
  {"WM_GETDLGCODE",             0x0087},
  {"WM_SYNCPAINT",              0x0088},
  {"WM_NCMOUSEMOVE",            0x00A0},
  {"WM_NCLBUTTONDOWN",          0x00A1},
  {"WM_NCLBUTTONUP",            0x00A2},
  {"WM_NCLBUTTONDBLCLK",        0x00A3},
  {"WM_NCRBUTTONDOWN",          0x00A4},
  {"WM_NCRBUTTONUP",            0x00A5},
  {"WM_NCRBUTTONDBLCLK",        0x00A6},
  {"WM_NCMBUTTONDOWN",          0x00A7},
  {"WM_NCMBUTTONUP",            0x00A8},
  {"WM_NCMBUTTONDBLCLK",        0x00A9},
  {"EM_GETSEL",                 0x00B0},
  {"EM_SETSEL",                 0x00B1},
  {"EM_GETRECT",                0x00B2},
  {"EM_SETRECT",                0x00B3},
  {"EM_SETRECTNP",              0x00B4},
  {"EM_SCROLL",                 0x00B5},
  {"EM_LINESCROLL",             0x00B6},
  {"EM_SCROLLCARET",            0x00B7},
  {"EM_GETMODIFY",              0x00B8},
  {"EM_SETMODIFY",              0x00B9},
  {"EM_GETLINECOUNT",           0x00BA},
  {"EM_LINEINDEX",              0x00BB},
  {"EM_SETHANDLE",              0x00BC},
  {"EM_GETHANDLE",              0x00BD},
  {"EM_GETTHUMB",               0x00BE},
  {"EM_LINELENGTH",             0x00C1},
  {"EM_REPLACESEL",             0x00C2},
  {"EM_GETLINE",                0x00C4},
  {"EM_LIMITTEXT",              0x00C5},
  {"EM_CANUNDO",                0x00C6},
  {"EM_UNDO",                   0x00C7},
  {"EM_FMTLINES",               0x00C8},
  {"EM_LINEFROMCHAR",           0x00C9},
  {"EM_SETTABSTOPS",            0x00CB},
  {"EM_SETPASSWORDCHAR",        0x00CC},
  {"EM_EMPTYUNDOBUFFER",        0x00CD},
  {"EM_GETFIRSTVISIBLELINE",    0x00CE},
  {"EM_SETREADONLY",            0x00CF},
  {"EM_SETWORDBREAKPROC",       0x00D0},
  {"EM_GETWORDBREAKPROC",       0x00D1},
  {"EM_GETPASSWORDCHAR",        0x00D2},
  {"EM_SETMARGINS",             0x00D3},
  {"EM_GETMARGINS",             0x00D4},
  {"EM_GETLIMITTEXT",           0x00D5},
  {"EM_POSFROMCHAR",            0x00D6},
  {"EM_CHARFROMPOS",            0x00D7},
  {"EM_SETIMESTATUS",           0x00D8},
  {"EM_GETIMESTATUS",           0x00D9},
  {"SBM_SETPOS",                0x00E0},
  {"SBM_GETPOS",                0x00E1},
  {"SBM_SETRANGE",              0x00E2},
  {"SBM_SETRANGEREDRAW",        0x00E6},
  {"SBM_GETRANGE",              0x00E3},
  {"SBM_ENABLE_ARROWS",         0x00E4},
  {"SBM_SETSCROLLINFO",         0x00E9},
  {"SBM_GETSCROLLINFO",         0x00EA},
  {"WM_KEYDOWN",                0x0100},
  {"WM_KEYUP",                  0x0101},
  {"WM_CHAR",                   0x0102},
  {"WM_DEADCHAR",               0x0103},
  {"WM_SYSKEYDOWN",             0x0104},
  {"WM_SYSKEYUP",               0x0105},
  {"WM_SYSCHAR",                0x0106},
  {"WM_SYSDEADCHAR",            0x0107},
  {"WM_KEYLAST",                0x0108},
  {"WM_IME_STARTCOMPOSITION",   0x010D},
  {"WM_IME_ENDCOMPOSITION",     0x010E},
  {"WM_IME_COMPOSITION",        0x010F},
  {"WM_INITDIALOG",             0x0110},
  {"WM_COMMAND",                0x0111},
  {"WM_SYSCOMMAND",             0x0112},
  {"WM_TIMER",                  0x0113},
  {"WM_HSCROLL",                0x0114},
  {"WM_VSCROLL",                0x0115},
  {"WM_INITMENU",               0x0116},
  {"WM_INITMENUPOPUP",          0x0117},
  {"WM_MENUSELECT",             0x011F},
  {"WM_MENUCHAR",               0x0120},
  {"WM_ENTERIDLE",              0x0121},
  {"WM_MENURBUTTONUP",          0x0122},
  {"WM_MENUDRAG",               0x0123},
  {"WM_MENUGETOBJECT",          0x0124},
  {"WM_UNINITMENUPOPUP",        0x0125},
  {"WM_MENUCOMMAND",            0x0126},
  {"WM_CTLCOLORMSGBOX",         0x0132},
  {"WM_CTLCOLOREDIT",           0x0133},
  {"WM_CTLCOLORLISTBOX",        0x0134},
  {"WM_CTLCOLORBTN",            0x0135},
  {"WM_CTLCOLORDLG",            0x0136},
  {"WM_CTLCOLORSCROLLBAR",      0x0137},
  {"WM_CTLCOLORSTATIC",         0x0138},
  {"CB_GETEDITSEL",             0x0140},
  {"CB_LIMITTEXT",              0x0141},
  {"CB_SETEDITSEL",             0x0142},
  {"CB_ADDSTRING",              0x0143},
  {"CB_DELETESTRING",           0x0144},
  {"CB_DIR",                    0x0145},
  {"CB_GETCOUNT",               0x0146},
  {"CB_GETCURSEL",              0x0147},
  {"CB_GETLBTEXT",              0x0148},
  {"CB_GETLBTEXTLEN",           0x0149},
  {"CB_INSERTSTRING",           0x014A},
  {"CB_RESETCONTENT",           0x014B},
  {"CB_FINDSTRING",             0x014C},
  {"CB_SELECTSTRING",           0x014D},
  {"CB_SETCURSEL",              0x014E},
  {"CB_SHOWDROPDOWN",           0x014F},
  {"CB_GETITEMDATA",            0x0150},
  {"CB_SETITEMDATA",            0x0151},
  {"CB_GETDROPPEDCONTROLRECT",  0x0152},
  {"CB_SETITEMHEIGHT",          0x0153},
  {"CB_GETITEMHEIGHT",          0x0154},
  {"CB_SETEXTENDEDUI",          0x0155},
  {"CB_GETEXTENDEDUI",          0x0156},
  {"CB_GETDROPPEDSTATE",        0x0157},
  {"CB_FINDSTRINGEXACT",        0x0158},
  {"CB_SETLOCALE",              0x0159},
  {"CB_GETLOCALE",              0x015A},
  {"CB_GETTOPINDEX",            0x015b},
  {"CB_SETTOPINDEX",            0x015c},
  {"CB_GETHORIZONTALEXTENT",    0x015d},
  {"CB_SETHORIZONTALEXTENT",    0x015e},
  {"CB_GETDROPPEDWIDTH",        0x015f},
  {"CB_SETDROPPEDWIDTH",        0x0160},
  {"CB_INITSTORAGE",            0x0161},
  {"CB_MSGMAX",                 0x0162},
  {"LB_ADDSTRING",              0x0180},
  {"LB_INSERTSTRING",           0x0181},
  {"LB_DELETESTRING",           0x0182},
  {"LB_SELITEMRANGEEX",         0x0183},
  {"LB_RESETCONTENT",           0x0184},
  {"LB_SETSEL",                 0x0185},
  {"LB_SETCURSEL",              0x0186},
  {"LB_GETSEL",                 0x0187},
  {"LB_GETCURSEL",              0x0188},
  {"LB_GETTEXT",                0x0189},
  {"LB_GETTEXTLEN",             0x018A},
  {"LB_GETCOUNT",               0x018B},
  {"LB_SELECTSTRING",           0x018C},
  {"LB_DIR",                    0x018D},
  {"LB_GETTOPINDEX",            0x018E},
  {"LB_FINDSTRING",             0x018F},
  {"LB_GETSELCOUNT",            0x0190},
  {"LB_GETSELITEMS",            0x0191},
  {"LB_SETTABSTOPS",            0x0192},
  {"LB_GETHORIZONTALEXTENT",    0x0193},
  {"LB_SETHORIZONTALEXTENT",    0x0194},
  {"LB_SETCOLUMNWIDTH",         0x0195},
  {"LB_ADDFILE",                0x0196},
  {"LB_SETTOPINDEX",            0x0197},
  {"LB_GETITEMRECT",            0x0198},
  {"LB_GETITEMDATA",            0x0199},
  {"LB_SETITEMDATA",            0x019A},
  {"LB_SELITEMRANGE",           0x019B},
  {"LB_SETANCHORINDEX",         0x019C},
  {"LB_GETANCHORINDEX",         0x019D},
  {"LB_SETCARETINDEX",          0x019E},
  {"LB_GETCARETINDEX",          0x019F},
  {"LB_SETITEMHEIGHT",          0x01A0},
  {"LB_GETITEMHEIGHT",          0x01A1},
  {"LB_FINDSTRINGEXACT",        0x01A2},
  {"LB_SETLOCALE",              0x01A5},
  {"LB_GETLOCALE",              0x01A6},
  {"LB_SETCOUNT",               0x01A7},
  {"LB_INITSTORAGE",            0x01A8},
  {"LB_ITEMFROMPOINT",          0x01A9},
  {"LB_MSGMAX",                 0x01B0},
  {"WM_MOUSEMOVE",              0x0200},
  {"WM_LBUTTONDOWN",            0x0201},
  {"WM_LBUTTONUP",              0x0202},
  {"WM_LBUTTONDBLCLK",          0x0203},
  {"WM_RBUTTONDOWN",            0x0204},
  {"WM_RBUTTONUP",              0x0205},
  {"WM_RBUTTONDBLCLK",          0x0206},
  {"WM_MBUTTONDOWN",            0x0207},
  {"WM_MBUTTONUP",              0x0208},
  {"WM_MBUTTONDBLCLK",          0x0209},
  {"WM_MOUSEWHEEL",             0x020A},
  {"WM_MOUSEHWHEEL",            0x020E},
  {"WM_PARENTNOTIFY",           0x0210},
  {"WM_ENTERMENULOOP",          0x0211},
  {"WM_EXITMENULOOP",           0x0212},
  {"WM_NEXTMENU",               0x0213},
  {"WM_SIZING",                 0x0214},
  {"WM_CAPTURECHANGED",         0x0215},
  {"WM_MOVING",                 0x0216},
  {"WM_POWERBROADCAST",         0x0218},
  {"WM_DEVICECHANGE",           0x0219},
  {"WM_MDICREATE",              0x0220},
  {"WM_MDIDESTROY",             0x0221},
  {"WM_MDIACTIVATE",            0x0222},
  {"WM_MDIRESTORE",             0x0223},
  {"WM_MDINEXT",                0x0224},
  {"WM_MDIMAXIMIZE",            0x0225},
  {"WM_MDITILE",                0x0226},
  {"WM_MDICASCADE",             0x0227},
  {"WM_MDIICONARRANGE",         0x0228},
  {"WM_MDIGETACTIVE",           0x0229},
  {"WM_MDISETMENU",             0x0230},
  {"WM_ENTERSIZEMOVE",          0x0231},
  {"WM_EXITSIZEMOVE",           0x0232},
  {"WM_DROPFILES",              0x0233},
  {"WM_MDIREFRESHMENU",         0x0234},
  {"WM_IME_SETCONTEXT",         0x0281},
  {"WM_IME_NOTIFY",             0x0282},
  {"WM_IME_CONTROL",            0x0283},
  {"WM_IME_COMPOSITIONFULL",    0x0284},
  {"WM_IME_SELECT",             0x0285},
  {"WM_IME_CHAR",               0x0286},
  {"WM_IME_REQUEST",            0x0288},
  {"WM_IME_KEYDOWN",            0x0290},
  {"WM_IME_KEYUP",              0x0291},
  {"WM_NCMOUSEHOVER",           0x02A0},
  {"WM_MOUSEHOVER",             0x02A1},
  {"WM_MOUSELEAVE",             0x02A3},
  {"WM_CUT",                    0x0300},
  {"WM_COPY",                   0x0301},
  {"WM_PASTE",                  0x0302},
  {"WM_CLEAR",                  0x0303},
  {"WM_UNDO",                   0x0304},
  {"WM_RENDERFORMAT",           0x0305},
  {"WM_RENDERALLFORMATS",       0x0306},
  {"WM_DESTROYCLIPBOARD",       0x0307},
  {"WM_DRAWCLIPBOARD",          0x0308},
  {"WM_PAINTCLIPBOARD",         0x0309},
  {"WM_VSCROLLCLIPBOARD",       0x030A},
  {"WM_SIZECLIPBOARD",          0x030B},
  {"WM_ASKCBFORMATNAME",        0x030C},
  {"WM_CHANGECBCHAIN",          0x030D},
  {"WM_HSCROLLCLIPBOARD",       0x030E},
  {"WM_QUERYNEWPALETTE",        0x030F},
  {"WM_PALETTEISCHANGING",      0x0310},
  {"WM_PALETTECHANGED",         0x0311},
  {"WM_HOTKEY",                 0x0312},
  {"WM_PRINT",                  0x0317},
  {"WM_PRINTCLIENT",            0x0318},
  {"WM_THEMECHANGED",           0x031A},
  {"WM_HANDHELDFIRST",          0x0358},
  {"WM_HANDHELDLAST",           0x035F},
  {"WM_AFXFIRST",               0x0360},
  {"WM_AFXLAST",                0x037F},
  {"WM_PENWINFIRST",            0x0380},
  {"WM_PENWINLAST",             0x038F},
  {"WM_APP",                    0x8000},
  {NULL, 0x0}
};


static long gEventCounter = 0;
static long gLastEventMsg = 0;

void PrintEvent(UINT msg, PRBool aShowAllEvents, PRBool aShowMouseMoves)
{
  int inx = 0;
  while (gAllEvents[inx].mId != (long)msg && gAllEvents[inx].mStr != NULL) {
    inx++;
  }
  if (aShowAllEvents || (!aShowAllEvents && gLastEventMsg != (long)msg)) {
    if (aShowMouseMoves || (!aShowMouseMoves && msg != 0x0020 && msg != 0x0200 && msg != 0x0084)) {
      printf("%6d - 0x%04X %s\n", gEventCounter++, msg, gAllEvents[inx].mStr ? gAllEvents[inx].mStr : "Unknown");
      gLastEventMsg = msg;
    }
  }
}

#endif

#define WM_XP_THEMECHANGED                 0x031A

// Static helper functions for heap dumping
static nsresult HeapDump(const char *filename, const char *heading)
{
#ifdef WINCE
  return NS_ERROR_NOT_IMPLEMENTED;
#else

  PRFileDesc *prfd = PR_Open(filename, PR_CREATE_FILE | PR_APPEND | PR_WRONLY, 0777);
  if (!prfd)
    return NS_ERROR_FAILURE;

  char buf[1024];
  PRUint32 n;
  PRUint32 written = 0;
  HANDLE heapHandle[64];
  DWORD nheap = GetProcessHeaps(64, heapHandle);
  if (nheap == 0 || nheap > 64) {
    return NS_ERROR_FAILURE;
  }

  n = PR_snprintf(buf, sizeof buf, "BEGIN HEAPDUMP : %s\n", heading);
  PR_Write(prfd, buf, n);
  for (DWORD i = 0; i < nheap; i++) {
    // Dump each heap
    PROCESS_HEAP_ENTRY ent = {0};
    n = PR_snprintf(buf, sizeof buf, "BEGIN heap %d : 0x%p\n", i+1, heapHandle[i]);
    PR_Write(prfd, buf, n);
    ent.lpData = NULL;
    while (HeapWalk(heapHandle[i], &ent)) {
      if (ent.wFlags & PROCESS_HEAP_REGION)
        n = PR_snprintf(buf, sizeof buf, "REGION %08p : overhead %d committed %d uncommitted %d firstblock %08p lastblock %08p\n",
                        ent.lpData, ent.cbOverhead,
                        ent.Region.dwCommittedSize, ent.Region.dwUnCommittedSize,
                        ent.Region.lpFirstBlock, ent.Region.lpLastBlock);
      else
        n = PR_snprintf(buf, sizeof buf, "%s %08p : %6d overhead %2d\n",
                        (ent.wFlags & PROCESS_HEAP_UNCOMMITTED_RANGE) ? "----" : ((ent.wFlags & PROCESS_HEAP_ENTRY_BUSY) ? "USED" : "FREE"),
                        ent.lpData, ent.cbData, ent.cbOverhead);
      PR_Write(prfd, buf, n);
    }
    n = PR_snprintf(buf, sizeof buf, "END heap %d : 0x%p\n", i+1, heapHandle[i]);
    PR_Write(prfd, buf, n);
  }
  n = PR_snprintf(buf, sizeof buf, "END HEAPDUMP : %s\n", heading);
  PR_Write(prfd, buf, n);

  PR_Close(prfd);
  return NS_OK;
#endif // WINCE
}

// Recursively dispatch synchronous paints for nsIWidget
// descendants with invalidated rectangles.

BOOL CALLBACK nsWindow::DispatchStarvedPaints(HWND aWnd, LPARAM aMsg)
{
  LONG proc = ::GetWindowLongW(aWnd, GWL_WNDPROC);
  if (proc == (LONG)&nsWindow::WindowProc) {
    // its one of our windows so check to see if it has a
    // invalidated rect. If it does. Dispatch a synchronous
    // paint.
    if (GetUpdateRect(aWnd, NULL, FALSE)) {
      VERIFY(::UpdateWindow(aWnd));
    }
  }
  return TRUE;
}

// Check for pending paints and dispatch any pending paint
// messages for any nsIWidget which is a descendant of the
// top-level window that *this* window is embedded within.
// 
// Note: We do not dispatch pending paint messages for non
// nsIWidget managed windows.

void nsWindow::DispatchPendingEvents()
{
  gLastInputEventTime = PR_IntervalToMicroseconds(PR_IntervalNow());

  // We need to ensure that reflow events do not get starved.
  // At the same time, we don't want to recurse through here
  // as that would prevent us from dispatching starved paints.
  static int recursionBlocker = 0;
  if (recursionBlocker++ == 0) {
    NS_ProcessPendingEvents(nsnull, PR_MillisecondsToInterval(100));
    --recursionBlocker;
  }

  // Quickly check to see if there are any
  // paint events pending.
  if (::GetQueueStatus(QS_PAINT)) {
    // Find the top level window.
    HWND topWnd = GetTopLevelHWND(mWnd);

    // Dispatch pending paints for all topWnd's descendant windows.
    // Note: EnumChildWindows enumerates all descendant windows not just
    // it's children.
    ::EnumChildWindows(topWnd, nsWindow::DispatchStarvedPaints, NULL);
  }
}

#ifndef WINCE
void nsWindow::PostSleepWakeNotification(const char* aNotification)
{
  nsCOMPtr<nsIObserverService> observerService = do_GetService("@mozilla.org/observer-service;1");
  if (observerService)
  {
    observerService->NotifyObservers(nsnull, aNotification, nsnull);
  }
}
#endif

PRBool nsWindow::ProcessMessage(UINT msg, WPARAM wParam, LPARAM lParam, LRESULT *aRetValue)
{
  static UINT vkKeyCached = 0;              // caches VK code fon WM_KEYDOWN
  PRBool result = PR_FALSE;                 // call the default nsWindow proc
  static PRBool getWheelInfo = PR_TRUE;
  *aRetValue = 0;
  PRBool isMozWindowTakingFocus = PR_TRUE;
  nsPaletteInfo palInfo;

  // Uncomment this to see all windows messages
  // first param shows all events
  // second param indicates whether to show mouse move events
  //PrintEvent(msg, PR_FALSE, PR_FALSE);

  switch (msg) {
    case WM_COMMAND:
    {
      WORD wNotifyCode = HIWORD(wParam); // notification code
      if ((CBN_SELENDOK == wNotifyCode) || (CBN_SELENDCANCEL == wNotifyCode)) { // Combo box change
        nsGUIEvent event(PR_TRUE, NS_CONTROL_CHANGE, this);
        nsPoint point(0,0);
        InitEvent(event, &point); // this add ref's event.widget
        result = DispatchWindowEvent(&event);
        NS_RELEASE(event.widget);
      } else if (wNotifyCode == 0) { // Menu selection
        nsMenuEvent event(PR_TRUE, NS_MENU_SELECTED, this);
        event.mCommand = LOWORD(wParam);
        InitEvent(event);
        result = DispatchWindowEvent(&event);
        NS_RELEASE(event.widget);
      }
    }
    break;

#ifndef WINCE
    case WM_DISPLAYCHANGE:
      DispatchStandardEvent(NS_DISPLAYCHANGED);
      break;
#endif

    case WM_SYSCOLORCHANGE:
      // Note: This is sent for child windows as well as top-level windows.
      // The Win32 toolkit normally only sends these events to top-level windows.
      // But we cycle through all of the childwindows and send it to them as well
      // so all presentations get notified properly.
      // See nsWindow::GlobalMsgWindowProc.
      DispatchStandardEvent(NS_SYSCOLORCHANGED);
      break;

    case WM_NOTIFY:
      // TAB change
    {
      LPNMHDR pnmh = (LPNMHDR) lParam;

        switch (pnmh->code) {
          case TCN_SELCHANGE:
          {
            DispatchStandardEvent(NS_TABCHANGE);
            result = PR_TRUE;
          }
          break;
        }
    }
    break;

    case WM_XP_THEMECHANGED:
    {
      DispatchStandardEvent(NS_THEMECHANGED);

      // Invalidate the window so that the repaint will
      // pick up the new theme.
      Invalidate(PR_FALSE);
    }
    break;

    case WM_FONTCHANGE:
    {
      nsresult rv;
      PRBool didChange = PR_FALSE;

      // update the global font list
      nsCOMPtr<nsIFontEnumerator> fontEnum = do_GetService("@mozilla.org/gfx/fontenumerator;1", &rv);
      if (NS_SUCCEEDED(rv)) {
        fontEnum->UpdateFontList(&didChange);
        //didChange is TRUE only if new font langGroup is added to the list.
        if (didChange)  {
          // update device context font cache
          // Dirty but easiest way:
          // Changing nsIPrefBranch entry which triggers callbacks
          // and flows into calling mDeviceContext->FlushFontCache()
          // to update the font cache in all the instance of Browsers
          nsCOMPtr<nsIPrefService> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
          if (prefs) {
            nsCOMPtr<nsIPrefBranch> fiPrefs;
            prefs->GetBranch("font.internaluseonly.", getter_AddRefs(fiPrefs));
            if (fiPrefs) {
              PRBool fontInternalChange = PR_FALSE;
              fiPrefs->GetBoolPref("changed", &fontInternalChange);
              fiPrefs->SetBoolPref("changed", !fontInternalChange);
            }
          }
        }
      } //if (NS_SUCCEEDED(rv))
    }
    break;

#ifndef WINCE
    case WM_POWERBROADCAST:
      // only hidden window handle this
      // to prevent duplicate notification
      if (mWindowType == eWindowType_invisible) {
        switch (wParam)
        {
          case PBT_APMSUSPEND:
            PostSleepWakeNotification("sleep_notification");
            break;
          case PBT_APMRESUMEAUTOMATIC:
          case PBT_APMRESUMECRITICAL:
          case PBT_APMRESUMESUSPEND:
            PostSleepWakeNotification("wake_notification");
            break;
        }
      }
      break;
#endif

    case WM_MOVE: // Window moved
    {
      PRInt32 x = GET_X_LPARAM(lParam); // horizontal position in screen coordinates
      PRInt32 y = GET_Y_LPARAM(lParam); // vertical position in screen coordinates
      result = OnMove(x, y);
    }
    break;

    case WM_CLOSE: // close request
      DispatchStandardEvent(NS_XUL_CLOSE);
      result = PR_TRUE; // abort window closure
      break;

    case WM_DESTROY:
      // clean up.
      OnDestroy();
      result = PR_TRUE;
      break;

    case WM_PAINT:
#ifdef MOZ_CAIRO_GFX
      *aRetValue = (int) OnPaint();
      result = PR_TRUE;
#else
      result = OnPaint();
#endif
      break;

#ifndef WINCE
    case WM_PRINTCLIENT:
      result = OnPaint((HDC) wParam);
      break;
#endif

#ifdef WINCE
      // This needs to move into nsIDOMKeyEvent.idl && nsGUIEvent.h
    case WM_HOTKEY:
    {
      // SmartPhones has a one or two menu buttons at the
      // bottom of the screen.  They are dispatched via a
      // menu resource, rather then a hotkey.  To make
      // this look consistent, we have mapped this menu to
      // fire hotkey events.  See
      // http://msdn.microsoft.com/library/default.asp?url=/library/en-us/win_ce/html/pwc_TheBackButtonandOtherInterestingButtons.asp
      
      if (VK_TSOFT1 == HIWORD(lParam) && (0 != (MOD_KEYUP & LOWORD(lParam))))
      {
        keybd_event(VK_F19, 0, 0, 0);
        keybd_event(VK_F19, 0, KEYEVENTF_KEYUP, 0);
        result = 0;
        break;
      }
      
      if (VK_TSOFT2 == HIWORD(lParam) && (0 != (MOD_KEYUP & LOWORD(lParam))))
      {
        keybd_event(VK_F20, 0, 0, 0);
        keybd_event(VK_F20, 0, KEYEVENTF_KEYUP, 0);
        result = 0;
        break;
      }
      
      if (VK_TBACK == HIWORD(lParam) && (0 != (MOD_KEYUP & LOWORD(lParam))))
      {
        keybd_event(VK_BACK, 0, 0, 0);
        keybd_event(VK_BACK, 0, KEYEVENTF_KEYUP, 0);
        result = 0;
        break;
      }

      switch (wParam) 
      {
        case VK_APP1:
          keybd_event(VK_F1, 0, 0, 0);
          keybd_event(VK_F1, 0, KEYEVENTF_KEYUP, 0);
          result = 0;
          break;

        case VK_APP2:
          keybd_event(VK_F2, 0, 0, 0);
          keybd_event(VK_F2, 0, KEYEVENTF_KEYUP, 0);
          result = 0;
          break;

        case VK_APP3:
          keybd_event(VK_F3, 0, 0, 0);
          keybd_event(VK_F3, 0, KEYEVENTF_KEYUP, 0);
          result = 0;
          break;

        case VK_APP4:
          keybd_event(VK_F4, 0, 0, 0);
          keybd_event(VK_F4, 0, KEYEVENTF_KEYUP, 0);
          result = 0;
          break;

        case VK_APP5:
          keybd_event(VK_F5, 0, 0, 0);
          keybd_event(VK_F5, 0, KEYEVENTF_KEYUP, 0);
          result = 0;
          break;

        case VK_APP6:
          keybd_event(VK_F6, 0, 0, 0);
          keybd_event(VK_F6, 0, KEYEVENTF_KEYUP, 0);
          result = 0;
          break;
      }
    }
    break;
#endif

    case WM_SYSCHAR:
    case WM_CHAR:
    {
#ifdef KE_DEBUG
      printf("%s\tchar=%c\twp=%4x\tlp=%8x\n", (msg == WM_SYSCHAR) ? "WM_SYSCHAR" : "WM_CHAR", wParam, wParam, lParam);
#endif
      result = OnChar(wParam, lParam);
    }
    break;

    case WM_SYSKEYUP:
    case WM_KEYUP:

#ifdef KE_DEBUG
      printf("%s\t\twp=%x\tlp=%x\n", (WM_KEYUP==msg) ? "WM_KEYUP" : "WM_SYSKEYUP", wParam, lParam);
#endif
      mIsShiftDown   = IS_VK_DOWN(NS_VK_SHIFT);
      mIsControlDown = IS_VK_DOWN(NS_VK_CONTROL);
      mIsAltDown     = IS_VK_DOWN(NS_VK_ALT);

      // Note: the original code passed (HIWORD(lParam)) to OnKeyUp as
      // scan code. However, this breaks Alt+Num pad input.
      // http://msdn.microsoft.com/library/en-us/winui/winui/windowsuserinterface/userinput/keyboardinput/keyboardinputreference/keyboardinputfunctions/toascii.asp
      // states the following:
      //  Typically, ToAscii performs the translation based on the
      //  virtual-key code. In some cases, however, bit 15 of the
      //  uScanCode parameter may be used to distinguish between a key
      //  press and a key release. The scan code is used for
      //  translating ALT+number key combinations.

      // ignore [shift+]alt+space so the OS can handle it
      if (mIsAltDown && !mIsControlDown && IS_VK_DOWN(NS_VK_SPACE)) {
        result = PR_FALSE;
        DispatchPendingEvents();
        break;
      }

      if (!sIMEIsComposing && (msg != WM_KEYUP || wParam != VK_MENU)) {
        // Ignore VK_MENU if it's not a system key release, so that the menu bar does not trigger
        // This helps avoid triggering the menu bar for ALT key accelerators used in
        // assistive technologies such as Window-Eyes and ZoomText, and when using Alt+Tab
        // to switch back to Mozilla in Windows 95 and Windows 98
        result = OnKeyUp(wParam, (HIWORD(lParam)), lParam);
      }
      else {
        result = PR_FALSE;
      }

      DispatchPendingEvents();
      break;

    // Let the fall through if it isn't a key pad
    case WM_SYSKEYDOWN:
    case WM_KEYDOWN:
#ifdef KE_DEBUG
      printf("%s\t\twp=%4x\tlp=%8x\n", (WM_KEYDOWN==msg) ? "WM_KEYDOWN" : "WM_SYSKEYDOWN", wParam, lParam);
#endif

      mIsShiftDown   = IS_VK_DOWN(NS_VK_SHIFT);
      mIsControlDown = IS_VK_DOWN(NS_VK_CONTROL);
      mIsAltDown     = IS_VK_DOWN(NS_VK_ALT);

      // Note: the original code passed (HIWORD(lParam)) to OnKeyDown as
      // scan code. However, this breaks Alt+Num pad input.
      // http://msdn.microsoft.com/library/en-us/winui/winui/windowsuserinterface/userinput/keyboardinput/keyboardinputreference/keyboardinputfunctions/toascii.asp
      // states the following:
      //  Typically, ToAscii performs the translation based on the
      //  virtual-key code. In some cases, however, bit 15 of the
      //  uScanCode parameter may be used to distinguish between a key
      //  press and a key release. The scan code is used for
      //  translating ALT+number key combinations.

      // ignore [shift+]alt+space so the OS can handle it
      if (mIsAltDown && !mIsControlDown && IS_VK_DOWN(NS_VK_SPACE)) {
        result = PR_FALSE;
        DispatchPendingEvents();
        break;
      }

      if (mIsAltDown && sIMEIsStatusChanged) {
        sIMEIsStatusChanged = FALSE;
        result = PR_FALSE;
      }
      else if (!sIMEIsComposing) {
        result = OnKeyDown(wParam, (HIWORD(lParam)), lParam);
      }
      else
        result = PR_FALSE;
#ifndef WINCE
      if (wParam == VK_MENU || (wParam == VK_F10 && !mIsShiftDown)) {
        // We need to let Windows handle this keypress,
        // by returning PR_FALSE, if there's a native menu
        // bar somewhere in our containing window hierarchy.
        // Otherwise we handle the keypress and don't pass
        // it on to Windows, by returning PR_TRUE.
        PRBool hasNativeMenu = PR_FALSE;
        HWND hWnd = mWnd;
        while (hWnd) {
          if (::GetMenu(hWnd)) {
            hasNativeMenu = PR_TRUE;
            break;
          }
          hWnd = ::GetParent(hWnd);
        }
        result = !hasNativeMenu;
      }
#endif
      DispatchPendingEvents();
      break;

    // say we've dealt with erase background if widget does
    // not need auto-erasing
    case WM_ERASEBKGND:
      if (! AutoErase()) {
        *aRetValue = 1;
        result = PR_TRUE;
      }
      break;

    case WM_GETDLGCODE:
      *aRetValue = DLGC_WANTALLKEYS;
      result = PR_TRUE;
      break;

    case WM_MOUSEMOVE:
    {
      // Suppress dispatch of pending events
      // when mouse moves are generated by widget
      // creation instead of user input.
      LPARAM lParamScreen = lParamToScreen(lParam);
      POINT mp;
      mp.x      = GET_X_LPARAM(lParamScreen);
      mp.y      = GET_Y_LPARAM(lParamScreen);
      PRBool userMovedMouse = PR_FALSE;
      if ((gLastMouseMovePoint.x != mp.x) || (gLastMouseMovePoint.y != mp.y)) {
        userMovedMouse = PR_TRUE;
      }

      result = DispatchMouseEvent(NS_MOUSE_MOVE, wParam, lParam);
      if (userMovedMouse) {
        DispatchPendingEvents();
      }
    }
    break;

    case WM_LBUTTONDOWN:
      //SetFocus(); // this is bad
      //RelayMouseEvent(msg,wParam, lParam);
    {
#ifdef WINCE
      if (!gRollupListener && !gRollupWidget) 
      {
        SHRGINFO  shrg;
        shrg.cbSize = sizeof(shrg);
        shrg.hwndClient = mWnd;
        shrg.ptDown.x = LOWORD(lParam);
        shrg.ptDown.y = HIWORD(lParam);
        shrg.dwFlags = SHRG_RETURNCMD;
        if (SHRecognizeGesture(&shrg)  == GN_CONTEXTMENU)
        {
          result = DispatchMouseEvent(NS_MOUSE_BUTTON_DOWN, wParam, lParam,
                                       PR_FALSE, nsMouseEvent::eRightButton);
          result = DispatchMouseEvent(NS_MOUSE_BUTTON_UP, wParam, lParam,
                                      PR_FALSE, nsMouseEvent::eRightButton);
          break;
        }
      }
#endif
      // check whether IME window do mouse operation
      if (IMEMouseHandling(IMEMOUSE_LDOWN, lParam))
        break;

      result = DispatchMouseEvent(NS_MOUSE_BUTTON_DOWN, wParam, lParam,
                                  PR_FALSE, nsMouseEvent::eLeftButton);
      DispatchPendingEvents();
    }
    break;

    case WM_LBUTTONUP:
      //RelayMouseEvent(msg,wParam, lParam);
      result = DispatchMouseEvent(NS_MOUSE_BUTTON_UP, wParam, lParam,
                                  PR_FALSE, nsMouseEvent::eLeftButton);
      DispatchPendingEvents();
      break;

#ifndef WINCE
    case WM_MOUSELEAVE:
    {
      // We use MINLONG | MINSHORT as the mouse position to make sure
      // EventStateManager doesn't convert this EXIT message to
      // a MOVE message (besides, WM_MOUSELEAVE doesn't have the position
      // in lParam). 
      // We also need to check mouse button states and put them in for
      // wParam.
      WPARAM mouseState = (GetKeyState(VK_LBUTTON) ? MK_LBUTTON : 0)
        | (GetKeyState(VK_MBUTTON) ? MK_MBUTTON : 0)
        | (GetKeyState(VK_RBUTTON) ? MK_RBUTTON : 0);
      DispatchMouseEvent(NS_MOUSE_EXIT, mouseState, MINLONG | MINSHORT);
    }
    break;
#endif

    case WM_CONTEXTMENU:
    {
      // if the context menu is brought up from the keyboard, |lParam|
      // will be maxlong.
      LPARAM pos;
      PRBool contextMenukey = PR_FALSE;
      if (lParam == 0xFFFFFFFF)
      {
        contextMenukey = PR_TRUE;
        pos = lParamToClient(GetMessagePos());
      }
      else
      {
        pos = lParamToClient(lParam);
      }
      result = DispatchMouseEvent(NS_CONTEXTMENU, wParam, pos, contextMenukey,
                                  nsMouseEvent::eRightButton);
    }
    break;

    case WM_LBUTTONDBLCLK:
      result = DispatchMouseEvent(NS_MOUSE_DOUBLECLICK, wParam, lParam, PR_FALSE,
                                  nsMouseEvent::eLeftButton);
      break;

    case WM_MBUTTONDOWN:
    {
      // check whether IME window do mouse operation
      if (IMEMouseHandling(IMEMOUSE_MDOWN, lParam))
        break;
      result = DispatchMouseEvent(NS_MOUSE_BUTTON_DOWN, wParam, lParam, PR_FALSE,
                                  nsMouseEvent::eMiddleButton);
      DispatchPendingEvents();
    }
    break;

    case WM_MBUTTONUP:
      result = DispatchMouseEvent(NS_MOUSE_BUTTON_UP, wParam, lParam, PR_FALSE,
                                  nsMouseEvent::eMiddleButton);
      DispatchPendingEvents();
      break;

    case WM_MBUTTONDBLCLK:
      result = DispatchMouseEvent(NS_MOUSE_BUTTON_DOWN, wParam, lParam, PR_FALSE,
                                  nsMouseEvent::eMiddleButton);
      break;

    case WM_RBUTTONDOWN:
    {
      // check whether IME window do mouse operation
      if (IMEMouseHandling(IMEMOUSE_RDOWN, lParam))
        break;
      result = DispatchMouseEvent(NS_MOUSE_BUTTON_DOWN, wParam, lParam, PR_FALSE,
                                  nsMouseEvent::eRightButton);
      DispatchPendingEvents();
    }
    break;

    case WM_RBUTTONUP:
      result = DispatchMouseEvent(NS_MOUSE_BUTTON_UP, wParam, lParam, PR_FALSE,
                                  nsMouseEvent::eRightButton);
      DispatchPendingEvents();
      break;

    case WM_RBUTTONDBLCLK:
      result = DispatchMouseEvent(NS_MOUSE_DOUBLECLICK, wParam, lParam, PR_FALSE,
                                  nsMouseEvent::eRightButton);
      break;

    case WM_APPCOMMAND:
    {
      PRUint32 appCommand = GET_APPCOMMAND_LPARAM(lParam);

      switch (appCommand)
      {
        case APPCOMMAND_BROWSER_BACKWARD:
        case APPCOMMAND_BROWSER_FORWARD:
        case APPCOMMAND_BROWSER_REFRESH:
        case APPCOMMAND_BROWSER_STOP:
        case APPCOMMAND_BROWSER_SEARCH:
        case APPCOMMAND_BROWSER_FAVORITES:
        case APPCOMMAND_BROWSER_HOME:
          DispatchCommandEvent(appCommand);
          // tell the driver that we handled the event
          *aRetValue = 1;
          result = PR_TRUE;
          break;
      }
      // default = PR_FALSE - tell the driver that the event was not handled
    }
    break;

    case WM_HSCROLL:
    case WM_VSCROLL:
      // check for the incoming nsWindow handle to be null in which case
      // we assume the message is coming from a horizontal scrollbar inside
      // a listbox and we don't bother processing it (well, we don't have to)
      if (lParam) {
        nsWindow* scrollbar = GetNSWindowPtr((HWND)lParam);

        if (scrollbar) {
          result = scrollbar->OnScroll(LOWORD(wParam), (short)HIWORD(wParam));
        }
      }
      break;

    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORBTN:
    //case WM_CTLCOLORSCROLLBAR: //XXX causes the scrollbar to be drawn incorrectly
    case WM_CTLCOLORSTATIC:
      if (lParam) {
        nsWindow* control = GetNSWindowPtr((HWND)lParam);
          if (control) {
            control->SetUpForPaint((HDC)wParam);
            *aRetValue = (LPARAM)control->OnControlColor();
          }
      }

      result = PR_TRUE;
      break;

    case WM_ACTIVATE:
      if (mEventCallback) {
        PRInt32 fActive = LOWORD(wParam);

        if (WA_INACTIVE == fActive) {
          gJustGotDeactivate = PR_TRUE;
          if (mIsTopWidgetWindow)
            mLastKeyboardLayout = gKeyboardLayout;
        } else {
          gJustGotActivate = PR_TRUE;
          nsMouseEvent event(PR_TRUE, NS_MOUSE_ACTIVATE, this,
                             nsMouseEvent::eReal);
          InitEvent(event);

          event.acceptActivation = PR_TRUE;

          PRBool result = DispatchWindowEvent(&event);
          NS_RELEASE(event.widget);

          if (event.acceptActivation)
            *aRetValue = MA_ACTIVATE;
          else
            *aRetValue = MA_NOACTIVATE;

          if (gSwitchKeyboardLayout && mLastKeyboardLayout)
            ActivateKeyboardLayout(mLastKeyboardLayout, 0);
        }
      }
      break;

#ifndef WINCE
    case WM_WINDOWPOSCHANGING:
    {
      LPWINDOWPOS info = (LPWINDOWPOS) lParam;
      // enforce local z-order rules
      if (!(info->flags & SWP_NOZORDER))
        ConstrainZLevel(&info->hwndInsertAfter);
      // prevent rude external programs from making hidden window visible
      if (mWindowType == eWindowType_invisible)
        info->flags &= ~SWP_SHOWWINDOW;
    }
    break;
#endif

    case WM_SETFOCUS:
      result = DispatchFocus(NS_GOTFOCUS, isMozWindowTakingFocus);
      if (gJustGotActivate) {
        gJustGotActivate = PR_FALSE;
        gJustGotDeactivate = PR_FALSE;
        result = DispatchFocus(NS_ACTIVATE, isMozWindowTakingFocus);
      }
#ifdef ACCESSIBILITY
      if (nsWindow::gIsAccessibilityOn) {
        // Create it for the first time so that it can start firing events
        nsCOMPtr<nsIAccessible> rootAccessible = GetRootAccessible();
      }
#endif

#ifdef WINCE
      // On Windows CE, we have a window that overlaps
      // the ISP button.  In this case, we should always
      // try to hide it when we are activated
      if (mWindowType == eWindowType_dialog || mWindowType == eWindowType_toplevel) {
        
        // This should work on all platforms, but it doesn't...
        SHFullScreen(mWnd, SHFS_HIDESIPBUTTON);
        
        HWND hWndSIP = FindWindow( _T( "MS_SIPBUTTON" ), NULL );
        if (hWndSIP) 
        {
          ShowWindow( hWndSIP, SW_HIDE );
          SetWindowPos(hWndSIP, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
        }
      }
      {
        // Get current input context
        HIMC hC = ImmGetContext(mWnd);		
        // Open the IME 
        ImmSetOpenStatus(hC, TRUE);
        // Set "multi-press" input mode
        ImmEscapeW(NULL, hC, IME_ESC_SET_MODE, (LPVOID)IM_SPELL);
      }
#endif
      break;

    case WM_KILLFOCUS:
#ifdef WINCE
      {
        // Get current input context
        HIMC hC = ImmGetContext(mWnd);
        // Close the IME 
        ImmSetOpenStatus(hC, FALSE);
      }
#endif
      WCHAR className[kMaxClassNameLength];
      ::GetClassNameW((HWND)wParam, className, kMaxClassNameLength);
      if (wcscmp(className, kWClassNameUI) &&
          wcscmp(className, kWClassNameContent) &&
          wcscmp(className, kWClassNameContentFrame) &&
          wcscmp(className, kWClassNameDialog) &&
          wcscmp(className, kWClassNameGeneral)) {
        isMozWindowTakingFocus = PR_FALSE;
      }
      if (gJustGotDeactivate) {
        gJustGotDeactivate = PR_FALSE;
        result = DispatchFocus(NS_DEACTIVATE, isMozWindowTakingFocus);
      }
      result = DispatchFocus(NS_LOSTFOCUS, isMozWindowTakingFocus);
      break;

    case WM_WINDOWPOSCHANGED:
    {
      WINDOWPOS *wp = (LPWINDOWPOS)lParam;

      // We only care about a resize, so filter out things like z-order
      // changes. Note: there's a WM_MOVE handler above which is why we're
      // not handling them here...
      if (0 == (wp->flags & SWP_NOSIZE)) {
        // XXX Why are we using the client size area? If the size notification
        // is for the client area then the origin should be (0,0) and not
        // the window origin in screen coordinates...
        RECT r;
        ::GetWindowRect(mWnd, &r);
        PRInt32 newWidth, newHeight;
        newWidth = PRInt32(r.right - r.left);
        newHeight = PRInt32(r.bottom - r.top);
        nsRect rect(wp->x, wp->y, newWidth, newHeight);


#ifdef MOZ_XUL
        if (mIsTranslucent)
          ResizeTranslucentWindow(newWidth, newHeight);
#endif

        if (newWidth > mLastSize.width)
        {
          RECT drect;

          //getting wider
          drect.left = wp->x + mLastSize.width;
          drect.top = wp->y;
          drect.right = drect.left + (newWidth - mLastSize.width);
          drect.bottom = drect.top + newHeight;

          ::RedrawWindow(mWnd, &drect, NULL,
                         RDW_INVALIDATE | RDW_NOERASE | RDW_NOINTERNALPAINT | RDW_ERASENOW | RDW_ALLCHILDREN);
        }
        if (newHeight > mLastSize.height)
        {
          RECT drect;

          //getting taller
          drect.left = wp->x;
          drect.top = wp->y + mLastSize.height;
          drect.right = drect.left + newWidth;
          drect.bottom = drect.top + (newHeight - mLastSize.height);

          ::RedrawWindow(mWnd, &drect, NULL,
                         RDW_INVALIDATE | RDW_NOERASE | RDW_NOINTERNALPAINT | RDW_ERASENOW | RDW_ALLCHILDREN);
        }

        mBounds.width  = newWidth;
        mBounds.height = newHeight;
        mLastSize.width = newWidth;
        mLastSize.height = newHeight;
        ///nsRect rect(wp->x, wp->y, wp->cx, wp->cy);

        // If we're being minimized, don't send the resize event to Gecko because
        // it will cause the scrollbar in the content area to go away and we'll
        // forget the scroll position of the page.
        if ( !newWidth && !newHeight ) {
          result = PR_FALSE;
          break;
        }

        // recalculate the width and height
        // this time based on the client area
        if (::GetClientRect(mWnd, &r)) {
          rect.width  = PRInt32(r.right - r.left);
          rect.height = PRInt32(r.bottom - r.top);
        }
        result = OnResize(rect);
      }

      /* handle size mode changes
         (the framechanged message seems a handy place to hook in,
         because it happens early enough (WM_SIZE is too late) and
         because in testing it seems an accurate harbinger of
         an impending min/max/restore change (WM_NCCALCSIZE would
         also work, but it's also sent when merely resizing.)) */
      if (wp->flags & SWP_FRAMECHANGED && ::IsWindowVisible(mWnd)) {
        WINDOWPLACEMENT pl;
        pl.length = sizeof(pl);
        ::GetWindowPlacement(mWnd, &pl);

        nsSizeModeEvent event(PR_TRUE, NS_SIZEMODE, this);
        if (pl.showCmd == SW_SHOWMAXIMIZED)
          event.mSizeMode = nsSizeMode_Maximized;
        else if (pl.showCmd == SW_SHOWMINIMIZED)
          event.mSizeMode = nsSizeMode_Minimized;
        else
          event.mSizeMode = nsSizeMode_Normal;
        InitEvent(event);

        result = DispatchWindowEvent(&event);

        if (pl.showCmd == SW_SHOWMINIMIZED) {
          // Deactivate
          WCHAR className[kMaxClassNameLength];
          ::GetClassNameW((HWND)wParam, className, kMaxClassNameLength);
          if (wcscmp(className, kWClassNameUI) &&
              wcscmp(className, kWClassNameContent) &&
              wcscmp(className, kWClassNameContentFrame) &&
              wcscmp(className, kWClassNameDialog) &&
              wcscmp(className, kWClassNameGeneral)) {
            isMozWindowTakingFocus = PR_FALSE;
          }
          gJustGotDeactivate = PR_FALSE;
          result = DispatchFocus(NS_DEACTIVATE, isMozWindowTakingFocus);
        } else if (pl.showCmd == SW_SHOWNORMAL){
          // Make sure we're active
          result = DispatchFocus(NS_GOTFOCUS, PR_TRUE);
          result = DispatchFocus(NS_ACTIVATE, PR_TRUE);
        }

        NS_RELEASE(event.widget);
      }
    }
    break;

    case WM_SETTINGCHANGE:
#ifdef WINCE
      if (wParam == SPI_SETWORKAREA)
      {
        RECT workArea;
        ::SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
     
        SetWindowPos(mWnd, 
                     nsnull, 
                     workArea.left, 
                     workArea.top, 
                     workArea.right, 
                     workArea.bottom, 
                     SWP_NOACTIVATE | SWP_NOOWNERZORDER);
      }
      else
#endif
        getWheelInfo = PR_TRUE;
      break;

    case WM_PALETTECHANGED:
      if ((HWND)wParam == mWnd) {
        // We caused the WM_PALETTECHANGED message so avoid realizing
        // another foreground palette
        result = PR_TRUE;
        break;
      }
      // fall thru...

    case WM_QUERYNEWPALETTE:      // this window is about to become active
      mContext->GetPaletteInfo(palInfo);
      if (palInfo.isPaletteDevice && palInfo.palette) {
        HDC hDC = ::GetDC(mWnd);
        HPALETTE hOldPal = ::SelectPalette(hDC, (HPALETTE)palInfo.palette, TRUE);

        // Realize the drawing palette
        int i = ::RealizePalette(hDC);

#ifdef DEBUG
        //printf("number of colors that changed=%d\n",i);
#endif
        // we should always invalidate.. because the lookup may have changed
        ::InvalidateRect(mWnd, (LPRECT)NULL, TRUE);

        ::ReleaseDC(mWnd, hDC);
        *aRetValue = TRUE;
      }
      result = PR_TRUE;
      break;

#ifndef WINCE
    case WM_INPUTLANGCHANGEREQUEST:
      *aRetValue = TRUE;
      result = PR_FALSE;
      break;

    case WM_INPUTLANGCHANGE:
      result = OnInputLangChange((HKL)lParam, aRetValue);
      break;

    case WM_IME_STARTCOMPOSITION:
      result = OnIMEStartComposition();
      break;

    case WM_IME_COMPOSITION:
      result = OnIMEComposition(lParam);
      break;

    case WM_IME_ENDCOMPOSITION:
      result = OnIMEEndComposition();
      break;

    case WM_IME_CHAR:
      // We receive double byte char code. No need to worry about the <Shift>
      mIsShiftDown = PR_FALSE;
      result = OnIMEChar((BYTE)(wParam >> 8), (BYTE)(wParam & 0x00FF), lParam);
      break;

    case WM_IME_NOTIFY:
      result = OnIMENotify(wParam, lParam, aRetValue);
      break;

    // This is a Window 98/2000 only message
    case WM_IME_REQUEST:
      result = OnIMERequest(wParam, lParam, aRetValue);

      break;

    case WM_IME_SELECT:
      result = OnIMESelect(wParam, (WORD)(lParam & 0x0FFFF));
      break;

    case WM_IME_SETCONTEXT:
      result = OnIMESetContext(wParam, lParam);
      break;

    case WM_DROPFILES:
    {
#if 0
      HDROP hDropInfo = (HDROP) wParam;
      UINT nFiles = ::DragQueryFile(hDropInfo, (UINT)-1, NULL, 0);

      for (UINT iFile = 0; iFile < nFiles; iFile++) {
        TCHAR szFileName[_MAX_PATH];
        ::DragQueryFile(hDropInfo, iFile, szFileName, _MAX_PATH);
#ifdef DEBUG
        printf("szFileName [%s]\n", szFileName);
#endif  // DEBUG
        nsAutoString fileStr(szFileName);
        nsEventStatus status;
        nsDragDropEvent event(NS_DRAGDROP_EVENT, this);
        InitEvent(event);
        event.mType      = nsDragDropEventStatus_eDrop;
        event.mIsFileURL = PR_FALSE;
        event.mURL       = fileStr.get();
        DispatchEvent(&event, status);
        NS_RELEASE(event.widget);
      }
#endif // 0
    }
    break;
#endif // WINCE

    case WM_DESTROYCLIPBOARD:
    {
      nsIClipboard* clipboard;
      nsresult rv = CallGetService(kCClipboardCID, &clipboard);
      clipboard->EmptyClipboard(nsIClipboard::kGlobalClipboard);
      NS_RELEASE(clipboard);
    }
    break;

#ifdef ACCESSIBILITY
    case WM_GETOBJECT:
    {
      LRESULT lAcc = 0;
      IAccessible *msaaAccessible = NULL;
      if (lParam == OBJID_CLIENT) { // oleacc.dll will be loaded dynamically
        nsCOMPtr<nsIAccessible> rootAccessible = GetRootAccessible(); // Held by a11y cache
        if (rootAccessible) {
          rootAccessible->GetNativeInterface((void**)&msaaAccessible); // does an addref
        }
      }
      else if (lParam == OBJID_CARET) {  // each root accessible owns a caret accessible
        nsCOMPtr<nsIAccessible> rootAccessible = GetRootAccessible();  // Held by a11y cache
        nsCOMPtr<nsIAccessibleDocument> accDoc(do_QueryInterface(rootAccessible));
        if (accDoc) {
          nsCOMPtr<nsIAccessible> accessibleCaret;
          accDoc->GetCaretAccessible(getter_AddRefs(accessibleCaret));
          if (accessibleCaret) {
            accessibleCaret->GetNativeInterface((void**)&msaaAccessible);
          }
        }
      }
      if (msaaAccessible) {
        lAcc = LresultFromObject(IID_IAccessible, wParam, msaaAccessible); // does an addref
        msaaAccessible->Release(); // release extra addref
      }
      return (*aRetValue = lAcc) != 0;
    }
#endif

#ifndef WINCE
    case WM_SYSCOMMAND:
      // prevent Windows from trimming the working set. bug 76831
      if (!gTrimOnMinimize && wParam == SC_MINIMIZE) {
        ::ShowWindow(mWnd, SW_SHOWMINIMIZED);
        result = PR_TRUE;
      }
      break;
#endif


#ifdef WINCE
  case WM_HIBERNATE:        
    nsMemory::HeapMinimize(PR_TRUE);
    break;
#endif
    default:
    {
      // Handle both flavors of mouse wheel events.
#ifndef WINCE
      if (msg == WM_MOUSEWHEEL || msg == WM_MOUSEHWHEEL) {
        static int iDeltaPerLine, iDeltaPerChar;
        static ULONG ulScrollLines, ulScrollChars = 1;
        static int currentVDelta, currentHDelta;
        static HWND currentWindow = 0;

        PRBool isVertical = msg == WM_MOUSEWHEEL;

        // Get mouse wheel metrics (but only once).
        if (getWheelInfo) {
          getWheelInfo = PR_FALSE;

          SystemParametersInfo (SPI_GETWHEELSCROLLLINES, 0, &ulScrollLines, 0);

          // ulScrollLines usually equals 3 or 0 (for no scrolling)
          // WHEEL_DELTA equals 120, so iDeltaPerLine will be 40.

          // However, if ulScrollLines > WHEEL_DELTA, we assume that
          // the mouse driver wants a page scroll.  The docs state that
          // ulScrollLines should explicitly equal WHEEL_PAGESCROLL, but
          // since some mouse drivers use an arbitrary large number instead,
          // we have to handle that as well.

          iDeltaPerLine = 0;
          if (ulScrollLines) {
            if (ulScrollLines <= WHEEL_DELTA) {
              iDeltaPerLine = WHEEL_DELTA / ulScrollLines;
            } else {
              ulScrollLines = WHEEL_PAGESCROLL;
            }
          }

          if (!SystemParametersInfo(SPI_GETWHEELSCROLLCHARS, 0,
                                    &ulScrollChars, 0)) {
            // Note that we may always fail to get the value before Win Vista.
            ulScrollChars = 1;
          }

          iDeltaPerChar = 0;
          if (ulScrollChars) {
            if (ulScrollChars <= WHEEL_DELTA) {
              iDeltaPerChar = WHEEL_DELTA / ulScrollChars;
            } else {
              ulScrollChars = WHEEL_PAGESCROLL;
            }
          }
        }

        if ((isVertical  && ulScrollLines != WHEEL_PAGESCROLL && !iDeltaPerLine) ||
            (!isVertical && ulScrollChars != WHEEL_PAGESCROLL && !iDeltaPerChar))
          return 0;

        // The mousewheel event will be dispatched to the toplevel
        // window.  We need to give it to the child window

        POINT point;
        point.x = GET_X_LPARAM(lParam);
        point.y = GET_Y_LPARAM(lParam);
        HWND destWnd = ::WindowFromPoint(point);

        // Since we receive mousewheel events for as long as
        // we are focused, it's entirely possible that there
        // is another app's window or no window under the
        // pointer.

        if (!destWnd) {
          // No window is under the pointer
          break;
        }

        // We don't care about windows belonging to other processes.
        DWORD processId = 0;
        GetWindowThreadProcessId(destWnd, &processId);
        if (processId != GetCurrentProcessId())
        {
          // Somebody elses window
          break;
        }

        LONG proc = ::GetWindowLongW(destWnd, GWL_WNDPROC);
        if (proc != (LONG)&nsWindow::WindowProc) {
          // Some other app, or a plugin window.
          // Windows directs WM_MOUSEWHEEL to the focused window.
          // However, Mozilla does not like plugins having focus, so a
          // Mozilla window (ie, the plugin's parent (us!) has focus.)
          // Therefore, plugins etc _should_ get first grab at the
          // message, but this focus vaguary means the plugin misses
          // out. If the window is a child of ours, forward it on.
          // Determine if a child by walking the parent list until
          // we find a parent matching our wndproc.
          HWND parentWnd = ::GetParent(destWnd);
          while (parentWnd) {
            LONG parentWndProc = ::GetClassLongW(parentWnd, GCL_WNDPROC);
            if (parentWndProc == (LONG)&nsWindow::DefaultWindowProc || parentWndProc == (LONG)&nsWindow::WindowProc) {
              // We have a child window - quite possibly a plugin window.
              // However, not all plugins are created equal - some will handle this message themselves,
              // some will forward directly back to us, while others will call DefWndProc, which
              // itself still forwards back to us.
              // So if we have sent it once, we need to handle it ourself.
              if (mIsInMouseWheelProcessing) {
                destWnd = parentWnd;
              } else {
                // First time we have seen this message.
                // Call the child - either it will consume it, or
                // it will wind it's way back to us, triggering the destWnd case above.
                // either way, when the call returns, we are all done with the message,
                mIsInMouseWheelProcessing = PR_TRUE;
                if (0 == ::SendMessageW(destWnd, msg, wParam, lParam)) {
                  result = PR_TRUE; // consumed - don't call DefWndProc
                }
                destWnd = nsnull;
                mIsInMouseWheelProcessing = PR_FALSE;
              }
              break; // stop parent search
            }
            parentWnd = ::GetParent(parentWnd);
          } // while parentWnd
        }
        if (destWnd == nsnull)
          break; // done with this message.
        if (destWnd != mWnd) {
          nsWindow* destWindow = GetNSWindowPtr(destWnd);
          if (destWindow) {
            return destWindow->ProcessMessage(msg, wParam, lParam, aRetValue);
          }
#ifdef DEBUG
          else
            printf("WARNING: couldn't get child window for MW event\n");
#endif
        }

        // We should cancel the surplus delta if the current window is not
        // same as previous.
        if (currentWindow != mWnd) {
          currentVDelta = 0;
          currentHDelta = 0;
          currentWindow = mWnd;
        }

        nsMouseScrollEvent scrollEvent(PR_TRUE, NS_MOUSE_SCROLL, this);
        scrollEvent.delta = 0;
        if (isVertical) {
          scrollEvent.scrollFlags = nsMouseScrollEvent::kIsVertical;
          if (ulScrollLines == WHEEL_PAGESCROLL) {
            scrollEvent.scrollFlags |= nsMouseScrollEvent::kIsFullPage;
            scrollEvent.delta = (((short) HIWORD (wParam)) > 0) ? -1 : 1;
          } else {
            currentVDelta -= (short) HIWORD (wParam);
            if (PR_ABS(currentVDelta) >= iDeltaPerLine) {
              scrollEvent.delta = currentVDelta / iDeltaPerLine;
              currentVDelta %= iDeltaPerLine;
            }
          }
        } else {
          scrollEvent.scrollFlags = nsMouseScrollEvent::kIsHorizontal;
          if (ulScrollChars == WHEEL_PAGESCROLL) {
            scrollEvent.scrollFlags |= nsMouseScrollEvent::kIsFullPage;
            scrollEvent.delta = (((short) HIWORD (wParam)) > 0) ? 1 : -1;
          } else {
            currentHDelta += (short) HIWORD (wParam);
            if (PR_ABS(currentHDelta) >= iDeltaPerChar) {
              scrollEvent.delta = currentHDelta / iDeltaPerChar;
              currentHDelta %= iDeltaPerChar;
            }
          }
        }

        scrollEvent.isShift   = IS_VK_DOWN(NS_VK_SHIFT);
        scrollEvent.isControl = IS_VK_DOWN(NS_VK_CONTROL);
        scrollEvent.isMeta    = PR_FALSE;
        scrollEvent.isAlt     = IS_VK_DOWN(NS_VK_ALT);
        InitEvent(scrollEvent);
        if (nsnull != mEventCallback) {
          result = DispatchWindowEvent(&scrollEvent);
        }
        NS_RELEASE(scrollEvent.widget);
        // Note that we should return zero if we process WM_MOUSEWHEEL.
        // But if we process WM_MOUSEHWHEEL, we should return non-zero.
        if (result)
          *aRetValue = isVertical ? 0 : TRUE;
      } // msg == WM_MOUSEWHEEL || msg == WM_MOUSEHWHEEL

      else if (msg == nsWindow::uWM_HEAP_DUMP) {
        // XXX for now we use c:\heapdump.txt until we figure out how to
        // XXX pass in message parameters.
        HeapDump("c:\\heapdump.txt", "whatever");
        result = PR_TRUE;
      }
#endif // WINCE

    }
    break;
  }

  //*aRetValue = result;
  if (mWnd) {
    return result;
  }
  else {
    //Events which caused mWnd destruction and aren't consumed
    //will crash during the Windows default processing.
    return PR_TRUE;
  }
}

//-------------------------------------------------------------------------
//
// return the window class name and initialize the class if needed
//
//-------------------------------------------------------------------------

#define CS_XP_DROPSHADOW       0x00020000

LPCWSTR nsWindow::WindowClassW()
{
  if (!nsWindow::sIsRegistered) {
    WNDCLASSW wc;

//    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.style         = CS_DBLCLKS;
    wc.lpfnWndProc   = nsWindow::DefaultWindowProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = nsToolkit::mDllInstance;
    // XXX : we don't need LoadIconW for now (see bug 171349, comment 181)
    wc.hIcon         = ::LoadIcon(::GetModuleHandle(NULL), IDI_APPLICATION);
    wc.hCursor       = NULL;
    wc.hbrBackground = mBrush;
    wc.lpszMenuName  = NULL;
    wc.lpszClassName = kWClassNameHidden;

    BOOL succeeded = ::RegisterClassW(&wc) != 0;
    nsWindow::sIsRegistered = succeeded;

    wc.lpszClassName = kWClassNameContentFrame;
    if (!::RegisterClassW(&wc)) {
      nsWindow::sIsRegistered = FALSE;
    }

    wc.lpszClassName = kWClassNameContent;
    if (!::RegisterClassW(&wc)) {
      nsWindow::sIsRegistered = FALSE;
    }

    wc.lpszClassName = kWClassNameUI;
    if (!::RegisterClassW(&wc)) {
      nsWindow::sIsRegistered = FALSE;
    }

    wc.lpszClassName = kWClassNameGeneral;
    ATOM generalClassAtom = ::RegisterClassW(&wc);
    if (!generalClassAtom) {
      nsWindow::sIsRegistered = FALSE;
    }

    wc.lpszClassName = kWClassNameDialog;
    wc.hIcon = 0;
    if (!::RegisterClassW(&wc)) {
      nsWindow::sIsRegistered = FALSE;
    }
  }

  if (mWindowType == eWindowType_invisible) {
    return kWClassNameHidden;
  }
  if (mWindowType == eWindowType_dialog) {
    return kWClassNameDialog;
  }
  if (mContentType == eContentTypeContent) {
    return kWClassNameContent;
  }
  if (mContentType == eContentTypeContentFrame) {
    return kWClassNameContentFrame;
  }
  if (mContentType == eContentTypeUI) {
    return kWClassNameUI;
  }
  return kWClassNameGeneral;
}

LPCWSTR nsWindow::WindowPopupClassW()
{
  const LPCWSTR className = L"MozillaDropShadowWindowClass";

  if (!nsWindow::sIsPopupClassRegistered) {
    WNDCLASSW wc;

    wc.style = CS_DBLCLKS | CS_XP_DROPSHADOW;
    wc.lpfnWndProc   = nsWindow::DefaultWindowProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = nsToolkit::mDllInstance;
    // XXX : we don't need LoadIconW for now (see bug 171349, comment 181)
    wc.hIcon         = ::LoadIcon(::GetModuleHandle(NULL), IDI_APPLICATION);
    wc.hCursor       = NULL;
    wc.hbrBackground = mBrush;
    wc.lpszMenuName  = NULL;
    wc.lpszClassName = className;

    nsWindow::sIsPopupClassRegistered = ::RegisterClassW(&wc);
    if (!nsWindow::sIsPopupClassRegistered) {
      // For older versions of Win32 (i.e., not XP), the registration will
      // fail, so we have to re-register without the CS_XP_DROPSHADOW flag.
      wc.style = CS_DBLCLKS;
      nsWindow::sIsPopupClassRegistered = ::RegisterClassW(&wc);
    }
  }

  return className;
}

LPCSTR nsWindow::WindowClass()
{
  // Call into the wide version to make sure things get
  // registered properly.
  LPCWSTR classNameW = WindowClassW();

  // XXX: The class name used here must be kept in sync with
  //      the classname used in WindowClassW();

  if (classNameW == kWClassNameHidden) {
    return kClassNameHidden;
  }
  if (classNameW == kWClassNameDialog) {
    return kClassNameDialog;
  }
  if (classNameW == kWClassNameUI) {
    return kClassNameUI;
  }
  if (classNameW == kWClassNameContent) {
    return kClassNameContent;
  }
  if (classNameW == kWClassNameContentFrame) {
    return kClassNameContentFrame;
  }
  return kClassNameGeneral;
}

LPCSTR nsWindow::WindowPopupClass()
{
  // Call into the wide version to make sure things get
  // registered properly.
  WindowPopupClassW();

  // XXX: The class name used here must be kept in sync with
  //      the classname used in WindowPopupClassW();
  return "MozillaDropShadowWindowClass";
}

//-------------------------------------------------------------------------
//
// return nsWindow styles
//
//-------------------------------------------------------------------------
DWORD nsWindow::WindowStyle()
{
  DWORD style;

#ifdef WINCE
  switch (mWindowType) {
    case eWindowType_child:
      style = WS_CHILD;
      break;

    case eWindowType_dialog:
    case eWindowType_popup:
      style = WS_BORDER | WS_POPUP;
      break;

    default:
      NS_ASSERTION(0, "unknown border style");
      // fall through

    case eWindowType_toplevel:
    case eWindowType_invisible:
      style = WS_BORDER;
      break;
  }

#else
  switch (mWindowType) {
    case eWindowType_child:
      style = WS_OVERLAPPED;
      break;

    case eWindowType_dialog:
      if (mBorderStyle == eBorderStyle_default) {
        style = WS_OVERLAPPED | WS_BORDER | WS_DLGFRAME | WS_SYSMENU |
                DS_3DLOOK | DS_MODALFRAME;
      } else {
        style = WS_OVERLAPPED | WS_BORDER | WS_DLGFRAME | WS_SYSMENU |
                DS_3DLOOK | DS_MODALFRAME |
                WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
      }
      break;

    case eWindowType_popup:
      style = WS_OVERLAPPED | WS_POPUP;
      break;

    default:
      NS_ASSERTION(0, "unknown border style");
      // fall through

    case eWindowType_toplevel:
    case eWindowType_invisible:
      style = WS_OVERLAPPED | WS_BORDER | WS_DLGFRAME | WS_SYSMENU |
              WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
      break;
  }

  if (mBorderStyle != eBorderStyle_default && mBorderStyle != eBorderStyle_all) {
    if (mBorderStyle == eBorderStyle_none || !(mBorderStyle & eBorderStyle_border))
      style &= ~WS_BORDER;

    if (mBorderStyle == eBorderStyle_none || !(mBorderStyle & eBorderStyle_title)) {
      style &= ~WS_DLGFRAME;
      style |= WS_POPUP;
    }

    if (mBorderStyle == eBorderStyle_none || !(mBorderStyle & eBorderStyle_close))
      style &= ~0;
    // XXX The close box can only be removed by changing the window class,
    // as far as I know   --- roc+moz@cs.cmu.edu

    if (mBorderStyle == eBorderStyle_none ||
      !(mBorderStyle & (eBorderStyle_menu | eBorderStyle_close)))
      style &= ~WS_SYSMENU;
    // Looks like getting rid of the system menu also does away with the
    // close box. So, we only get rid of the system menu if you want neither it
    // nor the close box. How does the Windows "Dialog" window class get just
    // closebox and no sysmenu? Who knows.

    if (mBorderStyle == eBorderStyle_none || !(mBorderStyle & eBorderStyle_resizeh))
      style &= ~WS_THICKFRAME;

    if (mBorderStyle == eBorderStyle_none || !(mBorderStyle & eBorderStyle_minimize))
      style &= ~WS_MINIMIZEBOX;

    if (mBorderStyle == eBorderStyle_none || !(mBorderStyle & eBorderStyle_maximize))
      style &= ~WS_MAXIMIZEBOX;
  }
#endif // WINCE
  return style;
}


//-------------------------------------------------------------------------
//
// return nsWindow extended styles
//
//-------------------------------------------------------------------------
DWORD nsWindow::WindowExStyle()
{
  switch (mWindowType)
  {
    case eWindowType_child:
      return 0;

    case eWindowType_dialog:
      return WS_EX_WINDOWEDGE | WS_EX_DLGMODALFRAME;

    case eWindowType_popup:
      return WS_EX_TOPMOST | WS_EX_TOOLWINDOW;

    default:
      NS_ASSERTION(0, "unknown border style");
      // fall through

    case eWindowType_toplevel:
    case eWindowType_invisible:
      return WS_EX_WINDOWEDGE;
  }
}


// -----------------------------------------------------------------------
//
// Subclass (or remove the subclass from) this component's nsWindow
//
// -----------------------------------------------------------------------
void nsWindow::SubclassWindow(BOOL bState)
{
  if (NULL != mWnd) {
    NS_PRECONDITION(::IsWindow(mWnd), "Invalid window handle");

    if (bState) {
      // change the nsWindow proc
      if (mUnicodeWidget)
        mPrevWndProc = (WNDPROC)::SetWindowLongW(mWnd, GWL_WNDPROC,
                                                (LONG)nsWindow::WindowProc);
      else
        mPrevWndProc = (WNDPROC)::SetWindowLongA(mWnd, GWL_WNDPROC,
                                                (LONG)nsWindow::WindowProc);
      NS_ASSERTION(mPrevWndProc, "Null standard window procedure");
      // connect the this pointer to the nsWindow handle
      SetNSWindowPtr(mWnd, this);
    }
    else {
      if (mUnicodeWidget)
        ::SetWindowLongW(mWnd, GWL_WNDPROC, (LONG)mPrevWndProc);
      else
        ::SetWindowLongA(mWnd, GWL_WNDPROC, (LONG)mPrevWndProc);
      SetNSWindowPtr(mWnd, NULL);
      mPrevWndProc = NULL;
    }
  }
}


//-------------------------------------------------------------------------
//
// WM_DESTROY has been called
//
//-------------------------------------------------------------------------
void nsWindow::OnDestroy()
{
  mOnDestroyCalled = PR_TRUE;

  SubclassWindow(FALSE);
  mWnd = NULL;

  // free GDI objects
  if (mBrush) {
    VERIFY(::DeleteObject(mBrush));
    mBrush = NULL;
  }

#if 0
  if (mPalette) {
    VERIFY(::DeleteObject(mPalette));
    mPalette = NULL;
  }
#endif

  // if we were in the middle of deferred window positioning then
  // free the memory for the multiple-window position structure
  if (mDeferredPositioner) {
    VERIFY(::EndDeferWindowPos(mDeferredPositioner));
    mDeferredPositioner = NULL;
  }

  // release references to children, device context, toolkit, and app shell
  nsBaseWidget::OnDestroy();

  // dispatch the event
  if (!mIsDestroying) {
    // dispatching of the event may cause the reference count to drop to 0
    // and result in this object being destroyed. To avoid that, add a reference
    // and then release it after dispatching the event
    AddRef();
    DispatchStandardEvent(NS_DESTROY);
    Release();
  }
}

//-------------------------------------------------------------------------
//
// Move
//
//-------------------------------------------------------------------------
PRBool nsWindow::OnMove(PRInt32 aX, PRInt32 aY)
{
  mBounds.x = aX;
  mBounds.y = aY;

  nsGUIEvent event(PR_TRUE, NS_MOVE, this);
  InitEvent(event);
  event.refPoint.x = aX;
  event.refPoint.y = aY;

  PRBool result = DispatchWindowEvent(&event);
  NS_RELEASE(event.widget);
  return result;
}

//-------------------------------------------------------------------------
//
// Paint
//
//-------------------------------------------------------------------------
PRBool nsWindow::OnPaint(HDC aDC)
{
  nsRect bounds;
  PRBool result = PR_TRUE;
  PAINTSTRUCT ps;
  nsEventStatus eventStatus = nsEventStatus_eIgnore;

#ifdef MOZ_XUL
  if (!aDC && mIsTranslucent)
  {
    // For layered translucent windows all drawing should go to memory DC and no
    // WM_PAINT messages are normally generated. To support asynchronous painting
    // we force generation of WM_PAINT messages by invalidating window areas with
    // RedrawWindow, InvalidateRect or InvalidateRgn function calls.
    // BeginPaint/EndPaint must be called to make Windows think that invalid area
    // is painted. Otherwise it will continue sending the same message endlessly.
    ::BeginPaint(mWnd, &ps);
    ::EndPaint(mWnd, &ps);

    aDC = mMemoryDC;
  }
#endif

  mPainting = PR_TRUE;

#ifdef NS_DEBUG
  HRGN debugPaintFlashRegion = NULL;
  HDC debugPaintFlashDC = NULL;

  if (debug_WantPaintFlashing())
  {
    debugPaintFlashRegion = ::CreateRectRgn(0, 0, 0, 0);
    ::GetUpdateRgn(mWnd, debugPaintFlashRegion, TRUE);
    debugPaintFlashDC = ::GetDC(mWnd);
  }
#endif // NS_DEBUG

  HDC hDC = aDC ? aDC : (::BeginPaint(mWnd, &ps));
  mPaintDC = hDC;
  RECT paintRect;

#ifdef MOZ_XUL
  if (aDC || mIsTranslucent) {
#else
  if (aDC) {
#endif
    ::GetClientRect(mWnd, &paintRect);
  }
  else {
    paintRect = ps.rcPaint;
  }

  if (!IsRectEmpty(&paintRect))
  {
    // call the event callback
    if (mEventCallback)
    {
      nsPaintEvent event(PR_TRUE, NS_PAINT, this);

      InitEvent(event);

      nsRect rect(paintRect.left,
                  paintRect.top,
                  paintRect.right - paintRect.left,
                  paintRect.bottom - paintRect.top);
      event.region = nsnull;
      event.rect = &rect;
      // Should probably pass in a real region here, using GetRandomRgn
      // http://msdn.microsoft.com/library/default.asp?url=/library/en-us/gdi/clipping_4q0e.asp

#ifdef NS_DEBUG
      debug_DumpPaintEvent(stdout,
                           this,
                           &event,
                           nsCAutoString("noname"),
                           (PRInt32) mWnd);
#endif // NS_DEBUG

#ifdef MOZ_CAIRO_GFX
      nsRefPtr<gfxASurface> targetSurface = new gfxWindowsSurface(hDC);
      nsRefPtr<gfxContext> thebesContext = new gfxContext(targetSurface);

#ifdef MOZ_XUL
      if (mIsTranslucent) {
        // If we're rendering with translucency, we're going to be
        // rendering the whole window; make sure we clear it first
        thebesContext->SetOperator(gfxContext::OPERATOR_CLEAR);
        thebesContext->Paint();
        thebesContext->SetOperator(gfxContext::OPERATOR_OVER);
      } else {
        // If we're not doing translucency, then double buffer
        thebesContext->PushGroup(gfxASurface::CONTENT_COLOR);
      }
#endif

      nsCOMPtr<nsIRenderingContext> rc;
      nsresult rv = mContext->CreateRenderingContextInstance (*getter_AddRefs(rc));
      if (NS_FAILED(rv)) {
        NS_WARNING("CreateRenderingContextInstance failed");
        return PR_FALSE;
      }

      rv = rc->Init(mContext, thebesContext);
      if (NS_FAILED(rv)) {
        NS_WARNING("RC::Init failed");
        return PR_FALSE;
      }

      event.renderingContext = rc;
      result = DispatchWindowEvent(&event, eventStatus);
      event.renderingContext = nsnull;

#ifdef MOZ_XUL
      if (mIsTranslucent) {
        // Data from offscreen drawing surface was copied to memory bitmap of transparent
        // bitmap. Now it can be read from memory bitmap to apply alpha channel and after
        // that displayed on the screen.
        UpdateTranslucentWindow();
      } else if (result) {
        // Only update if DispatchWindowEvent returned TRUE; otherwise, nothing handled
        // this, and we'll just end up painting with black.
        thebesContext->PopGroupToSource();
        thebesContext->SetOperator(gfxContext::OPERATOR_SOURCE);
        thebesContext->Paint();
      }
#endif

#else
      /* Non-cairo GFX */
      if (NS_SUCCEEDED(CallCreateInstance(kRenderingContextCID, &event.renderingContext)))
      {
        nsIRenderingContextWin *winrc;
        if (NS_SUCCEEDED(CallQueryInterface(event.renderingContext, &winrc)))
        {
          nsIDrawingSurface* surf;

          //i know all of this seems a little backwards. i'll fix it, i swear. MMP

          if (NS_OK == winrc->CreateDrawingSurface(hDC, surf))
          {
            event.renderingContext->Init(mContext, surf);
            result = DispatchWindowEvent(&event, eventStatus);
            event.renderingContext->DestroyDrawingSurface(surf);

#ifdef MOZ_XUL
            if (mIsTranslucent)
            {
              // Data from offscreen drawing surface was copied to memory bitmap of transparent
              // bitmap. Now it can be read from memory bitmap to apply alpha channel and after
              // that displayed on the screen.
              UpdateTranslucentWindow();
            }
#endif
          }

          NS_RELEASE(winrc);
        }

        NS_RELEASE(event.renderingContext);
      }
      else
        result = PR_FALSE;
#endif

      NS_RELEASE(event.widget);
    }
  }

  if (!aDC) {
    ::EndPaint(mWnd, &ps);
  }

  mPaintDC = nsnull;

#ifdef NS_DEBUG
  if (debug_WantPaintFlashing())
  {
    // Only flash paint events which have not ignored the paint message.
    // Those that ignore the paint message aren't painting anything so there
    // is only the overhead of the dispatching the paint event.
    if (nsEventStatus_eIgnore != eventStatus) {
      ::InvertRgn(debugPaintFlashDC, debugPaintFlashRegion);
      PR_Sleep(PR_MillisecondsToInterval(30));
      ::InvertRgn(debugPaintFlashDC, debugPaintFlashRegion);
      PR_Sleep(PR_MillisecondsToInterval(30));
    }
    ::ReleaseDC(mWnd, debugPaintFlashDC);
    ::DeleteObject(debugPaintFlashRegion);
  }
#endif // NS_DEBUG

  mPainting = PR_FALSE;

  return result;
}


//-------------------------------------------------------------------------
//
// Send a resize message to the listener
//
//-------------------------------------------------------------------------
PRBool nsWindow::OnResize(nsRect &aWindowRect)
{
  // call the event callback
  if (mEventCallback) {
    nsSizeEvent event(PR_TRUE, NS_SIZE, this);
    InitEvent(event);
    event.windowSize = &aWindowRect;
    RECT r;
    if (::GetWindowRect(mWnd, &r)) {
      event.mWinWidth  = PRInt32(r.right - r.left);
      event.mWinHeight = PRInt32(r.bottom - r.top);
    } else {
      event.mWinWidth  = 0;
      event.mWinHeight = 0;
    }
    PRBool result = DispatchWindowEvent(&event);
    NS_RELEASE(event.widget);
    return result;
  }

  return PR_FALSE;
}

//-------------------------------------------------------------------------
//
// Deal with all sort of mouse event
//
//-------------------------------------------------------------------------
PRBool nsWindow::DispatchMouseEvent(PRUint32 aEventType, WPARAM wParam,
                                    LPARAM lParam, PRBool aIsContextMenuKey,
                                    PRInt16 aButton)
{
  PRBool result = PR_FALSE;

  if (nsnull == mEventCallback && nsnull == mMouseListener) {
    return result;
  }

  nsPoint eventPoint;
  eventPoint.x = GET_X_LPARAM(lParam);
  eventPoint.y = GET_Y_LPARAM(lParam);

  nsMouseEvent event(PR_TRUE, aEventType, this, nsMouseEvent::eReal,
                     aIsContextMenuKey
                     ? nsMouseEvent::eContextMenuKey
                     : nsMouseEvent::eNormal);
  if (aEventType == NS_CONTEXTMENU && aIsContextMenuKey) {
    nsPoint zero(0, 0);
    InitEvent(event, &zero);
  } else {
    InitEvent(event, &eventPoint);
  }

  event.isShift   = IS_VK_DOWN(NS_VK_SHIFT);
  event.isControl = IS_VK_DOWN(NS_VK_CONTROL);
  event.isMeta    = PR_FALSE;
  event.isAlt     = IS_VK_DOWN(NS_VK_ALT);
  event.button    = aButton;

  nsRect mpWidget;
  nsRect mpScreen;
  mpWidget.x = eventPoint.x;
  mpWidget.y = eventPoint.y;
  WidgetToScreen(mpWidget, mpScreen);

  // Suppress mouse moves caused by widget creation
  if (aEventType == NS_MOUSE_MOVE) 
  {
    if ((gLastMouseMovePoint.x == mpScreen.x) && (gLastMouseMovePoint.y == mpScreen.y))
    {
      NS_RELEASE(event.widget);
      return result;
    }
    gLastMouseMovePoint.x = mpScreen.x;
    gLastMouseMovePoint.y = mpScreen.y;
  }

  PRBool insideMovementThreshold = (abs(gLastMousePoint.x - eventPoint.x) < (short)::GetSystemMetrics(SM_CXDOUBLECLK)) &&
                                   (abs(gLastMousePoint.y - eventPoint.y) < (short)::GetSystemMetrics(SM_CYDOUBLECLK));

  BYTE eventButton;
  switch (aButton) {
    case nsMouseEvent::eLeftButton:
      eventButton = VK_LBUTTON;
      break;
    case nsMouseEvent::eMiddleButton:
      eventButton = VK_MBUTTON;
      break;
    case nsMouseEvent::eRightButton:
      eventButton = VK_RBUTTON;
      break;
    default:
      eventButton = 0;
      break;
  }

  // Doubleclicks are used to set the click count, then changed to mousedowns
  // We're going to time double-clicks from mouse *up* to next mouse *down*
  LONG curMsgTime = ::GetMessageTime();

  if (aEventType == NS_MOUSE_DOUBLECLICK) {
    event.message = NS_MOUSE_BUTTON_DOWN;
    event.button = aButton;
    gLastClickCount = 2;
  }
  else if (aEventType == NS_MOUSE_BUTTON_UP) {
    // remember when this happened for the next mouse down
    gLastMousePoint.x = eventPoint.x;
    gLastMousePoint.y = eventPoint.y;
    gLastMouseButton = eventButton;
  }
  else if (aEventType == NS_MOUSE_BUTTON_DOWN) {
    // now look to see if we want to convert this to a double- or triple-click

#ifdef NS_DEBUG_XX
    printf("Msg: %d Last: %d Dif: %d Max %d\n", curMsgTime, gLastMouseDownTime, curMsgTime-gLastMouseDownTime, ::GetDoubleClickTime());
    printf("Mouse %d %d\n", abs(gLastMousePoint.x - mp.x), abs(gLastMousePoint.y - mp.y));
#endif
    if (((curMsgTime - gLastMouseDownTime) < (LONG)::GetDoubleClickTime()) && insideMovementThreshold &&
        eventButton == gLastMouseButton) {
      gLastClickCount ++;
    } else {
      // reset the click count, to count *this* click
      gLastClickCount = 1;
    }
    // Set last Click time on MouseDown only
    gLastMouseDownTime = curMsgTime;
  }
  else if (aEventType == NS_MOUSE_MOVE && !insideMovementThreshold) {
    gLastClickCount = 0;
  }
  event.clickCount = gLastClickCount;

#ifdef NS_DEBUG_XX
  printf("Msg Time: %d Click Count: %d\n", curMsgTime, event.clickCount);
#endif

  nsPluginEvent pluginEvent;

  switch (aEventType)
  {
    case NS_MOUSE_BUTTON_DOWN:
      switch (aButton) {
        case nsMouseEvent::eLeftButton:
          pluginEvent.event = WM_LBUTTONDOWN;
          break;
        case nsMouseEvent::eMiddleButton:
          pluginEvent.event = WM_MBUTTONDOWN;
          break;
        case nsMouseEvent::eRightButton:
          pluginEvent.event = WM_RBUTTONDOWN;
          break;
        default:
          break;
      }
      break;
    case NS_MOUSE_BUTTON_UP:
      switch (aButton) {
        case nsMouseEvent::eLeftButton:
          pluginEvent.event = WM_LBUTTONUP;
          break;
        case nsMouseEvent::eMiddleButton:
          pluginEvent.event = WM_MBUTTONUP;
          break;
        case nsMouseEvent::eRightButton:
          pluginEvent.event = WM_RBUTTONUP;
          break;
        default:
          break;
      }
      break;
    case NS_MOUSE_DOUBLECLICK:
      switch (aButton) {
        case nsMouseEvent::eLeftButton:
          pluginEvent.event = WM_LBUTTONDBLCLK;
          break;
        case nsMouseEvent::eMiddleButton:
          pluginEvent.event = WM_MBUTTONDBLCLK;
          break;
        case nsMouseEvent::eRightButton:
          pluginEvent.event = WM_RBUTTONDBLCLK;
          break;
        default:
          break;
      }
      break;
    case NS_MOUSE_MOVE:
      pluginEvent.event = WM_MOUSEMOVE;
      break;
    default:
      pluginEvent.event = WM_NULL;
      break;
  }

  pluginEvent.wParam = wParam;     // plugins NEED raw OS event flags!
  pluginEvent.lParam = lParam;

  event.nativeMsg = (void *)&pluginEvent;

  // call the event callback
  if (nsnull != mEventCallback) {
    if (nsToolkit::gMouseTrailer)
      nsToolkit::gMouseTrailer->Disable();
    if (aEventType == NS_MOUSE_MOVE) {
      if (nsToolkit::gMouseTrailer && !mIsInMouseCapture) {
        nsToolkit::gMouseTrailer->SetMouseTrailerWindow(mWnd);
      }
      nsRect rect;
      GetBounds(rect);
      rect.x = 0;
      rect.y = 0;

      if (rect.Contains(event.refPoint)) {
        if (gCurrentWindow == NULL || gCurrentWindow != this) {
          if ((nsnull != gCurrentWindow) && (!gCurrentWindow->mIsDestroying)) {
            LPARAM pos = gCurrentWindow->lParamToClient(lParamToScreen(lParam));
            gCurrentWindow->DispatchMouseEvent(NS_MOUSE_EXIT, wParam, pos);
          }
          gCurrentWindow = this;
          if (!mIsDestroying) {
            LPARAM pos = gCurrentWindow->lParamToClient(lParamToScreen(lParam));
            gCurrentWindow->DispatchMouseEvent(NS_MOUSE_ENTER, wParam, pos);
          }
        }
      }
    } else if (aEventType == NS_MOUSE_EXIT) {
      if (gCurrentWindow == this) {
        gCurrentWindow = nsnull;
      }
    }

    result = DispatchWindowEvent(&event);

    if (nsToolkit::gMouseTrailer)
      nsToolkit::gMouseTrailer->Enable();

    // Release the widget with NS_IF_RELEASE() just in case
    // the context menu key code in nsEventListenerManager::HandleEvent()
    // released it already.
    NS_IF_RELEASE(event.widget);
    return result;
  }

  if (nsnull != mMouseListener) {
    switch (aEventType) {
      case NS_MOUSE_MOVE:
      {
        result = ConvertStatus(mMouseListener->MouseMoved(event));
        nsRect rect;
        GetBounds(rect);
        if (rect.Contains(event.refPoint)) {
          if (gCurrentWindow == NULL || gCurrentWindow != this) {
            gCurrentWindow = this;
          }
        }
      }
      break;

      case NS_MOUSE_BUTTON_DOWN:
        result = ConvertStatus(mMouseListener->MousePressed(event));
        break;

      case NS_MOUSE_BUTTON_UP:
        result = ConvertStatus(mMouseListener->MouseReleased(event));
        result = ConvertStatus(mMouseListener->MouseClicked(event));
        break;
    } // switch
  }

  NS_RELEASE(event.widget);
  return result;
}

//-------------------------------------------------------------------------
//
// Deal with accessibile event
//
//-------------------------------------------------------------------------
#ifdef ACCESSIBILITY
PRBool nsWindow::DispatchAccessibleEvent(PRUint32 aEventType, nsIAccessible** aAcc, nsPoint* aPoint)
{
  PRBool result = PR_FALSE;

  if (nsnull == mEventCallback) {
    return result;
  }

  *aAcc = nsnull;

  nsAccessibleEvent event(PR_TRUE, aEventType, this);
  InitEvent(event, aPoint);

  event.isShift   = IS_VK_DOWN(NS_VK_SHIFT);
  event.isControl = IS_VK_DOWN(NS_VK_CONTROL);
  event.isMeta    = PR_FALSE;
  event.isAlt     = IS_VK_DOWN(NS_VK_ALT);
  event.accessible = nsnull;

  result = DispatchWindowEvent(&event);

  // if the event returned an accesssible get it.
  if (event.accessible)
    *aAcc = event.accessible;

  NS_RELEASE(event.widget);

  return result;
}
#endif

//-------------------------------------------------------------------------
//
// Deal with focus messages
//
//-------------------------------------------------------------------------
PRBool nsWindow::DispatchFocus(PRUint32 aEventType, PRBool isMozWindowTakingFocus)
{
  // call the event callback
  if (mEventCallback) {
    nsFocusEvent event(PR_TRUE, aEventType, this);
    InitEvent(event);

    //focus and blur event should go to their base widget loc, not current mouse pos
    event.refPoint.x = 0;
    event.refPoint.y = 0;

    event.isMozWindowTakingFocus = isMozWindowTakingFocus;

    nsPluginEvent pluginEvent;

    switch (aEventType)//~~~
    {
      case NS_GOTFOCUS:
        pluginEvent.event = WM_SETFOCUS;
        break;
      case NS_LOSTFOCUS:
        pluginEvent.event = WM_KILLFOCUS;
        break;
      case NS_PLUGIN_ACTIVATE:
        pluginEvent.event = WM_KILLFOCUS;
        break;
      default:
        break;
    }

    event.nativeMsg = (void *)&pluginEvent;

    PRBool result = DispatchWindowEvent(&event);

    NS_RELEASE(event.widget);

    return result;
  }
  return PR_FALSE;
}


//-------------------------------------------------------------------------
//
// Deal with scrollbar messages (actually implemented only in nsScrollbar)
//
//-------------------------------------------------------------------------
PRBool nsWindow::OnScroll(UINT scrollCode, int cPos)
{
  return PR_FALSE;
}


//-------------------------------------------------------------------------
//
// Return the brush used to paint the background of this control
//
//-------------------------------------------------------------------------
HBRUSH nsWindow::OnControlColor()
{
  return mBrush;
}

//-------------------------------------------------------------------------
//
// Deal with all sort of mouse event
//
//-------------------------------------------------------------------------
PRBool ChildWindow::DispatchMouseEvent(PRUint32 aEventType, WPARAM wParam, LPARAM lParam,
                                       PRBool aIsContextMenuKey, PRInt16 aButton)
{
  PRBool result = PR_FALSE;

  if (nsnull == mEventCallback && nsnull == mMouseListener) {
    return result;
  }

  switch (aEventType) {
    case NS_MOUSE_BUTTON_DOWN:
      CaptureMouse(PR_TRUE);
      break;

    // NS_MOUSE_MOVE and NS_MOUSE_EXIT are here because we need to make sure capture flag
    // isn't left on after a drag where we wouldn't see a button up message (see bug 324131).
    case NS_MOUSE_BUTTON_UP:
    case NS_MOUSE_MOVE:
    case NS_MOUSE_EXIT:
      if (!(wParam & (MK_LBUTTON | MK_MBUTTON | MK_RBUTTON)) && mIsInMouseCapture)
        CaptureMouse(PR_FALSE);
      break;

    default:
      break;

  } // switch

  return nsWindow::DispatchMouseEvent(aEventType, wParam, lParam,
                                      aIsContextMenuKey, aButton);
}

//-------------------------------------------------------------------------
//
// return the style for a child nsWindow
//
//-------------------------------------------------------------------------
DWORD ChildWindow::WindowStyle()
{
  return WS_CHILD | WS_CLIPCHILDREN | nsWindow::WindowStyle();
}

NS_METHOD nsWindow::SetTitle(const nsAString& aTitle)
{
  const nsString& strTitle = PromiseFlatString(aTitle);
  ::SendMessageW(mWnd, WM_SETTEXT, (WPARAM)0, (LPARAM)(LPCWSTR)strTitle.get());
  return NS_OK;
}

NS_METHOD nsWindow::SetIcon(const nsAString& aIconSpec) 
{
  // Assume the given string is a local identifier for an icon file.

  nsCOMPtr<nsILocalFile> iconFile;
  ResolveIconName(aIconSpec, NS_LITERAL_STRING(".ico"),
                  getter_AddRefs(iconFile));
  if (!iconFile)
    return NS_OK; // not an error if icon is not found

  nsAutoString iconPath;
  iconFile->GetPath(iconPath);

  // XXX this should use MZLU (see bug 239279)

  ::SetLastError(0);

  HICON bigIcon = (HICON)::LoadImageW(NULL,
                                      (LPCWSTR)iconPath.get(),
                                      IMAGE_ICON,
                                      ::GetSystemMetrics(SM_CXICON),
                                      ::GetSystemMetrics(SM_CYICON),
                                      LR_LOADFROMFILE );
  HICON smallIcon = (HICON)::LoadImageW(NULL,
                                        (LPCWSTR)iconPath.get(),
                                        IMAGE_ICON,
                                        ::GetSystemMetrics(SM_CXSMICON),
                                        ::GetSystemMetrics(SM_CYSMICON),
                                        LR_LOADFROMFILE );

  if (bigIcon) {
    HICON icon = (HICON) ::SendMessageW(mWnd, WM_SETICON, (WPARAM)ICON_BIG, (LPARAM)bigIcon);
    if (icon)
      ::DestroyIcon(icon);
  }
#ifdef DEBUG_SetIcon
  else {
    NS_LossyConvertUTF16toASCII cPath(iconPath);
    printf( "\nIcon load error; icon=%s, rc=0x%08X\n\n", cPath.get(), ::GetLastError() );
  }
#endif
  if (smallIcon) {
    HICON icon = (HICON) ::SendMessageW(mWnd, WM_SETICON, (WPARAM)ICON_SMALL, (LPARAM)smallIcon);
    if (icon)
      ::DestroyIcon(icon);
  }
#ifdef DEBUG_SetIcon
  else {
    NS_LossyConvertUTF16toASCII cPath(iconPath);
    printf( "\nSmall icon load error; icon=%s, rc=0x%08X\n\n", cPath.get(), ::GetLastError() );
  }
#endif

  return NS_OK;
}


PRBool nsWindow::AutoErase()
{
  return PR_FALSE;
}

NS_METHOD nsWindow::GetPreferredSize(PRInt32& aWidth, PRInt32& aHeight)
{
  aWidth  = mPreferredWidth;
  aHeight = mPreferredHeight;
  return NS_ERROR_FAILURE;
}

NS_METHOD nsWindow::SetPreferredSize(PRInt32 aWidth, PRInt32 aHeight)
{
  mPreferredWidth  = aWidth;
  mPreferredHeight = aHeight;
  return NS_OK;
}

#define ZH_CN_INTELLEGENT_ABC_IME ((HKL)0xe0040804L)
#define ZH_CN_MS_PINYIN_IME_3_0 ((HKL)0xe00e0804L)
#define ZH_CN_NEIMA_IME ((HKL)0xe0050804L)
#define PINYIN_IME_ON_XP(kl) ((nsToolkit::mIsWinXP) && \
                              (ZH_CN_MS_PINYIN_IME_3_0 == (kl)))
PRBool gPinYinIMECaretCreated = PR_FALSE;

void
nsWindow::HandleTextEvent(HIMC hIMEContext,PRBool aCheckAttr)
{
  NS_ASSERTION(sIMECompUnicode, "sIMECompUnicode is null");
  NS_ASSERTION(sIMEIsComposing, "conflict state");

  if (!sIMECompUnicode)
    return;

  nsTextEvent event(PR_TRUE, NS_TEXT_TEXT, this);
  nsPoint point(0, 0);

  InitEvent(event, &point);

  if (aCheckAttr) {
    GetTextRangeList(&(event.rangeCount),&(event.rangeArray));
  } else {
    event.rangeCount = 0;
    event.rangeArray = nsnull;
  }

  event.theText = sIMECompUnicode->get();
  event.isShift = mIsShiftDown;
  event.isControl = mIsControlDown;
  event.isMeta = PR_FALSE;
  event.isAlt = mIsAltDown;

  DispatchWindowEvent(&event);
  NS_RELEASE(event.widget);

  if (event.rangeArray)
    delete [] event.rangeArray;

  //
  // Post process event
  //
  if (event.theReply.mCursorPosition.width || event.theReply.mCursorPosition.height)
  {
    nsRect cursorPosition;
    ResolveIMECaretPos(this, event.theReply.mCursorPosition, cursorPosition);
    CANDIDATEFORM candForm;
    candForm.dwIndex = 0;
    candForm.dwStyle = CFS_EXCLUDE;
    candForm.ptCurrentPos.x = cursorPosition.x;
    candForm.ptCurrentPos.y = cursorPosition.y;
    candForm.rcArea.right = candForm.rcArea.left = candForm.ptCurrentPos.x;
    candForm.rcArea.top = candForm.ptCurrentPos.y;
    candForm.rcArea.bottom = candForm.ptCurrentPos.y +
                             cursorPosition.height;

    if (gPinYinIMECaretCreated)
    {
      SetCaretPos(candForm.ptCurrentPos.x, candForm.ptCurrentPos.y);
    }

    ::ImmSetCandidateWindow(hIMEContext, &candForm);

    // somehow the "Intellegent ABC IME" in Simplified Chinese
    // window listen to the caret position to decide where to put the
    // candidate window
    if (gKeyboardLayout == ZH_CN_INTELLEGENT_ABC_IME)
    {
      CreateCaret(mWnd, nsnull, 1, 1);
      SetCaretPos(candForm.ptCurrentPos.x, candForm.ptCurrentPos.y);
      DestroyCaret();
    }

    // Record previous composing char position
    // The cursor is always on the right char before it, but not necessarily on the
    // left of next char, as what happens in wrapping.
    if (sIMECursorPosition && sIMECompCharPos &&
        sIMECursorPosition < IME_MAX_CHAR_POS) {
      sIMECompCharPos[sIMECursorPosition-1].right = cursorPosition.x;
      sIMECompCharPos[sIMECursorPosition-1].top = cursorPosition.y;
      sIMECompCharPos[sIMECursorPosition-1].bottom = cursorPosition.YMost();
      if (sIMECompCharPos[sIMECursorPosition-1].top != cursorPosition.y) {
        // wrapping, invalidate left position
        sIMECompCharPos[sIMECursorPosition-1].left = -1;
      }
      sIMECompCharPos[sIMECursorPosition].left = cursorPosition.x;
      sIMECompCharPos[sIMECursorPosition].top = cursorPosition.y;
      sIMECompCharPos[sIMECursorPosition].bottom = cursorPosition.YMost();
    }
    sIMECaretHeight = cursorPosition.height;
  } else {
    // for some reason we don't know yet, theReply may contain invalid result
    // need more debugging in nsCaret to find out the reason
    // the best we can do now is to ignore the invalid result
  }
}

BOOL
nsWindow::HandleStartComposition(HIMC hIMEContext)
{
  // ATOK send the messages following order at starting composition.
  // 1. WM_IME_COMPOSITION
  // 2. WM_IME_STARTCOMPOSITION
  // We call this function at both step #1 and #2.
  // However, the composition start event should occur only once.
  if (sIMEIsComposing)
    return PR_TRUE;

  if (sIMEReconvertUnicode) {
    nsMemory::Free(sIMEReconvertUnicode);
    sIMEReconvertUnicode = NULL;
  }

  nsCompositionEvent event(PR_TRUE, NS_COMPOSITION_START, this);
  nsPoint point(0, 0);
  CANDIDATEFORM candForm;

  InitEvent(event, &point);
  DispatchWindowEvent(&event);

  //
  // Post process event
  //
  if (event.theReply.mCursorPosition.width || event.theReply.mCursorPosition.height)
  {
    nsRect cursorPosition;
    ResolveIMECaretPos(this, event.theReply.mCursorPosition, cursorPosition);
    candForm.dwIndex = 0;
    candForm.dwStyle = CFS_CANDIDATEPOS;
    candForm.ptCurrentPos.x = cursorPosition.x + IME_X_OFFSET;
    candForm.ptCurrentPos.y = cursorPosition.y + IME_Y_OFFSET +
                              cursorPosition.height;
    candForm.rcArea.right = 0;
    candForm.rcArea.left = 0;
    candForm.rcArea.top = 0;
    candForm.rcArea.bottom = 0;
#ifdef DEBUG_IME2
    printf("Candidate window position: x=%d, y=%d\n", candForm.ptCurrentPos.x, candForm.ptCurrentPos.y);
#endif

    if (!gPinYinIMECaretCreated && PINYIN_IME_ON_XP(gKeyboardLayout))
    {
      gPinYinIMECaretCreated = CreateCaret(mWnd, nsnull, 1, 1);
      SetCaretPos(candForm.ptCurrentPos.x, candForm.ptCurrentPos.y);
    }

    ::ImmSetCandidateWindow(hIMEContext, &candForm);

    sIMECompCharPos = (RECT*)PR_MALLOC(IME_MAX_CHAR_POS*sizeof(RECT));
    if (sIMECompCharPos) {
      memset(sIMECompCharPos, -1, sizeof(RECT)*IME_MAX_CHAR_POS);
      sIMECompCharPos[0].left = cursorPosition.x;
      sIMECompCharPos[0].top = cursorPosition.y;
      sIMECompCharPos[0].bottom = cursorPosition.YMost();
    }
    sIMECaretHeight = cursorPosition.height;
  } else {
    // for some reason we don't know yet, theReply may contain invalid result
    // need more debugging in nsCaret to find out the reason
    // the best we can do now is to ignore the invalid result
  }

  NS_RELEASE(event.widget);

  if (!sIMECompUnicode)
    sIMECompUnicode = new nsAutoString();
  sIMEIsComposing = PR_TRUE;

  return PR_TRUE;
}

void
nsWindow::HandleEndComposition(void)
{
  if (!sIMEIsComposing)
    return;

  nsCompositionEvent event(PR_TRUE, NS_COMPOSITION_END, this);
  nsPoint point(0, 0);

  if (gPinYinIMECaretCreated)
  {
    DestroyCaret();
    gPinYinIMECaretCreated = PR_FALSE;
  }

  InitEvent(event,&point);
  DispatchWindowEvent(&event);
  NS_RELEASE(event.widget);
  PR_FREEIF(sIMECompCharPos);
  sIMECompCharPos = nsnull;
  sIMECaretHeight = 0;
  sIMEIsComposing = PR_FALSE;
}

static PRUint32 PlatformToNSAttr(PRUint8 aAttr)
{
  switch (aAttr)
  {
    case ATTR_INPUT_ERROR:
    // case ATTR_FIXEDCONVERTED:
    case ATTR_INPUT:
      return NS_TEXTRANGE_RAWINPUT;
    case ATTR_CONVERTED:
      return NS_TEXTRANGE_CONVERTEDTEXT;
    case ATTR_TARGET_NOTCONVERTED:
      return NS_TEXTRANGE_SELECTEDRAWTEXT;
    case ATTR_TARGET_CONVERTED:
      return NS_TEXTRANGE_SELECTEDCONVERTEDTEXT;
    default:
      NS_ASSERTION(PR_FALSE, "unknown attribute");
      return NS_TEXTRANGE_CARETPOSITION;
  }
}
//
// This function converts the composition string (CGS_COMPSTR) into Unicode while mapping the
//  attribute (GCS_ATTR) string t
void
nsWindow::GetTextRangeList(PRUint32* textRangeListLengthResult,nsTextRangeArray* textRangeListResult)
{
  NS_ASSERTION(sIMECompUnicode, "sIMECompUnicode is null");

  if (!sIMECompUnicode)
    return;

  long maxlen = sIMECompUnicode->Length();
  long cursor = sIMECursorPosition;
  NS_ASSERTION(cursor <= maxlen, "wrong cursor positoin");
  if (cursor > maxlen)
    cursor = maxlen;

  //
  // figure out the ranges from the compclause string
  //
  if (sIMECompClauseArrayLength == 0) {
    *textRangeListLengthResult = 2;
    *textRangeListResult = new nsTextRange[2];
    (*textRangeListResult)[0].mStartOffset = 0;
    (*textRangeListResult)[0].mEndOffset = sIMECompUnicode->Length();
    (*textRangeListResult)[0].mRangeType = NS_TEXTRANGE_RAWINPUT;
    (*textRangeListResult)[1].mStartOffset = cursor;
    (*textRangeListResult)[1].mEndOffset = cursor;
    (*textRangeListResult)[1].mRangeType = NS_TEXTRANGE_CARETPOSITION;
  } else {
    *textRangeListLengthResult = sIMECompClauseArrayLength;

    //
    //  allocate the offset array
    //
    *textRangeListResult = new nsTextRange[*textRangeListLengthResult];

    //
    // figure out the cursor position
    //
    (*textRangeListResult)[0].mStartOffset = cursor;
    (*textRangeListResult)[0].mEndOffset = cursor;
    (*textRangeListResult)[0].mRangeType = NS_TEXTRANGE_CARETPOSITION;

    //
    // iterate over the attributes and convert them into unicode 
    //
    int lastOffset = 0;
    for(int i = 1; i < sIMECompClauseArrayLength; i++) {
      long current = sIMECompClauseArray[i];
      NS_ASSERTION(current <= maxlen, "wrong offset");
      if(current > maxlen)
        current = maxlen;

      (*textRangeListResult)[i].mRangeType = 
        PlatformToNSAttr(sIMEAttributeArray[lastOffset]);
      (*textRangeListResult)[i].mStartOffset = lastOffset;
      (*textRangeListResult)[i].mEndOffset = current;

      lastOffset = current;
    } // for
  } // if else
}


//==========================================================================
BOOL nsWindow::OnInputLangChange(HKL aHKL, LRESULT *oRetValue)
{
#ifdef KE_DEBUG
  printf("OnInputLanguageChange\n");
#endif

  if (gKeyboardLayout != aHKL)
  {
    gKeyboardLayout = aHKL;

    gKbdLayout.LoadLayout();
  }

  ResetInputState();

  if (sIMEIsComposing) {
    HandleEndComposition();
  }

  return PR_FALSE;   // always pass to child window
}
//==========================================================================
BOOL nsWindow::OnIMEChar(BYTE aByte1, BYTE aByte2, LPARAM aKeyState)
{
#ifdef DEBUG_IME
  printf("OnIMEChar\n");
#endif
  wchar_t uniChar;
  int err = 0;

  uniChar = MAKEWORD(aByte2, aByte1);

#ifdef DEBUG_IME
  if (!err) {
    DWORD lastError = ::GetLastError();
    switch (lastError)
    {
      case ERROR_INSUFFICIENT_BUFFER:
        printf("ERROR_INSUFFICIENT_BUFFER\n");
        break;

      case ERROR_INVALID_FLAGS:
        printf("ERROR_INVALID_FLAGS\n");
        break;

      case ERROR_INVALID_PARAMETER:
        printf("ERROR_INVALID_PARAMETER\n");
        break;

      case ERROR_NO_UNICODE_TRANSLATION:
        printf("ERROR_NO_UNICODE_TRANSLATION\n");
        break;
    }
  }
#endif

  // We need to return TRUE here so that Windows doesn't
  // send two WM_CHAR msgs
  DispatchKeyEvent(NS_KEY_PRESS, uniChar, 0, 0);
  return PR_TRUE;
}

//==========================================================================
// This function is used when aIndex is GCS_COMPSTR, GCS_COMPREADSTR,
// GCS_RESULTSTR, and GCS_RESULTREADSTR.
// Otherwise use ::ImmGetCompositionStringW.
void nsWindow::GetCompositionString(HIMC aHIMC, DWORD aIndex, nsString* aStrUnicode)
{
  long lRtn;
  lRtn = ::ImmGetCompositionStringW(aHIMC, aIndex, NULL, 0);
  if (!EnsureStringLength(*aStrUnicode, (lRtn / sizeof(WCHAR)) + 1))
    return; // out of memory

  long buflen = lRtn + sizeof(WCHAR);
  lRtn = ::ImmGetCompositionStringW(aHIMC, aIndex, (LPVOID)aStrUnicode->BeginWriting(), buflen);
  lRtn = lRtn / sizeof(WCHAR);
  aStrUnicode->SetLength(lRtn);
}

//==========================================================================
BOOL nsWindow::OnIMEComposition(LPARAM aGCS)
{
#ifdef DEBUG_IME
  printf("OnIMEComposition\n");
#endif
  // for bug #60050
  // MS-IME 95/97/98/2000 may send WM_IME_COMPOSITION with non-conversion
  // mode before it send WM_IME_STARTCOMPOSITION.
  if (!sIMECompUnicode)
    sIMECompUnicode = new nsAutoString();

  NS_ASSERTION(sIMECompUnicode, "sIMECompUnicode is null");
  if (!sIMECompUnicode)
    return PR_TRUE;

  HIMC hIMEContext = ::ImmGetContext(mWnd);
  if (hIMEContext==NULL) 
    return PR_TRUE;

  // will change this if an IME message we handle
  BOOL result = PR_FALSE;

  PRBool startCompositionMessageHasBeenSent = sIMEIsComposing;

  //
  // This catches a fixed result
  //
  if (aGCS & GCS_RESULTSTR) {
#ifdef DEBUG_IME
    printf("Handling GCS_RESULTSTR\n");
#endif
    if (!sIMEIsComposing) 
      HandleStartComposition(hIMEContext);

    GetCompositionString(hIMEContext, GCS_RESULTSTR, sIMECompUnicode);
#ifdef DEBUG_IME
    printf("GCS_RESULTSTR compStrLen = %d\n", sIMECompUnicode->Length());
#endif
    result = PR_TRUE;
    HandleTextEvent(hIMEContext, PR_FALSE);
    HandleEndComposition();
  }


  //
  // This provides us with a composition string
  //
  if (aGCS & (GCS_COMPSTR | GCS_COMPATTR | GCS_COMPCLAUSE | GCS_CURSORPOS))
  {
#ifdef DEBUG_IME
    printf("Handling GCS_COMPSTR\n");
#endif

    if (!sIMEIsComposing) 
      HandleStartComposition(hIMEContext);

    //--------------------------------------------------------
    // 1. Get GCS_COMPSTR
    //--------------------------------------------------------
    GetCompositionString(hIMEContext, GCS_COMPSTR, sIMECompUnicode);

    // See https://bugzilla.mozilla.org/show_bug.cgi?id=296339
    if (sIMECompUnicode->IsEmpty() &&
        !startCompositionMessageHasBeenSent) {
      // In this case, maybe, the sender is MSPinYin. That sends *only*
      // WM_IME_COMPOSITION with GCS_COMP* and GCS_RESULT* when
      // user inputted the Chinese full stop. So, that doesn't send
      // WM_IME_STARTCOMPOSITION and WM_IME_ENDCOMPOSITION.
      // If WM_IME_STARTCOMPOSITION was not sent and the composition
      // string is null (it indicates the composition transaction ended),
      // WM_IME_ENDCOMPOSITION may not be sent. If so, we cannot run
      // HandleEndComposition() in other place.
#ifdef DEBUG_IME
      printf("Aborting GCS_COMPSTR\n");
#endif
      HandleEndComposition();
      return result;
    }

#ifdef DEBUG_IME
    printf("GCS_COMPSTR compStrLen = %d\n", sIMECompUnicode->Length());
#endif

    //--------------------------------------------------------
    // 2. Get GCS_COMPCLAUSE
    //--------------------------------------------------------
    long compClauseLen, compClauseLen2;
    compClauseLen = ::ImmGetCompositionStringW(hIMEContext, GCS_COMPCLAUSE, NULL, 0);
#ifdef DEBUG_IME
    printf("GCS_COMPCLAUSE compClauseLen = %d\n", compClauseLen);
#endif
    compClauseLen = compClauseLen / sizeof(PRUint32);

    if (compClauseLen > sIMECompClauseArraySize) {
      if (sIMECompClauseArray) 
        delete [] sIMECompClauseArray;
      // Allocate some extra space to avoid reallocations.
      sIMECompClauseArray = new PRUint32[compClauseLen + 32];
      sIMECompClauseArraySize = compClauseLen + 32;
    }

    compClauseLen2 = ::ImmGetCompositionStringW(hIMEContext, GCS_COMPCLAUSE, sIMECompClauseArray,
      sIMECompClauseArraySize * sizeof(PRUint32));

    compClauseLen2 = compClauseLen2 / sizeof(PRUint32);
    NS_ASSERTION(compClauseLen2 == compClauseLen, "strange result");
    if (compClauseLen > compClauseLen2)
      compClauseLen = compClauseLen2;
    sIMECompClauseArrayLength = compClauseLen;

    //--------------------------------------------------------
    // 3. Get GCS_COMPATTR
    //--------------------------------------------------------
    // This provides us with the attribute string necessary 
    // for doing hiliting
    long attrStrLen;
    attrStrLen = ::ImmGetCompositionStringW(hIMEContext, GCS_COMPATTR, NULL, 0);
#ifdef DEBUG_IME
    printf("GCS_COMPATTR attrStrLen = %d\n", attrStrLen);
#endif
    if (attrStrLen > sIMEAttributeArraySize) {
      if (sIMEAttributeArray) 
        delete [] sIMEAttributeArray;
      // Allocate some extra space to avoid reallocations.
      sIMEAttributeArray = new PRUint8[attrStrLen + 64];
      sIMEAttributeArraySize = attrStrLen + 64;
    }
    attrStrLen = ::ImmGetCompositionStringW(hIMEContext, GCS_COMPATTR, sIMEAttributeArray, sIMEAttributeArraySize);

    sIMEAttributeArrayLength = attrStrLen;

    //--------------------------------------------------------
    // 4. Get GCS_CURSOPOS
    //--------------------------------------------------------
    sIMECursorPosition = ::ImmGetCompositionStringW(hIMEContext, GCS_CURSORPOS, NULL, 0);

    NS_ASSERTION(sIMECursorPosition <= sIMECompUnicode->Length(), "illegal pos");

#ifdef DEBUG_IME
    printf("sIMECursorPosition(Unicode): %d\n", sIMECursorPosition);
#endif
    //--------------------------------------------------------
    // 5. Send the text event
    //--------------------------------------------------------
    HandleTextEvent(hIMEContext);
    result = PR_TRUE;
  }
  if (!result) {
#ifdef DEBUG_IME
    fprintf(stderr,"Haandle 0 length TextEvent. \n");
#endif
    if (!sIMEIsComposing) 
      HandleStartComposition(hIMEContext);

    sIMECompUnicode->Truncate();
    HandleTextEvent(hIMEContext, PR_FALSE);
    result = PR_TRUE;
  }

  ::ImmReleaseContext(mWnd, hIMEContext);
  return result;
}
//==========================================================================
BOOL nsWindow::OnIMECompositionFull()
{
#ifdef DEBUG_IME2
  printf("OnIMECompositionFull\n");
#endif

  // not implement yet
  return PR_FALSE;
}
//==========================================================================
BOOL nsWindow::OnIMEEndComposition()
{
#ifdef DEBUG_IME
  printf("OnIMEEndComposition\n");
#endif
  if (sIMEIsComposing) {
    HIMC hIMEContext;

    if (sIMEProperty & (IME_PROP_SPECIAL_UI | IME_PROP_AT_CARET)) 
      return PR_FALSE;

    hIMEContext = ::ImmGetContext(mWnd);
    if (hIMEContext==NULL) 
      return PR_TRUE;

    // IME on Korean NT somehow send WM_IME_ENDCOMPOSITION
    // first when we hit space in composition mode
    // we need to clear out the current composition string
    // in that case.
    sIMECompUnicode->Truncate(0);

    HandleTextEvent(hIMEContext, PR_FALSE);

    HandleEndComposition();
    ::ImmReleaseContext(mWnd, hIMEContext);
  }
  return PR_TRUE;
}
//==========================================================================
BOOL nsWindow::OnIMENotify(WPARAM aIMN, LPARAM aData, LRESULT *oResult)
{
#ifdef DEBUG_IME2
  printf("OnIMENotify ");
  switch (aIMN) {
    case IMN_CHANGECANDIDATE:
      printf("IMN_CHANGECANDIDATE %x\n", aData);
      break;
    case IMN_CLOSECANDIDATE:
      printf("IMN_CLOSECANDIDATE %x\n", aData);
      break;
    case IMN_CLOSESTATUSWINDOW:
      printf("IMN_CLOSESTATUSWINDOW\n");
      break;
    case IMN_GUIDELINE:
      printf("IMN_GUIDELINE\n");
      break;
    case IMN_OPENCANDIDATE:
      printf("IMN_OPENCANDIDATE %x\n", aData);
      break;
    case IMN_OPENSTATUSWINDOW:
      printf("IMN_OPENSTATUSWINDOW\n");
      break;
    case IMN_SETCANDIDATEPOS:
      printf("IMN_SETCANDIDATEPOS %x\n", aData);
      break;
    case IMN_SETCOMPOSITIONFONT:
      printf("IMN_SETCOMPOSITIONFONT\n");
      break;
    case IMN_SETCOMPOSITIONWINDOW:
      printf("IMN_SETCOMPOSITIONWINDOW\n");
      break;
    case IMN_SETCONVERSIONMODE:
      printf("IMN_SETCONVERSIONMODE\n");
      break;
    case IMN_SETOPENSTATUS:
      printf("IMN_SETOPENSTATUS\n");
      break;
    case IMN_SETSENTENCEMODE:
      printf("IMN_SETSENTENCEMODE\n");
      break;
    case IMN_SETSTATUSWINDOWPOS:
      printf("IMN_SETSTATUSWINDOWPOS\n");
      break;
    case IMN_PRIVATE:
      printf("IMN_PRIVATE\n");
      break;
  };
#endif

  // add hacky code here
  if (IS_VK_DOWN(NS_VK_ALT)) {
    mIsShiftDown = PR_FALSE;
    mIsControlDown = PR_FALSE;
    mIsAltDown = PR_TRUE;

    DispatchKeyEvent(NS_KEY_PRESS, 0, 192, 0); // XXX hack hack hack
    if (aIMN == IMN_SETOPENSTATUS)
      sIMEIsStatusChanged = PR_TRUE;
  }
  // not implemented yet
  return PR_FALSE;
}
//==========================================================================
BOOL nsWindow::OnIMERequest(WPARAM aIMR, LPARAM aData, LRESULT *oResult)
{
#ifdef DEBUG_IME
  printf("OnIMERequest\n");
#endif
  PRBool result = PR_FALSE;

  switch (aIMR) {
    case IMR_RECONVERTSTRING:
      result = OnIMEReconvert(aData, oResult);
      break;
    case IMR_QUERYCHARPOSITION:
      result = OnIMEQueryCharPosition(aData, oResult);
      break;
  }

  return result;
}

//==========================================================================
PRBool nsWindow::OnIMEReconvert(LPARAM aData, LRESULT *oResult)
{
#ifdef DEBUG_IME
  printf("OnIMEReconvert\n");
#endif

  PRBool           result  = PR_FALSE;
  RECONVERTSTRING* pReconv = (RECONVERTSTRING*) aData;
  int              len = 0;

  if (!pReconv) {

    //
    // When reconvert, it must return need size to reconvert.
    //
    if (sIMEReconvertUnicode) {
      nsMemory::Free(sIMEReconvertUnicode);
      sIMEReconvertUnicode = NULL;
    }

    // Get reconversion string
    nsReconversionEvent event(PR_TRUE, NS_RECONVERSION_QUERY, this);
    nsPoint point(0, 0);

    InitEvent(event, &point);
    event.theReply.mReconversionString = NULL;
    DispatchWindowEvent(&event);

    sIMEReconvertUnicode = event.theReply.mReconversionString;
    NS_RELEASE(event.widget);

    // Return need size

    if (sIMEReconvertUnicode) {
      len = nsCRT::strlen(sIMEReconvertUnicode);
      *oResult = sizeof(RECONVERTSTRING) + len * sizeof(WCHAR);

      result = PR_TRUE;
    }
  } else {

    //
    // Fill reconvert struct
    //

    len = nsCRT::strlen(sIMEReconvertUnicode);
    *oResult = sizeof(RECONVERTSTRING) + len * sizeof(WCHAR);

    if (pReconv->dwSize < *oResult) {
      *oResult = 0;
      return PR_FALSE;
    }

    DWORD tmpSize = pReconv->dwSize;
    ::ZeroMemory(pReconv, tmpSize);
    pReconv->dwSize            = tmpSize;
    pReconv->dwVersion         = 0;
    pReconv->dwStrLen          = len;
    pReconv->dwStrOffset       = sizeof(RECONVERTSTRING);
    pReconv->dwCompStrLen      = len;
    pReconv->dwCompStrOffset   = 0;
    pReconv->dwTargetStrLen    = len;
    pReconv->dwTargetStrOffset = 0;

    ::CopyMemory((LPVOID) (aData + sizeof(RECONVERTSTRING)),
                 sIMEReconvertUnicode, len * sizeof(WCHAR));

    result = PR_TRUE;
  }

  return result;
}

//==========================================================================
PRBool nsWindow::OnIMEQueryCharPosition(LPARAM aData, LRESULT *oResult)
{
#ifdef DEBUG_IME
  printf("OnIMEQueryCharPosition\n");
#endif
  IMECHARPOSITION* pCharPosition = (IMECHARPOSITION*)aData;
  if (!pCharPosition ||
      pCharPosition->dwSize < sizeof(IMECHARPOSITION) ||
      ::GetFocus() != mWnd) {
    *oResult = FALSE;
    return PR_FALSE;
  }

  if (!sIMEIsComposing) {  // Including |!sIMECompUnicode| and |!sIMECompUnicode->IsEmpty|.
    if (pCharPosition->dwCharPos != 0) {
      *oResult = FALSE;
      return PR_FALSE;
    }
    nsPoint point(0, 0);
    nsQueryCaretRectEvent event(PR_TRUE, NS_QUERYCARETRECT, this);
    InitEvent(event, &point);
    DispatchWindowEvent(&event);
    // The active widget doesn't support this event.
    if (!event.theReply.mRectIsValid) {
      *oResult = FALSE;
      return PR_FALSE;
    }
    NS_RELEASE(event.widget);

    nsRect screenRect;
    ResolveIMECaretPos(nsnull, event.theReply.mCaretRect, screenRect);
    pCharPosition->pt.x = screenRect.x;
    pCharPosition->pt.y = screenRect.y;

    pCharPosition->cLineHeight = event.theReply.mCaretRect.height;

    ::GetWindowRect(mWnd, &pCharPosition->rcDocument);

    *oResult = TRUE;
    return PR_TRUE;
  }

  // If the char positions are not cached, we should not return the values by LPARAM.
  // Because in this case, the active widget is not editor.
  if (!sIMECompCharPos) {
    *oResult = FALSE;
    return PR_FALSE;
  }

  long charPosition;
  if (pCharPosition->dwCharPos > sIMECompUnicode->Length()) {
    *oResult = FALSE;
    return PR_FALSE;
  }
  charPosition = pCharPosition->dwCharPos;

  // We only support insertion at the cursor position or at the leftmost position.
  // Because sIMECompCharPos may be broken by user converting the string.
  // But leftmost position and cursor position is always correctly.
  if ((charPosition != 0 && charPosition != sIMECursorPosition) ||
      charPosition > IME_MAX_CHAR_POS) {
    *oResult = FALSE;
    return PR_FALSE;
  }
  POINT pt;
  pt.x = sIMECompCharPos[charPosition].left;
  pt.y = sIMECompCharPos[charPosition].top;
  ::ClientToScreen(mWnd, &pt);
  pCharPosition->pt = pt;

  pCharPosition->cLineHeight = sIMECaretHeight;

  ::GetWindowRect(mWnd, &pCharPosition->rcDocument);

  *oResult = TRUE;
  return PR_TRUE;
}

//==========================================================================
void
nsWindow::ResolveIMECaretPos(nsWindow* aClient,
                             nsRect&   aEventResult,
                             nsRect&   aResult)
{
  // RootView coordinates -> Screen coordinates
  GetTopLevelWindow()->WidgetToScreen(aEventResult, aResult);
  // if aClient is nsnull, returns screen coordinates
  if (!aClient)
    return;
  // screen coordinates -> client coordinates
  aClient->ScreenToWidget(aResult, aResult);
}

//==========================================================================
BOOL nsWindow::OnIMESelect(BOOL  aSelected, WORD aLangID)
{
#ifdef DEBUG_IME2
  printf("OnIMESelect\n");
#endif

  // not implement yet
  return PR_FALSE;
}
//==========================================================================
BOOL nsWindow::OnIMESetContext(BOOL aActive, LPARAM& aISC)
{
#ifdef DEBUG_IME2
  printf("OnIMESetContext %x %s %s %s Candidate[%s%s%s%s]\n", this,
    (aActive ? "Active" : "Deactiv"),
    ((aISC & ISC_SHOWUICOMPOSITIONWINDOW) ? "[Comp]" : ""),
    ((aISC & ISC_SHOWUIGUIDELINE) ? "[GUID]" : ""),
    ((aISC & ISC_SHOWUICANDIDATEWINDOW) ? "0" : ""),
    ((aISC & (ISC_SHOWUICANDIDATEWINDOW<<1)) ? "1" : ""),
    ((aISC & (ISC_SHOWUICANDIDATEWINDOW<<2)) ? "2" : ""),
    ((aISC & (ISC_SHOWUICANDIDATEWINDOW<<3)) ? "3" : ""));
#endif
  if (! aActive)
    ResetInputState();

  aISC &= ~ISC_SHOWUICOMPOSITIONWINDOW;

  // We still return false here because we need to pass the
  // aISC w/ ISC_SHOWUICOMPOSITIONWINDOW clear to the default
  // window proc so it will draw the candidcate window for us...
  return PR_FALSE;
}
//==========================================================================
BOOL nsWindow::OnIMEStartComposition()
{
#ifdef DEBUG_IME
  printf("OnIMEStartComposition\n");
#endif
  HIMC hIMEContext;

  if (sIMEProperty & (IME_PROP_SPECIAL_UI | IME_PROP_AT_CARET))
    return PR_FALSE;

  hIMEContext = ::ImmGetContext(mWnd);
  if (hIMEContext == NULL)
    return PR_TRUE;

  PRBool rtn = HandleStartComposition(hIMEContext);
  ::ImmReleaseContext(mWnd, hIMEContext);
  return rtn;
}

//==========================================================================
NS_IMETHODIMP nsWindow::ResetInputState()
{
#ifdef DEBUG_KBSTATE
  printf("ResetInputState\n");
#endif
  HIMC hIMC = ::ImmGetContext(mWnd);
  if (hIMC) {
    BOOL ret = FALSE;
    ret = ::ImmNotifyIME(hIMC, NI_COMPOSITIONSTR, CPS_COMPLETE, NULL);
    ret = ::ImmNotifyIME(hIMC, NI_COMPOSITIONSTR, CPS_CANCEL, NULL);
    //NS_ASSERTION(ret, "ImmNotify failed");
    ::ImmReleaseContext(mWnd, hIMC);
  }
  return NS_OK;
}

//==========================================================================
NS_IMETHODIMP nsWindow::SetIMEOpenState(PRBool aState)
{
#ifdef DEBUG_KBSTATE
  printf("SetIMEOpenState %s\n", (aState ? "Open" : "Close"));
#endif 
  HIMC hIMC = ::ImmGetContext(mWnd);
  if (hIMC) {
    ::ImmSetOpenStatus(hIMC, aState ? TRUE : FALSE);
    ::ImmReleaseContext(mWnd, hIMC);
  }
  return NS_OK;
}

//==========================================================================
NS_IMETHODIMP nsWindow::GetIMEOpenState(PRBool* aState)
{
  HIMC hIMC = ::ImmGetContext(mWnd);
  if (hIMC) {
    BOOL isOpen = ::ImmGetOpenStatus(hIMC);
    *aState = isOpen ? PR_TRUE : PR_FALSE;
    ::ImmReleaseContext(mWnd, hIMC);
  } else 
    *aState = PR_FALSE;
  return NS_OK;
}

//==========================================================================
NS_IMETHODIMP nsWindow::SetIMEEnabled(PRBool aState)
{
  if (sIMEIsComposing)
    ResetInputState();
  if (!aState != !mOldIMC)
    return NS_OK;
  mOldIMC = ::ImmAssociateContext(mWnd, aState ? mOldIMC : NULL);
  NS_ASSERTION(!aState || !mOldIMC, "Another IMC was associated");

  return NS_OK;
}

//==========================================================================
NS_IMETHODIMP nsWindow::GetIMEEnabled(PRBool* aState)
{
  *aState = !mOldIMC;
  return NS_OK;
}

//==========================================================================
NS_IMETHODIMP nsWindow::CancelIMEComposition()
{
#ifdef DEBUG_KBSTATE
  printf("CancelIMEComposition\n");
#endif 
  HIMC hIMC = ::ImmGetContext(mWnd);
  if (hIMC) {
    BOOL ret = FALSE;
    ret = ::ImmNotifyIME(hIMC, NI_COMPOSITIONSTR, CPS_CANCEL, NULL);
    ::ImmReleaseContext(mWnd, hIMC);
  }
  return NS_OK;
}


#define PT_IN_RECT(pt, rc)  ((pt).x>(rc).left && (pt).x <(rc).right && (pt).y>(rc).top && (pt).y<(rc).bottom)

// Mouse operation of IME
PRBool
nsWindow::IMEMouseHandling(PRInt32 aAction, LPARAM lParam)
{
#ifndef WINCE
  POINT ptPos;
  ptPos.x = (short)LOWORD(lParam);
  ptPos.y = (short)HIWORD(lParam);

  if (sIMEIsComposing && nsWindow::uWM_MSIME_MOUSE) {
    if (IMECompositionHitTest(&ptPos))
      if (HandleMouseActionOfIME(aAction, &ptPos))
        return PR_TRUE;
  } else {
    HWND parentWnd = ::GetParent(mWnd);
    if (parentWnd) {
      nsWindow* parentWidget = GetNSWindowPtr(parentWnd);
      if (parentWidget && parentWidget->sIMEIsComposing && nsWindow::uWM_MSIME_MOUSE) {
        if (parentWidget->IMECompositionHitTest(&ptPos))
          if (parentWidget->HandleMouseActionOfIME(aAction, &ptPos))
            return PR_TRUE;
      }
    }
  }
#endif
  return PR_FALSE;
}


PRBool
nsWindow::HandleMouseActionOfIME(int aAction, POINT *ptPos)
{
  PRBool IsHandle = PR_FALSE;

  if (mWnd) {
    HIMC hIMC = ::ImmGetContext(mWnd);
    if (hIMC) {
      int positioning = 0;
      int offset = 0;

      // calcurate positioning and offset
      // char :            JCH1|JCH2|JCH3
      // offset:           0011 1122 2233
      // positioning:      2301 2301 2301

      // Note: hitText has been done, so no check of sIMECompCharPos
      // and composing char maximum limit is necessary.
      PRUint32 i = 0;
      for (i = 0; i < sIMECompUnicode->Length(); i++) {
        if (PT_IN_RECT(*ptPos, sIMECompCharPos[i]))
          break;
      }
      offset = i;
      if (ptPos->x - sIMECompCharPos[i].left > sIMECompCharPos[i].right - ptPos->x)
        offset++;

      positioning = (ptPos->x - sIMECompCharPos[i].left) * 4 /
                    (sIMECompCharPos[i].right - sIMECompCharPos[i].left);
      positioning = (positioning + 2) % 4;

      // send MS_MSIME_MOUSE message to default IME window.
      HWND imeWnd = ::ImmGetDefaultIMEWnd(mWnd);
      if (::SendMessageW(imeWnd, nsWindow::uWM_MSIME_MOUSE,
                         MAKELONG(MAKEWORD(aAction, positioning), offset),
                         (LPARAM) hIMC) == 1)
        IsHandle = PR_TRUE;
    }
    ::ImmReleaseContext(mWnd, hIMC);
  }

  return IsHandle;
}

//The coordinate is relative to the upper-left corner of the client area.
PRBool nsWindow::IMECompositionHitTest(POINT * ptPos)
{
  PRBool IsHit = PR_FALSE;

  if (sIMECompCharPos){
    // figure out how many char in composing string,
    // but keep it below the limit we can handle
    PRInt32 len = sIMECompUnicode->Length();
    if (len > IME_MAX_CHAR_POS)
      len = IME_MAX_CHAR_POS;

    PRInt32 i;
    PRInt32 aveWidth = 0;
    // found per char width
    for (i = 0; i < len; i++) {
      if (sIMECompCharPos[i].left >= 0 && sIMECompCharPos[i].right > 0) {
        aveWidth = sIMECompCharPos[i].right - sIMECompCharPos[i].left;
        break;
      }
    }

    // validate each rect and test
    for (i = 0; i < len; i++) {
      if (sIMECompCharPos[i].left < 0) {
        if (i != 0 && sIMECompCharPos[i-1].top == sIMECompCharPos[i].top)
          sIMECompCharPos[i].left = sIMECompCharPos[i-1].right;
        else
          sIMECompCharPos[i].left = sIMECompCharPos[i].right - aveWidth;
      }
      if (sIMECompCharPos[i].right < 0)
        sIMECompCharPos[i].right = sIMECompCharPos[i].left + aveWidth;
      if (sIMECompCharPos[i].top < 0) {
        sIMECompCharPos[i].top = sIMECompCharPos[i-1].top;
        sIMECompCharPos[i].bottom = sIMECompCharPos[i-1].bottom;
      }

      if (PT_IN_RECT(*ptPos, sIMECompCharPos[i])) {
        IsHit = PR_TRUE;
        break;
      }
    }
  }

  return IsHit;
}

void nsWindow::GetCompositionWindowPos(HIMC hIMC, PRUint32 aEventType, COMPOSITIONFORM *cpForm)
{
  nsTextEvent event(PR_TRUE, 0, this);
  POINT point;
  point.x = 0;
  point.y = 0;
  DWORD pos = ::GetMessagePos();

  point.x = GET_X_LPARAM(pos);
  point.y = GET_Y_LPARAM(pos);

  if (mWnd != NULL) {
    ::ScreenToClient(mWnd, &point);
    event.refPoint.x = point.x;
    event.refPoint.y = point.y;
  } else {
    event.refPoint.x = 0;
    event.refPoint.y = 0;
  }

  ::ImmGetCompositionWindow(hIMC, cpForm);

  cpForm->ptCurrentPos.x = event.theReply.mCursorPosition.x + IME_X_OFFSET;
  cpForm->ptCurrentPos.y = event.theReply.mCursorPosition.y + IME_Y_OFFSET +
                           event.theReply.mCursorPosition.height;
  cpForm->rcArea.left = cpForm->ptCurrentPos.x;
  cpForm->rcArea.top = cpForm->ptCurrentPos.y;
  cpForm->rcArea.right = cpForm->ptCurrentPos.x + event.theReply.mCursorPosition.width;
  cpForm->rcArea.bottom = cpForm->ptCurrentPos.y + event.theReply.mCursorPosition.height;
}

// This function is called on a timer to do the flashing.  It simply toggles the flash
// status until the window comes to the foreground.
static VOID CALLBACK nsGetAttentionTimerFunc(HWND hwnd, UINT uMsg, UINT idEvent, DWORD dwTime)
{
  // flash the window until we're in the foreground.
  if (::GetForegroundWindow() != hwnd)
  {
    // flash the outermost owner
    HWND flashwnd = gAttentionTimerMonitor->GetFlashWindowFor(hwnd);

    PRInt32 maxFlashCount = gAttentionTimerMonitor->GetMaxFlashCount(hwnd);
    PRInt32 flashCount = gAttentionTimerMonitor->GetFlashCount(hwnd);
    if (maxFlashCount > 0) {
      // We have a max flash count, if we haven't met it yet, flash again.
      if (flashCount < maxFlashCount) {
        ::FlashWindow(flashwnd, TRUE);
        gAttentionTimerMonitor->IncrementFlashCount(hwnd);
      }
      else
        gAttentionTimerMonitor->KillTimer(hwnd);
    }
    else {
      // The caller didn't specify a flash count.
      ::FlashWindow(flashwnd, TRUE);
    }

    gAttentionTimerMonitor->SetFlashed(hwnd);
  }
  else
    gAttentionTimerMonitor->KillTimer(hwnd);
}

// Draw user's attention to this window until it comes to foreground.
NS_IMETHODIMP
nsWindow::GetAttention(PRInt32 aCycleCount)
{
  // Got window?
  if (!mWnd)
    return NS_ERROR_NOT_INITIALIZED;

  // Don't flash if the flash count is 0.
  if (aCycleCount == 0)
    return NS_OK;

  // timer is on the parentmost window; window to flash is its ownermost
  HWND timerwnd = GetTopLevelHWND(mWnd);
  HWND flashwnd = timerwnd;
  HWND nextwnd;
  while ((nextwnd = ::GetWindow(flashwnd, GW_OWNER)) != 0)
    flashwnd = nextwnd;

  // If window is in foreground, no notification is necessary.
  if (::GetForegroundWindow() != timerwnd) {
    // kick off a timer that does single flash until the window comes to the foreground
    if (!gAttentionTimerMonitor)
      gAttentionTimerMonitor = new nsAttentionTimerMonitor;
    if (gAttentionTimerMonitor) {
      gAttentionTimerMonitor->AddTimer(timerwnd, flashwnd, aCycleCount, NS_FLASH_TIMER_ID);
      ::SetTimer(timerwnd, NS_FLASH_TIMER_ID, GetCaretBlinkTime(), (TIMERPROC)nsGetAttentionTimerFunc);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWindow::GetLastInputEventTime(PRUint32& aTime)
{
  WORD qstatus = HIWORD(GetQueueStatus(QS_INPUT));

  // If there is pending input or the user is currently
  // moving the window then return the current time.
  // Note: When the user is moving the window WIN32 spins
  // a separate event loop and input events are not
  // reported to the application.
  nsToolkit* toolkit = (nsToolkit *)mToolkit;
  if (qstatus || (toolkit && toolkit->UserIsMovingWindow())) {
    gLastInputEventTime = PR_IntervalToMicroseconds(PR_IntervalNow());
  }

  aTime = gLastInputEventTime;

  return NS_OK;
}

//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
//-- NOTE!!! These hook functions can be removed when we migrate to
//-- XBL-Form Controls
//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
//#define DISPLAY_NOISY_MSGF_MSG

#ifdef DISPLAY_NOISY_MSGF_MSG
typedef struct {
  char * mStr;
  int    mId;
} MSGFEventMsgInfo;

MSGFEventMsgInfo gMSGFEvents[] = {
  "MSGF_DIALOGBOX",      0,
  "MSGF_MESSAGEBOX",     1,
  "MSGF_MENU",           2,
  "MSGF_SCROLLBAR",      5,
  "MSGF_NEXTWINDOW",     6,
  "MSGF_MAX",            8,
  "MSGF_USER",           4096,
  NULL, 0};

  void PrintEvent(UINT msg, PRBool aShowAllEvents, PRBool aShowMouseMoves);
  int gLastMsgCode = 0;

#define DISPLAY_NMM_PRT(_arg) printf((_arg));
#else
#define DISPLAY_NMM_PRT(_arg)
#endif



#ifndef WINCE

//-------------------------------------------------------------------------
// Schedules a timer for a window, so we can rollup after processing the hook event
void nsWindow::ScheduleHookTimer(HWND aWnd, UINT aMsgId)
{
  // In some cases multiple hooks may be scheduled
  // so ignore any other requests once one timer is scheduled
  if (gHookTimerId == 0) {
    // Remember the window handle and the message ID to be used later
    gRollupMsgId = aMsgId;
    gRollupMsgWnd = aWnd;
    // Schedule native timer for doing the rollup after
    // this event is done being processed
    gHookTimerId = ::SetTimer(NULL, 0, 0, (TIMERPROC)HookTimerForPopups);
    NS_ASSERTION(gHookTimerId, "Timer couldn't be created.");
  }
}

//-------------------------------------------------------------------------
// Process Menu messages
// Rollup when when is clicked

LRESULT CALLBACK nsWindow::MozSpecialMsgFilter(int code, WPARAM wParam, LPARAM lParam)
{
#ifdef DISPLAY_NOISY_MSGF_MSG
  if (gProcessHook) {
    MSG* pMsg = (MSG*)lParam;

    int inx = 0;
    while (gMSGFEvents[inx].mId != code && gMSGFEvents[inx].mStr != NULL) {
      inx++;
    }
    if (code != gLastMsgCode) {
      if (gMSGFEvents[inx].mId == code) {
#ifdef DEBUG
        printf("MozSpecialMessageProc - code: 0x%X  - %s  hw: %p\n", code, gMSGFEvents[inx].mStr, pMsg->hwnd);
#endif
      } else {
#ifdef DEBUG
        printf("MozSpecialMessageProc - code: 0x%X  - %d  hw: %p\n", code, gMSGFEvents[inx].mId, pMsg->hwnd);
#endif
      }
      gLastMsgCode = code;
    }
    PrintEvent(pMsg->message, FALSE, FALSE);
  }
#endif

  if (gProcessHook && code == MSGF_MENU) {
    MSG* pMsg = (MSG*)lParam;
    ScheduleHookTimer( pMsg->hwnd, pMsg->message);
  }

  return ::CallNextHookEx(gMsgFilterHook, code, wParam, lParam);
}

//-------------------------------------------------------------------------
// Process all mouse messages
// Roll up when a click is in a native window that doesn't have an nsIWidget
LRESULT CALLBACK nsWindow::MozSpecialMouseProc(int code, WPARAM wParam, LPARAM lParam)
{
  if (gProcessHook) {
    MOUSEHOOKSTRUCT* ms = (MOUSEHOOKSTRUCT*)lParam;
    if (wParam == WM_LBUTTONDOWN) {
      nsIWidget* mozWin = (nsIWidget*)GetNSWindowPtr(ms->hwnd);
      if (mozWin == NULL) {
        ScheduleHookTimer(ms->hwnd, (UINT)wParam);
      }
    }
  }
  return ::CallNextHookEx(gCallMouseHook, code, wParam, lParam);
}

//-------------------------------------------------------------------------
// Process all messages
// Roll up when the window is moving, or is resizing or when maximized or mininized
LRESULT CALLBACK nsWindow::MozSpecialWndProc(int code, WPARAM wParam, LPARAM lParam)
{
#ifdef DISPLAY_NOISY_MSGF_MSG
  if (gProcessHook) {
    CWPSTRUCT* cwpt = (CWPSTRUCT*)lParam;
    PrintEvent(cwpt->message, FALSE, FALSE);
  }
#endif

  if (gProcessHook) {
    CWPSTRUCT* cwpt = (CWPSTRUCT*)lParam;
    if (cwpt->message == WM_MOVING ||
        cwpt->message == WM_SIZING ||
        cwpt->message == WM_GETMINMAXINFO) {
      ScheduleHookTimer(cwpt->hwnd, (UINT)cwpt->message);
    }
  }

  return ::CallNextHookEx(gCallProcHook, code, wParam, lParam);
}

//-------------------------------------------------------------------------
// Register the special "hooks" for dropdown processing

void nsWindow::RegisterSpecialDropdownHooks()
{
  NS_ASSERTION(!gMsgFilterHook, "gMsgFilterHook must be NULL!");
  NS_ASSERTION(!gCallProcHook,  "gCallProcHook must be NULL!");

  DISPLAY_NMM_PRT("***************** Installing Msg Hooks ***************\n");

  //HMODULE hMod = GetModuleHandle("gkwidget.dll");

  // Install msg hook for moving the window and resizing
  if (!gMsgFilterHook) {
    DISPLAY_NMM_PRT("***** Hooking gMsgFilterHook!\n");
    gMsgFilterHook = SetWindowsHookEx(WH_MSGFILTER, MozSpecialMsgFilter, NULL, GetCurrentThreadId());
#ifdef DISPLAY_NOISY_MSGF_MSG
    if (!gMsgFilterHook) {
      printf("***** SetWindowsHookEx is NOT installed for WH_MSGFILTER!\n");
    }
#endif
  }

  // Install msg hook for menus
  if (!gCallProcHook) {
    DISPLAY_NMM_PRT("***** Hooking gCallProcHook!\n");
    gCallProcHook  = SetWindowsHookEx(WH_CALLWNDPROC, MozSpecialWndProc, NULL, GetCurrentThreadId());
#ifdef DISPLAY_NOISY_MSGF_MSG
    if (!gCallProcHook) {
      printf("***** SetWindowsHookEx is NOT installed for WH_CALLWNDPROC!\n");
    }
#endif
  }

  // Install msg hook for the mouse
  if (!gCallMouseHook) {
    DISPLAY_NMM_PRT("***** Hooking gCallMouseHook!\n");
    gCallMouseHook  = SetWindowsHookEx(WH_MOUSE, MozSpecialMouseProc, NULL, GetCurrentThreadId());
#ifdef DISPLAY_NOISY_MSGF_MSG
    if (!gCallMouseHook) {
      printf("***** SetWindowsHookEx is NOT installed for WH_MOUSE!\n");
    }
#endif
  }
}

//-------------------------------------------------------------------------
// Unhook special message hooks for dropdowns

void nsWindow::UnregisterSpecialDropdownHooks()
{
  DISPLAY_NMM_PRT("***************** De-installing Msg Hooks ***************\n");

  if (gCallProcHook) {
    DISPLAY_NMM_PRT("***** Unhooking gCallProcHook!\n");
    if (!::UnhookWindowsHookEx(gCallProcHook)) {
      DISPLAY_NMM_PRT("***** UnhookWindowsHookEx failed for gCallProcHook!\n");
    }
    gCallProcHook = NULL;
  }

  if (gMsgFilterHook) {
    DISPLAY_NMM_PRT("***** Unhooking gMsgFilterHook!\n");
    if (!::UnhookWindowsHookEx(gMsgFilterHook)) {
      DISPLAY_NMM_PRT("***** UnhookWindowsHookEx failed for gMsgFilterHook!\n");
    }
    gMsgFilterHook = NULL;
  }

  if (gCallMouseHook) {
    DISPLAY_NMM_PRT("***** Unhooking gCallMouseHook!\n");
    if (!::UnhookWindowsHookEx(gCallMouseHook)) {
      DISPLAY_NMM_PRT("***** UnhookWindowsHookEx failed for gCallMouseHook!\n");
    }
    gCallMouseHook = NULL;
  }
}


//-------------------------------------------------------------------------
// This timer is designed to only fire one time at most each time a "hook" function
// is used to rollup the dropdown
// In some cases, the timer may be scheduled from the hook, but that hook event or
// a subsequent event may roll up the dropdown before this timer function is executed.
//
// For example, if an MFC control takes focus, the combobox will lose focus and rollup
// before this function fires.
//
VOID CALLBACK nsWindow::HookTimerForPopups(HWND hwnd, UINT uMsg, UINT idEvent, DWORD dwTime)
{
  if (gHookTimerId != 0) {
    // if the window is NULL then we need to use the ID to kill the timer
    BOOL status = ::KillTimer(NULL, gHookTimerId);
    NS_ASSERTION(status, "Hook Timer was not killed.");
    gHookTimerId = 0;
  }

  if (gRollupMsgId != 0) {
    // Note: DealWithPopups does the check to make sure that
    // gRollupListener and gRollupWidget are not NULL
    LRESULT popupHandlingResult;
    DealWithPopups(gRollupMsgWnd, gRollupMsgId, 0, 0, &popupHandlingResult);
    gRollupMsgId = 0;
    gRollupMsgWnd = NULL;
  }
}
#endif // WinCE

//
// DealWithPopups
//
// Handle events that may cause a popup (combobox, XPMenu, etc) to need to rollup.
//

BOOL
nsWindow :: DealWithPopups ( HWND inWnd, UINT inMsg, WPARAM inWParam, LPARAM inLParam, LRESULT* outResult )
{
  if (gRollupListener && gRollupWidget && ::IsWindowVisible(inWnd)) {

    if (inMsg == WM_LBUTTONDOWN || inMsg == WM_RBUTTONDOWN || inMsg == WM_MBUTTONDOWN ||
        inMsg == WM_MOUSEWHEEL || inMsg == WM_MOUSEHWHEEL || inMsg == WM_ACTIVATE
#ifndef WINCE
        || 
        inMsg == WM_NCRBUTTONDOWN || 
        inMsg == WM_MOVING || 
        inMsg == WM_SIZING || 
        inMsg == WM_GETMINMAXINFO ||
        inMsg == WM_NCLBUTTONDOWN || 
        inMsg == WM_NCMBUTTONDOWN ||
        inMsg == WM_MOUSEACTIVATE ||
        inMsg == WM_ACTIVATEAPP ||
        inMsg == WM_MENUSELECT ||
        (inMsg == WM_GETMINMAXINFO && !::GetParent(inWnd))
#endif
        )
    {
      // Rollup if the event is outside the popup.
      PRBool rollup = !nsWindow::EventIsInsideWindow(inMsg, (nsWindow*)gRollupWidget);

      if (rollup && (inMsg == WM_MOUSEWHEEL || inMsg == WM_MOUSEHWHEEL))
      {
        gRollupListener->ShouldRollupOnMouseWheelEvent(&rollup);
        *outResult = PR_TRUE;
      }

      // If we're dealing with menus, we probably have submenus and we don't
      // want to rollup if the click is in a parent menu of the current submenu.
      if (rollup) {
        nsCOMPtr<nsIMenuRollup> menuRollup ( do_QueryInterface(gRollupListener) );
        if ( menuRollup ) {
          nsCOMPtr<nsISupportsArray> widgetChain;
          menuRollup->GetSubmenuWidgetChain ( getter_AddRefs(widgetChain) );
          if ( widgetChain ) {
            PRUint32 count = 0;
            widgetChain->Count(&count);
            for ( PRUint32 i = 0; i < count; ++i ) {
              nsCOMPtr<nsISupports> genericWidget;
              widgetChain->GetElementAt ( i, getter_AddRefs(genericWidget) );
              nsCOMPtr<nsIWidget> widget ( do_QueryInterface(genericWidget) );
              if ( widget ) {
                nsIWidget* temp = widget.get();
                if ( nsWindow::EventIsInsideWindow(inMsg, (nsWindow*)temp) ) {
                  rollup = PR_FALSE;
                  break;
                }
              }
            } // foreach parent menu widget
          }
        } // if rollup listener knows about menus
      }

#ifndef WINCE
      if (inMsg == WM_MOUSEACTIVATE) {
        // Prevent the click inside the popup from causing a change in window
        // activation. Since the popup is shown non-activated, we need to eat
        // any requests to activate the window while it is displayed. Windows
        // will automatically activate the popup on the mousedown otherwise.
        if (!rollup) {
          *outResult = MA_NOACTIVATE;
          return TRUE;
        }
        else
        {
          UINT uMsg = HIWORD(inLParam);
          if (uMsg == WM_MOUSEMOVE)
          {
            // WM_MOUSEACTIVATE cause by moving the mouse - X-mouse (eg. TweakUI)
            // must be enabled in Windows.
            gRollupListener->ShouldRollupOnMouseActivate(&rollup);
            if (!rollup)
            {
              *outResult = MA_NOACTIVATE;
              return true;
            }
          }
        }
      }
      // if we've still determined that we should still rollup everything, do it.
      else
#endif
      if ( rollup ) {
        gRollupListener->Rollup();

        // Tell hook to stop processing messages
        gProcessHook = PR_FALSE;
        gRollupMsgId = 0;
        gRollupMsgWnd = NULL;

        // return TRUE tells Windows that the event is consumed,
        // false allows the event to be dispatched
        //
        // So if we are NOT supposed to be consuming events, let it go through
        if (gRollupConsumeRollupEvent && inMsg != WM_RBUTTONDOWN) {
          *outResult = TRUE;
          return TRUE;
        }
      }
    } // if event that might trigger a popup to rollup
  } // if rollup listeners registered

  return FALSE;
} // DealWithPopups



#ifdef ACCESSIBILITY
already_AddRefed<nsIAccessible> nsWindow::GetRootAccessible()
{
  nsWindow::gIsAccessibilityOn = TRUE;

  if (mIsDestroying || mOnDestroyCalled || mWindowType == eWindowType_invisible) {
    return nsnull;
  }

  nsIAccessible *rootAccessible = nsnull;

  // If accessibility is turned on, we create this even before it is requested
  // when the window gets focused. We need it to be created early so it can 
  // generate accessibility events right away
  nsWindow* accessibleWindow = nsnull;
  if (mContentType != eContentTypeInherit) {
    // We're on a MozillaContentWindowClass or MozillaUIWindowClass window.
    // Search for the correct visible child window to get an accessible 
    // document from. Make sure to use an active child window
    HWND accessibleWnd = ::GetTopWindow(mWnd);
    while (accessibleWnd) {
      // Loop through windows and find the first one with accessibility info
      accessibleWindow = GetNSWindowPtr(accessibleWnd);
      if (accessibleWindow) {
        accessibleWindow->DispatchAccessibleEvent(NS_GETACCESSIBLE, &rootAccessible);
        if (rootAccessible) {
          break;  // Success, one of the child windows was active
        }
      }
      accessibleWnd = ::GetNextWindow(accessibleWnd, GW_HWNDNEXT);
    }
  }
  else {
    DispatchAccessibleEvent(NS_GETACCESSIBLE, &rootAccessible);
  }
  return rootAccessible;
}

HINSTANCE nsWindow::gmAccLib = 0;
LPFNLRESULTFROMOBJECT nsWindow::gmLresultFromObject = 0;

STDMETHODIMP_(LRESULT) nsWindow::LresultFromObject(REFIID riid, WPARAM wParam, LPUNKNOWN pAcc)
{
  // open the dll dynamically
  if (!gmAccLib)
    gmAccLib =::LoadLibrary("OLEACC.DLL");

  if (gmAccLib) {
    if (!gmLresultFromObject)
      gmLresultFromObject = (LPFNLRESULTFROMOBJECT)GetProcAddress(gmAccLib,"LresultFromObject");

    if (gmLresultFromObject)
      return gmLresultFromObject(riid,wParam,pAcc);
  }

  return 0;
}
#endif

#ifdef MOZ_XUL

nsWindow* nsWindow::GetTopLevelWindow()
{
  nsWindow* curWindow = this;

  while (PR_TRUE)
  {
    nsWindow* parentWindow = curWindow->GetParent(PR_TRUE);

    if (parentWindow)
      curWindow = parentWindow;
    else
      return curWindow;
  }
}

#ifdef MOZ_CAIRO_GFX
gfxASurface *nsWindow::GetThebesSurface()
{
  if (mPaintDC)
    return (new gfxWindowsSurface(mPaintDC));

  return (new gfxWindowsSurface(mWnd));
}
#endif

void nsWindow::ResizeTranslucentWindow(PRInt32 aNewWidth, PRInt32 aNewHeight, PRBool force)
{
  if (!force && aNewWidth == mBounds.width && aNewHeight == mBounds.height)
    return;

#ifndef MOZ_CAIRO_GFX
  // resize the alpha mask
  PRUint8* pBits;

  if (aNewWidth > 0 && aNewHeight > 0)
  {
    pBits = new PRUint8 [aNewWidth * aNewHeight];

    if (pBits && mAlphaMask)
    {
      PRInt32 copyWidth, copyHeight;
      PRInt32 growWidth, growHeight;

      if (aNewWidth > mBounds.width)
      {
        copyWidth = mBounds.width;
        growWidth = aNewWidth - mBounds.width;
      } else
      {
        copyWidth = aNewWidth;
        growWidth = 0;
      }

      if (aNewHeight > mBounds.height)
      {
        copyHeight = mBounds.height;
        growHeight = aNewHeight - mBounds.height;
      } else
      {
        copyHeight = aNewHeight;
        growHeight = 0;
      }

      PRUint8* pSrc = mAlphaMask;
      PRUint8* pDest = pBits;

      for (PRInt32 cy = 0 ; cy < copyHeight ; cy++)
      {
        memcpy(pDest, pSrc, copyWidth);
        memset(pDest + copyWidth, 255, growWidth);
        pSrc += mBounds.width;
        pDest += aNewWidth;
      }

      for (PRInt32 gy = 0 ; gy < growHeight ; gy++)
      {
        memset(pDest, 255, aNewWidth);
        pDest += aNewWidth;
      }
    }
  } else
    pBits = nsnull;

  delete [] mAlphaMask;
  mAlphaMask = pBits;
#endif

  if (!mMemoryDC)
    mMemoryDC = ::CreateCompatibleDC(NULL);

  // Always use at least 24-bit (32 with cairo) bitmaps regardless of the device context.
  int depth = ::GetDeviceCaps(mMemoryDC, BITSPIXEL);
#ifdef MOZ_CAIRO_GFX
  if (depth < 32)
    depth = 32;
#else
  if (depth < 24)
    depth = 24;
#endif

  // resize the memory bitmap
  BITMAPINFO bi = { 0 };
  bi.bmiHeader.biSize = sizeof (BITMAPINFOHEADER);
  bi.bmiHeader.biWidth = aNewWidth;
  bi.bmiHeader.biHeight = -aNewHeight;
  bi.bmiHeader.biPlanes = 1;
  bi.bmiHeader.biBitCount = depth;
  bi.bmiHeader.biCompression = BI_RGB;

  mMemoryBitmap = ::CreateDIBSection(mMemoryDC, &bi, DIB_RGB_COLORS, (void**)&mMemoryBits, NULL, 0);

  if (mMemoryBitmap)
  {
    HGDIOBJ oldBitmap = ::SelectObject(mMemoryDC, mMemoryBitmap);
    ::DeleteObject(oldBitmap);
  }
}

NS_IMETHODIMP nsWindow::GetWindowTranslucency(PRBool& aTranslucent)
{
  aTranslucent = GetTopLevelWindow()->GetWindowTranslucencyInner();

  return NS_OK;
}

NS_IMETHODIMP nsWindow::SetWindowTranslucency(PRBool aTranslucent)
{
  nsresult rv = GetTopLevelWindow()->SetWindowTranslucencyInner(aTranslucent);

  return rv;
}

NS_IMETHODIMP nsWindow::UpdateTranslucentWindowAlpha(const nsRect& aRect, PRUint8* aAlphas)
{
  GetTopLevelWindow()->UpdateTranslucentWindowAlphaInner(aRect, aAlphas);

  return NS_OK;
}

nsresult nsWindow::SetWindowTranslucencyInner(PRBool aTranslucent)
{
  if (aTranslucent == mIsTranslucent)
    return NS_OK;
  
  HWND hWnd = GetTopLevelHWND(mWnd, PR_TRUE);
  nsWindow* topWindow = GetNSWindowPtr(hWnd);

  if (!topWindow)
  {
    NS_WARNING("Trying to use transparent chrome in an embedded context");
    return NS_ERROR_FAILURE;
  }

  LONG style, exStyle;

  if (aTranslucent)
  {
    style = ::GetWindowLongW(hWnd, GWL_STYLE) &
            ~(WS_CAPTION | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
    exStyle = ::GetWindowLongW(hWnd, GWL_EXSTYLE) &
              ~(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE);

    exStyle |= WS_EX_LAYERED;
  } else
  {
    style = WindowStyle();
    exStyle = WindowExStyle();
  }
  ::SetWindowLongW(hWnd, GWL_STYLE, style);
  ::SetWindowLongW(hWnd, GWL_EXSTYLE, exStyle);

  mIsTranslucent = aTranslucent;
  topWindow->mIsTopTranslucent = aTranslucent;

  nsresult rv = NS_OK;

  rv = SetupTranslucentWindowMemoryBitmap(aTranslucent);

  if (aTranslucent)
  {
    if (!mBounds.IsEmpty())
    {
      PRInt32 alphaBytes = mBounds.width * mBounds.height;
      mAlphaMask = new PRUint8 [alphaBytes];

      if (mAlphaMask)
        memset(mAlphaMask, 255, alphaBytes);
      else
        rv = NS_ERROR_OUT_OF_MEMORY;
    } else
      mAlphaMask = nsnull;
  } else
  {
    delete [] mAlphaMask;
    mAlphaMask = nsnull;
  }

  return rv;
}

nsresult nsWindow::SetupTranslucentWindowMemoryBitmap(PRBool aTranslucent)
{
  if (aTranslucent) {
    ResizeTranslucentWindow(mBounds.width, mBounds.height, PR_TRUE);
  } else {
    if (mMemoryDC)
      ::DeleteDC(mMemoryDC);
    if (mMemoryBitmap)
      ::DeleteObject(mMemoryBitmap);

    mMemoryDC = NULL;
    mMemoryBitmap = NULL;
  }

  return NS_OK;
}

void nsWindow::UpdateTranslucentWindowAlphaInner(const nsRect& aRect, PRUint8* aAlphas)
{
#ifdef MOZ_CAIRO_GFX
  NS_ERROR("nsWindow::UpdateTranslucentWindowAlphaInner called, when it sholdn't be!");
#else
  NS_ASSERTION(mIsTranslucent, "Window is not transparent");
  NS_ASSERTION(aRect.x >= 0 && aRect.y >= 0 &&
               aRect.XMost() <= mBounds.width && aRect.YMost() <= mBounds.height,
               "Rect is out of window bounds");

  PRBool transparencyMaskChanged = PR_FALSE;

  if (!aRect.IsEmpty())
  {
    PRUint8* pSrcRow = aAlphas;
    PRUint8* pDestRow = mAlphaMask + aRect.y * mBounds.width + aRect.x;

    for (PRInt32 y = 0 ; y < aRect.height ; y++)
    {
      memcpy(pDestRow, pSrcRow, aRect.width);

      pSrcRow += aRect.width;
      pDestRow += mBounds.width;
    }
  }

  // Windows 2000 and newer versions support layered windows which allow to implement
  // full 256 level alpha translucency.
  // The real screen update is performed in OnPaint() handler only after rendered
  // bits from offscreen drawing surface are copied back to memory bitmap.
#endif
}

nsresult nsWindow::UpdateTranslucentWindow()
{
  if (mBounds.IsEmpty())
    return NS_OK;

  nsresult rv = NS_ERROR_FAILURE;

  ::GdiFlush();

  HDC hMemoryDC;
#ifndef MOZ_CAIRO_GFX
  HBITMAP hAlphaBitmap;
#endif
  PRBool needConversion;

#ifdef MOZ_CAIRO_GFX

  hMemoryDC = mMemoryDC;
  needConversion = PR_FALSE;

  rv = NS_OK;

#else

  int depth = ::GetDeviceCaps(mMemoryDC, BITSPIXEL);
  if (depth < 24)
    depth = 24;

  needConversion = (depth == 24);

  if (needConversion)
  {
    hMemoryDC = ::CreateCompatibleDC(NULL);

    if (hMemoryDC)
    {
      // Memory bitmap with alpha channel
      BITMAPINFO bi = { 0 };
      bi.bmiHeader.biSize = sizeof (BITMAPINFOHEADER);
      bi.bmiHeader.biWidth = mBounds.width;
      bi.bmiHeader.biHeight = -mBounds.height;
      bi.bmiHeader.biPlanes = 1;
      bi.bmiHeader.biBitCount = 32;
      bi.bmiHeader.biCompression = BI_RGB;

      PRUint8* pBits32 = nsnull;
      hAlphaBitmap = ::CreateDIBSection(hMemoryDC, &bi, DIB_RGB_COLORS, (void**)&pBits32, NULL, 0);
      
      if (hAlphaBitmap)
      {
        HGDIOBJ oldBitmap = ::SelectObject(hMemoryDC, hAlphaBitmap);

        PRUint8* pPixel32 = pBits32;
        PRUint8* pAlpha = mAlphaMask;
        PRUint32 rasWidth = RASWIDTH(mBounds.width, 24);

        for (PRInt32 y = 0 ; y < mBounds.height ; y++)
        {
          PRUint8* pPixel = mMemoryBits + y * rasWidth;

          for (PRInt32 x = 0 ; x < mBounds.width ; x++)
          {
            *pPixel32++ = *pPixel++;
            *pPixel32++ = *pPixel++;
            *pPixel32++ = *pPixel++;
            *pPixel32++ = *pAlpha++;
          }
        }

        rv = NS_OK;
      }
    }
  } else
  {
    hMemoryDC = mMemoryDC;

    if (hMemoryDC)
    {
      PRUint8* pPixel = mMemoryBits + 3;    // Point to alpha component of pixel
      PRUint8* pAlpha = mAlphaMask;
      PRInt32 pixels = mBounds.width * mBounds.height;

      for (PRInt32 cnt = 0 ; cnt < pixels ; cnt++)
      {
        *pPixel = *pAlpha++;
        pPixel += 4;
      }

      rv = NS_OK;
    }
  }
#endif /* MOZ_CAIRO_GFX */

  if (rv == NS_OK)
  {
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    SIZE winSize = { mBounds.width, mBounds.height };
    POINT srcPos = { 0, 0 };
    HWND hWnd = GetTopLevelHWND(mWnd, PR_TRUE);
    RECT winRect;
    ::GetWindowRect(hWnd, &winRect);

    // perform the alpha blend
    if (!::UpdateLayeredWindow(hWnd, NULL, (POINT*)&winRect, &winSize, hMemoryDC, &srcPos, 0, &bf, ULW_ALPHA))
      rv = NS_ERROR_FAILURE;
  }


  if (needConversion)
  {
#ifndef MOZ_CAIRO_GFX
    ::DeleteObject(hAlphaBitmap);
#endif
    ::DeleteDC(hMemoryDC);
  }

  return rv;
}

#endif
