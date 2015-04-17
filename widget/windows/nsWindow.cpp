/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sts=2 sw=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * nsWindow - Native window management and event handling.
 * 
 * nsWindow is organized into a set of major blocks and
 * block subsections. The layout is as follows:
 *
 *  Includes
 *  Variables
 *  nsIWidget impl.
 *     nsIWidget methods and utilities
 *  nsSwitchToUIThread impl.
 *     nsSwitchToUIThread methods and utilities
 *  Moz events
 *     Event initialization
 *     Event dispatching
 *  Native events
 *     Wndproc(s)
 *     Event processing
 *     OnEvent event handlers
 *  IME management and accessibility
 *  Transparency
 *  Popup hook handling
 *  Misc. utilities
 *  Child window impl.
 *
 * Search for "BLOCK:" to find major blocks.
 * Search for "SECTION:" to find specific sections.
 *
 * Blocks should be split out into separate files if they
 * become unmanageable.
 *
 * Related source:
 *
 *  nsWindowDefs.h     - Definitions, macros, structs, enums
 *                       and general setup.
 *  nsWindowDbg.h/.cpp - Debug related code and directives.
 *  nsWindowGfx.h/.cpp - Graphics and painting.
 *
 */

/**************************************************************
 **************************************************************
 **
 ** BLOCK: Includes
 **
 ** Include headers.
 **
 **************************************************************
 **************************************************************/

#include "gfx2DGlue.h"
#include "gfxPlatform.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/MiscEvents.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/TouchEvents.h"

#include "mozilla/ipc/MessageChannel.h"
#include <algorithm>
#include <limits>

#include "nsWindow.h"

#include <shellapi.h>
#include <windows.h>
#include <wtsapi32.h>
#include <process.h>
#include <commctrl.h>
#include <unknwn.h>
#include <psapi.h>

#include "prlog.h"
#include "prtime.h"
#include "prprf.h"
#include "prmem.h"
#include "prenv.h"

#include "mozilla/WidgetTraceEvent.h"
#include "nsIAppShell.h"
#include "nsISupportsPrimitives.h"
#include "nsIDOMMouseEvent.h"
#include "nsITheme.h"
#include "nsIObserverService.h"
#include "nsIScreenManager.h"
#include "imgIContainer.h"
#include "nsIFile.h"
#include "nsIRollupListener.h"
#include "nsIServiceManager.h"
#include "nsIClipboard.h"
#include "nsIMM32Handler.h"
#include "WinMouseScrollHandler.h"
#include "nsFontMetrics.h"
#include "nsIFontEnumerator.h"
#include "nsFont.h"
#include "nsRect.h"
#include "nsThreadUtils.h"
#include "nsNativeCharsetUtils.h"
#include "nsGkAtoms.h"
#include "nsCRT.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsXPIDLString.h"
#include "nsWidgetsCID.h"
#include "nsTHashtable.h"
#include "nsHashKeys.h"
#include "nsString.h"
#include "mozilla/Services.h"
#include "nsNativeThemeWin.h"
#include "nsWindowsDllInterceptor.h"
#include "nsLayoutUtils.h"
#include "nsView.h"
#include "nsIWindowMediator.h"
#include "nsIServiceManager.h"
#include "nsWindowGfx.h"
#include "gfxWindowsPlatform.h"
#include "Layers.h"
#include "nsPrintfCString.h"
#include "mozilla/Preferences.h"
#include "nsISound.h"
#include "WinTaskbar.h"
#include "WinUtils.h"
#include "WidgetUtils.h"
#include "nsIWidgetListener.h"
#include "mozilla/dom/Touch.h"
#include "mozilla/gfx/2D.h"
#include "nsToolkitCompsCID.h"
#include "nsIAppStartup.h"
#include "mozilla/WindowsVersion.h"
#include "mozilla/TextEvents.h" // For WidgetKeyboardEvent
#include "nsThemeConstants.h"

#include "nsIGfxInfo.h"
#include "nsUXThemeConstants.h"
#include "KeyboardLayout.h"
#include "nsNativeDragTarget.h"
#include <mmsystem.h> // needed for WIN32_LEAN_AND_MEAN
#include <zmouse.h>
#include <richedit.h>

#if defined(ACCESSIBILITY)

#ifdef DEBUG
#include "mozilla/a11y/Logging.h"
#endif

#include "oleidl.h"
#include <winuser.h>
#include "nsAccessibilityService.h"
#include "mozilla/a11y/DocAccessible.h"
#include "mozilla/a11y/Platform.h"
#if !defined(WINABLEAPI)
#include <winable.h>
#endif // !defined(WINABLEAPI)
#endif // defined(ACCESSIBILITY)

#include "nsIWinTaskbar.h"
#define NS_TASKBAR_CONTRACTID "@mozilla.org/windows-taskbar;1"

#include "nsWindowDefs.h"

#include "nsCrashOnException.h"
#include "nsIXULRuntime.h"

#include "nsIContent.h"

#include "mozilla/HangMonitor.h"
#include "WinIMEHandler.h"

#include "npapi.h"

#include <d3d11.h>

#if !defined(SM_CONVERTIBLESLATEMODE)
#define SM_CONVERTIBLESLATEMODE 0x2003
#endif

#include "mozilla/layers/APZCTreeManager.h"
#include "mozilla/layers/InputAPZContext.h"
#include "InputData.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::gfx;
using namespace mozilla::layers;
using namespace mozilla::widget;

namespace mozilla {
namespace widget {
  extern int32_t IsTouchDeviceSupportPresent();
}
}

/**************************************************************
 **************************************************************
 **
 ** BLOCK: Variables
 **
 ** nsWindow Class static initializations and global variables. 
 **
 **************************************************************
 **************************************************************/

/**************************************************************
 *
 * SECTION: nsWindow statics
 *
 **************************************************************/

bool            nsWindow::sDropShadowEnabled      = true;
uint32_t        nsWindow::sInstanceCount          = 0;
bool            nsWindow::sSwitchKeyboardLayout   = false;
BOOL            nsWindow::sIsOleInitialized       = FALSE;
HCURSOR         nsWindow::sHCursor                = nullptr;
imgIContainer*  nsWindow::sCursorImgContainer     = nullptr;
nsWindow*       nsWindow::sCurrentWindow          = nullptr;
bool            nsWindow::sJustGotDeactivate      = false;
bool            nsWindow::sJustGotActivate        = false;
bool            nsWindow::sIsInMouseCapture       = false;

// imported in nsWidgetFactory.cpp
TriStateBool    nsWindow::sCanQuit                = TRI_UNKNOWN;

// Hook Data Memebers for Dropdowns. sProcessHook Tells the
// hook methods whether they should be processing the hook
// messages.
HHOOK           nsWindow::sMsgFilterHook          = nullptr;
HHOOK           nsWindow::sCallProcHook           = nullptr;
HHOOK           nsWindow::sCallMouseHook          = nullptr;
bool            nsWindow::sProcessHook            = false;
UINT            nsWindow::sRollupMsgId            = 0;
HWND            nsWindow::sRollupMsgWnd           = nullptr;
UINT            nsWindow::sHookTimerId            = 0;

// Mouse Clicks - static variable definitions for figuring
// out 1 - 3 Clicks.
POINT           nsWindow::sLastMousePoint         = {0};
POINT           nsWindow::sLastMouseMovePoint     = {0};
LONG            nsWindow::sLastMouseDownTime      = 0L;
LONG            nsWindow::sLastClickCount         = 0L;
BYTE            nsWindow::sLastMouseButton        = 0;

// Trim heap on minimize. (initialized, but still true.)
int             nsWindow::sTrimOnMinimize         = 2;

// Default value for general window class (used when the pref is the empty string).
const char*     nsWindow::sDefaultMainWindowClass = kClassNameGeneral;

TriStateBool nsWindow::sHasBogusPopupsDropShadowOnMultiMonitor = TRI_UNKNOWN;

DWORD           nsWindow::sFirstEventTime = 0;
TimeStamp       nsWindow::sFirstEventTimeStamp = TimeStamp();

/**************************************************************
 *
 * SECTION: globals variables
 *
 **************************************************************/

static const char *sScreenManagerContractID       = "@mozilla.org/gfx/screenmanager;1";

#ifdef PR_LOGGING
extern PRLogModuleInfo* gWindowsLog;
#endif

// Global used in Show window enumerations.
static bool     gWindowsVisible                   = false;

// True if we have sent a notification that we are suspending/sleeping.
static bool     gIsSleepMode                      = false;

static NS_DEFINE_CID(kCClipboardCID, NS_CLIPBOARD_CID);

// General purpose user32.dll hook object
static WindowsDllInterceptor sUser32Intercept;

// 2 pixel offset for eTransparencyBorderlessGlass which equals the size of
// the default window border Windows paints. Glass will be extended inward
// this distance to remove the border.
static const int32_t kGlassMarginAdjustment = 2;

// When the client area is extended out into the default window frame area,
// this is the minimum amount of space along the edge of resizable windows
// we will always display a resize cursor in, regardless of the underlying
// content.
static const int32_t kResizableBorderMinSize = 3;

// Cached pointer events enabler value, True if pointer events are enabled.
static bool gIsPointerEventsEnabled = false;

// We should never really try to accelerate windows bigger than this. In some
// cases this might lead to no D3D9 acceleration where we could have had it
// but D3D9 does not reliably report when it supports bigger windows. 8192
// is as safe as we can get, we know at least D3D10 hardware always supports
// this, other hardware we expect to report correctly in D3D9.
#define MAX_ACCELERATED_DIMENSION 8192

// On window open (as well as after), Windows has an unfortunate habit of
// sending rather a lot of WM_NCHITTEST messages. Because we have to do point
// to DOM target conversions for these, we cache responses for a given
// coordinate this many milliseconds:
#define HITTEST_CACHE_LIFETIME_MS 50

// Times attached to messages wrap when they overflow
static const DWORD kEventTimeRange = std::numeric_limits<DWORD>::max();
static const DWORD kEventTimeHalfRange = kEventTimeRange / 2;


/**************************************************************
 **************************************************************
 **
 ** BLOCK: nsIWidget impl.
 **
 ** nsIWidget interface implementation, broken down into
 ** sections.
 **
 **************************************************************
 **************************************************************/

/**************************************************************
 *
 * SECTION: nsWindow construction and destruction
 *
 **************************************************************/

nsWindow::nsWindow() : nsWindowBase()
{
  mIconSmall            = nullptr;
  mIconBig              = nullptr;
  mWnd                  = nullptr;
  mPaintDC              = nullptr;
  mCompositeDC          = nullptr;
  mPrevWndProc          = nullptr;
  mNativeDragTarget     = nullptr;
  mInDtor               = false;
  mIsVisible            = false;
  mIsTopWidgetWindow    = false;
  mUnicodeWidget        = true;
  mDisplayPanFeedback   = false;
  mTouchWindow          = false;
  mFutureMarginsToUse   = false;
  mCustomNonClient      = false;
  mHideChrome           = false;
  mFullscreenMode       = false;
  mMousePresent         = false;
  mDestroyCalled        = false;
  mHasTaskbarIconBeenCreated = false;
  mMouseTransparent     = false;
  mPickerDisplayCount   = 0;
  mWindowType           = eWindowType_child;
  mBorderStyle          = eBorderStyle_default;
  mOldSizeMode          = nsSizeMode_Normal;
  mLastSizeMode         = nsSizeMode_Normal;
  mLastSize.width       = 0;
  mLastSize.height      = 0;
  mOldStyle             = 0;
  mOldExStyle           = 0;
  mPainting             = 0;
  mLastKeyboardLayout   = 0;
  mBlurSuppressLevel    = 0;
  mLastPaintEndTime     = TimeStamp::Now();
  mCachedHitTestPoint.x = 0;
  mCachedHitTestPoint.y = 0;
  mCachedHitTestTime    = TimeStamp::Now();
  mCachedHitTestResult  = 0;
#ifdef MOZ_XUL
  mTransparentSurface   = nullptr;
  mMemoryDC             = nullptr;
  mTransparencyMode     = eTransparencyOpaque;
  memset(&mGlassMargins, 0, sizeof mGlassMargins);
#endif
  DWORD background      = ::GetSysColor(COLOR_BTNFACE);
  mBrush                = ::CreateSolidBrush(NSRGB_2_COLOREF(background));

  mTaskbarPreview = nullptr;

  // Global initialization
  if (!sInstanceCount) {
    // Global app registration id for Win7 and up. See
    // WinTaskbar.cpp for details.
    mozilla::widget::WinTaskbar::RegisterAppUserModelID();
    KeyboardLayout::GetInstance()->OnLayoutChange(::GetKeyboardLayout(0));
    IMEHandler::Initialize();
    if (SUCCEEDED(::OleInitialize(nullptr))) {
      sIsOleInitialized = TRUE;
    }
    NS_ASSERTION(sIsOleInitialized, "***** OLE is not initialized!\n");
    MouseScrollHandler::Initialize();
    // Init titlebar button info for custom frames.
    nsUXThemeData::InitTitlebarInfo();
    // Init theme data
    nsUXThemeData::UpdateNativeThemeInfo();
    RedirectedKeyDownMessageManager::Forget();

    Preferences::AddBoolVarCache(&gIsPointerEventsEnabled,
                                 "dom.w3c_pointer_events.enabled",
                                 gIsPointerEventsEnabled);
  } // !sInstanceCount

  mIdleService = nullptr;

  sInstanceCount++;
}

nsWindow::~nsWindow()
{
  mInDtor = true;

  // If the widget was released without calling Destroy() then the native window still
  // exists, and we need to destroy it. This will also result in a call to OnDestroy.
  //
  // XXX How could this happen???
  if (nullptr != mWnd)
    Destroy();

  // Free app icon resources.  This must happen after `OnDestroy` (see bug 708033).
  if (mIconSmall)
    ::DestroyIcon(mIconSmall);

  if (mIconBig)
    ::DestroyIcon(mIconBig);

  sInstanceCount--;

  // Global shutdown
  if (sInstanceCount == 0) {
    IMEHandler::Terminate();
    NS_IF_RELEASE(sCursorImgContainer);
    if (sIsOleInitialized) {
      ::OleFlushClipboard();
      ::OleUninitialize();
      sIsOleInitialized = FALSE;
    }
  }

  NS_IF_RELEASE(mNativeDragTarget);
}

NS_IMPL_ISUPPORTS_INHERITED0(nsWindow, nsBaseWidget)

/**************************************************************
 *
 * SECTION: nsIWidget::Create, nsIWidget::Destroy
 *
 * Creating and destroying windows for this widget.
 *
 **************************************************************/

// Allow Derived classes to modify the height that is passed
// when the window is created or resized.
int32_t nsWindow::GetHeight(int32_t aProposedHeight)
{
  return aProposedHeight;
}

// Create the proper widget
nsresult
nsWindow::Create(nsIWidget *aParent,
                 nsNativeWidget aNativeParent,
                 const nsIntRect &aRect,
                 nsWidgetInitData *aInitData)
{
  nsWidgetInitData defaultInitData;
  if (!aInitData)
    aInitData = &defaultInitData;

  mUnicodeWidget = aInitData->mUnicode;

  nsIWidget *baseParent = aInitData->mWindowType == eWindowType_dialog ||
                          aInitData->mWindowType == eWindowType_toplevel ||
                          aInitData->mWindowType == eWindowType_invisible ?
                          nullptr : aParent;

  mIsTopWidgetWindow = (nullptr == baseParent);
  mBounds = aRect;

  // Ensure that the toolkit is created.
  nsToolkit::GetToolkit();

  BaseCreate(baseParent, aRect, aInitData);

  HWND parent;
  if (aParent) { // has a nsIWidget parent
    parent = aParent ? (HWND)aParent->GetNativeData(NS_NATIVE_WINDOW) : nullptr;
    mParent = aParent;
  } else { // has a nsNative parent
    parent = (HWND)aNativeParent;
    mParent = aNativeParent ?
      WinUtils::GetNSWindowPtr((HWND)aNativeParent) : nullptr;
  }

  mIsRTL = aInitData->mRTL;

  DWORD style = WindowStyle();
  DWORD extendedStyle = WindowExStyle();

  if (mWindowType == eWindowType_popup) {
    if (!aParent) {
      parent = nullptr;
    }

    if (IsVistaOrLater() && !IsWin8OrLater() &&
        HasBogusPopupsDropShadowOnMultiMonitor()) {
      extendedStyle |= WS_EX_COMPOSITED;
    }

    if (aInitData->mMouseTransparent) {
      // This flag makes the window transparent to mouse events
      mMouseTransparent = true;
      extendedStyle |= WS_EX_TRANSPARENT;
    }
  } else if (mWindowType == eWindowType_invisible) {
    // Make sure CreateWindowEx succeeds at creating a toplevel window
    style &= ~0x40000000; // WS_CHILDWINDOW
  } else {
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

  nsAutoString className;
  if (aInitData->mDropShadow) {
    GetWindowPopupClass(className);
  } else {
    GetWindowClass(className);
  }
  // Plugins are created in the disabled state so that they can't
  // steal focus away from our main window.  This is especially
  // important if the plugin has loaded in a background tab.
  if (aInitData->mWindowType == eWindowType_plugin ||
      aInitData->mWindowType == eWindowType_plugin_ipc_chrome ||
      aInitData->mWindowType == eWindowType_plugin_ipc_content) {
    style |= WS_DISABLED;
  }
  mWnd = ::CreateWindowExW(extendedStyle,
                           className.get(),
                           L"",
                           style,
                           aRect.x,
                           aRect.y,
                           aRect.width,
                           GetHeight(aRect.height),
                           parent,
                           nullptr,
                           nsToolkit::mDllInstance,
                           nullptr);

  if (!mWnd) {
    NS_WARNING("nsWindow CreateWindowEx failed.");
    return NS_ERROR_FAILURE;
  }

  if (mIsRTL && WinUtils::dwmSetWindowAttributePtr) {
    DWORD dwAttribute = TRUE;    
    WinUtils::dwmSetWindowAttributePtr(mWnd, DWMWA_NONCLIENT_RTL_LAYOUT, &dwAttribute, sizeof dwAttribute);
  }

  if (!IsPlugin() &&
      mWindowType != eWindowType_invisible &&
      MouseScrollHandler::Device::IsFakeScrollableWindowNeeded()) {
    // Ugly Thinkpad Driver Hack (Bugs 507222 and 594977)
    //
    // We create two zero-sized windows as descendants of the top-level window,
    // like so:
    //
    //   Top-level window (MozillaWindowClass)
    //     FAKETRACKPOINTSCROLLCONTAINER (MozillaWindowClass)
    //       FAKETRACKPOINTSCROLLABLE (MozillaWindowClass)
    //
    // We need to have the middle window, otherwise the Trackpoint driver
    // will fail to deliver scroll messages.  WM_MOUSEWHEEL messages are
    // sent to the FAKETRACKPOINTSCROLLABLE, which then propagate up the
    // window hierarchy until they are handled by nsWindow::WindowProc.
    // WM_HSCROLL messages are also sent to the FAKETRACKPOINTSCROLLABLE,
    // but these do not propagate automatically, so we have the window
    // procedure pretend that they were dispatched to the top-level window
    // instead.
    //
    // The FAKETRACKPOINTSCROLLABLE needs to have the specific window styles it
    // is given below so that it catches the Trackpoint driver's heuristics.
    HWND scrollContainerWnd = ::CreateWindowW
      (className.get(), L"FAKETRACKPOINTSCROLLCONTAINER",
       WS_CHILD | WS_VISIBLE,
       0, 0, 0, 0, mWnd, nullptr, nsToolkit::mDllInstance, nullptr);
    HWND scrollableWnd = ::CreateWindowW
      (className.get(), L"FAKETRACKPOINTSCROLLABLE",
       WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP | 0x30,
       0, 0, 0, 0, scrollContainerWnd, nullptr, nsToolkit::mDllInstance,
       nullptr);

    // Give the FAKETRACKPOINTSCROLLABLE window a specific ID so that
    // WindowProcInternal can distinguish it from the top-level window
    // easily.
    ::SetWindowLongPtrW(scrollableWnd, GWLP_ID, eFakeTrackPointScrollableID);

    // Make FAKETRACKPOINTSCROLLABLE use nsWindow::WindowProc, and store the
    // old window procedure in its "user data".
    WNDPROC oldWndProc;
    if (mUnicodeWidget)
      oldWndProc = (WNDPROC)::SetWindowLongPtrW(scrollableWnd, GWLP_WNDPROC,
                                                (LONG_PTR)nsWindow::WindowProc);
    else
      oldWndProc = (WNDPROC)::SetWindowLongPtrA(scrollableWnd, GWLP_WNDPROC,
                                                (LONG_PTR)nsWindow::WindowProc);
    ::SetWindowLongPtrW(scrollableWnd, GWLP_USERDATA, (LONG_PTR)oldWndProc);
  }

  SubclassWindow(TRUE);

  // Starting with Windows XP, a process always runs within a terminal services
  // session. In order to play nicely with RDP, fast user switching, and the
  // lock screen, we should be handling WM_WTSSESSION_CHANGE. We must register
  // our HWND in order to receive this message.
  DebugOnly<BOOL> wtsRegistered = ::WTSRegisterSessionNotification(mWnd,
                                                       NOTIFY_FOR_THIS_SESSION);
  NS_ASSERTION(wtsRegistered, "WTSRegisterSessionNotification failed!\n");

  IMEHandler::InitInputContext(this, mInputContext);

  // If the internal variable set by the config.trim_on_minimize pref has not
  // been initialized, and if this is the hidden window (conveniently created
  // before any visible windows, and after the profile has been initialized),
  // do some initialization work.
  if (sTrimOnMinimize == 2 && mWindowType == eWindowType_invisible) {
    // Our internal trim prevention logic is effective on 2K/XP at maintaining
    // the working set when windows are minimized, but on Vista and up it has
    // little to no effect. Since this feature has been the source of numerous
    // bugs over the years, disable it (sTrimOnMinimize=1) on Vista and up.
    sTrimOnMinimize =
      Preferences::GetBool("config.trim_on_minimize",
        IsVistaOrLater() ? 1 : 0);
    sSwitchKeyboardLayout =
      Preferences::GetBool("intl.keyboard.per_window_layout", false);
  }

  // Query for command button metric data for rendering the titlebar. We
  // only do this once on the first window.
  if (mWindowType == eWindowType_toplevel &&
      (!nsUXThemeData::sTitlebarInfoPopulatedThemed ||
       !nsUXThemeData::sTitlebarInfoPopulatedAero)) {
    nsUXThemeData::UpdateTitlebarInfo(mWnd);
  }
  return NS_OK;
}

// Close this nsWindow
NS_METHOD nsWindow::Destroy()
{
  // WM_DESTROY has already fired, avoid calling it twice
  if (mOnDestroyCalled)
    return NS_OK;

  // Don't destroy windows that have file pickers open, we'll tear these down
  // later once the picker is closed.
  mDestroyCalled = true;
  if (mPickerDisplayCount)
    return NS_OK;

  // During the destruction of all of our children, make sure we don't get deleted.
  nsCOMPtr<nsIWidget> kungFuDeathGrip(this);

  /**
   * On windows the LayerManagerOGL destructor wants the widget to be around for
   * cleanup. It also would like to have the HWND intact, so we nullptr it here.
   */
  DestroyLayerManager();

  /* We should clear our cached resources now and not wait for the GC to
   * delete the nsWindow. */
  ClearCachedResources();

  // The DestroyWindow function destroys the specified window. The function sends WM_DESTROY
  // and WM_NCDESTROY messages to the window to deactivate it and remove the keyboard focus
  // from it. The function also destroys the window's menu, flushes the thread message queue,
  // destroys timers, removes clipboard ownership, and breaks the clipboard viewer chain (if
  // the window is at the top of the viewer chain).
  //
  // If the specified window is a parent or owner window, DestroyWindow automatically destroys
  // the associated child or owned windows when it destroys the parent or owner window. The
  // function first destroys child or owned windows, and then it destroys the parent or owner
  // window.
  VERIFY(::DestroyWindow(mWnd));
  
  // Our windows can be subclassed which may prevent us receiving WM_DESTROY. If OnDestroy()
  // didn't get called, call it now.
  if (false == mOnDestroyCalled) {
    MSGResult msgResult;
    mWindowHook.Notify(mWnd, WM_DESTROY, 0, 0, msgResult);
    OnDestroy();
  }

  return NS_OK;
}

/**************************************************************
 *
 * SECTION: Window class utilities
 *
 * Utilities for calculating the proper window class name for
 * Create window.
 *
 **************************************************************/

void nsWindow::RegisterWindowClass(const nsString& aClassName, UINT aExtraStyle,
                                   LPWSTR aIconID)
{
  WNDCLASSW wc;
  if (::GetClassInfoW(nsToolkit::mDllInstance, aClassName.get(), &wc)) {
    // already registered
    return;
  }

  wc.style         = CS_DBLCLKS | aExtraStyle;
  wc.lpfnWndProc   = ::DefWindowProcW;
  wc.cbClsExtra    = 0;
  wc.cbWndExtra    = 0;
  wc.hInstance     = nsToolkit::mDllInstance;
  wc.hIcon         = aIconID ? ::LoadIconW(::GetModuleHandleW(nullptr), aIconID) : nullptr;
  wc.hCursor       = nullptr;
  wc.hbrBackground = mBrush;
  wc.lpszMenuName  = nullptr;
  wc.lpszClassName = aClassName.get();

  if (!::RegisterClassW(&wc)) {
    // For older versions of Win32 (i.e., not XP), the registration may
    // fail with aExtraStyle, so we have to re-register without it.
    wc.style = CS_DBLCLKS;
    ::RegisterClassW(&wc);
  }
}

static LPWSTR const gStockApplicationIcon = MAKEINTRESOURCEW(32512);

// Return the proper window class for everything except popups.
void nsWindow::GetWindowClass(nsString& aWindowClass)
{
  switch (mWindowType) {
  case eWindowType_invisible:
    aWindowClass.AssignLiteral(kClassNameHidden);
    RegisterWindowClass(aWindowClass, 0, gStockApplicationIcon);
    break;
  case eWindowType_dialog:
    aWindowClass.AssignLiteral(kClassNameDialog);
    RegisterWindowClass(aWindowClass, 0, 0);
    break;
  default:
    GetMainWindowClass(aWindowClass);
    RegisterWindowClass(aWindowClass, 0, gStockApplicationIcon);
    break;
  }
}

// Return the proper popup window class
void nsWindow::GetWindowPopupClass(nsString& aWindowClass)
{
  aWindowClass.AssignLiteral(kClassNameDropShadow);
  RegisterWindowClass(aWindowClass, CS_XP_DROPSHADOW, gStockApplicationIcon);
}

/**************************************************************
 *
 * SECTION: Window styles utilities
 *
 * Return the proper windows styles and extended styles.
 *
 **************************************************************/

// Return nsWindow styles
DWORD nsWindow::WindowStyle()
{
  DWORD style;

  switch (mWindowType) {
    case eWindowType_plugin:
    case eWindowType_plugin_ipc_chrome:
    case eWindowType_plugin_ipc_content:
    case eWindowType_child:
      style = WS_OVERLAPPED;
      break;

    case eWindowType_dialog:
      style = WS_OVERLAPPED | WS_BORDER | WS_DLGFRAME | WS_SYSMENU | DS_3DLOOK |
              DS_MODALFRAME | WS_CLIPCHILDREN;
      if (mBorderStyle != eBorderStyle_default)
        style |= WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
      break;

    case eWindowType_popup:
      style = WS_POPUP;
      if (!HasGlass()) {
        style |= WS_OVERLAPPED;
      }
      break;

    default:
      NS_ERROR("unknown border style");
      // fall through

    case eWindowType_toplevel:
    case eWindowType_invisible:
      style = WS_OVERLAPPED | WS_BORDER | WS_DLGFRAME | WS_SYSMENU |
              WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_CLIPCHILDREN;
      break;
  }

  if (mBorderStyle != eBorderStyle_default && mBorderStyle != eBorderStyle_all) {
    if (mBorderStyle == eBorderStyle_none || !(mBorderStyle & eBorderStyle_border))
      style &= ~WS_BORDER;

    if (mBorderStyle == eBorderStyle_none || !(mBorderStyle & eBorderStyle_title)) {
      style &= ~WS_DLGFRAME;
      style |= WS_POPUP;
      style &= ~WS_CHILD;
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

    if (IsPopupWithTitleBar()) {
      style |= WS_CAPTION;
      if (mBorderStyle & eBorderStyle_close) {
        style |= WS_SYSMENU;
      }
    }
  }

  VERIFY_WINDOW_STYLE(style);
  return style;
}

// Return nsWindow extended styles
DWORD nsWindow::WindowExStyle()
{
  switch (mWindowType)
  {
    case eWindowType_plugin:
    case eWindowType_plugin_ipc_chrome:
    case eWindowType_plugin_ipc_content:
    case eWindowType_child:
      return 0;

    case eWindowType_dialog:
      return WS_EX_WINDOWEDGE | WS_EX_DLGMODALFRAME;

    case eWindowType_popup:
    {
      DWORD extendedStyle = WS_EX_TOOLWINDOW;
      if (mPopupLevel == ePopupLevelTop)
        extendedStyle |= WS_EX_TOPMOST;
      return extendedStyle;
    }
    default:
      NS_ERROR("unknown border style");
      // fall through

    case eWindowType_toplevel:
    case eWindowType_invisible:
      return WS_EX_WINDOWEDGE;
  }
}

/**************************************************************
 *
 * SECTION: Window subclassing utilities
 *
 * Set or clear window subclasses on native windows. Used in
 * Create and Destroy.
 *
 **************************************************************/

// Subclass (or remove the subclass from) this component's nsWindow
void nsWindow::SubclassWindow(BOOL bState)
{
  if (bState) {
    if (!mWnd || !IsWindow(mWnd)) {
      NS_ERROR("Invalid window handle");
    }

    if (mUnicodeWidget) {
      mPrevWndProc =
        reinterpret_cast<WNDPROC>(
          SetWindowLongPtrW(mWnd,
                            GWLP_WNDPROC,
                            reinterpret_cast<LONG_PTR>(nsWindow::WindowProc)));
    } else {
      mPrevWndProc =
        reinterpret_cast<WNDPROC>(
          SetWindowLongPtrA(mWnd,
                            GWLP_WNDPROC,
                            reinterpret_cast<LONG_PTR>(nsWindow::WindowProc)));
    }
    NS_ASSERTION(mPrevWndProc, "Null standard window procedure");
    // connect the this pointer to the nsWindow handle
    WinUtils::SetNSWindowBasePtr(mWnd, this);
  } else {
    if (IsWindow(mWnd)) {
      if (mUnicodeWidget) {
        SetWindowLongPtrW(mWnd,
                          GWLP_WNDPROC,
                          reinterpret_cast<LONG_PTR>(mPrevWndProc));
      } else {
        SetWindowLongPtrA(mWnd,
                          GWLP_WNDPROC,
                          reinterpret_cast<LONG_PTR>(mPrevWndProc));
      }
    }
    WinUtils::SetNSWindowBasePtr(mWnd, nullptr);
    mPrevWndProc = nullptr;
  }
}

/**************************************************************
 *
 * SECTION: nsIWidget::SetParent, nsIWidget::GetParent
 *
 * Set or clear the parent widgets using window properties, and
 * handles calculating native parent handles.
 *
 **************************************************************/

// Get and set parent widgets
NS_IMETHODIMP nsWindow::SetParent(nsIWidget *aNewParent)
{
  mParent = aNewParent;

  nsCOMPtr<nsIWidget> kungFuDeathGrip(this);
  nsIWidget* parent = GetParent();
  if (parent) {
    parent->RemoveChild(this);
  }
  if (aNewParent) {
    ReparentNativeWidget(aNewParent);
    aNewParent->AddChild(this);
    return NS_OK;
  }
  if (mWnd) {
    // If we have no parent, SetParent should return the desktop.
    VERIFY(::SetParent(mWnd, nullptr));
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWindow::ReparentNativeWidget(nsIWidget* aNewParent)
{
  NS_PRECONDITION(aNewParent, "");

  mParent = aNewParent;
  if (mWindowType == eWindowType_popup) {
    return NS_OK;
  }
  HWND newParent = (HWND)aNewParent->GetNativeData(NS_NATIVE_WINDOW);
  NS_ASSERTION(newParent, "Parent widget has a null native window handle");
  if (newParent && mWnd) {
    ::SetParent(mWnd, newParent);
  }
  return NS_OK;
}

nsIWidget* nsWindow::GetParent(void)
{
  return GetParentWindow(false);
}

float nsWindow::GetDPI()
{
  HDC dc = ::GetDC(mWnd);
  if (!dc)
    return 96.0f;

  double heightInches = ::GetDeviceCaps(dc, VERTSIZE)/MM_PER_INCH_FLOAT;
  int heightPx = ::GetDeviceCaps(dc, VERTRES);
  ::ReleaseDC(mWnd, dc);
  if (heightInches < 0.25) {
    // Something's broken
    return 96.0f;
  }
  return float(heightPx/heightInches);
}

double nsWindow::GetDefaultScaleInternal()
{
  return WinUtils::LogToPhysFactor();
}

nsWindow*
nsWindow::GetParentWindow(bool aIncludeOwner)
{
  return static_cast<nsWindow*>(GetParentWindowBase(aIncludeOwner));
}

nsWindowBase*
nsWindow::GetParentWindowBase(bool aIncludeOwner)
{
  if (mIsTopWidgetWindow) {
    // Must use a flag instead of mWindowType to tell if the window is the
    // owned by the topmost widget, because a child window can be embedded inside
    // a HWND which is not associated with a nsIWidget.
    return nullptr;
  }

  // If this widget has already been destroyed, pretend we have no parent.
  // This corresponds to code in Destroy which removes the destroyed
  // widget from its parent's child list.
  if (mInDtor || mOnDestroyCalled)
    return nullptr;


  // aIncludeOwner set to true implies walking the parent chain to retrieve the
  // root owner. aIncludeOwner set to false implies the search will stop at the
  // true parent (default).
  nsWindow* widget = nullptr;
  if (mWnd) {
    HWND parent = nullptr;
    if (aIncludeOwner)
      parent = ::GetParent(mWnd);
    else
      parent = ::GetAncestor(mWnd, GA_PARENT);

    if (parent) {
      widget = WinUtils::GetNSWindowPtr(parent);
      if (widget) {
        // If the widget is in the process of being destroyed then
        // do NOT return it
        if (widget->mInDtor) {
          widget = nullptr;
        }
      }
    }
  }

  return static_cast<nsWindowBase*>(widget);
}
 
BOOL CALLBACK
nsWindow::EnumAllChildWindProc(HWND aWnd, LPARAM aParam)
{
  nsWindow *wnd = WinUtils::GetNSWindowPtr(aWnd);
  if (wnd) {
    ((nsWindow::WindowEnumCallback*)aParam)(wnd);
  }
  return TRUE;
}

BOOL CALLBACK
nsWindow::EnumAllThreadWindowProc(HWND aWnd, LPARAM aParam)
{
  nsWindow *wnd = WinUtils::GetNSWindowPtr(aWnd);
  if (wnd) {
    ((nsWindow::WindowEnumCallback*)aParam)(wnd);
  }
  EnumChildWindows(aWnd, EnumAllChildWindProc, aParam);
  return TRUE;
}

void
nsWindow::EnumAllWindows(WindowEnumCallback aCallback)
{
  EnumThreadWindows(GetCurrentThreadId(),
                    EnumAllThreadWindowProc,
                    (LPARAM)aCallback);
}

/**************************************************************
 *
 * SECTION: nsIWidget::Show
 *
 * Hide or show this component.
 *
 **************************************************************/

NS_METHOD nsWindow::Show(bool bState)
{
  if (mWindowType == eWindowType_popup) {
    // See bug 603793. When we try to draw D3D9/10 windows with a drop shadow
    // without the DWM on a secondary monitor, windows fails to composite
    // our windows correctly. We therefor switch off the drop shadow for
    // pop-up windows when the DWM is disabled and two monitors are
    // connected.
    if (HasBogusPopupsDropShadowOnMultiMonitor() &&
        WinUtils::GetMonitorCount() > 1 &&
        !nsUXThemeData::CheckForCompositor())
    {
      if (sDropShadowEnabled) {
        ::SetClassLongA(mWnd, GCL_STYLE, 0);
        sDropShadowEnabled = false;
      }
    } else {
      if (!sDropShadowEnabled) {
        ::SetClassLongA(mWnd, GCL_STYLE, CS_DROPSHADOW);
        sDropShadowEnabled = true;
      }
    }

    // WS_EX_COMPOSITED conflicts with the WS_EX_LAYERED style and causes
    // some popup menus to become invisible.
    LONG_PTR exStyle = ::GetWindowLongPtrW(mWnd, GWL_EXSTYLE);
    if (exStyle & WS_EX_LAYERED) {
      ::SetWindowLongPtrW(mWnd, GWL_EXSTYLE, exStyle & ~WS_EX_COMPOSITED);
    }
  }

  bool syncInvalidate = false;

  bool wasVisible = mIsVisible;
  // Set the status now so that anyone asking during ShowWindow or
  // SetWindowPos would get the correct answer.
  mIsVisible = bState;

  // We may have cached an out of date visible state. This can happen
  // when session restore sets the full screen mode.
  if (mIsVisible)
    mOldStyle |= WS_VISIBLE;
  else
    mOldStyle &= ~WS_VISIBLE;

  if (!mIsVisible && wasVisible) {
      ClearCachedResources();
  }

  if (mWnd) {
    if (bState) {
      if (!wasVisible && mWindowType == eWindowType_toplevel) {
        // speed up the initial paint after show for
        // top level windows:
        syncInvalidate = true;
        switch (mSizeMode) {
          case nsSizeMode_Fullscreen:
            ::ShowWindow(mWnd, SW_SHOW);
            break;
          case nsSizeMode_Maximized :
            ::ShowWindow(mWnd, SW_SHOWMAXIMIZED);
            break;
          case nsSizeMode_Minimized :
            ::ShowWindow(mWnd, SW_SHOWMINIMIZED);
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
              else if (GetWindowLongPtrW(wndAfter, GWL_EXSTYLE) & WS_EX_TOPMOST)
                wndAfter = HWND_TOP;
              ::SetWindowPos(mWnd, wndAfter, 0, 0, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE | 
                             SWP_NOMOVE | SWP_NOACTIVATE);
              GetAttention(2);
            }
            break;
        }
      } else {
        DWORD flags = SWP_NOSIZE | SWP_NOMOVE | SWP_SHOWWINDOW;
        if (wasVisible)
          flags |= SWP_NOZORDER;

        if (mWindowType == eWindowType_popup) {
          // ensure popups are the topmost of the TOPMOST
          // layer. Remember not to set the SWP_NOZORDER
          // flag as that might allow the taskbar to overlap
          // the popup.
          flags |= SWP_NOACTIVATE;
          HWND owner = ::GetWindow(mWnd, GW_OWNER);
          ::SetWindowPos(mWnd, owner ? 0 : HWND_TOPMOST, 0, 0, 0, 0, flags);
        } else {
          if (mWindowType == eWindowType_dialog && !CanTakeFocus())
            flags |= SWP_NOACTIVATE;

          ::SetWindowPos(mWnd, HWND_TOP, 0, 0, 0, 0, flags);
        }
      }

      if (!wasVisible && (mWindowType == eWindowType_toplevel || mWindowType == eWindowType_dialog)) {
        // when a toplevel window or dialog is shown, initialize the UI state
        ::SendMessageW(mWnd, WM_CHANGEUISTATE, MAKEWPARAM(UIS_INITIALIZE, UISF_HIDEFOCUS | UISF_HIDEACCEL), 0);
      }
    } else {
      // Clear contents to avoid ghosting of old content if we display
      // this window again.
      if (wasVisible && mTransparencyMode == eTransparencyTransparent) {
        ClearTranslucentWindow();
      }
      if (mWindowType != eWindowType_dialog) {
        ::ShowWindow(mWnd, SW_HIDE);
      } else {
        ::SetWindowPos(mWnd, 0, 0, 0, 0, 0, SWP_HIDEWINDOW | SWP_NOSIZE | SWP_NOMOVE |
                       SWP_NOZORDER | SWP_NOACTIVATE);
      }
    }
  }
  
#ifdef MOZ_XUL
  if (!wasVisible && bState) {
    Invalidate();
    if (syncInvalidate && !mInDtor && !mOnDestroyCalled) {
      ::UpdateWindow(mWnd);
    }
  }
#endif

  return NS_OK;
}

/**************************************************************
 *
 * SECTION: nsIWidget::IsVisible
 *
 * Returns the visibility state.
 *
 **************************************************************/

// Return true if the whether the component is visible, false otherwise
bool nsWindow::IsVisible() const
{
  return mIsVisible;
}

/**************************************************************
 *
 * SECTION: Window clipping utilities
 *
 * Used in Size and Move operations for setting the proper
 * window clipping regions for window transparency.
 *
 **************************************************************/

// XP and Vista visual styles sometimes require window clipping regions to be applied for proper
// transparency. These routines are called on size and move operations.
void nsWindow::ClearThemeRegion()
{
  if (IsVistaOrLater() && !HasGlass() &&
      (mWindowType == eWindowType_popup && !IsPopupWithTitleBar() &&
       (mPopupType == ePopupTypeTooltip || mPopupType == ePopupTypePanel))) {
    SetWindowRgn(mWnd, nullptr, false);
  }
}

void nsWindow::SetThemeRegion()
{
  // Popup types that have a visual styles region applied (bug 376408). This can be expanded
  // for other window types as needed. The regions are applied generically to the base window
  // so default constants are used for part and state. At some point we might need part and
  // state values from nsNativeThemeWin's GetThemePartAndState, but currently windows that
  // change shape based on state haven't come up.
  if (IsVistaOrLater() && !HasGlass() &&
      (mWindowType == eWindowType_popup && !IsPopupWithTitleBar() &&
       (mPopupType == ePopupTypeTooltip || mPopupType == ePopupTypePanel))) {
    HRGN hRgn = nullptr;
    RECT rect = {0,0,mBounds.width,mBounds.height};
    
    HDC dc = ::GetDC(mWnd);
    GetThemeBackgroundRegion(nsUXThemeData::GetTheme(eUXTooltip), dc, TTP_STANDARD, TS_NORMAL, &rect, &hRgn);
    if (hRgn) {
      if (!SetWindowRgn(mWnd, hRgn, false)) // do not delete or alter hRgn if accepted.
        DeleteObject(hRgn);
    }
    ::ReleaseDC(mWnd, dc);
  }
}

/**************************************************************
 *
 * SECTION: Touch and APZ-related functions
 *
 **************************************************************/

void nsWindow::ConfigureAPZCTreeManager()
{
  // We currently only enable APZ for E10s windows.
  if (!mozilla::BrowserTabsRemoteAutostart() || !mRequireOffMainThreadCompositing) {
    return;
  }

  nsBaseWidget::ConfigureAPZCTreeManager();

  // When APZ is enabled, we can actually enable raw touch events because we
  // have code that can deal with them properly. If APZ is not enabled, this
  // function doesn't get called, and |mGesture| will take care of touch-based
  // scrolling. Note that RegisterTouchWindow may still do nothing depending
  // on touch/pointer events prefs, and so it is possible to enable APZ without
  // also enabling touch support.
  RegisterTouchWindow();
}

void nsWindow::RegisterTouchWindow() {
  if (Preferences::GetInt("dom.w3c_touch_events.enabled", 0) ||
      gIsPointerEventsEnabled) {
    mTouchWindow = true;
    mGesture.RegisterTouchWindow(mWnd);
    ::EnumChildWindows(mWnd, nsWindow::RegisterTouchForDescendants, 0);
  }
}

BOOL CALLBACK nsWindow::RegisterTouchForDescendants(HWND aWnd, LPARAM aMsg) {
  nsWindow* win = WinUtils::GetNSWindowPtr(aWnd);
  if (win)
    win->mGesture.RegisterTouchWindow(aWnd);
  return TRUE;
}

/**************************************************************
 *
 * SECTION: nsIWidget::Move, nsIWidget::Resize,
 * nsIWidget::Size, nsIWidget::BeginResizeDrag
 *
 * Repositioning and sizing a window.
 *
 **************************************************************/

void
nsWindow::SetSizeConstraints(const SizeConstraints& aConstraints)
{
  SizeConstraints c = aConstraints;
  if (mWindowType != eWindowType_popup) {
    c.mMinSize.width = std::max(int32_t(::GetSystemMetrics(SM_CXMINTRACK)), c.mMinSize.width);
    c.mMinSize.height = std::max(int32_t(::GetSystemMetrics(SM_CYMINTRACK)), c.mMinSize.height);
  }

  nsBaseWidget::SetSizeConstraints(c);
}

// Move this component
NS_METHOD nsWindow::Move(double aX, double aY)
{
  if (mWindowType == eWindowType_toplevel ||
      mWindowType == eWindowType_dialog) {
    SetSizeMode(nsSizeMode_Normal);
  }

  // for top-level windows only, convert coordinates from global display pixels
  // (the "parent" coordinate space) to the window's device pixel space
  CSSToLayoutDeviceScale scale = BoundsUseDisplayPixels() ? GetDefaultScale()
                                    : CSSToLayoutDeviceScale(1.0);
  int32_t x = NSToIntRound(aX * scale.scale);
  int32_t y = NSToIntRound(aY * scale.scale);

  // Check to see if window needs to be moved first
  // to avoid a costly call to SetWindowPos. This check
  // can not be moved to the calling code in nsView, because
  // some platforms do not position child windows correctly

  // Only perform this check for non-popup windows, since the positioning can
  // in fact change even when the x/y do not.  We always need to perform the
  // check. See bug #97805 for details.
  if (mWindowType != eWindowType_popup && (mBounds.x == x) && (mBounds.y == y))
  {
    // Nothing to do, since it is already positioned correctly.
    return NS_OK;
  }

  mBounds.x = x;
  mBounds.y = y;

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
          if (x < 0 || x >= workArea.right || y < 0 || y >= workArea.bottom) {
            PR_LOG(gWindowsLog, PR_LOG_ALWAYS,
                   ("window moved to offscreen position\n"));
          }
        }
      ::ReleaseDC(mWnd, dc);
      }
    }
#endif
    ClearThemeRegion();

    UINT flags = SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSIZE;
    // Workaround SetWindowPos bug with D3D9. If our window has a clip
    // region, some drivers or OSes may incorrectly copy into the clipped-out
    // area.
    if (IsPlugin() &&
        (!mLayerManager || mLayerManager->GetBackendType() == LayersBackend::LAYERS_D3D9) &&
        mClipRects &&
        (mClipRectCount != 1 || !mClipRects[0].IsEqualInterior(nsIntRect(0, 0, mBounds.width, mBounds.height)))) {
      flags |= SWP_NOCOPYBITS;
    }
    VERIFY(::SetWindowPos(mWnd, nullptr, x, y, 0, 0, flags));

    SetThemeRegion();
  }
  NotifyRollupGeometryChange();
  return NS_OK;
}

// Resize this component
NS_METHOD nsWindow::Resize(double aWidth, double aHeight, bool aRepaint)
{
  // for top-level windows only, convert coordinates from global display pixels
  // (the "parent" coordinate space) to the window's device pixel space
  CSSToLayoutDeviceScale scale = BoundsUseDisplayPixels() ? GetDefaultScale()
                                    : CSSToLayoutDeviceScale(1.0);
  int32_t width = NSToIntRound(aWidth * scale.scale);
  int32_t height = NSToIntRound(aHeight * scale.scale);

  NS_ASSERTION((width >= 0) , "Negative width passed to nsWindow::Resize");
  NS_ASSERTION((height >= 0), "Negative height passed to nsWindow::Resize");

  ConstrainSize(&width, &height);

  // Avoid unnecessary resizing calls
  if (mBounds.width == width && mBounds.height == height) {
    if (aRepaint) {
      Invalidate();
    }
    return NS_OK;
  }

#ifdef MOZ_XUL
  if (eTransparencyTransparent == mTransparencyMode)
    ResizeTranslucentWindow(width, height);
#endif

  // Set cached value for lightweight and printing
  mBounds.width  = width;
  mBounds.height = height;

  if (mWnd) {
    UINT  flags = SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE;

    if (!aRepaint) {
      flags |= SWP_NOREDRAW;
    }

    ClearThemeRegion();
    VERIFY(::SetWindowPos(mWnd, nullptr, 0, 0,
                          width, GetHeight(height), flags));
    SetThemeRegion();
  }

  if (aRepaint)
    Invalidate();

  NotifyRollupGeometryChange();
  return NS_OK;
}

// Resize this component
NS_METHOD nsWindow::Resize(double aX, double aY, double aWidth, double aHeight, bool aRepaint)
{
  // for top-level windows only, convert coordinates from global display pixels
  // (the "parent" coordinate space) to the window's device pixel space
  CSSToLayoutDeviceScale scale = BoundsUseDisplayPixels() ? GetDefaultScale()
                                    : CSSToLayoutDeviceScale(1.0);
  int32_t x = NSToIntRound(aX * scale.scale);
  int32_t y = NSToIntRound(aY * scale.scale);
  int32_t width = NSToIntRound(aWidth * scale.scale);
  int32_t height = NSToIntRound(aHeight * scale.scale);

  NS_ASSERTION((width >= 0),  "Negative width passed to nsWindow::Resize");
  NS_ASSERTION((height >= 0), "Negative height passed to nsWindow::Resize");

  ConstrainSize(&width, &height);

  // Avoid unnecessary resizing calls
  if (mBounds.x == x && mBounds.y == y &&
      mBounds.width == width && mBounds.height == height) {
    if (aRepaint) {
      Invalidate();
    }
    return NS_OK;
  }

#ifdef MOZ_XUL
  if (eTransparencyTransparent == mTransparencyMode)
    ResizeTranslucentWindow(width, height);
#endif

  // Set cached value for lightweight and printing
  mBounds.x      = x;
  mBounds.y      = y;
  mBounds.width  = width;
  mBounds.height = height;

  if (mWnd) {
    UINT  flags = SWP_NOZORDER | SWP_NOACTIVATE;
    if (!aRepaint) {
      flags |= SWP_NOREDRAW;
    }

    ClearThemeRegion();
    VERIFY(::SetWindowPos(mWnd, nullptr, x, y,
                          width, GetHeight(height), flags));
    SetThemeRegion();
  }

  if (aRepaint)
    Invalidate();

  NotifyRollupGeometryChange();
  return NS_OK;
}

NS_IMETHODIMP
nsWindow::BeginResizeDrag(WidgetGUIEvent* aEvent,
                          int32_t aHorizontal,
                          int32_t aVertical)
{
  NS_ENSURE_ARG_POINTER(aEvent);

  if (aEvent->mClass != eMouseEventClass) {
    // you can only begin a resize drag with a mouse event
    return NS_ERROR_INVALID_ARG;
  }

  if (aEvent->AsMouseEvent()->button != WidgetMouseEvent::eLeftButton) {
    // you can only begin a resize drag with the left mouse button
    return NS_ERROR_INVALID_ARG;
  }

  // work out what sizemode we're talking about
  WPARAM syscommand;
  if (aVertical < 0) {
    if (aHorizontal < 0) {
      syscommand = SC_SIZE | WMSZ_TOPLEFT;
    } else if (aHorizontal == 0) {
      syscommand = SC_SIZE | WMSZ_TOP;
    } else {
      syscommand = SC_SIZE | WMSZ_TOPRIGHT;
    }
  } else if (aVertical == 0) {
    if (aHorizontal < 0) {
      syscommand = SC_SIZE | WMSZ_LEFT;
    } else if (aHorizontal == 0) {
      return NS_ERROR_INVALID_ARG;
    } else {
      syscommand = SC_SIZE | WMSZ_RIGHT;
    }
  } else {
    if (aHorizontal < 0) {
      syscommand = SC_SIZE | WMSZ_BOTTOMLEFT;
    } else if (aHorizontal == 0) {
      syscommand = SC_SIZE | WMSZ_BOTTOM;
    } else {
      syscommand = SC_SIZE | WMSZ_BOTTOMRIGHT;
    }
  }

  // resizing doesn't work if the mouse is already captured
  CaptureMouse(false);

  // find the top-level window
  HWND toplevelWnd = WinUtils::GetTopLevelHWND(mWnd, true);

  // tell Windows to start the resize
  ::PostMessage(toplevelWnd, WM_SYSCOMMAND, syscommand,
                POINTTOPOINTS(aEvent->refPoint));

  return NS_OK;
}

/**************************************************************
 *
 * SECTION: Window Z-order and state.
 *
 * nsIWidget::PlaceBehind, nsIWidget::SetSizeMode,
 * nsIWidget::ConstrainPosition
 *
 * Z-order, positioning, restore, minimize, and maximize.
 *
 **************************************************************/

// Position the window behind the given window
NS_METHOD nsWindow::PlaceBehind(nsTopLevelWidgetZPlacement aPlacement,
                                nsIWidget *aWidget, bool aActivate)
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
    else if (!(GetWindowLongPtrW(wndAfter, GWL_EXSTYLE) & WS_EX_TOPMOST))
      behind = wndAfter;
    flags |= SWP_NOACTIVATE;
  }

  ::SetWindowPos(mWnd, behind, 0, 0, 0, 0, flags);
  return NS_OK;
}

// Maximize, minimize or restore the window.
NS_IMETHODIMP nsWindow::SetSizeMode(int32_t aMode) {

  nsresult rv;

  // Let's not try and do anything if we're already in that state.
  // (This is needed to prevent problems when calling window.minimize(), which
  // calls us directly, and then the OS triggers another call to us.)
  if (aMode == mSizeMode)
    return NS_OK;

  // save the requested state
  mLastSizeMode = mSizeMode;
  rv = nsBaseWidget::SetSizeMode(aMode);
  if (NS_SUCCEEDED(rv) && mIsVisible) {
    int mode;

    switch (aMode) {
      case nsSizeMode_Fullscreen :
        mode = SW_SHOW;
        break;

      case nsSizeMode_Maximized :
        mode = SW_MAXIMIZE;
        break;

      case nsSizeMode_Minimized :
        // Using SW_SHOWMINIMIZED prevents the working set from being trimmed but
        // keeps the window active in the tray. So after the window is minimized,
        // windows will fire WM_WINDOWPOSCHANGED (OnWindowPosChanged) at which point
        // we will do some additional processing to get the active window set right.
        // If sTrimOnMinimize is set, we let windows handle minimization normally
        // using SW_MINIMIZE.
        mode = sTrimOnMinimize ? SW_MINIMIZE : SW_SHOWMINIMIZED;
        break;

      default :
        mode = SW_RESTORE;
    }

    WINDOWPLACEMENT pl;
    pl.length = sizeof(pl);
    ::GetWindowPlacement(mWnd, &pl);
    // Don't call ::ShowWindow if we're trying to "restore" a window that is
    // already in a normal state.  Prevents a bug where snapping to one side
    // of the screen and then minimizing would cause Windows to forget our
    // window's correct restored position/size.
    if( !(pl.showCmd == SW_SHOWNORMAL && mode == SW_RESTORE) ) {
      ::ShowWindow(mWnd, mode);
    }
    // we activate here to ensure that the right child window is focused
    if (mode == SW_MAXIMIZE || mode == SW_SHOW)
      DispatchFocusToTopLevelWindow(true);
  }
  return rv;
}

// Constrain a potential move to fit onscreen
// Position (aX, aY) is specified in Windows screen (logical) pixels
NS_METHOD nsWindow::ConstrainPosition(bool aAllowSlop,
                                      int32_t *aX, int32_t *aY)
{
  if (!mIsTopWidgetWindow) // only a problem for top-level windows
    return NS_OK;

  double dpiScale = GetDefaultScale().scale;

  // we need to use the window size in logical screen pixels
  int32_t logWidth = std::max<int32_t>(NSToIntRound(mBounds.width / dpiScale), 1);
  int32_t logHeight = std::max<int32_t>(NSToIntRound(mBounds.height / dpiScale), 1);

  /* get our playing field. use the current screen, or failing that
    for any reason, use device caps for the default screen. */
  RECT screenRect;

  nsCOMPtr<nsIScreenManager> screenmgr = do_GetService(sScreenManagerContractID);
  if (screenmgr) {
    nsCOMPtr<nsIScreen> screen;
    int32_t left, top, width, height;

    screenmgr->ScreenForRect(*aX, *aY, logWidth, logHeight,
                             getter_AddRefs(screen));
    if (screen) {
      if (mSizeMode != nsSizeMode_Fullscreen) {
        // For normalized windows, use the desktop work area.
        screen->GetAvailRectDisplayPix(&left, &top, &width, &height);
      } else {
        // For full screen windows, use the desktop.
        screen->GetRectDisplayPix(&left, &top, &width, &height);
      }
      screenRect.left = left;
      screenRect.right = left + width;
      screenRect.top = top;
      screenRect.bottom = top + height;
    }
  } else {
    if (mWnd) {
      HDC dc = ::GetDC(mWnd);
      if (dc) {
        if (::GetDeviceCaps(dc, TECHNOLOGY) == DT_RASDISPLAY) {
          if (mSizeMode != nsSizeMode_Fullscreen) {
            ::SystemParametersInfo(SPI_GETWORKAREA, 0, &screenRect, 0);
          } else {
            screenRect.left = screenRect.top = 0;
            screenRect.right = GetSystemMetrics(SM_CXFULLSCREEN);
            screenRect.bottom = GetSystemMetrics(SM_CYFULLSCREEN);
          }
        }
        ::ReleaseDC(mWnd, dc);
      }
    }
  }

  if (aAllowSlop) {
    if (*aX < screenRect.left - logWidth + kWindowPositionSlop)
      *aX = screenRect.left - logWidth + kWindowPositionSlop;
    else if (*aX >= screenRect.right - kWindowPositionSlop)
      *aX = screenRect.right - kWindowPositionSlop;

    if (*aY < screenRect.top - logHeight + kWindowPositionSlop)
      *aY = screenRect.top - logHeight + kWindowPositionSlop;
    else if (*aY >= screenRect.bottom - kWindowPositionSlop)
      *aY = screenRect.bottom - kWindowPositionSlop;

  } else {

    if (*aX < screenRect.left)
      *aX = screenRect.left;
    else if (*aX >= screenRect.right - logWidth)
      *aX = screenRect.right - logWidth;

    if (*aY < screenRect.top)
      *aY = screenRect.top;
    else if (*aY >= screenRect.bottom - logHeight)
      *aY = screenRect.bottom - logHeight;
  }

  return NS_OK;
}

/**************************************************************
 *
 * SECTION: nsIWidget::Enable, nsIWidget::IsEnabled
 *
 * Enabling and disabling the widget.
 *
 **************************************************************/

// Enable/disable this component
NS_METHOD nsWindow::Enable(bool bState)
{
  if (mWnd) {
    ::EnableWindow(mWnd, bState);
  }
  return NS_OK;
}

// Return the current enable state
bool nsWindow::IsEnabled() const
{
  return !mWnd ||
         (::IsWindowEnabled(mWnd) &&
          ::IsWindowEnabled(::GetAncestor(mWnd, GA_ROOT)));
}


/**************************************************************
 *
 * SECTION: nsIWidget::SetFocus
 *
 * Give the focus to this widget.
 *
 **************************************************************/

NS_METHOD nsWindow::SetFocus(bool aRaise)
{
  if (mWnd) {
#ifdef WINSTATE_DEBUG_OUTPUT
    if (mWnd == WinUtils::GetTopLevelHWND(mWnd)) {
      PR_LOG(gWindowsLog, PR_LOG_ALWAYS,
             ("*** SetFocus: [  top] raise=%d\n", aRaise));
    } else {
      PR_LOG(gWindowsLog, PR_LOG_ALWAYS,
             ("*** SetFocus: [child] raise=%d\n", aRaise));
    }
#endif
    // Uniconify, if necessary
    HWND toplevelWnd = WinUtils::GetTopLevelHWND(mWnd);
    if (aRaise && ::IsIconic(toplevelWnd)) {
      ::ShowWindow(toplevelWnd, SW_RESTORE);
    }
    ::SetFocus(mWnd);
  }
  return NS_OK;
}


/**************************************************************
 *
 * SECTION: Bounds
 *
 * GetBounds, GetClientBounds, GetScreenBounds,
 * GetRestoredBounds, GetClientOffset
 * SetDrawsInTitlebar, GetNonClientMargins, SetNonClientMargins
 *
 * Bound calculations.
 *
 **************************************************************/

// Return the window's full dimensions in screen coordinates.
// If the window has a parent, converts the origin to an offset
// of the parent's screen origin.
NS_METHOD nsWindow::GetBounds(nsIntRect &aRect)
{
  if (mWnd) {
    RECT r;
    VERIFY(::GetWindowRect(mWnd, &r));

    // assign size
    aRect.width  = r.right - r.left;
    aRect.height = r.bottom - r.top;

    // popup window bounds' are in screen coordinates, not relative to parent
    // window
    if (mWindowType == eWindowType_popup) {
      aRect.x = r.left;
      aRect.y = r.top;
      return NS_OK;
    }

    // chrome on parent:
    //  ___      5,5   (chrome start)
    // |  ____   10,10 (client start)
    // | |  ____ 20,20 (child start)
    // | | |
    // 20,20 - 5,5 = 15,15 (??)
    // minus GetClientOffset:
    // 15,15 - 5,5 = 10,10
    //
    // no chrome on parent:
    //  ______   10,10 (win start)
    // |  ____   20,20 (child start)
    // | |
    // 20,20 - 10,10 = 10,10
    //
    // walking the chain:
    //  ___      5,5   (chrome start)
    // |  ___    10,10 (client start)
    // | |  ___  20,20 (child start)
    // | | |  __ 30,30 (child start)
    // | | | |
    // 30,30 - 20,20 = 10,10 (offset from second child to first)
    // 20,20 - 5,5 = 15,15 + 10,10 = 25,25 (??)
    // minus GetClientOffset:
    // 25,25 - 5,5 = 20,20 (offset from second child to parent client)

    // convert coordinates if parent exists
    HWND parent = ::GetParent(mWnd);
    if (parent) {
      RECT pr;
      VERIFY(::GetWindowRect(parent, &pr));
      r.left -= pr.left;
      r.top  -= pr.top;
      // adjust for chrome
      nsWindow* pWidget = static_cast<nsWindow*>(GetParent());
      if (pWidget && pWidget->IsTopLevelWidget()) {
        nsIntPoint clientOffset = pWidget->GetClientOffset();
        r.left -= clientOffset.x;
        r.top  -= clientOffset.y;
      }
    }
    aRect.x = r.left;
    aRect.y = r.top;
  } else {
    aRect = mBounds;
  }
  return NS_OK;
}

// Get this component dimension
NS_METHOD nsWindow::GetClientBounds(nsIntRect &aRect)
{
  if (mWnd) {
    RECT r;
    VERIFY(::GetClientRect(mWnd, &r));

    nsIntRect bounds;
    GetBounds(bounds);
    aRect.MoveTo(bounds.TopLeft() + GetClientOffset());
    aRect.width  = r.right - r.left;
    aRect.height = r.bottom - r.top;

  } else {
    aRect.SetRect(0,0,0,0);
  }
  return NS_OK;
}

// Like GetBounds, but don't offset by the parent
NS_METHOD nsWindow::GetScreenBounds(nsIntRect &aRect)
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

NS_METHOD nsWindow::GetRestoredBounds(nsIntRect &aRect)
{
  if (SizeMode() == nsSizeMode_Normal) {
    return GetScreenBounds(aRect);
  }
  if (!mWnd) {
    return NS_ERROR_FAILURE;
  }

  WINDOWPLACEMENT pl = { sizeof(WINDOWPLACEMENT) };
  VERIFY(::GetWindowPlacement(mWnd, &pl));
  const RECT& r = pl.rcNormalPosition;

  HMONITOR monitor = ::MonitorFromWindow(mWnd, MONITOR_DEFAULTTONULL);
  if (!monitor) {
    return NS_ERROR_FAILURE;
  }
  MONITORINFO mi = { sizeof(MONITORINFO) };
  VERIFY(::GetMonitorInfo(monitor, &mi));

  aRect.SetRect(r.left, r.top, r.right - r.left, r.bottom - r.top);
  aRect.MoveBy(mi.rcWork.left - mi.rcMonitor.left,
               mi.rcWork.top - mi.rcMonitor.top);
  return NS_OK;
}

// return the x,y offset of the client area from the origin
// of the window. If the window is borderless returns (0,0).
nsIntPoint nsWindow::GetClientOffset()
{
  if (!mWnd) {
    return nsIntPoint(0, 0);
  }

  RECT r1;
  GetWindowRect(mWnd, &r1);
  LayoutDeviceIntPoint pt = WidgetToScreenOffset();
  return nsIntPoint(pt.x - r1.left, pt.y - r1.top);
}

void
nsWindow::SetDrawsInTitlebar(bool aState)
{
  nsWindow * window = GetTopLevelWindow(true);
  if (window && window != this) {
    return window->SetDrawsInTitlebar(aState);
  }

  if (aState) {
    // top, right, bottom, left for nsIntMargin
    nsIntMargin margins(0, -1, -1, -1);
    SetNonClientMargins(margins);
  }
  else {
    nsIntMargin margins(-1, -1, -1, -1);
    SetNonClientMargins(margins);
  }
}

NS_IMETHODIMP
nsWindow::GetNonClientMargins(nsIntMargin &margins)
{
  nsWindow * window = GetTopLevelWindow(true);
  if (window && window != this) {
    return window->GetNonClientMargins(margins);
  }

  if (mCustomNonClient) {
    margins = mNonClientMargins;
    return NS_OK;
  }

  margins.top = GetSystemMetrics(SM_CYCAPTION);
  margins.bottom = GetSystemMetrics(SM_CYFRAME);
  margins.top += margins.bottom;
  margins.left = margins.right = GetSystemMetrics(SM_CXFRAME);

  return NS_OK;
}

void
nsWindow::ResetLayout()
{
  // This will trigger a frame changed event, triggering
  // nc calc size and a sizemode gecko event.
  SetWindowPos(mWnd, 0, 0, 0, 0, 0,
               SWP_FRAMECHANGED|SWP_NOACTIVATE|SWP_NOMOVE|
               SWP_NOOWNERZORDER|SWP_NOSIZE|SWP_NOZORDER);

  // If hidden, just send the frame changed event for now.
  if (!mIsVisible)
    return;

  // Send a gecko size event to trigger reflow.
  RECT clientRc = {0};
  GetClientRect(mWnd, &clientRc);
  nsIntRect evRect(WinUtils::ToIntRect(clientRc));
  OnResize(evRect);

  // Invalidate and update
  Invalidate();
}

// Internally track the caption status via a window property. Required
// due to our internal handling of WM_NCACTIVATE when custom client
// margins are set.
static const wchar_t kManageWindowInfoProperty[] = L"ManageWindowInfoProperty";
typedef BOOL (WINAPI *GetWindowInfoPtr)(HWND hwnd, PWINDOWINFO pwi);
static GetWindowInfoPtr sGetWindowInfoPtrStub = nullptr;

BOOL WINAPI
GetWindowInfoHook(HWND hWnd, PWINDOWINFO pwi)
{
  if (!sGetWindowInfoPtrStub) {
    NS_ASSERTION(FALSE, "Something is horribly wrong in GetWindowInfoHook!");
    return FALSE;
  }
  int windowStatus = 
    reinterpret_cast<LONG_PTR>(GetPropW(hWnd, kManageWindowInfoProperty));
  // No property set, return the default data.
  if (!windowStatus)
    return sGetWindowInfoPtrStub(hWnd, pwi);
  // Call GetWindowInfo and update dwWindowStatus with our
  // internally tracked value. 
  BOOL result = sGetWindowInfoPtrStub(hWnd, pwi);
  if (result && pwi)
    pwi->dwWindowStatus = (windowStatus == 1 ? 0 : WS_ACTIVECAPTION);
  return result;
}

void
nsWindow::UpdateGetWindowInfoCaptionStatus(bool aActiveCaption)
{
  if (!mWnd)
    return;

  if (!sGetWindowInfoPtrStub) {
    sUser32Intercept.Init("user32.dll");
    if (!sUser32Intercept.AddHook("GetWindowInfo", reinterpret_cast<intptr_t>(GetWindowInfoHook),
                                  (void**) &sGetWindowInfoPtrStub))
      return;
  }
  // Update our internally tracked caption status
  SetPropW(mWnd, kManageWindowInfoProperty, 
    reinterpret_cast<HANDLE>(static_cast<int>(aActiveCaption) + 1));
}

/**
 * Called when the window layout changes: full screen mode transitions,
 * theme changes, and composition changes. Calculates the new non-client
 * margins and fires off a frame changed event, which triggers an nc calc
 * size windows event, kicking the changes in.
 *
 * The offsets calculated here are based on the value of `mNonClientMargins`
 * which is specified in the "chromemargins" attribute of the window.  For
 * each margin, the value specified has the following meaning:
 *    -1 - leave the default frame in place
 *     0 - remove the frame
 *    >0 - frame size equals min(0, (default frame size - margin value))
 *
 * This function calculates and populates `mNonClientOffset`.
 * In our processing of `WM_NCCALCSIZE`, the frame size will be calculated
 * as (default frame size - offset).  For example, if the left frame should
 * be 1 pixel narrower than the default frame size, `mNonClientOffset.left`
 * will equal 1.
 *
 * For maximized, fullscreen, and minimized windows, the values stored in
 * `mNonClientMargins` are ignored, and special processing takes place.
 *
 * For non-glass windows, we only allow frames to be their default size
 * or removed entirely.
 */
bool
nsWindow::UpdateNonClientMargins(int32_t aSizeMode, bool aReflowWindow)
{
  if (!mCustomNonClient)
    return false;

  if (aSizeMode == -1) {
    aSizeMode = mSizeMode;
  }

  bool hasCaption = (mBorderStyle
                    & (eBorderStyle_all
                     | eBorderStyle_title
                     | eBorderStyle_menu
                     | eBorderStyle_default));

  // mCaptionHeight is the default size of the NC area at
  // the top of the window. If the window has a caption,
  // the size is calculated as the sum of:
  //      SM_CYFRAME        - The thickness of the sizing border
  //                          around a resizable window
  //      SM_CXPADDEDBORDER - The amount of border padding
  //                          for captioned windows
  //      SM_CYCAPTION      - The height of the caption area
  //
  // If the window does not have a caption, mCaptionHeight will be equal to
  // `GetSystemMetrics(SM_CYFRAME)`
  mCaptionHeight = GetSystemMetrics(SM_CYFRAME)
                 + (hasCaption ? GetSystemMetrics(SM_CYCAPTION)
                                 + GetSystemMetrics(SM_CXPADDEDBORDER)
                               : 0);

  // mHorResizeMargin is the size of the default NC areas on the
  // left and right sides of our window.  It is calculated as
  // the sum of:
  //      SM_CXFRAME        - The thickness of the sizing border
  //      SM_CXPADDEDBORDER - The amount of border padding
  //                          for captioned windows
  //
  // If the window does not have a caption, mHorResizeMargin will be equal to
  // `GetSystemMetrics(SM_CXFRAME)`
  mHorResizeMargin = GetSystemMetrics(SM_CXFRAME)
                   + (hasCaption ? GetSystemMetrics(SM_CXPADDEDBORDER) : 0);

  // mVertResizeMargin is the size of the default NC area at the
  // bottom of the window. It is calculated as the sum of:
  //      SM_CYFRAME        - The thickness of the sizing border
  //      SM_CXPADDEDBORDER - The amount of border padding
  //                          for captioned windows.
  //
  // If the window does not have a caption, mVertResizeMargin will be equal to
  // `GetSystemMetrics(SM_CYFRAME)`
  mVertResizeMargin = GetSystemMetrics(SM_CYFRAME)
                    + (hasCaption ? GetSystemMetrics(SM_CXPADDEDBORDER) : 0);

  if (aSizeMode == nsSizeMode_Minimized) {
    // Use default frame size for minimized windows
    mNonClientOffset.top = 0;
    mNonClientOffset.left = 0;
    mNonClientOffset.right = 0;
    mNonClientOffset.bottom = 0;
  } else if (aSizeMode == nsSizeMode_Fullscreen) {
    // Remove the default frame from the top of our fullscreen window.  This
    // makes the whole caption part of our client area, allowing us to draw
    // in the whole caption area.  Additionally remove the default frame from
    // the left, right, and bottom.
    mNonClientOffset.top = mCaptionHeight;
    mNonClientOffset.bottom = mVertResizeMargin;
    mNonClientOffset.left = mHorResizeMargin;
    mNonClientOffset.right = mHorResizeMargin;
  } else if (aSizeMode == nsSizeMode_Maximized) {
    // Remove the default frame from the top of our maximized window.  This
    // makes the whole caption part of our client area, allowing us to draw
    // in the whole caption area.  Use default frame size on left, right, and
    // bottom. The reason this works is that, for maximized windows,
    // Windows positions them so that their frames fall off the screen.
    // This gives the illusion of windows having no frames when they are
    // maximized.  If we try to mess with the frame sizes by setting these
    // offsets to positive values, our client area will fall off the screen.
    mNonClientOffset.top = mCaptionHeight;
    mNonClientOffset.bottom = 0;
    mNonClientOffset.left = 0;
    mNonClientOffset.right = 0;

    APPBARDATA appBarData;
    appBarData.cbSize = sizeof(appBarData);
    UINT taskbarState = SHAppBarMessage(ABM_GETSTATE, &appBarData);
    if (ABS_AUTOHIDE & taskbarState) {
      UINT edge = -1;
      appBarData.hWnd = FindWindow(L"Shell_TrayWnd", nullptr);
      if (appBarData.hWnd) {
        HMONITOR taskbarMonitor = ::MonitorFromWindow(appBarData.hWnd,
                                                      MONITOR_DEFAULTTOPRIMARY);
        HMONITOR windowMonitor = ::MonitorFromWindow(mWnd,
                                                     MONITOR_DEFAULTTONEAREST);
        if (taskbarMonitor == windowMonitor) {
          SHAppBarMessage(ABM_GETTASKBARPOS, &appBarData);
          edge = appBarData.uEdge;
        }
      }

      if (ABE_LEFT == edge) {
        mNonClientOffset.left -= 1;
      } else if (ABE_RIGHT == edge) {
        mNonClientOffset.right -= 1;
      } else if (ABE_BOTTOM == edge || ABE_TOP == edge) {
        mNonClientOffset.bottom -= 1;
      }
    }
  } else {
    bool glass = nsUXThemeData::CheckForCompositor();

    // We're dealing with a "normal" window (not maximized, minimized, or
    // fullscreen), so process `mNonClientMargins` and set `mNonClientOffset`
    // accordingly.
    //
    // Setting `mNonClientOffset` to 0 has the effect of leaving the default
    // frame intact.  Setting it to a value greater than 0 reduces the frame
    // size by that amount.

    if (mNonClientMargins.top > 0 && glass) {
      mNonClientOffset.top = std::min(mCaptionHeight, mNonClientMargins.top);
    } else if (mNonClientMargins.top == 0) {
      mNonClientOffset.top = mCaptionHeight;
    } else {
      mNonClientOffset.top = 0;
    }

    if (mNonClientMargins.bottom > 0 && glass) {
      mNonClientOffset.bottom = std::min(mVertResizeMargin, mNonClientMargins.bottom);
    } else if (mNonClientMargins.bottom == 0) {
      mNonClientOffset.bottom = mVertResizeMargin;
    } else {
      mNonClientOffset.bottom = 0;
    }

    if (mNonClientMargins.left > 0 && glass) {
      mNonClientOffset.left = std::min(mHorResizeMargin, mNonClientMargins.left);
    } else if (mNonClientMargins.left == 0) {
      mNonClientOffset.left = mHorResizeMargin;
    } else {
      mNonClientOffset.left = 0;
    }

    if (mNonClientMargins.right > 0 && glass) {
      mNonClientOffset.right = std::min(mHorResizeMargin, mNonClientMargins.right);
    } else if (mNonClientMargins.right == 0) {
      mNonClientOffset.right = mHorResizeMargin;
    } else {
      mNonClientOffset.right = 0;
    }
  }

  if (aReflowWindow) {
    // Force a reflow of content based on the new client
    // dimensions.
    ResetLayout();
  }

  return true;
}

NS_IMETHODIMP
nsWindow::SetNonClientMargins(nsIntMargin &margins)
{
  if (!mIsTopWidgetWindow ||
      mBorderStyle & eBorderStyle_none)
    return NS_ERROR_INVALID_ARG;

  if (mHideChrome) {
    mFutureMarginsOnceChromeShows = margins;
    mFutureMarginsToUse = true;
    return NS_OK;
  }
  mFutureMarginsToUse = false;

  // Request for a reset
  if (margins.top == -1 && margins.left == -1 &&
      margins.right == -1 && margins.bottom == -1) {
    mCustomNonClient = false;
    mNonClientMargins = margins;
    // Force a reflow of content based on the new client
    // dimensions.
    ResetLayout();

    int windowStatus =
      reinterpret_cast<LONG_PTR>(GetPropW(mWnd, kManageWindowInfoProperty));
    if (windowStatus) {
      ::SendMessageW(mWnd, WM_NCACTIVATE, 1 != windowStatus, 0);
    }

    return NS_OK;
  }

  if (margins.top < -1 || margins.bottom < -1 ||
      margins.left < -1 || margins.right < -1)
    return NS_ERROR_INVALID_ARG;

  mNonClientMargins = margins;
  mCustomNonClient = true;
  if (!UpdateNonClientMargins()) {
    NS_WARNING("UpdateNonClientMargins failed!");
    return NS_OK;
  }

  return NS_OK;
}

void
nsWindow::InvalidateNonClientRegion()
{
  // +-+-----------------------+-+
  // | | app non-client chrome | |
  // | +-----------------------+ |
  // | |   app client chrome   | | }
  // | +-----------------------+ | }
  // | |      app content      | | } area we don't want to invalidate
  // | +-----------------------+ | }
  // | |   app client chrome   | | }
  // | +-----------------------+ | 
  // +---------------------------+ <
  //  ^                         ^    windows non-client chrome
  // client area = app *
  RECT rect;
  GetWindowRect(mWnd, &rect);
  MapWindowPoints(nullptr, mWnd, (LPPOINT)&rect, 2);
  HRGN winRgn = CreateRectRgnIndirect(&rect);

  // Subtract app client chrome and app content leaving
  // windows non-client chrome and app non-client chrome
  // in winRgn.
  GetWindowRect(mWnd, &rect);
  rect.top += mCaptionHeight;
  rect.right -= mHorResizeMargin;
  rect.bottom -= mHorResizeMargin;
  rect.left += mVertResizeMargin;
  MapWindowPoints(nullptr, mWnd, (LPPOINT)&rect, 2);
  HRGN clientRgn = CreateRectRgnIndirect(&rect);
  CombineRgn(winRgn, winRgn, clientRgn, RGN_DIFF);
  DeleteObject(clientRgn);

  // triggers ncpaint and paint events for the two areas
  RedrawWindow(mWnd, nullptr, winRgn, RDW_FRAME | RDW_INVALIDATE);
  DeleteObject(winRgn);
}

HRGN
nsWindow::ExcludeNonClientFromPaintRegion(HRGN aRegion)
{
  RECT rect;
  HRGN rgn = nullptr;
  if (aRegion == (HRGN)1) { // undocumented value indicating a full refresh
    GetWindowRect(mWnd, &rect);
    rgn = CreateRectRgnIndirect(&rect);
  } else {
    rgn = aRegion;
  }
  GetClientRect(mWnd, &rect);
  MapWindowPoints(mWnd, nullptr, (LPPOINT)&rect, 2);
  HRGN nonClientRgn = CreateRectRgnIndirect(&rect);
  CombineRgn(rgn, rgn, nonClientRgn, RGN_DIFF);
  DeleteObject(nonClientRgn);
  return rgn;
}

/**************************************************************
 *
 * SECTION: nsIWidget::SetBackgroundColor
 *
 * Sets the window background paint color.
 *
 **************************************************************/

void nsWindow::SetBackgroundColor(const nscolor &aColor)
{
  if (mBrush)
    ::DeleteObject(mBrush);

  mBrush = ::CreateSolidBrush(NSRGB_2_COLOREF(aColor));
  if (mWnd != nullptr) {
    ::SetClassLongPtrW(mWnd, GCLP_HBRBACKGROUND, (LONG_PTR)mBrush);
  }
}

/**************************************************************
 *
 * SECTION: nsIWidget::SetCursor
 *
 * SetCursor and related utilities for manging cursor state.
 *
 **************************************************************/

// Set this component cursor
NS_METHOD nsWindow::SetCursor(nsCursor aCursor)
{
  // Only change cursor if it's changing

  //XXX mCursor isn't always right.  Scrollbars and others change it, too.
  //XXX If we want this optimization we need a better way to do it.
  //if (aCursor != mCursor) {
  HCURSOR newCursor = nullptr;

  switch (aCursor) {
    case eCursor_select:
      newCursor = ::LoadCursor(nullptr, IDC_IBEAM);
      break;

    case eCursor_wait:
      newCursor = ::LoadCursor(nullptr, IDC_WAIT);
      break;

    case eCursor_hyperlink:
    {
      newCursor = ::LoadCursor(nullptr, IDC_HAND);
      break;
    }

    case eCursor_standard:
    case eCursor_context_menu: // XXX See bug 258960.
      newCursor = ::LoadCursor(nullptr, IDC_ARROW);
      break;

    case eCursor_n_resize:
    case eCursor_s_resize:
      newCursor = ::LoadCursor(nullptr, IDC_SIZENS);
      break;

    case eCursor_w_resize:
    case eCursor_e_resize:
      newCursor = ::LoadCursor(nullptr, IDC_SIZEWE);
      break;

    case eCursor_nw_resize:
    case eCursor_se_resize:
      newCursor = ::LoadCursor(nullptr, IDC_SIZENWSE);
      break;

    case eCursor_ne_resize:
    case eCursor_sw_resize:
      newCursor = ::LoadCursor(nullptr, IDC_SIZENESW);
      break;

    case eCursor_crosshair:
      newCursor = ::LoadCursor(nullptr, IDC_CROSS);
      break;

    case eCursor_move:
      newCursor = ::LoadCursor(nullptr, IDC_SIZEALL);
      break;

    case eCursor_help:
      newCursor = ::LoadCursor(nullptr, IDC_HELP);
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
      newCursor = ::LoadCursor(nullptr, IDC_APPSTARTING);
      break;

    case eCursor_zoom_in:
      newCursor = ::LoadCursor(nsToolkit::mDllInstance, MAKEINTRESOURCE(IDC_ZOOMIN));
      break;

    case eCursor_zoom_out:
      newCursor = ::LoadCursor(nsToolkit::mDllInstance, MAKEINTRESOURCE(IDC_ZOOMOUT));
      break;

    case eCursor_not_allowed:
    case eCursor_no_drop:
      newCursor = ::LoadCursor(nullptr, IDC_NO);
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
      newCursor = ::LoadCursor(nullptr, IDC_SIZEALL);
      break;

    case eCursor_nesw_resize:
      newCursor = ::LoadCursor(nullptr, IDC_SIZENESW);
      break;

    case eCursor_nwse_resize:
      newCursor = ::LoadCursor(nullptr, IDC_SIZENWSE);
      break;

    case eCursor_ns_resize:
      newCursor = ::LoadCursor(nullptr, IDC_SIZENS);
      break;

    case eCursor_ew_resize:
      newCursor = ::LoadCursor(nullptr, IDC_SIZEWE);
      break;

    case eCursor_none:
      newCursor = ::LoadCursor(nsToolkit::mDllInstance, MAKEINTRESOURCE(IDC_NONE));
      break;

    default:
      NS_ERROR("Invalid cursor type");
      break;
  }

  if (nullptr != newCursor) {
    mCursor = aCursor;
    HCURSOR oldCursor = ::SetCursor(newCursor);
    
    if (sHCursor == oldCursor) {
      NS_IF_RELEASE(sCursorImgContainer);
      if (sHCursor != nullptr)
        ::DestroyIcon(sHCursor);
      sHCursor = nullptr;
    }
  }

  return NS_OK;
}

// Setting the actual cursor
NS_IMETHODIMP nsWindow::SetCursor(imgIContainer* aCursor,
                                  uint32_t aHotspotX, uint32_t aHotspotY)
{
  if (sCursorImgContainer == aCursor && sHCursor) {
    ::SetCursor(sHCursor);
    return NS_OK;
  }

  int32_t width;
  int32_t height;

  nsresult rv;
  rv = aCursor->GetWidth(&width);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aCursor->GetHeight(&height);
  NS_ENSURE_SUCCESS(rv, rv);

  // Reject cursors greater than 128 pixels in either direction, to prevent
  // spoofing.
  // XXX ideally we should rescale. Also, we could modify the API to
  // allow trusted content to set larger cursors.
  if (width > 128 || height > 128)
    return NS_ERROR_NOT_AVAILABLE;

  HCURSOR cursor;
  // No scaling
  gfxIntSize size(0, 0);
  rv = nsWindowGfx::CreateIcon(aCursor, true, aHotspotX, aHotspotY, size, &cursor);
  NS_ENSURE_SUCCESS(rv, rv);

  mCursor = nsCursor(-1);
  ::SetCursor(cursor);

  NS_IF_RELEASE(sCursorImgContainer);
  sCursorImgContainer = aCursor;
  NS_ADDREF(sCursorImgContainer);

  if (sHCursor != nullptr)
    ::DestroyIcon(sHCursor);
  sHCursor = cursor;

  return NS_OK;
}

/**************************************************************
 *
 * SECTION: nsIWidget::Get/SetTransparencyMode
 *
 * Manage the transparency mode of the top-level window
 * containing this widget.
 *
 **************************************************************/

#ifdef MOZ_XUL
nsTransparencyMode nsWindow::GetTransparencyMode()
{
  return GetTopLevelWindow(true)->GetWindowTranslucencyInner();
}

void nsWindow::SetTransparencyMode(nsTransparencyMode aMode)
{
  GetTopLevelWindow(true)->SetWindowTranslucencyInner(aMode);
}

void nsWindow::UpdateOpaqueRegion(const nsIntRegion &aOpaqueRegion)
{
  if (!HasGlass() || GetParent())
    return;

  // If there is no opaque region or hidechrome=true, set margins
  // to support a full sheet of glass. Comments in MSDN indicate
  // all values must be set to -1 to get a full sheet of glass.
  MARGINS margins = { -1, -1, -1, -1 };
  if (!aOpaqueRegion.IsEmpty()) {
    nsIntRect pluginBounds;
    for (nsIWidget* child = GetFirstChild(); child; child = child->GetNextSibling()) {
      if (child->IsPlugin()) {
        // Collect the bounds of all plugins for GetLargestRectangle.
        nsIntRect childBounds;
        child->GetBounds(childBounds);
        pluginBounds.UnionRect(pluginBounds, childBounds);
      }
    }

    nsIntRect clientBounds;
    GetClientBounds(clientBounds);

    // Find the largest rectangle and use that to calculate the inset. Our top
    // priority is to include the bounds of all plugins.
    nsIntRect largest = aOpaqueRegion.GetLargestRectangle(pluginBounds);
    margins.cxLeftWidth = largest.x;
    margins.cxRightWidth = clientBounds.width - largest.XMost();
    margins.cyBottomHeight = clientBounds.height - largest.YMost();
    if (mCustomNonClient) {
      // The minimum glass height must be the caption buttons height,
      // otherwise the buttons are drawn incorrectly.
      largest.y = std::max<uint32_t>(largest.y,
                         nsUXThemeData::sCommandButtons[CMDBUTTONIDX_BUTTONBOX].cy);
    }
    margins.cyTopHeight = largest.y;
  }

  // Only update glass area if there are changes
  if (memcmp(&mGlassMargins, &margins, sizeof mGlassMargins)) {
    mGlassMargins = margins;
    UpdateGlass();
  }
}

void nsWindow::UpdateGlass()
{
  MARGINS margins = mGlassMargins;

  // DWMNCRP_USEWINDOWSTYLE - The non-client rendering area is
  //                          rendered based on the window style.
  // DWMNCRP_ENABLED        - The non-client area rendering is
  //                          enabled; the window style is ignored.
  DWMNCRENDERINGPOLICY policy = DWMNCRP_USEWINDOWSTYLE;
  switch (mTransparencyMode) {
  case eTransparencyBorderlessGlass:
    // Only adjust if there is some opaque rectangle
    if (margins.cxLeftWidth >= 0) {
      margins.cxLeftWidth += kGlassMarginAdjustment;
      margins.cyTopHeight += kGlassMarginAdjustment;
      margins.cxRightWidth += kGlassMarginAdjustment;
      margins.cyBottomHeight += kGlassMarginAdjustment;
    }
    // Fall through
  case eTransparencyGlass:
    policy = DWMNCRP_ENABLED;
    break;
  default:
    break;
  }

  PR_LOG(gWindowsLog, PR_LOG_ALWAYS,
         ("glass margins: left:%d top:%d right:%d bottom:%d\n",
          margins.cxLeftWidth, margins.cyTopHeight,
          margins.cxRightWidth, margins.cyBottomHeight));

  // Extends the window frame behind the client area
  if (nsUXThemeData::CheckForCompositor()) {
    WinUtils::dwmExtendFrameIntoClientAreaPtr(mWnd, &margins);
    WinUtils::dwmSetWindowAttributePtr(mWnd, DWMWA_NCRENDERING_POLICY, &policy, sizeof policy);
  }
}
#endif

/**************************************************************
 *
 * SECTION: nsIWidget::HideWindowChrome
 *
 * Show or hide window chrome.
 *
 **************************************************************/

NS_IMETHODIMP nsWindow::HideWindowChrome(bool aShouldHide)
{
  HWND hwnd = WinUtils::GetTopLevelHWND(mWnd, true);
  if (!WinUtils::GetNSWindowPtr(hwnd))
  {
    NS_WARNING("Trying to hide window decorations in an embedded context");
    return NS_ERROR_FAILURE;
  }

  if (mHideChrome == aShouldHide)
    return NS_OK;

  DWORD_PTR style, exStyle;
  mHideChrome = aShouldHide;
  if (aShouldHide) {
    DWORD_PTR tempStyle = ::GetWindowLongPtrW(hwnd, GWL_STYLE);
    DWORD_PTR tempExStyle = ::GetWindowLongPtrW(hwnd, GWL_EXSTYLE);

    style = tempStyle & ~(WS_CAPTION | WS_THICKFRAME);
    exStyle = tempExStyle & ~(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE |
                              WS_EX_CLIENTEDGE | WS_EX_STATICEDGE);

    mOldStyle = tempStyle;
    mOldExStyle = tempExStyle;
  }
  else {
    if (!mOldStyle || !mOldExStyle) {
      mOldStyle = ::GetWindowLongPtrW(hwnd, GWL_STYLE);
      mOldExStyle = ::GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    }

    style = mOldStyle;
    exStyle = mOldExStyle;
    if (mFutureMarginsToUse) {
      SetNonClientMargins(mFutureMarginsOnceChromeShows);
    }
  }

  VERIFY_WINDOW_STYLE(style);
  ::SetWindowLongPtrW(hwnd, GWL_STYLE, style);
  ::SetWindowLongPtrW(hwnd, GWL_EXSTYLE, exStyle);

  return NS_OK;
}

/**************************************************************
 *
 * SECTION: nsWindow::Invalidate
 *
 * Invalidate an area of the client for painting.
 *
 **************************************************************/

// Invalidate this component visible area
NS_METHOD nsWindow::Invalidate(bool aEraseBackground, 
                               bool aUpdateNCArea,
                               bool aIncludeChildren)
{
  if (!mWnd) {
    return NS_OK;
  }

#ifdef WIDGET_DEBUG_OUTPUT
  debug_DumpInvalidate(stdout,
                       this,
                       nullptr,
                       nsAutoCString("noname"),
                       (int32_t) mWnd);
#endif // WIDGET_DEBUG_OUTPUT

  DWORD flags = RDW_INVALIDATE;
  if (aEraseBackground) {
    flags |= RDW_ERASE;
  }
  if (aUpdateNCArea) {
    flags |= RDW_FRAME;
  }
  if (aIncludeChildren) {
    flags |= RDW_ALLCHILDREN;
  }

  VERIFY(::RedrawWindow(mWnd, nullptr, nullptr, flags));
  return NS_OK;
}

// Invalidate this component visible area
NS_METHOD nsWindow::Invalidate(const nsIntRect & aRect)
{
  if (mWnd)
  {
#ifdef WIDGET_DEBUG_OUTPUT
    debug_DumpInvalidate(stdout,
                         this,
                         &aRect,
                         nsAutoCString("noname"),
                         (int32_t) mWnd);
#endif // WIDGET_DEBUG_OUTPUT

    RECT rect;

    rect.left   = aRect.x;
    rect.top    = aRect.y;
    rect.right  = aRect.x + aRect.width;
    rect.bottom = aRect.y + aRect.height;

    VERIFY(::InvalidateRect(mWnd, &rect, FALSE));
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWindow::MakeFullScreen(bool aFullScreen, nsIScreen* aTargetScreen)
{
  // taskbarInfo will be nullptr pre Windows 7 until Bug 680227 is resolved.
  nsCOMPtr<nsIWinTaskbar> taskbarInfo =
    do_GetService(NS_TASKBAR_CONTRACTID);

  mFullscreenMode = aFullScreen;
  if (aFullScreen) {
    if (mSizeMode == nsSizeMode_Fullscreen)
      return NS_OK;
    mOldSizeMode = mSizeMode;
    SetSizeMode(nsSizeMode_Fullscreen);

    // Notify the taskbar that we will be entering full screen mode.
    if (taskbarInfo) {
      taskbarInfo->PrepareFullScreenHWND(mWnd, TRUE);
    }
  } else {
    if (mSizeMode != nsSizeMode_Fullscreen)
      return NS_OK;
    SetSizeMode(mOldSizeMode);
  }

  UpdateNonClientMargins();

  bool visible = mIsVisible;
  if (mOldSizeMode == nsSizeMode_Normal)
    Show(false);
  
  // Will call hide chrome, reposition window. Note this will
  // also cache dimensions for restoration, so it should only
  // be called once per fullscreen request.
  nsresult rv = nsBaseWidget::MakeFullScreen(aFullScreen, aTargetScreen);

  if (visible) {
    Show(true);
    Invalidate();

    if (!aFullScreen && mOldSizeMode == nsSizeMode_Normal) {
      // Ensure the window exiting fullscreen get activated. Window
      // activation was bypassed by SetSizeMode, and hiding window for
      // transition could also blur the current window.
      DispatchFocusToTopLevelWindow(true);
    }
  }

  // Notify the taskbar that we have exited full screen mode.
  if (!aFullScreen && taskbarInfo) {
    taskbarInfo->PrepareFullScreenHWND(mWnd, FALSE);
  }

  if (mWidgetListener)
    mWidgetListener->SizeModeChanged(mSizeMode);

  return rv;
}

/**************************************************************
 *
 * SECTION: Native data storage
 *
 * nsIWidget::GetNativeData
 * nsIWidget::FreeNativeData
 *
 * Set or clear native data based on a constant.
 *
 **************************************************************/

// Return some native data according to aDataType
void* nsWindow::GetNativeData(uint32_t aDataType)
{
  nsAutoString className;
  switch (aDataType) {
    case NS_NATIVE_TMP_WINDOW:
      GetWindowClass(className);
      return (void*)::CreateWindowExW(mIsRTL ? WS_EX_LAYOUTRTL : 0,
                                      className.get(),
                                      L"",
                                      WS_CHILD,
                                      CW_USEDEFAULT,
                                      CW_USEDEFAULT,
                                      CW_USEDEFAULT,
                                      CW_USEDEFAULT,
                                      mWnd,
                                      nullptr,
                                      nsToolkit::mDllInstance,
                                      nullptr);
    case NS_NATIVE_PLUGIN_ID:
    case NS_NATIVE_PLUGIN_PORT:
    case NS_NATIVE_WIDGET:
    case NS_NATIVE_WINDOW:
      return (void*)mWnd;
    case NS_NATIVE_SHAREABLE_WINDOW:
      return (void*) WinUtils::GetTopLevelHWND(mWnd);
    case NS_NATIVE_GRAPHIC:
      // XXX:  This is sleezy!!  Remember to Release the DC after using it!
#ifdef MOZ_XUL
      return (void*)(eTransparencyTransparent == mTransparencyMode) ?
        mMemoryDC : ::GetDC(mWnd);
#else
      return (void*)::GetDC(mWnd);
#endif

    case NS_NATIVE_TSF_THREAD_MGR:
    case NS_NATIVE_TSF_CATEGORY_MGR:
    case NS_NATIVE_TSF_DISPLAY_ATTR_MGR:
      return IMEHandler::GetNativeData(aDataType);

    default:
      break;
  }

  return nullptr;
}

// Free some native data according to aDataType
void nsWindow::FreeNativeData(void * data, uint32_t aDataType)
{
  switch (aDataType)
  {
    case NS_NATIVE_GRAPHIC:
#ifdef MOZ_XUL
      if (eTransparencyTransparent != mTransparencyMode)
        ::ReleaseDC(mWnd, (HDC)data);
#else
      ::ReleaseDC(mWnd, (HDC)data);
#endif
      break;
    case NS_NATIVE_WIDGET:
    case NS_NATIVE_WINDOW:
    case NS_NATIVE_PLUGIN_PORT:
      break;
    default:
      break;
  }
}

/**************************************************************
 *
 * SECTION: nsIWidget::SetTitle
 *
 * Set the main windows title text.
 *
 **************************************************************/

NS_METHOD nsWindow::SetTitle(const nsAString& aTitle)
{
  const nsString& strTitle = PromiseFlatString(aTitle);
  ::SendMessageW(mWnd, WM_SETTEXT, (WPARAM)0, (LPARAM)(LPCWSTR)strTitle.get());
  return NS_OK;
}

/**************************************************************
 *
 * SECTION: nsIWidget::SetIcon
 *
 * Set the main windows icon.
 *
 **************************************************************/

NS_METHOD nsWindow::SetIcon(const nsAString& aIconSpec) 
{
  // Assume the given string is a local identifier for an icon file.

  nsCOMPtr<nsIFile> iconFile;
  ResolveIconName(aIconSpec, NS_LITERAL_STRING(".ico"),
                  getter_AddRefs(iconFile));
  if (!iconFile)
    return NS_OK; // not an error if icon is not found

  nsAutoString iconPath;
  iconFile->GetPath(iconPath);

  // XXX this should use MZLU (see bug 239279)

  ::SetLastError(0);

  HICON bigIcon = (HICON)::LoadImageW(nullptr,
                                      (LPCWSTR)iconPath.get(),
                                      IMAGE_ICON,
                                      ::GetSystemMetrics(SM_CXICON),
                                      ::GetSystemMetrics(SM_CYICON),
                                      LR_LOADFROMFILE );
  HICON smallIcon = (HICON)::LoadImageW(nullptr,
                                        (LPCWSTR)iconPath.get(),
                                        IMAGE_ICON,
                                        ::GetSystemMetrics(SM_CXSMICON),
                                        ::GetSystemMetrics(SM_CYSMICON),
                                        LR_LOADFROMFILE );

  if (bigIcon) {
    HICON icon = (HICON) ::SendMessageW(mWnd, WM_SETICON, (WPARAM)ICON_BIG, (LPARAM)bigIcon);
    if (icon)
      ::DestroyIcon(icon);
    mIconBig = bigIcon;
  }
#ifdef DEBUG_SetIcon
  else {
    NS_LossyConvertUTF16toASCII cPath(iconPath);
    PR_LOG(gWindowsLog, PR_LOG_ALWAYS,
           ("\nIcon load error; icon=%s, rc=0x%08X\n\n", 
            cPath.get(), ::GetLastError()));
  }
#endif
  if (smallIcon) {
    HICON icon = (HICON) ::SendMessageW(mWnd, WM_SETICON, (WPARAM)ICON_SMALL, (LPARAM)smallIcon);
    if (icon)
      ::DestroyIcon(icon);
    mIconSmall = smallIcon;
  }
#ifdef DEBUG_SetIcon
  else {
    NS_LossyConvertUTF16toASCII cPath(iconPath);
    PR_LOG(gWindowsLog, PR_LOG_ALWAYS,
           ("\nSmall icon load error; icon=%s, rc=0x%08X\n\n", 
            cPath.get(), ::GetLastError()));
  }
#endif
  return NS_OK;
}

/**************************************************************
 *
 * SECTION: nsIWidget::WidgetToScreenOffset
 *
 * Return this widget's origin in screen coordinates.
 *
 **************************************************************/

LayoutDeviceIntPoint nsWindow::WidgetToScreenOffset()
{
  POINT point;
  point.x = 0;
  point.y = 0;
  ::ClientToScreen(mWnd, &point);
  return LayoutDeviceIntPoint(point.x, point.y);
}

LayoutDeviceIntSize
nsWindow::ClientToWindowSize(const LayoutDeviceIntSize& aClientSize)
{
  if (mWindowType == eWindowType_popup && !IsPopupWithTitleBar())
    return aClientSize;

  // just use (200, 200) as the position
  RECT r;
  r.left = 200;
  r.top = 200;
  r.right = 200 + aClientSize.width;
  r.bottom = 200 + aClientSize.height;
  ::AdjustWindowRectEx(&r, WindowStyle(), false, WindowExStyle());

  return LayoutDeviceIntSize(r.right - r.left, r.bottom - r.top);
}

/**************************************************************
 *
 * SECTION: nsIWidget::EnableDragDrop
 *
 * Enables/Disables drag and drop of files on this widget.
 *
 **************************************************************/

NS_METHOD nsWindow::EnableDragDrop(bool aEnable)
{
  NS_ASSERTION(mWnd, "nsWindow::EnableDragDrop() called after Destroy()");

  nsresult rv = NS_ERROR_FAILURE;
  if (aEnable) {
    if (nullptr == mNativeDragTarget) {
       mNativeDragTarget = new nsNativeDragTarget(this);
       if (nullptr != mNativeDragTarget) {
         mNativeDragTarget->AddRef();
         if (S_OK == ::CoLockObjectExternal((LPUNKNOWN)mNativeDragTarget,TRUE,FALSE)) {
           if (S_OK == ::RegisterDragDrop(mWnd, (LPDROPTARGET)mNativeDragTarget)) {
             rv = NS_OK;
           }
         }
       }
    }
  } else {
    if (nullptr != mWnd && nullptr != mNativeDragTarget) {
      ::RevokeDragDrop(mWnd);
      if (S_OK == ::CoLockObjectExternal((LPUNKNOWN)mNativeDragTarget, FALSE, TRUE)) {
        rv = NS_OK;
      }
      mNativeDragTarget->DragCancel();
      NS_RELEASE(mNativeDragTarget);
    }
  }
  return rv;
}

/**************************************************************
 *
 * SECTION: nsIWidget::CaptureMouse
 *
 * Enables/Disables system mouse capture.
 *
 **************************************************************/

NS_METHOD nsWindow::CaptureMouse(bool aCapture)
{
  if (!nsToolkit::gMouseTrailer) {
    NS_ERROR("nsWindow::CaptureMouse called after nsToolkit destroyed");
    return NS_OK;
  }

  if (aCapture) {
    nsToolkit::gMouseTrailer->SetCaptureWindow(mWnd);
    ::SetCapture(mWnd);
  } else {
    nsToolkit::gMouseTrailer->SetCaptureWindow(nullptr);
    ::ReleaseCapture();
  }
  sIsInMouseCapture = aCapture;
  return NS_OK;
}

/**************************************************************
 *
 * SECTION: nsIWidget::CaptureRollupEvents
 *
 * Dealing with event rollup on destroy for popups. Enables &
 * Disables system capture of any and all events that would
 * cause a dropdown to be rolled up.
 *
 **************************************************************/

NS_IMETHODIMP nsWindow::CaptureRollupEvents(nsIRollupListener * aListener,
                                            bool aDoCapture)
{
  if (aDoCapture) {
    gRollupListener = aListener;
    if (!sMsgFilterHook && !sCallProcHook && !sCallMouseHook) {
      RegisterSpecialDropdownHooks();
    }
    sProcessHook = true;
  } else {
    gRollupListener = nullptr;
    sProcessHook = false;
    UnregisterSpecialDropdownHooks();
  }

  return NS_OK;
}

/**************************************************************
 *
 * SECTION: nsIWidget::GetAttention
 *
 * Bring this window to the user's attention.
 *
 **************************************************************/

// Draw user's attention to this window until it comes to foreground.
NS_IMETHODIMP
nsWindow::GetAttention(int32_t aCycleCount)
{
  // Got window?
  if (!mWnd)
    return NS_ERROR_NOT_INITIALIZED;

  HWND flashWnd = WinUtils::GetTopLevelHWND(mWnd, false, false);
  HWND fgWnd = ::GetForegroundWindow();
  // Don't flash if the flash count is 0 or if the foreground window is our
  // window handle or that of our owned-most window.
  if (aCycleCount == 0 || 
      flashWnd == fgWnd ||
      flashWnd == WinUtils::GetTopLevelHWND(fgWnd, false, false)) {
    return NS_OK;
  }

  DWORD defaultCycleCount = 0;
  ::SystemParametersInfo(SPI_GETFOREGROUNDFLASHCOUNT, 0, &defaultCycleCount, 0);

  FLASHWINFO flashInfo = { sizeof(FLASHWINFO), flashWnd,
    FLASHW_ALL, aCycleCount > 0 ? aCycleCount : defaultCycleCount, 0 };
  ::FlashWindowEx(&flashInfo);

  return NS_OK;
}

void nsWindow::StopFlashing()
{
  HWND flashWnd = mWnd;
  while (HWND ownerWnd = ::GetWindow(flashWnd, GW_OWNER)) {
    flashWnd = ownerWnd;
  }

  FLASHWINFO flashInfo = { sizeof(FLASHWINFO), flashWnd,
    FLASHW_STOP, 0, 0 };
  ::FlashWindowEx(&flashInfo);
}

/**************************************************************
 *
 * SECTION: nsIWidget::HasPendingInputEvent
 *
 * Ask whether there user input events pending.  All input events are
 * included, including those not targeted at this nsIwidget instance.
 *
 **************************************************************/

bool
nsWindow::HasPendingInputEvent()
{
  // If there is pending input or the user is currently
  // moving the window then return true.
  // Note: When the user is moving the window WIN32 spins
  // a separate event loop and input events are not
  // reported to the application.
  if (HIWORD(GetQueueStatus(QS_INPUT)))
    return true;
  GUITHREADINFO guiInfo;
  guiInfo.cbSize = sizeof(GUITHREADINFO);
  if (!GetGUIThreadInfo(GetCurrentThreadId(), &guiInfo))
    return false;
  return GUI_INMOVESIZE == (guiInfo.flags & GUI_INMOVESIZE);
}

/**************************************************************
 *
 * SECTION: nsIWidget::GetLayerManager
 *
 * Get the layer manager associated with this widget.
 *
 **************************************************************/

struct LayerManagerPrefs {
  LayerManagerPrefs()
    : mAccelerateByDefault(true)
    , mDisableAcceleration(false)
    , mPreferOpenGL(false)
    , mPreferD3D9(false)
  {}
  bool mAccelerateByDefault;
  bool mDisableAcceleration;
  bool mForceAcceleration;
  bool mPreferOpenGL;
  bool mPreferD3D9;
};

static void
GetLayerManagerPrefs(LayerManagerPrefs* aManagerPrefs)
{
  Preferences::GetBool("layers.acceleration.disabled",
                       &aManagerPrefs->mDisableAcceleration);
  Preferences::GetBool("layers.acceleration.force-enabled",
                       &aManagerPrefs->mForceAcceleration);
  Preferences::GetBool("layers.prefer-opengl",
                       &aManagerPrefs->mPreferOpenGL);
  Preferences::GetBool("layers.prefer-d3d9",
                       &aManagerPrefs->mPreferD3D9);

  const char *acceleratedEnv = PR_GetEnv("MOZ_ACCELERATED");
  aManagerPrefs->mAccelerateByDefault =
    aManagerPrefs->mAccelerateByDefault ||
    (acceleratedEnv && (*acceleratedEnv != '0'));

  bool safeMode = false;
  nsCOMPtr<nsIXULRuntime> xr = do_GetService("@mozilla.org/xre/runtime;1");
  if (xr)
    xr->GetInSafeMode(&safeMode);
  aManagerPrefs->mDisableAcceleration =
    aManagerPrefs->mDisableAcceleration || safeMode;
}

LayerManager*
nsWindow::GetLayerManager(PLayerTransactionChild* aShadowManager,
                          LayersBackend aBackendHint,
                          LayerManagerPersistence aPersistence,
                          bool* aAllowRetaining)
{
  if (aAllowRetaining) {
    *aAllowRetaining = true;
  }

  RECT windowRect;
  ::GetClientRect(mWnd, &windowRect);

  // Try OMTC first.
  if (!mLayerManager && ShouldUseOffMainThreadCompositing()) {
    gfxWindowsPlatform::GetPlatform()->UpdateRenderMode();

    // e10s uses the parameter to pass in the shadow manager from the TabChild
    // so we don't expect to see it there since this doesn't support e10s.
    NS_ASSERTION(aShadowManager == nullptr, "Async Compositor not supported with e10s");
    CreateCompositor();
  }

  if (!mLayerManager) {
    MOZ_ASSERT(!mCompositorParent && !mCompositorChild);
    mLayerManager = CreateBasicLayerManager();
  }

  NS_ASSERTION(mLayerManager, "Couldn't provide a valid layer manager.");

  return mLayerManager;
}

/**************************************************************
 *
 * SECTION: nsIWidget::OnDefaultButtonLoaded
 *
 * Called after the dialog is loaded and it has a default button.
 *
 **************************************************************/
 
NS_IMETHODIMP
nsWindow::OnDefaultButtonLoaded(const nsIntRect &aButtonRect)
{
  if (aButtonRect.IsEmpty())
    return NS_OK;

  // Don't snap when we are not active.
  HWND activeWnd = ::GetActiveWindow();
  if (activeWnd != ::GetForegroundWindow() ||
      WinUtils::GetTopLevelHWND(mWnd, true) !=
        WinUtils::GetTopLevelHWND(activeWnd, true)) {
    return NS_OK;
  }

  bool isAlwaysSnapCursor =
    Preferences::GetBool("ui.cursor_snapping.always_enabled", false);

  if (!isAlwaysSnapCursor) {
    BOOL snapDefaultButton;
    if (!::SystemParametersInfo(SPI_GETSNAPTODEFBUTTON, 0,
                                &snapDefaultButton, 0) || !snapDefaultButton)
      return NS_OK;
  }

  nsIntRect widgetRect;
  nsresult rv = GetScreenBounds(widgetRect);
  NS_ENSURE_SUCCESS(rv, rv);
  nsIntRect buttonRect(aButtonRect + widgetRect.TopLeft());

  nsIntPoint centerOfButton(buttonRect.x + buttonRect.width / 2,
                            buttonRect.y + buttonRect.height / 2);
  // The center of the button can be outside of the widget.
  // E.g., it could be hidden by scrolling.
  if (!widgetRect.Contains(centerOfButton)) {
    return NS_OK;
  }

  if (!::SetCursorPos(centerOfButton.x, centerOfButton.y)) {
    NS_ERROR("SetCursorPos failed");
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWindow::OverrideSystemMouseScrollSpeed(double aOriginalDeltaX,
                                         double aOriginalDeltaY,
                                         double& aOverriddenDeltaX,
                                         double& aOverriddenDeltaY)
{
  // The default vertical and horizontal scrolling speed is 3, this is defined
  // on the document of SystemParametersInfo in MSDN.
  const uint32_t kSystemDefaultScrollingSpeed = 3;

  double absOriginDeltaX = Abs(aOriginalDeltaX);
  double absOriginDeltaY = Abs(aOriginalDeltaY);

  // Compute the simple overridden speed.
  double absComputedOverriddenDeltaX, absComputedOverriddenDeltaY;
  nsresult rv =
    nsBaseWidget::OverrideSystemMouseScrollSpeed(absOriginDeltaX,
                                                 absOriginDeltaY,
                                                 absComputedOverriddenDeltaX,
                                                 absComputedOverriddenDeltaY);
  NS_ENSURE_SUCCESS(rv, rv);

  aOverriddenDeltaX = aOriginalDeltaX;
  aOverriddenDeltaY = aOriginalDeltaY;

  if (absComputedOverriddenDeltaX == absOriginDeltaX &&
      absComputedOverriddenDeltaY == absOriginDeltaY) {
    // We don't override now.
    return NS_OK;
  }

  // Otherwise, we should check whether the user customized the system settings
  // or not.  If the user did it, we should respect the will.
  UINT systemSpeed;
  if (!::SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &systemSpeed, 0)) {
    return NS_ERROR_FAILURE;
  }
  // The default vertical scrolling speed is 3, this is defined on the document
  // of SystemParametersInfo in MSDN.
  if (systemSpeed != kSystemDefaultScrollingSpeed) {
    return NS_OK;
  }

  // Only Vista and later, Windows has the system setting of horizontal
  // scrolling by the mouse wheel.
  if (IsVistaOrLater()) {
    if (!::SystemParametersInfo(SPI_GETWHEELSCROLLCHARS, 0, &systemSpeed, 0)) {
      return NS_ERROR_FAILURE;
    }
    // The default horizontal scrolling speed is 3, this is defined on the
    // document of SystemParametersInfo in MSDN.
    if (systemSpeed != kSystemDefaultScrollingSpeed) {
      return NS_OK;
    }
  }

  // Limit the overridden delta value from the system settings.  The mouse
  // driver might accelerate the scrolling speed already.  If so, we shouldn't
  // override the scrolling speed for preventing the unexpected high speed
  // scrolling.
  double absDeltaLimitX, absDeltaLimitY;
  rv =
    nsBaseWidget::OverrideSystemMouseScrollSpeed(kSystemDefaultScrollingSpeed,
                                                 kSystemDefaultScrollingSpeed,
                                                 absDeltaLimitX,
                                                 absDeltaLimitY);
  NS_ENSURE_SUCCESS(rv, rv);

  // If the given delta is larger than our computed limitation value, the delta
  // was accelerated by the mouse driver.  So, we should do nothing here.
  if (absDeltaLimitX <= absOriginDeltaX || absDeltaLimitY <= absOriginDeltaY) {
    return NS_OK;
  }

  aOverriddenDeltaX = std::min(absComputedOverriddenDeltaX, absDeltaLimitX);
  aOverriddenDeltaY = std::min(absComputedOverriddenDeltaY, absDeltaLimitY);

  if (aOriginalDeltaX < 0) {
    aOverriddenDeltaX *= -1;
  }
  if (aOriginalDeltaY < 0) {
    aOverriddenDeltaY *= -1;
  }
  return NS_OK;
}

mozilla::TemporaryRef<mozilla::gfx::DrawTarget>
nsWindow::StartRemoteDrawing()
{
  MOZ_ASSERT(!mCompositeDC);
  NS_ASSERTION(IsRenderMode(gfxWindowsPlatform::RENDER_DIRECT2D) ||
               IsRenderMode(gfxWindowsPlatform::RENDER_GDI),
               "Unexpected render mode for remote drawing");

  HDC dc = (HDC)GetNativeData(NS_NATIVE_GRAPHIC);
  nsRefPtr<gfxASurface> surf;

  if (mTransparencyMode == eTransparencyTransparent) {
    if (!mTransparentSurface) {
      SetupTranslucentWindowMemoryBitmap(mTransparencyMode);
    }
    if (mTransparentSurface) {
      surf = mTransparentSurface;
    }
  } 
  
  if (!surf) {
    if (!dc) {
      return nullptr;
    }
    uint32_t flags = (mTransparencyMode == eTransparencyOpaque) ? 0 :
        gfxWindowsSurface::FLAG_IS_TRANSPARENT;
    surf = new gfxWindowsSurface(dc, flags);
  }

  mozilla::gfx::IntSize size(surf->GetSize().width, surf->GetSize().height);
  if (size.width <= 0 || size.height <= 0) {
    if (dc) {
      FreeNativeData(dc, NS_NATIVE_GRAPHIC);
    }
    return nullptr;
  }

  MOZ_ASSERT(!mCompositeDC);
  mCompositeDC = dc;

  return mozilla::gfx::Factory::CreateDrawTargetForCairoSurface(surf->CairoSurface(), size);
}

void
nsWindow::EndRemoteDrawing()
{
  if (mTransparencyMode == eTransparencyTransparent) {
    MOZ_ASSERT(IsRenderMode(gfxWindowsPlatform::RENDER_DIRECT2D)
               || mTransparentSurface);
    UpdateTranslucentWindow();
  }
  if (mCompositeDC) {
    FreeNativeData(mCompositeDC, NS_NATIVE_GRAPHIC);
  }
  mCompositeDC = nullptr;
}

void
nsWindow::UpdateThemeGeometries(const nsTArray<ThemeGeometry>& aThemeGeometries)
{
  nsIntRegion clearRegion;
  for (size_t i = 0; i < aThemeGeometries.Length(); i++) {
    if (aThemeGeometries[i].mType == nsNativeThemeWin::eThemeGeometryTypeWindowButtons &&
        nsUXThemeData::CheckForCompositor())
    {
      nsIntRect bounds = aThemeGeometries[i].mRect;
      clearRegion = nsIntRect(bounds.X(), bounds.Y(), bounds.Width(), bounds.Height() - 2.0);
      clearRegion.Or(clearRegion, nsIntRect(bounds.X() + 1.0, bounds.YMost() - 2.0, bounds.Width() - 1.0, 1.0));
      clearRegion.Or(clearRegion, nsIntRect(bounds.X() + 2.0, bounds.YMost() - 1.0, bounds.Width() - 3.0, 1.0));
    }
  }

  nsRefPtr<LayerManager> layerManager = GetLayerManager();
  if (layerManager) {
    layerManager->SetRegionToClear(clearRegion);
  }
}

uint32_t
nsWindow::GetMaxTouchPoints() const
{
  if (IsWin7OrLater() && IsTouchDeviceSupportPresent()) {
    return GetSystemMetrics(SM_MAXIMUMTOUCHES);
  }
  return 0;
}

/**************************************************************
 **************************************************************
 **
 ** BLOCK: Moz Events
 **
 ** Moz GUI event management. 
 **
 **************************************************************
 **************************************************************/

/**************************************************************
 *
 * SECTION: Mozilla event initialization
 *
 * Helpers for initializing moz events.
 *
 **************************************************************/

// Event intialization
void nsWindow::InitEvent(WidgetGUIEvent& event, nsIntPoint* aPoint)
{
  if (nullptr == aPoint) {     // use the point from the event
    // get the message position in client coordinates
    if (mWnd != nullptr) {

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
  else {  
    // use the point override if provided
    event.refPoint.x = aPoint->x;
    event.refPoint.y = aPoint->y;
  }

  event.time = ::GetMessageTime();
  event.timeStamp = GetMessageTimeStamp(event.time);
}

/**************************************************************
 *
 * SECTION: Moz event dispatch helpers
 *
 * Helpers for dispatching different types of moz events.
 *
 **************************************************************/

// Main event dispatch. Invokes callback and ProcessEvent method on
// Event Listener object. Part of nsIWidget.
NS_IMETHODIMP nsWindow::DispatchEvent(WidgetGUIEvent* event,
                                      nsEventStatus& aStatus)
{
#ifdef WIDGET_DEBUG_OUTPUT
  debug_DumpEvent(stdout,
                  event->widget,
                  event,
                  nsAutoCString("something"),
                  (int32_t) mWnd);
#endif // WIDGET_DEBUG_OUTPUT

  aStatus = nsEventStatus_eIgnore;

  // Top level windows can have a view attached which requires events be sent
  // to the underlying base window and the view. Added when we combined the
  // base chrome window with the main content child for nc client area (title
  // bar) rendering.
  if (mAttachedWidgetListener) {
    aStatus = mAttachedWidgetListener->HandleEvent(event, mUseAttachedEvents);
  }
  else if (mWidgetListener) {
    aStatus = mWidgetListener->HandleEvent(event, mUseAttachedEvents);
  }

  // the window can be destroyed during processing of seemingly innocuous events like, say,
  // mousedowns due to the magic of scripting. mousedowns will return nsEventStatus_eIgnore,
  // which causes problems with the deleted window. therefore:
  if (mOnDestroyCalled)
    aStatus = nsEventStatus_eConsumeNoDefault;
  return NS_OK;
}

bool nsWindow::DispatchStandardEvent(uint32_t aMsg)
{
  WidgetGUIEvent event(true, aMsg, this);
  InitEvent(event);

  bool result = DispatchWindowEvent(&event);
  return result;
}

bool nsWindow::DispatchKeyboardEvent(WidgetKeyboardEvent* event)
{
  nsEventStatus status = DispatchInputEvent(event);
  return ConvertStatus(status);
}

bool nsWindow::DispatchContentCommandEvent(WidgetContentCommandEvent* aEvent)
{
  nsEventStatus status;
  DispatchEvent(aEvent, status);
  return ConvertStatus(status);
}

bool nsWindow::DispatchWheelEvent(WidgetWheelEvent* aEvent)
{
  nsEventStatus status = DispatchAPZAwareEvent(aEvent->AsInputEvent());
  return ConvertStatus(status);
}

bool nsWindow::DispatchWindowEvent(WidgetGUIEvent* event)
{
  nsEventStatus status;
  DispatchEvent(event, status);
  return ConvertStatus(status);
}

bool nsWindow::DispatchWindowEvent(WidgetGUIEvent* event,
                                   nsEventStatus& aStatus)
{
  DispatchEvent(event, aStatus);
  return ConvertStatus(aStatus);
}

// Recursively dispatch synchronous paints for nsIWidget
// descendants with invalidated rectangles.
BOOL CALLBACK nsWindow::DispatchStarvedPaints(HWND aWnd, LPARAM aMsg)
{
  LONG_PTR proc = ::GetWindowLongPtrW(aWnd, GWLP_WNDPROC);
  if (proc == (LONG_PTR)&nsWindow::WindowProc) {
    // its one of our windows so check to see if it has a
    // invalidated rect. If it does. Dispatch a synchronous
    // paint.
    if (GetUpdateRect(aWnd, nullptr, FALSE))
      VERIFY(::UpdateWindow(aWnd));
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
  if (mPainting) {
    NS_WARNING("We were asked to dispatch pending events during painting, "
               "denying since that's unsafe.");
    return;
  }

  // We need to ensure that reflow events do not get starved.
  // At the same time, we don't want to recurse through here
  // as that would prevent us from dispatching starved paints.
  static int recursionBlocker = 0;
  if (recursionBlocker++ == 0) {
    NS_ProcessPendingEvents(nullptr, PR_MillisecondsToInterval(100));
    --recursionBlocker;
  }

  // Quickly check to see if there are any paint events pending,
  // but only dispatch them if it has been long enough since the
  // last paint completed.
  if (::GetQueueStatus(QS_PAINT) &&
      ((TimeStamp::Now() - mLastPaintEndTime).ToMilliseconds() >= 50)) {
    // Find the top level window.
    HWND topWnd = WinUtils::GetTopLevelHWND(mWnd);

    // Dispatch pending paints for topWnd and all its descendant windows.
    // Note: EnumChildWindows enumerates all descendant windows not just
    // the children (but not the window itself).
    nsWindow::DispatchStarvedPaints(topWnd, 0);
    ::EnumChildWindows(topWnd, nsWindow::DispatchStarvedPaints, 0);
  }
}

bool nsWindow::DispatchPluginEvent(UINT aMessage,
                                     WPARAM aWParam,
                                     LPARAM aLParam,
                                     bool aDispatchPendingEvents)
{
  bool ret = nsWindowBase::DispatchPluginEvent(
               WinUtils::InitMSG(aMessage, aWParam, aLParam, mWnd));
  if (aDispatchPendingEvents && !Destroyed()) {
    DispatchPendingEvents();
  }
  return ret;
}

// Deal with all sort of mouse event
bool nsWindow::DispatchMouseEvent(uint32_t aEventType, WPARAM wParam,
                                    LPARAM lParam, bool aIsContextMenuKey,
                                    int16_t aButton, uint16_t aInputSource)
{
  bool result = false;

  UserActivity();

  if (!mWidgetListener) {
    return result;
  }

  if (WinUtils::GetIsMouseFromTouch(aEventType) && mTouchWindow) {
    // If mTouchWindow is true, then we must have APZ enabled and be
    // feeding it raw touch events. In that case we don't need to
    // send touch-generated mouse events to content.
    MOZ_ASSERT(mAPZC);
    return result;
  }

  switch (aEventType) {
    case NS_MOUSE_BUTTON_DOWN:
      CaptureMouse(true);
      break;

    // NS_MOUSE_MOVE and NS_MOUSE_EXIT are here because we need to make sure capture flag
    // isn't left on after a drag where we wouldn't see a button up message (see bug 324131).
    case NS_MOUSE_BUTTON_UP:
    case NS_MOUSE_MOVE:
    case NS_MOUSE_EXIT:
      if (!(wParam & (MK_LBUTTON | MK_MBUTTON | MK_RBUTTON)) && sIsInMouseCapture)
        CaptureMouse(false);
      break;

    default:
      break;

  } // switch

  nsIntPoint eventPoint;
  eventPoint.x = GET_X_LPARAM(lParam);
  eventPoint.y = GET_Y_LPARAM(lParam);

  WidgetMouseEvent event(true, aEventType, this, WidgetMouseEvent::eReal,
                         aIsContextMenuKey ? WidgetMouseEvent::eContextMenuKey :
                                             WidgetMouseEvent::eNormal);
  if (aEventType == NS_CONTEXTMENU && aIsContextMenuKey) {
    nsIntPoint zero(0, 0);
    InitEvent(event, &zero);
  } else {
    InitEvent(event, &eventPoint);
  }

  ModifierKeyState modifierKeyState;
  modifierKeyState.InitInputEvent(event);
  event.button    = aButton;
  event.inputSource = aInputSource;
  // If we get here the mouse events must be from non-touch sources, so
  // convert it to pointer events as well
  event.convertToPointer = true;

  nsIntPoint mpScreen = eventPoint + WidgetToScreenOffsetUntyped();

  // Suppress mouse moves caused by widget creation
  if (aEventType == NS_MOUSE_MOVE) 
  {
    if ((sLastMouseMovePoint.x == mpScreen.x) && (sLastMouseMovePoint.y == mpScreen.y))
      return result;
    sLastMouseMovePoint.x = mpScreen.x;
    sLastMouseMovePoint.y = mpScreen.y;
  }

  bool insideMovementThreshold = (DeprecatedAbs(sLastMousePoint.x - eventPoint.x) < (short)::GetSystemMetrics(SM_CXDOUBLECLK)) &&
                                   (DeprecatedAbs(sLastMousePoint.y - eventPoint.y) < (short)::GetSystemMetrics(SM_CYDOUBLECLK));

  BYTE eventButton;
  switch (aButton) {
    case WidgetMouseEvent::eLeftButton:
      eventButton = VK_LBUTTON;
      break;
    case WidgetMouseEvent::eMiddleButton:
      eventButton = VK_MBUTTON;
      break;
    case WidgetMouseEvent::eRightButton:
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
    sLastClickCount = 2;
    sLastMouseDownTime = curMsgTime;
  }
  else if (aEventType == NS_MOUSE_BUTTON_UP) {
    // remember when this happened for the next mouse down
    sLastMousePoint.x = eventPoint.x;
    sLastMousePoint.y = eventPoint.y;
    sLastMouseButton = eventButton;
  }
  else if (aEventType == NS_MOUSE_BUTTON_DOWN) {
    // now look to see if we want to convert this to a double- or triple-click
    if (((curMsgTime - sLastMouseDownTime) < (LONG)::GetDoubleClickTime()) && insideMovementThreshold &&
        eventButton == sLastMouseButton) {
      sLastClickCount ++;
    } else {
      // reset the click count, to count *this* click
      sLastClickCount = 1;
    }
    // Set last Click time on MouseDown only
    sLastMouseDownTime = curMsgTime;
  }
  else if (aEventType == NS_MOUSE_MOVE && !insideMovementThreshold) {
    sLastClickCount = 0;
  }
  else if (aEventType == NS_MOUSE_EXIT) {
    event.exit = IsTopLevelMouseExit(mWnd) ?
                   WidgetMouseEvent::eTopLevel : WidgetMouseEvent::eChild;
  }
  event.clickCount = sLastClickCount;

#ifdef NS_DEBUG_XX
  PR_LOG(gWindowsLog, PR_LOG_ALWAYS,
         ("Msg Time: %d Click Count: %d\n", curMsgTime, event.clickCount));
#endif

  NPEvent pluginEvent;

  switch (aEventType)
  {
    case NS_MOUSE_BUTTON_DOWN:
      switch (aButton) {
        case WidgetMouseEvent::eLeftButton:
          pluginEvent.event = WM_LBUTTONDOWN;
          break;
        case WidgetMouseEvent::eMiddleButton:
          pluginEvent.event = WM_MBUTTONDOWN;
          break;
        case WidgetMouseEvent::eRightButton:
          pluginEvent.event = WM_RBUTTONDOWN;
          break;
        default:
          break;
      }
      break;
    case NS_MOUSE_BUTTON_UP:
      switch (aButton) {
        case WidgetMouseEvent::eLeftButton:
          pluginEvent.event = WM_LBUTTONUP;
          break;
        case WidgetMouseEvent::eMiddleButton:
          pluginEvent.event = WM_MBUTTONUP;
          break;
        case WidgetMouseEvent::eRightButton:
          pluginEvent.event = WM_RBUTTONUP;
          break;
        default:
          break;
      }
      break;
    case NS_MOUSE_DOUBLECLICK:
      switch (aButton) {
        case WidgetMouseEvent::eLeftButton:
          pluginEvent.event = WM_LBUTTONDBLCLK;
          break;
        case WidgetMouseEvent::eMiddleButton:
          pluginEvent.event = WM_MBUTTONDBLCLK;
          break;
        case WidgetMouseEvent::eRightButton:
          pluginEvent.event = WM_RBUTTONDBLCLK;
          break;
        default:
          break;
      }
      break;
    case NS_MOUSE_MOVE:
      pluginEvent.event = WM_MOUSEMOVE;
      break;
    case NS_MOUSE_EXIT:
      pluginEvent.event = WM_MOUSELEAVE;
      break;
    default:
      pluginEvent.event = WM_NULL;
      break;
  }

  pluginEvent.wParam = wParam;     // plugins NEED raw OS event flags!
  pluginEvent.lParam = lParam;

  event.mPluginEvent.Copy(pluginEvent);

  // call the event callback
  if (mWidgetListener) {
    if (nsToolkit::gMouseTrailer)
      nsToolkit::gMouseTrailer->Disable();
    if (aEventType == NS_MOUSE_MOVE) {
      if (nsToolkit::gMouseTrailer && !sIsInMouseCapture) {
        nsToolkit::gMouseTrailer->SetMouseTrailerWindow(mWnd);
      }
      nsIntRect rect;
      GetBounds(rect);
      rect.x = 0;
      rect.y = 0;

      if (rect.Contains(LayoutDeviceIntPoint::ToUntyped(event.refPoint))) {
        if (sCurrentWindow == nullptr || sCurrentWindow != this) {
          if ((nullptr != sCurrentWindow) && (!sCurrentWindow->mInDtor)) {
            LPARAM pos = sCurrentWindow->lParamToClient(lParamToScreen(lParam));
            sCurrentWindow->DispatchMouseEvent(NS_MOUSE_EXIT, wParam, pos, false, 
                                               WidgetMouseEvent::eLeftButton,
                                               aInputSource);
          }
          sCurrentWindow = this;
          if (!mInDtor) {
            LPARAM pos = sCurrentWindow->lParamToClient(lParamToScreen(lParam));
            sCurrentWindow->DispatchMouseEvent(NS_MOUSE_ENTER, wParam, pos, false,
                                               WidgetMouseEvent::eLeftButton,
                                               aInputSource);
          }
        }
      }
    } else if (aEventType == NS_MOUSE_EXIT) {
      if (sCurrentWindow == this) {
        sCurrentWindow = nullptr;
      }
    }

    result = ConvertStatus(DispatchInputEvent(&event));

    if (nsToolkit::gMouseTrailer)
      nsToolkit::gMouseTrailer->Enable();

    // Release the widget with NS_IF_RELEASE() just in case
    // the context menu key code in EventListenerManager::HandleEvent()
    // released it already.
    return result;
  }

  return result;
}

void nsWindow::DispatchFocusToTopLevelWindow(bool aIsActivate)
{
  if (aIsActivate)
    sJustGotActivate = false;
  sJustGotDeactivate = false;

  // retrive the toplevel window or dialog
  HWND curWnd = mWnd;
  HWND toplevelWnd = nullptr;
  while (curWnd) {
    toplevelWnd = curWnd;

    nsWindow *win = WinUtils::GetNSWindowPtr(curWnd);
    if (win) {
      nsWindowType wintype = win->WindowType();
      if (wintype == eWindowType_toplevel || wintype == eWindowType_dialog)
        break;
    }

    curWnd = ::GetParent(curWnd); // Parent or owner (if has no parent)
  }

  if (toplevelWnd) {
    nsWindow *win = WinUtils::GetNSWindowPtr(toplevelWnd);
    if (win && win->mWidgetListener) {
      if (aIsActivate) {
        win->mWidgetListener->WindowActivated();
      } else {
        if (!win->BlurEventsSuppressed()) {
          win->mWidgetListener->WindowDeactivated();
        }
      }
    }
  }
}

bool nsWindow::IsTopLevelMouseExit(HWND aWnd)
{
  DWORD pos = ::GetMessagePos();
  POINT mp;
  mp.x = GET_X_LPARAM(pos);
  mp.y = GET_Y_LPARAM(pos);
  HWND mouseWnd = ::WindowFromPoint(mp);

  // WinUtils::GetTopLevelHWND() will return a HWND for the window frame
  // (which includes the non-client area).  If the mouse has moved into
  // the non-client area, we should treat it as a top-level exit.
  HWND mouseTopLevel = WinUtils::GetTopLevelHWND(mouseWnd);
  if (mouseWnd == mouseTopLevel)
    return true;

  return WinUtils::GetTopLevelHWND(aWnd) != mouseTopLevel;
}

bool nsWindow::BlurEventsSuppressed()
{
  // are they suppressed in this window?
  if (mBlurSuppressLevel > 0)
    return true;

  // are they suppressed by any container widget?
  HWND parentWnd = ::GetParent(mWnd);
  if (parentWnd) {
    nsWindow *parent = WinUtils::GetNSWindowPtr(parentWnd);
    if (parent)
      return parent->BlurEventsSuppressed();
  }
  return false;
}

// In some circumstances (opening dependent windows) it makes more sense
// (and fixes a crash bug) to not blur the parent window. Called from
// nsFilePicker.
void nsWindow::SuppressBlurEvents(bool aSuppress)
{
  if (aSuppress)
    ++mBlurSuppressLevel; // for this widget
  else {
    NS_ASSERTION(mBlurSuppressLevel > 0, "unbalanced blur event suppression");
    if (mBlurSuppressLevel > 0)
      --mBlurSuppressLevel;
  }
}

bool nsWindow::ConvertStatus(nsEventStatus aStatus)
{
  return aStatus == nsEventStatus_eConsumeNoDefault;
}

/**************************************************************
 *
 * SECTION: IPC
 *
 * IPC related helpers.
 *
 **************************************************************/

// static
bool
nsWindow::IsAsyncResponseEvent(UINT aMsg, LRESULT& aResult)
{
  switch(aMsg) {
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
    case WM_ENABLE:
    case WM_WINDOWPOSCHANGING:
    case WM_WINDOWPOSCHANGED:
    case WM_PARENTNOTIFY:
    case WM_ACTIVATEAPP:
    case WM_NCACTIVATE:
    case WM_ACTIVATE:
    case WM_CHILDACTIVATE:
    case WM_IME_SETCONTEXT:
    case WM_IME_NOTIFY:
    case WM_SHOWWINDOW:
    case WM_CANCELMODE:
    case WM_MOUSEACTIVATE:
    case WM_CONTEXTMENU:
      aResult = 0;
    return true;

    case WM_SETTINGCHANGE:
    case WM_SETCURSOR:
    return false;
  }

#ifdef DEBUG
  char szBuf[200];
  sprintf(szBuf,
    "An unhandled ISMEX_SEND message was received during spin loop! (%X)", aMsg);
  NS_WARNING(szBuf);
#endif

  return false;
}

void
nsWindow::IPCWindowProcHandler(UINT& msg, WPARAM& wParam, LPARAM& lParam)
{
  MOZ_ASSERT_IF(msg != WM_GETOBJECT,
                !mozilla::ipc::MessageChannel::IsPumpingMessages());

  // Modal UI being displayed in windowless plugins.
  if (mozilla::ipc::MessageChannel::IsSpinLoopActive() &&
      (InSendMessageEx(nullptr) & (ISMEX_REPLIED|ISMEX_SEND)) == ISMEX_SEND) {
    LRESULT res;
    if (IsAsyncResponseEvent(msg, res)) {
      ReplyMessage(res);
    }
    return;
  }

  // Handle certain sync plugin events sent to the parent which
  // trigger ipc calls that result in deadlocks.

  DWORD dwResult = 0;
  bool handled = false;

  switch(msg) {
    // Windowless flash sending WM_ACTIVATE events to the main window
    // via calls to ShowWindow.
    case WM_ACTIVATE:
      if (lParam != 0 && LOWORD(wParam) == WA_ACTIVE &&
          IsWindow((HWND)lParam)) {
        // Check for Adobe Reader X sync activate message from their
        // helper window and ignore. Fixes an annoying focus problem.
        if ((InSendMessageEx(nullptr) & (ISMEX_REPLIED|ISMEX_SEND)) == ISMEX_SEND) {
          wchar_t szClass[10];
          HWND focusWnd = (HWND)lParam;
          if (IsWindowVisible(focusWnd) &&
              GetClassNameW(focusWnd, szClass,
                            sizeof(szClass)/sizeof(char16_t)) &&
              !wcscmp(szClass, L"Edit") &&
              !WinUtils::IsOurProcessWindow(focusWnd)) {
            break;
          }
        }
        handled = true;
      }
    break;
    // Plugins taking or losing focus triggering focus app messages.
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
    // Windowed plugins that pass sys key events to defwndproc generate
    // WM_SYSCOMMAND events to the main window.
    case WM_SYSCOMMAND:
    // Windowed plugins that fire context menu selection events to parent
    // windows.
    case WM_CONTEXTMENU:
    // IME events fired as a result of synchronous focus changes
    case WM_IME_SETCONTEXT:
      handled = true;
    break;
  }

  if (handled &&
      (InSendMessageEx(nullptr) & (ISMEX_REPLIED|ISMEX_SEND)) == ISMEX_SEND) {
    ReplyMessage(dwResult);
  }
}

/**************************************************************
 **************************************************************
 **
 ** BLOCK: Native events
 **
 ** Main Windows message handlers and OnXXX handlers for
 ** Windows event handling.
 **
 **************************************************************
 **************************************************************/

/**************************************************************
 *
 * SECTION: Wind proc.
 *
 * The main Windows event procedures and associated
 * message processing methods.
 *
 **************************************************************/

static bool
DisplaySystemMenu(HWND hWnd, nsSizeMode sizeMode, bool isRtl, int32_t x, int32_t y)
{
  HMENU hMenu = GetSystemMenu(hWnd, FALSE);
  if (hMenu) {
    MENUITEMINFO mii;
    mii.cbSize = sizeof(MENUITEMINFO);
    mii.fMask = MIIM_STATE;
    mii.fType = 0;

    // update the options
    mii.fState = MF_ENABLED;
    SetMenuItemInfo(hMenu, SC_RESTORE, FALSE, &mii);
    SetMenuItemInfo(hMenu, SC_SIZE, FALSE, &mii);
    SetMenuItemInfo(hMenu, SC_MOVE, FALSE, &mii);
    SetMenuItemInfo(hMenu, SC_MAXIMIZE, FALSE, &mii);
    SetMenuItemInfo(hMenu, SC_MINIMIZE, FALSE, &mii);

    mii.fState = MF_GRAYED;
    switch(sizeMode) {
      case nsSizeMode_Fullscreen:
        // intentional fall through
      case nsSizeMode_Maximized:
        SetMenuItemInfo(hMenu, SC_SIZE, FALSE, &mii);
        SetMenuItemInfo(hMenu, SC_MOVE, FALSE, &mii);
        SetMenuItemInfo(hMenu, SC_MAXIMIZE, FALSE, &mii);
        break;
      case nsSizeMode_Minimized:
        SetMenuItemInfo(hMenu, SC_MINIMIZE, FALSE, &mii);
        break;
      case nsSizeMode_Normal:
        SetMenuItemInfo(hMenu, SC_RESTORE, FALSE, &mii);
        break;
    }
    LPARAM cmd =
      TrackPopupMenu(hMenu,
                     (TPM_LEFTBUTTON|TPM_RIGHTBUTTON|
                      TPM_RETURNCMD|TPM_TOPALIGN|
                      (isRtl ? TPM_RIGHTALIGN : TPM_LEFTALIGN)),
                     x, y, 0, hWnd, nullptr);
    if (cmd) {
      PostMessage(hWnd, WM_SYSCOMMAND, cmd, 0);
      return true;
    }
  }
  return false;
}

inline static mozilla::HangMonitor::ActivityType ActivityTypeForMessage(UINT msg)
{
  if ((msg >= WM_KEYFIRST && msg <= WM_IME_KEYLAST) ||
      (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST) ||
      (msg >= MOZ_WM_MOUSEWHEEL_FIRST && msg <= MOZ_WM_MOUSEWHEEL_LAST) ||
      (msg >= NS_WM_IMEFIRST && msg <= NS_WM_IMELAST)) {
    return mozilla::HangMonitor::kUIActivity;
  }

  // This may not actually be right, but we don't want to reset the timer if
  // we're not actually processing a UI message.
  return mozilla::HangMonitor::kActivityUIAVail;
}

// The WndProc procedure for all nsWindows in this toolkit. This merely catches
// exceptions and passes the real work to WindowProcInternal. See bug 587406
// and http://msdn.microsoft.com/en-us/library/ms633573%28VS.85%29.aspx
LRESULT CALLBACK nsWindow::WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  MOZ_RELEASE_ASSERT(!ipc::ParentProcessIsBlocked());

  HangMonitor::NotifyActivity(ActivityTypeForMessage(msg));

  return mozilla::CallWindowProcCrashProtected(WindowProcInternal, hWnd, msg, wParam, lParam);
}

LRESULT CALLBACK nsWindow::WindowProcInternal(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  if (::GetWindowLongPtrW(hWnd, GWLP_ID) == eFakeTrackPointScrollableID) {
    // This message was sent to the FAKETRACKPOINTSCROLLABLE.
    if (msg == WM_HSCROLL) {
      // Route WM_HSCROLL messages to the main window.
      hWnd = ::GetParent(::GetParent(hWnd));
    } else {
      // Handle all other messages with its original window procedure.
      WNDPROC prevWindowProc = (WNDPROC)::GetWindowLongPtr(hWnd, GWLP_USERDATA);
      return ::CallWindowProcW(prevWindowProc, hWnd, msg, wParam, lParam);
    }
  }

  if (msg == MOZ_WM_TRACE) {
    // This is a tracer event for measuring event loop latency.
    // See WidgetTraceEvent.cpp for more details.
    mozilla::SignalTracerThread();
    return 0;
  }

  // Get the window which caused the event and ask it to process the message
  nsWindow *targetWindow = WinUtils::GetNSWindowPtr(hWnd);
  NS_ASSERTION(targetWindow, "nsWindow* is null!");
  if (!targetWindow)
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);

  // Hold the window for the life of this method, in case it gets
  // destroyed during processing, unless we're in the dtor already.
  nsCOMPtr<nsIWidget> kungFuDeathGrip;
  if (!targetWindow->mInDtor)
    kungFuDeathGrip = targetWindow;

  targetWindow->IPCWindowProcHandler(msg, wParam, lParam);

  // Create this here so that we store the last rolled up popup until after
  // the event has been processed.
  nsAutoRollup autoRollup;

  LRESULT popupHandlingResult;
  if (DealWithPopups(hWnd, msg, wParam, lParam, &popupHandlingResult))
    return popupHandlingResult;

  // Call ProcessMessage
  LRESULT retValue;
  if (targetWindow->ProcessMessage(msg, wParam, lParam, &retValue)) {
    return retValue;
  }

  LRESULT res = ::CallWindowProcW(targetWindow->GetPrevWindowProc(),
                                  hWnd, msg, wParam, lParam);

  return res;
}

// The main windows message processing method for plugins.
// The result means whether this method processed the native
// event for plugin. If false, the native event should be
// processed by the caller self.
bool
nsWindow::ProcessMessageForPlugin(const MSG &aMsg,
                                  MSGResult& aResult)
{
  aResult.mResult = 0;
  aResult.mConsumed = true;

  bool eventDispatched = false;
  switch (aMsg.message) {
    case WM_CHAR:
    case WM_SYSCHAR:
      aResult.mResult = ProcessCharMessage(aMsg, &eventDispatched);
      break;

    case WM_KEYUP:
    case WM_SYSKEYUP:
      aResult.mResult = ProcessKeyUpMessage(aMsg, &eventDispatched);
      break;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
      aResult.mResult = ProcessKeyDownMessage(aMsg, &eventDispatched);
      break;

    case WM_DEADCHAR:
    case WM_SYSDEADCHAR:

    case WM_CUT:
    case WM_COPY:
    case WM_PASTE:
    case WM_CLEAR:
    case WM_UNDO:
      break;

    default:
      return false;
  }

  if (!eventDispatched) {
    aResult.mConsumed = nsWindowBase::DispatchPluginEvent(aMsg);
  }
  if (!Destroyed()) {
    DispatchPendingEvents();
  }
  return true;
}

static void ForceFontUpdate()
{
  // update device context font cache
  // Dirty but easiest way:
  // Changing nsIPrefBranch entry which triggers callbacks
  // and flows into calling mDeviceContext->FlushFontCache()
  // to update the font cache in all the instance of Browsers
  static const char kPrefName[] = "font.internaluseonly.changed";
  bool fontInternalChange =
    Preferences::GetBool(kPrefName, false);
  Preferences::SetBool(kPrefName, !fontInternalChange);
}

static bool CleartypeSettingChanged()
{
  static int currentQuality = -1;
  BYTE quality = cairo_win32_get_system_text_quality();

  if (currentQuality == quality)
    return false;

  if (currentQuality < 0) {
    currentQuality = quality;
    return false;
  }
  currentQuality = quality;
  return true;
}

bool
nsWindow::ExternalHandlerProcessMessage(UINT aMessage,
                                        WPARAM& aWParam,
                                        LPARAM& aLParam,
                                        MSGResult& aResult)
{
  if (mWindowHook.Notify(mWnd, aMessage, aWParam, aLParam, aResult)) {
    return true;
  }

  if (IMEHandler::ProcessMessage(this, aMessage, aWParam, aLParam, aResult)) {
    return true;
  }

  if (MouseScrollHandler::ProcessMessage(this, aMessage, aWParam, aLParam,
                                         aResult)) {
    return true;
  }

  if (PluginHasFocus()) {
    MSG nativeMsg = WinUtils::InitMSG(aMessage, aWParam, aLParam, mWnd);
    if (ProcessMessageForPlugin(nativeMsg, aResult)) {
      return true;
    }
  }

  return false;
}

// The main windows message processing method.
bool
nsWindow::ProcessMessage(UINT msg, WPARAM& wParam, LPARAM& lParam,
                         LRESULT *aRetValue)
{
#if defined(EVENT_DEBUG_OUTPUT)
  // First param shows all events, second param indicates whether
  // to show mouse move events. See nsWindowDbg for details.
  PrintEvent(msg, SHOW_REPEAT_EVENTS, SHOW_MOUSEMOVE_EVENTS);
#endif

  MSGResult msgResult(aRetValue);
  if (ExternalHandlerProcessMessage(msg, wParam, lParam, msgResult)) {
    return (msgResult.mConsumed || !mWnd);
  }

  bool result = false;    // call the default nsWindow proc
  *aRetValue = 0;

  // Glass hit testing w/custom transparent margins
  LRESULT dwmHitResult;
  if (mCustomNonClient &&
      nsUXThemeData::CheckForCompositor() &&
      WinUtils::dwmDwmDefWindowProcPtr(mWnd, msg, wParam, lParam, &dwmHitResult)) {
    *aRetValue = dwmHitResult;
    return true;
  }

  // (Large blocks of code should be broken out into OnEvent handlers.)
  switch (msg) {
    // Manually reset the cursor if WM_SETCURSOR is requested while over
    // application content (as opposed to Windows borders, etc).
    case WM_SETCURSOR:
      if (LOWORD(lParam) == HTCLIENT)
      {
        SetCursor(GetCursor());
        result = true;
      }
      break;

    // WM_QUERYENDSESSION must be handled by all windows.
    // Otherwise Windows thinks the window can just be killed at will.
    case WM_QUERYENDSESSION:
      if (sCanQuit == TRI_UNKNOWN)
      {
        // Ask if it's ok to quit, and store the answer until we
        // get WM_ENDSESSION signaling the round is complete.
        nsCOMPtr<nsIObserverService> obsServ =
          mozilla::services::GetObserverService();
        nsCOMPtr<nsISupportsPRBool> cancelQuit =
          do_CreateInstance(NS_SUPPORTS_PRBOOL_CONTRACTID);
        cancelQuit->SetData(false);
        obsServ->NotifyObservers(cancelQuit, "quit-application-requested", nullptr);

        bool abortQuit;
        cancelQuit->GetData(&abortQuit);
        sCanQuit = abortQuit ? TRI_FALSE : TRI_TRUE;
      }
      *aRetValue = sCanQuit ? TRUE : FALSE;
      result = true;
      break;

    case WM_ENDSESSION:
    case MOZ_WM_APP_QUIT:
      if (msg == MOZ_WM_APP_QUIT || (wParam == TRUE && sCanQuit == TRI_TRUE))
      {
        // Let's fake a shutdown sequence without actually closing windows etc.
        // to avoid Windows killing us in the middle. A proper shutdown would
        // require having a chance to pump some messages. Unfortunately
        // Windows won't let us do that. Bug 212316.
        nsCOMPtr<nsIObserverService> obsServ =
          mozilla::services::GetObserverService();
        NS_NAMED_LITERAL_STRING(context, "shutdown-persist");
        obsServ->NotifyObservers(nullptr, "quit-application-granted", nullptr);
        obsServ->NotifyObservers(nullptr, "quit-application-forced", nullptr);
        obsServ->NotifyObservers(nullptr, "quit-application", nullptr);
        obsServ->NotifyObservers(nullptr, "profile-change-net-teardown", context.get());
        obsServ->NotifyObservers(nullptr, "profile-change-teardown", context.get());
        obsServ->NotifyObservers(nullptr, "profile-before-change", context.get());
        obsServ->NotifyObservers(nullptr, "profile-before-change2", context.get());
        // Then a controlled but very quick exit.
        _exit(0);
      }
      sCanQuit = TRI_UNKNOWN;
      result = true;
      break;

    case WM_SYSCOLORCHANGE:
      OnSysColorChanged();
      break;

    case WM_THEMECHANGED:
    {
      // Update non-client margin offsets 
      UpdateNonClientMargins();
      nsUXThemeData::InitTitlebarInfo();
      nsUXThemeData::UpdateNativeThemeInfo();

      NotifyThemeChanged();

      // Invalidate the window so that the repaint will
      // pick up the new theme.
      Invalidate(true, true, true);
    }
    break;

    case WM_WTSSESSION_CHANGE:
    {
      switch (wParam) {
        case WTS_CONSOLE_CONNECT:
        case WTS_REMOTE_CONNECT:
        case WTS_SESSION_UNLOCK:
          // When a session becomes visible, we should invalidate.
          Invalidate(true, true, true);
          break;
        default:
          break;
      }
    }
    break;

    case WM_FONTCHANGE:
    {
      // We only handle this message for the hidden window,
      // as we only need to update the (global) font list once
      // for any given change, not once per window!
      if (mWindowType != eWindowType_invisible) {
        break;
      }

      nsresult rv;
      bool didChange = false;

      // update the global font list
      nsCOMPtr<nsIFontEnumerator> fontEnum = do_GetService("@mozilla.org/gfx/fontenumerator;1", &rv);
      if (NS_SUCCEEDED(rv)) {
        fontEnum->UpdateFontList(&didChange);
        ForceFontUpdate();
      } //if (NS_SUCCEEDED(rv))
    }
    break;

    case WM_NCCALCSIZE:
    {
      if (mCustomNonClient) {
        // If `wParam` is `FALSE`, `lParam` points to a `RECT` that contains
        // the proposed window rectangle for our window.  During our
        // processing of the `WM_NCCALCSIZE` message, we are expected to
        // modify the `RECT` that `lParam` points to, so that its value upon
        // our return is the new client area.  We must return 0 if `wParam`
        // is `FALSE`.
        //
        // If `wParam` is `TRUE`, `lParam` points to a `NCCALCSIZE_PARAMS`
        // struct.  This struct contains an array of 3 `RECT`s, the first of
        // which has the exact same meaning as the `RECT` that is pointed to
        // by `lParam` when `wParam` is `FALSE`.  The remaining `RECT`s, in
        // conjunction with our return value, can
        // be used to specify portions of the source and destination window
        // rectangles that are valid and should be preserved.  We opt not to
        // implement an elaborate client-area preservation technique, and
        // simply return 0, which means "preserve the entire old client area
        // and align it with the upper-left corner of our new client area".
        RECT *clientRect = wParam
                         ? &(reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam))->rgrc[0]
                         : (reinterpret_cast<RECT*>(lParam));
        clientRect->top      += (mCaptionHeight - mNonClientOffset.top);
        clientRect->left     += (mHorResizeMargin - mNonClientOffset.left);
        clientRect->right    -= (mHorResizeMargin - mNonClientOffset.right);
        clientRect->bottom   -= (mVertResizeMargin - mNonClientOffset.bottom);

        result = true;
        *aRetValue = 0;
      }
      break;
    }

    case WM_NCHITTEST:
    {
      if (mMouseTransparent) {
        // Treat this window as transparent.
        *aRetValue = HTTRANSPARENT;
        result = true;
        break;
      }

      /*
       * If an nc client area margin has been moved, we are responsible
       * for calculating where the resize margins are and returning the
       * appropriate set of hit test constants. DwmDefWindowProc (above)
       * will handle hit testing on it's command buttons if we are on a
       * composited desktop.
       */

      if (!mCustomNonClient)
        break;

      *aRetValue =
        ClientMarginHitTestPoint(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
      result = true;
      break;
    }

    case WM_SETTEXT:
      /*
       * WM_SETTEXT paints the titlebar area. Avoid this if we have a
       * custom titlebar we paint ourselves.
       */

      if (!mCustomNonClient || mNonClientMargins.top == -1)
        break;

      {
        // From msdn, the way around this is to disable the visible state
        // temporarily. We need the text to be set but we don't want the
        // redraw to occur.
        DWORD style = GetWindowLong(mWnd, GWL_STYLE);
        SetWindowLong(mWnd, GWL_STYLE, style & ~WS_VISIBLE);
        *aRetValue = CallWindowProcW(GetPrevWindowProc(), mWnd,
                                     msg, wParam, lParam);
        SetWindowLong(mWnd, GWL_STYLE, style);
        return true;
      }

    case WM_NCACTIVATE:
    {
      /*
       * WM_NCACTIVATE paints nc areas. Avoid this and re-route painting
       * through WM_NCPAINT via InvalidateNonClientRegion.
       */
      UpdateGetWindowInfoCaptionStatus(FALSE != wParam);

      if (!mCustomNonClient)
        break;

      // let the dwm handle nc painting on glass
      // Never allow native painting if we are on fullscreen
      if(mSizeMode != nsSizeMode_Fullscreen &&
         nsUXThemeData::CheckForCompositor())
        break;

      if (wParam == TRUE) {
        // going active
        *aRetValue = FALSE; // ignored
        result = true;
        // invalidate to trigger a paint
        InvalidateNonClientRegion();
        break;
      } else {
        // going inactive
        *aRetValue = TRUE; // go ahead and deactive
        result = true;
        // invalidate to trigger a paint
        InvalidateNonClientRegion();
        break;
      }
    }

    case WM_NCPAINT:
    {
      /*
       * Reset the non-client paint region so that it excludes the
       * non-client areas we paint manually. Then call defwndproc
       * to do the actual painting.
       */

      if (!mCustomNonClient)
        break;

      // let the dwm handle nc painting on glass
      if(nsUXThemeData::CheckForCompositor())
        break;

      HRGN paintRgn = ExcludeNonClientFromPaintRegion((HRGN)wParam);
      LRESULT res = CallWindowProcW(GetPrevWindowProc(), mWnd,
                                    msg, (WPARAM)paintRgn, lParam);
      if (paintRgn != (HRGN)wParam)
        DeleteObject(paintRgn);
      *aRetValue = res;
      result = true;
    }
    break;

    case WM_POWERBROADCAST:
      switch (wParam)
      {
        case PBT_APMSUSPEND:
          PostSleepWakeNotification(true);
          break;
        case PBT_APMRESUMEAUTOMATIC:
        case PBT_APMRESUMECRITICAL:
        case PBT_APMRESUMESUSPEND:
          PostSleepWakeNotification(false);
          break;
      }
      break;

    case WM_CLOSE: // close request
      if (mWidgetListener)
        mWidgetListener->RequestWindowClose(this);
      result = true; // abort window closure
      break;

    case WM_DESTROY:
      // clean up.
      OnDestroy();
      result = true;
      break;

    case WM_PAINT:
      if (CleartypeSettingChanged()) {
        ForceFontUpdate();
        gfxFontCache *fc = gfxFontCache::GetCache();
        if (fc) {
          fc->Flush();
        }
      }
      *aRetValue = (int) OnPaint(nullptr, 0);
      result = true;
      break;

    case WM_PRINTCLIENT:
      result = OnPaint((HDC) wParam, 0);
      break;

    case WM_HOTKEY:
      result = OnHotKey(wParam, lParam);
      break;

    case WM_SYSCHAR:
    case WM_CHAR:
    {
      MSG nativeMsg = WinUtils::InitMSG(msg, wParam, lParam, mWnd);
      result = ProcessCharMessage(nativeMsg, nullptr);
      DispatchPendingEvents();
    }
    break;

    case WM_SYSKEYUP:
    case WM_KEYUP:
    {
      MSG nativeMsg = WinUtils::InitMSG(msg, wParam, lParam, mWnd);
      nativeMsg.time = ::GetMessageTime();
      result = ProcessKeyUpMessage(nativeMsg, nullptr);
      DispatchPendingEvents();
    }
    break;

    case WM_SYSKEYDOWN:
    case WM_KEYDOWN:
    {
      MSG nativeMsg = WinUtils::InitMSG(msg, wParam, lParam, mWnd);
      result = ProcessKeyDownMessage(nativeMsg, nullptr);
      DispatchPendingEvents();
    }
    break;

    // say we've dealt with erase background if widget does
    // not need auto-erasing
    case WM_ERASEBKGND:
      if (!AutoErase((HDC)wParam)) {
        *aRetValue = 1;
        result = true;
      }
      break;

    case WM_MOUSEMOVE:
    {
      mMousePresent = true;

      // Suppress dispatch of pending events
      // when mouse moves are generated by widget
      // creation instead of user input.
      LPARAM lParamScreen = lParamToScreen(lParam);
      POINT mp;
      mp.x      = GET_X_LPARAM(lParamScreen);
      mp.y      = GET_Y_LPARAM(lParamScreen);
      bool userMovedMouse = false;
      if ((sLastMouseMovePoint.x != mp.x) || (sLastMouseMovePoint.y != mp.y)) {
        userMovedMouse = true;
      }

      result = DispatchMouseEvent(NS_MOUSE_MOVE, wParam, lParam,
                                  false, WidgetMouseEvent::eLeftButton,
                                  MOUSE_INPUT_SOURCE());
      if (userMovedMouse) {
        DispatchPendingEvents();
      }
    }
    break;

    case WM_NCMOUSEMOVE:
      // If we receive a mouse move event on non-client chrome, make sure and
      // send an NS_MOUSE_EXIT event as well.
      if (mMousePresent && !sIsInMouseCapture)
        SendMessage(mWnd, WM_MOUSELEAVE, 0, 0);
    break;

    case WM_LBUTTONDOWN:
    {
      result = DispatchMouseEvent(NS_MOUSE_BUTTON_DOWN, wParam, lParam,
                                  false, WidgetMouseEvent::eLeftButton,
                                  MOUSE_INPUT_SOURCE());
      DispatchPendingEvents();
    }
    break;

    case WM_LBUTTONUP:
    {
      result = DispatchMouseEvent(NS_MOUSE_BUTTON_UP, wParam, lParam,
                                  false, WidgetMouseEvent::eLeftButton,
                                  MOUSE_INPUT_SOURCE());
      DispatchPendingEvents();
    }
    break;

    case WM_MOUSELEAVE:
    {
      if (!mMousePresent)
        break;
      mMousePresent = false;

      // We need to check mouse button states and put them in for
      // wParam.
      WPARAM mouseState = (GetKeyState(VK_LBUTTON) ? MK_LBUTTON : 0)
        | (GetKeyState(VK_MBUTTON) ? MK_MBUTTON : 0)
        | (GetKeyState(VK_RBUTTON) ? MK_RBUTTON : 0);
      // Synthesize an event position because we don't get one from
      // WM_MOUSELEAVE.
      LPARAM pos = lParamToClient(::GetMessagePos());
      DispatchMouseEvent(NS_MOUSE_EXIT, mouseState, pos, false,
                         WidgetMouseEvent::eLeftButton, MOUSE_INPUT_SOURCE());
    }
    break;

    case WM_CONTEXTMENU:
    {
      // if the context menu is brought up from the keyboard, |lParam|
      // will be -1.
      LPARAM pos;
      bool contextMenukey = false;
      if (lParam == -1)
      {
        contextMenukey = true;
        pos = lParamToClient(GetMessagePos());
      }
      else
      {
        pos = lParamToClient(lParam);
      }

      result = DispatchMouseEvent(NS_CONTEXTMENU, wParam, pos, contextMenukey,
                                  contextMenukey ?
                                    WidgetMouseEvent::eLeftButton :
                                    WidgetMouseEvent::eRightButton,
                                  MOUSE_INPUT_SOURCE());
      if (lParam != -1 && !result && mCustomNonClient) {
        WidgetMouseEvent event(true, NS_MOUSE_MOZHITTEST, this,
                               WidgetMouseEvent::eReal,
                               WidgetMouseEvent::eNormal);
        event.refPoint = LayoutDeviceIntPoint(GET_X_LPARAM(pos), GET_Y_LPARAM(pos));
        event.inputSource = MOUSE_INPUT_SOURCE();
        event.mFlags.mOnlyChromeDispatch = true;
        if (DispatchWindowEvent(&event)) {
          // Blank area hit, throw up the system menu.
          DisplaySystemMenu(mWnd, mSizeMode, mIsRTL, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
          result = true;
        }
      }
    }
    break;

    case WM_LBUTTONDBLCLK:
      result = DispatchMouseEvent(NS_MOUSE_DOUBLECLICK, wParam,
                                  lParam, false,
                                  WidgetMouseEvent::eLeftButton,
                                  MOUSE_INPUT_SOURCE());
      DispatchPendingEvents();
      break;

    case WM_MBUTTONDOWN:
      result = DispatchMouseEvent(NS_MOUSE_BUTTON_DOWN, wParam,
                                  lParam, false,
                                  WidgetMouseEvent::eMiddleButton,
                                  MOUSE_INPUT_SOURCE());
      DispatchPendingEvents();
      break;

    case WM_MBUTTONUP:
      result = DispatchMouseEvent(NS_MOUSE_BUTTON_UP, wParam,
                                  lParam, false,
                                  WidgetMouseEvent::eMiddleButton,
                                  MOUSE_INPUT_SOURCE());
      DispatchPendingEvents();
      break;

    case WM_MBUTTONDBLCLK:
      result = DispatchMouseEvent(NS_MOUSE_DOUBLECLICK, wParam,
                                  lParam, false,
                                  WidgetMouseEvent::eMiddleButton,
                                  MOUSE_INPUT_SOURCE());
      DispatchPendingEvents();
      break;

    case WM_NCMBUTTONDOWN:
      result = DispatchMouseEvent(NS_MOUSE_BUTTON_DOWN, 0,
                                  lParamToClient(lParam), false,
                                  WidgetMouseEvent::eMiddleButton,
                                  MOUSE_INPUT_SOURCE());
      DispatchPendingEvents();
      break;

    case WM_NCMBUTTONUP:
      result = DispatchMouseEvent(NS_MOUSE_BUTTON_UP, 0,
                                  lParamToClient(lParam), false,
                                  WidgetMouseEvent::eMiddleButton,
                                  MOUSE_INPUT_SOURCE());
      DispatchPendingEvents();
      break;

    case WM_NCMBUTTONDBLCLK:
      result = DispatchMouseEvent(NS_MOUSE_DOUBLECLICK, 0,
                                  lParamToClient(lParam), false,
                                  WidgetMouseEvent::eMiddleButton,
                                  MOUSE_INPUT_SOURCE());
      DispatchPendingEvents();
      break;

    case WM_RBUTTONDOWN:
      result = DispatchMouseEvent(NS_MOUSE_BUTTON_DOWN, wParam,
                                  lParam, false,
                                  WidgetMouseEvent::eRightButton,
                                  MOUSE_INPUT_SOURCE());
      DispatchPendingEvents();
      break;

    case WM_RBUTTONUP:
      result = DispatchMouseEvent(NS_MOUSE_BUTTON_UP, wParam,
                                  lParam, false,
                                  WidgetMouseEvent::eRightButton,
                                  MOUSE_INPUT_SOURCE());
      DispatchPendingEvents();
      break;

    case WM_RBUTTONDBLCLK:
      result = DispatchMouseEvent(NS_MOUSE_DOUBLECLICK, wParam,
                                  lParam, false,
                                  WidgetMouseEvent::eRightButton,
                                  MOUSE_INPUT_SOURCE());
      DispatchPendingEvents();
      break;

    case WM_NCRBUTTONDOWN:
      result = DispatchMouseEvent(NS_MOUSE_BUTTON_DOWN, 0,
                                  lParamToClient(lParam), false,
                                  WidgetMouseEvent::eRightButton,
                                  MOUSE_INPUT_SOURCE());
      DispatchPendingEvents();
      break;

    case WM_NCRBUTTONUP:
      result = DispatchMouseEvent(NS_MOUSE_BUTTON_UP, 0,
                                  lParamToClient(lParam), false,
                                  WidgetMouseEvent::eRightButton,
                                  MOUSE_INPUT_SOURCE());
      DispatchPendingEvents();
      break;

    case WM_NCRBUTTONDBLCLK:
      result = DispatchMouseEvent(NS_MOUSE_DOUBLECLICK, 0,
                                  lParamToClient(lParam), false,
                                  WidgetMouseEvent::eRightButton,
                                  MOUSE_INPUT_SOURCE());
      DispatchPendingEvents();
      break;

    case WM_EXITSIZEMOVE:
      if (!sIsInMouseCapture) {
        NotifySizeMoveDone();
      }
      break;

    case WM_NCLBUTTONDBLCLK:
      DispatchMouseEvent(NS_MOUSE_DOUBLECLICK, 0, lParamToClient(lParam),
                         false, WidgetMouseEvent::eLeftButton,
                         MOUSE_INPUT_SOURCE());
      result = 
        DispatchMouseEvent(NS_MOUSE_BUTTON_UP, 0, lParamToClient(lParam),
                           false, WidgetMouseEvent::eLeftButton,
                           MOUSE_INPUT_SOURCE());
      DispatchPendingEvents();
      break;

    case WM_APPCOMMAND:
    {
      MSG nativeMsg = WinUtils::InitMSG(msg, wParam, lParam, mWnd);
      result = HandleAppCommandMsg(nativeMsg, aRetValue);
      break;
    }

    // The WM_ACTIVATE event is fired when a window is raised or lowered,
    // and the loword of wParam specifies which. But we don't want to tell
    // the focus system about this until the WM_SETFOCUS or WM_KILLFOCUS
    // events are fired. Instead, set either the sJustGotActivate or
    // gJustGotDeactivate flags and activate/deactivate once the focus
    // events arrive.
    case WM_ACTIVATE:
      if (mWidgetListener) {
        int32_t fActive = LOWORD(wParam);

        if (WA_INACTIVE == fActive) {
          // when minimizing a window, the deactivation and focus events will
          // be fired in the reverse order. Instead, just deactivate right away.
          if (HIWORD(wParam))
            DispatchFocusToTopLevelWindow(false);
          else
            sJustGotDeactivate = true;

          if (mIsTopWidgetWindow)
            mLastKeyboardLayout = KeyboardLayout::GetInstance()->GetLayout();

        } else {
          StopFlashing();

          sJustGotActivate = true;
          WidgetMouseEvent event(true, NS_MOUSE_ACTIVATE, this,
                                 WidgetMouseEvent::eReal);
          InitEvent(event);
          ModifierKeyState modifierKeyState;
          modifierKeyState.InitInputEvent(event);
          DispatchInputEvent(&event);
          if (sSwitchKeyboardLayout && mLastKeyboardLayout)
            ActivateKeyboardLayout(mLastKeyboardLayout, 0);
        }
      }
      break;

    case WM_MOUSEACTIVATE:
      // A popup with a parent owner should not be activated when clicked but
      // should still allow the mouse event to be fired, so the return value
      // is set to MA_NOACTIVATE. But if the owner isn't the frontmost window,
      // just use default processing so that the window is activated.
      if (IsPopup() && IsOwnerForegroundWindow()) {
        *aRetValue = MA_NOACTIVATE;
        result = true;
      }
      break;

    case WM_WINDOWPOSCHANGING:
    {
      LPWINDOWPOS info = (LPWINDOWPOS)lParam;
      OnWindowPosChanging(info);
      result = true;
    }
    break;

    case WM_GETMINMAXINFO:
    {
      MINMAXINFO* mmi = (MINMAXINFO*)lParam;
      // Set the constraints. The minimum size should also be constrained to the
      // default window maximum size so that it fits on screen.
      mmi->ptMinTrackSize.x =
        std::min((int32_t)mmi->ptMaxTrackSize.x,
               std::max((int32_t)mmi->ptMinTrackSize.x, mSizeConstraints.mMinSize.width));
      mmi->ptMinTrackSize.y =
        std::min((int32_t)mmi->ptMaxTrackSize.y,
        std::max((int32_t)mmi->ptMinTrackSize.y, mSizeConstraints.mMinSize.height));
      mmi->ptMaxTrackSize.x = std::min((int32_t)mmi->ptMaxTrackSize.x, mSizeConstraints.mMaxSize.width);
      mmi->ptMaxTrackSize.y = std::min((int32_t)mmi->ptMaxTrackSize.y, mSizeConstraints.mMaxSize.height);
    }
    break;

    case WM_SETFOCUS:
      // If previous focused window isn't ours, it must have received the
      // redirected message.  So, we should forget it.
      if (!WinUtils::IsOurProcessWindow(HWND(wParam))) {
        RedirectedKeyDownMessageManager::Forget();
      }
      if (sJustGotActivate) {
        DispatchFocusToTopLevelWindow(true);
      }
      break;

    case WM_KILLFOCUS:
      if (sJustGotDeactivate) {
        DispatchFocusToTopLevelWindow(false);
      }
      break;

    case WM_WINDOWPOSCHANGED:
    {
      WINDOWPOS* wp = (LPWINDOWPOS)lParam;
      OnWindowPosChanged(wp);
      result = true;
    }
    break;

    case WM_INPUTLANGCHANGEREQUEST:
      *aRetValue = TRUE;
      result = false;
      break;

    case WM_INPUTLANGCHANGE:
      KeyboardLayout::GetInstance()->
        OnLayoutChange(reinterpret_cast<HKL>(lParam));
      result = false; // always pass to child window
      break;

    case WM_DESTROYCLIPBOARD:
    {
      nsIClipboard* clipboard;
      nsresult rv = CallGetService(kCClipboardCID, &clipboard);
      if(NS_SUCCEEDED(rv)) {
        clipboard->EmptyClipboard(nsIClipboard::kGlobalClipboard);
        NS_RELEASE(clipboard);
      }
    }
    break;

#ifdef ACCESSIBILITY
    case WM_GETOBJECT:
    {
      *aRetValue = 0;
      // Do explicit casting to make it working on 64bit systems (see bug 649236
      // for details).
      int32_t objId = static_cast<DWORD>(lParam);
      if (objId == OBJID_CLIENT) { // oleacc.dll will be loaded dynamically
        a11y::Accessible* rootAccessible = GetAccessible(); // Held by a11y cache
        if (rootAccessible) {
          IAccessible *msaaAccessible = nullptr;
          rootAccessible->GetNativeInterface((void**)&msaaAccessible); // does an addref
          if (msaaAccessible) {
            *aRetValue = LresultFromObject(IID_IAccessible, wParam, msaaAccessible); // does an addref
            msaaAccessible->Release(); // release extra addref
            result = true;  // We handled the WM_GETOBJECT message
          }
        }
      }
    }
#endif

    case WM_SYSCOMMAND:
    {
      WPARAM filteredWParam = (wParam &0xFFF0);
      // prevent Windows from trimming the working set. bug 76831
      if (!sTrimOnMinimize && filteredWParam == SC_MINIMIZE) {
        ::ShowWindow(mWnd, SW_SHOWMINIMIZED);
        result = true;
      }
	  
      if (mSizeMode == nsSizeMode_Fullscreen && filteredWParam == SC_RESTORE) {
        MakeFullScreen(false);
        result = true;
      }

      // Handle the system menu manually when we're in full screen mode
      // so we can set the appropriate options.
      if (filteredWParam == SC_KEYMENU && lParam == VK_SPACE &&
          mSizeMode == nsSizeMode_Fullscreen) {
        DisplaySystemMenu(mWnd, mSizeMode, mIsRTL,
                          MOZ_SYSCONTEXT_X_POS,
                          MOZ_SYSCONTEXT_Y_POS);
        result = true;
      }
    }
    break;

  case WM_DWMCOMPOSITIONCHANGED:
    // First, update the compositor state to latest one. All other methods
    // should use same state as here for consistency painting.
    nsUXThemeData::CheckForCompositor(true);

    UpdateNonClientMargins();
    BroadcastMsg(mWnd, WM_DWMCOMPOSITIONCHANGED);
    NotifyThemeChanged();
    UpdateGlass();
    Invalidate(true, true, true);
    break;

  case WM_UPDATEUISTATE:
  {
    // If the UI state has changed, fire an event so the UI updates the
    // keyboard cues based on the system setting and how the window was
    // opened. For example, a dialog opened via a keyboard press on a button
    // should enable cues, whereas the same dialog opened via a mouse click of
    // the button should not.
    int32_t action = LOWORD(wParam);
    if (action == UIS_SET || action == UIS_CLEAR) {
      int32_t flags = HIWORD(wParam);
      UIStateChangeType showAccelerators = UIStateChangeType_NoChange;
      UIStateChangeType showFocusRings = UIStateChangeType_NoChange;
      if (flags & UISF_HIDEACCEL)
        showAccelerators = (action == UIS_SET) ? UIStateChangeType_Clear : UIStateChangeType_Set;
      if (flags & UISF_HIDEFOCUS)
        showFocusRings = (action == UIS_SET) ? UIStateChangeType_Clear : UIStateChangeType_Set;
      NotifyUIStateChanged(showAccelerators, showFocusRings);
    }

    break;
  }

  /* Gesture support events */
  case WM_TABLET_QUERYSYSTEMGESTURESTATUS:
    // According to MS samples, this must be handled to enable
    // rotational support in multi-touch drivers.
    result = true;
    *aRetValue = TABLET_ROTATE_GESTURE_ENABLE;
    break;

  case WM_TOUCH:
    result = OnTouch(wParam, lParam);
    if (result) {
      *aRetValue = 0;
    }
    break;

  case WM_GESTURE:
    result = OnGesture(wParam, lParam);
    break;

  case WM_GESTURENOTIFY:
    {
      if (mWindowType != eWindowType_invisible &&
          !IsPlugin()) {
        // A GestureNotify event is dispatched to decide which single-finger panning
        // direction should be active (including none) and if pan feedback should
        // be displayed. Java and plugin windows can make their own calls.
        if (gIsPointerEventsEnabled) {
          result = false;
          break;
        }

        GESTURENOTIFYSTRUCT * gestureinfo = (GESTURENOTIFYSTRUCT*)lParam;
        nsPointWin touchPoint;
        touchPoint = gestureinfo->ptsLocation;
        touchPoint.ScreenToClient(mWnd);
        WidgetGestureNotifyEvent gestureNotifyEvent(true,
                                   NS_GESTURENOTIFY_EVENT_START, this);
        gestureNotifyEvent.refPoint = LayoutDeviceIntPoint::FromUntyped(touchPoint);
        nsEventStatus status;
        DispatchEvent(&gestureNotifyEvent, status);
        mDisplayPanFeedback = gestureNotifyEvent.displayPanFeedback;
        if (!mTouchWindow)
          mGesture.SetWinGestureSupport(mWnd, gestureNotifyEvent.panDirection);
      }
      result = false; //should always bubble to DefWindowProc
    }
    break;

    case WM_CLEAR:
    {
      WidgetContentCommandEvent command(true, NS_CONTENT_COMMAND_DELETE, this);
      DispatchWindowEvent(&command);
      result = true;
    }
    break;

    case WM_CUT:
    {
      WidgetContentCommandEvent command(true, NS_CONTENT_COMMAND_CUT, this);
      DispatchWindowEvent(&command);
      result = true;
    }
    break;

    case WM_COPY:
    {
      WidgetContentCommandEvent command(true, NS_CONTENT_COMMAND_COPY, this);
      DispatchWindowEvent(&command);
      result = true;
    }
    break;

    case WM_PASTE:
    {
      WidgetContentCommandEvent command(true, NS_CONTENT_COMMAND_PASTE, this);
      DispatchWindowEvent(&command);
      result = true;
    }
    break;

    case EM_UNDO:
    {
      WidgetContentCommandEvent command(true, NS_CONTENT_COMMAND_UNDO, this);
      DispatchWindowEvent(&command);
      *aRetValue = (LRESULT)(command.mSucceeded && command.mIsEnabled);
      result = true;
    }
    break;

    case EM_REDO:
    {
      WidgetContentCommandEvent command(true, NS_CONTENT_COMMAND_REDO, this);
      DispatchWindowEvent(&command);
      *aRetValue = (LRESULT)(command.mSucceeded && command.mIsEnabled);
      result = true;
    }
    break;

    case EM_CANPASTE:
    {
      // Support EM_CANPASTE message only when wParam isn't specified or
      // is plain text format.
      if (wParam == 0 || wParam == CF_TEXT || wParam == CF_UNICODETEXT) {
        WidgetContentCommandEvent command(true, NS_CONTENT_COMMAND_PASTE,
                                          this, true);
        DispatchWindowEvent(&command);
        *aRetValue = (LRESULT)(command.mSucceeded && command.mIsEnabled);
        result = true;
      }
    }
    break;

    case EM_CANUNDO:
    {
      WidgetContentCommandEvent command(true, NS_CONTENT_COMMAND_UNDO,
                                        this, true);
      DispatchWindowEvent(&command);
      *aRetValue = (LRESULT)(command.mSucceeded && command.mIsEnabled);
      result = true;
    }
    break;

    case EM_CANREDO:
    {
      WidgetContentCommandEvent command(true, NS_CONTENT_COMMAND_REDO,
                                        this, true);
      DispatchWindowEvent(&command);
      *aRetValue = (LRESULT)(command.mSucceeded && command.mIsEnabled);
      result = true;
    }
    break;

    case WM_SETTINGCHANGE:
      if (IsWin8OrLater() && lParam &&
          !wcsicmp(L"ConvertibleSlateMode", (wchar_t*)lParam)) {
        // If we're switching into slate mode, switch to Metro for hardware
        // that supports this feature if the pref is set.
        if (GetSystemMetrics(SM_CONVERTIBLESLATEMODE) == 0 &&
            Preferences::GetBool("browser.shell.desktop-auto-switch-enabled",
                                 false)) {
          nsCOMPtr<nsIAppStartup> appStartup(do_GetService(NS_APPSTARTUP_CONTRACTID));
          if (appStartup) {
            appStartup->Quit(nsIAppStartup::eForceQuit |
                             nsIAppStartup::eRestartTouchEnvironment);
          }
        }
      }
    break;

    default:
    {
      if (msg == nsAppShell::GetTaskbarButtonCreatedMessage()) {
        SetHasTaskbarIconBeenCreated();
      }
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
    return true;
  }
}

/**************************************************************
 *
 * SECTION: Broadcast messaging
 *
 * Broadcast messages to all windows.
 *
 **************************************************************/

// Enumerate all child windows sending aMsg to each of them
BOOL CALLBACK nsWindow::BroadcastMsgToChildren(HWND aWnd, LPARAM aMsg)
{
  WNDPROC winProc = (WNDPROC)::GetWindowLongPtrW(aWnd, GWLP_WNDPROC);
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
  ::EnumChildWindows(aTopWindow, nsWindow::BroadcastMsgToChildren, aMsg);
  return TRUE;
}

/**************************************************************
 *
 * SECTION: Event processing helpers
 *
 * Special processing for certain event types and 
 * synthesized events.
 *
 **************************************************************/

int32_t
nsWindow::ClientMarginHitTestPoint(int32_t mx, int32_t my)
{
  if (mSizeMode == nsSizeMode_Minimized ||
      mSizeMode == nsSizeMode_Fullscreen) {
    return HTCLIENT;
  }

  // Calculations are done in screen coords
  RECT winRect;
  GetWindowRect(mWnd, &winRect);

  // hit return constants:
  // HTBORDER                     - non-resizable border
  // HTBOTTOM, HTLEFT, HTRIGHT, HTTOP - resizable border
  // HTBOTTOMLEFT, HTBOTTOMRIGHT  - resizable corner
  // HTTOPLEFT, HTTOPRIGHT        - resizable corner
  // HTCAPTION                    - general title bar area
  // HTCLIENT                     - area considered the client
  // HTCLOSE                      - hovering over the close button
  // HTMAXBUTTON                  - maximize button
  // HTMINBUTTON                  - minimize button

  int32_t testResult = HTCLIENT;

  bool isResizable = (mBorderStyle & (eBorderStyle_all |
                                      eBorderStyle_resizeh |
                                      eBorderStyle_default)) > 0 ? true : false;
  if (mSizeMode == nsSizeMode_Maximized)
    isResizable = false;

  // Ensure being accessible to borders of window.  Even if contents are in
  // this area, the area must behave as border.
  nsIntMargin nonClientSize(std::max(mCaptionHeight - mNonClientOffset.top,
                                     kResizableBorderMinSize),
                            std::max(mHorResizeMargin - mNonClientOffset.right,
                                     kResizableBorderMinSize),
                            std::max(mVertResizeMargin - mNonClientOffset.bottom,
                                     kResizableBorderMinSize),
                            std::max(mHorResizeMargin - mNonClientOffset.left,
                                     kResizableBorderMinSize));

  bool allowContentOverride = mSizeMode == nsSizeMode_Maximized ||
                              (mx >= winRect.left + nonClientSize.left &&
                               mx <= winRect.right - nonClientSize.right &&
                               my >= winRect.top + nonClientSize.top &&
                               my <= winRect.bottom - nonClientSize.bottom);

  // The border size.  If there is no content under mouse cursor, the border
  // size should be larger than the values in system settings.  Otherwise,
  // contents under the mouse cursor should be able to override the behavior.
  // E.g., user must expect that Firefox button always opens the popup menu
  // even when the user clicks on the above edge of it.
  nsIntMargin borderSize(std::max(nonClientSize.top,    mVertResizeMargin),
                         std::max(nonClientSize.right,  mHorResizeMargin),
                         std::max(nonClientSize.bottom, mVertResizeMargin),
                         std::max(nonClientSize.left,   mHorResizeMargin));

  bool top    = false;
  bool bottom = false;
  bool left   = false;
  bool right  = false;

  if (my >= winRect.top && my < winRect.top + borderSize.top) {
    top = true;
  } else if (my <= winRect.bottom && my > winRect.bottom - borderSize.bottom) {
    bottom = true;
  }

  // (the 2x case here doubles the resize area for corners)
  int multiplier = (top || bottom) ? 2 : 1;
  if (mx >= winRect.left &&
      mx < winRect.left + (multiplier * borderSize.left)) {
    left = true;
  } else if (mx <= winRect.right &&
             mx > winRect.right - (multiplier * borderSize.right)) {
    right = true;
  }

  if (isResizable) {
    if (top) {
      testResult = HTTOP;
      if (left)
        testResult = HTTOPLEFT;
      else if (right)
        testResult = HTTOPRIGHT;
    } else if (bottom) {
      testResult = HTBOTTOM;
      if (left)
        testResult = HTBOTTOMLEFT;
      else if (right)
        testResult = HTBOTTOMRIGHT;
    } else {
      if (left)
        testResult = HTLEFT;
      if (right)
        testResult = HTRIGHT;
    }
  } else {
    if (top)
      testResult = HTCAPTION;
    else if (bottom || left || right)
      testResult = HTBORDER;
  }

  if (!sIsInMouseCapture && allowContentOverride) {
    POINT pt = { mx, my };
    ::ScreenToClient(mWnd, &pt);
    if (pt.x == mCachedHitTestPoint.x && pt.y == mCachedHitTestPoint.y &&
        TimeStamp::Now() - mCachedHitTestTime < TimeDuration::FromMilliseconds(HITTEST_CACHE_LIFETIME_MS)) {
      testResult = mCachedHitTestResult;
    } else {
      WidgetMouseEvent event(true, NS_MOUSE_MOZHITTEST, this,
                             WidgetMouseEvent::eReal,
                             WidgetMouseEvent::eNormal);
      event.refPoint = LayoutDeviceIntPoint(pt.x, pt.y);
      event.inputSource = MOUSE_INPUT_SOURCE();
      event.mFlags.mOnlyChromeDispatch = true;
      bool result = ConvertStatus(DispatchInputEvent(&event));
      if (result) {
        // The mouse is over a blank area
        testResult = testResult == HTCLIENT ? HTCAPTION : testResult;

      } else {
        // There's content over the mouse pointer. Set HTCLIENT
        // to possibly override a resizer border.
        testResult = HTCLIENT;
      }
      mCachedHitTestPoint = pt;
      mCachedHitTestTime = TimeStamp::Now();
      mCachedHitTestResult = testResult;
    }
  }

  return testResult;
}

TimeStamp
nsWindow::GetMessageTimeStamp(LONG aEventTime)
{
  // Conversion from native event time to timestamp is complicated by the fact
  // that native event time wraps. Also, for consistency's sake we avoid calling
  // ::GetTickCount() each time and for performance reasons we only use the
  // TimeStamp::NowLoRes except when recording the first event's time.

  // We currently require the event time to be passed in. This is an interim
  // measure to avoid calling GetMessageTime twice for each event--once when
  // storing the time directly and once when converting to a timestamp.
  //
  // Once we no longer store both a time and a timestamp on each event we can
  // perform the call to GetMessageTime here instead.
  DWORD eventTime = static_cast<DWORD>(aEventTime);

  // Record first event time
  if (sFirstEventTimeStamp.IsNull()) {
    nsWindow::UpdateFirstEventTime(eventTime);
  }
  TimeStamp roughlyNow = TimeStamp::NowLoRes();

  // If the event time is before the first event time there are two
  // possibilities:
  //
  //   a) Event time has wrapped
  //   b) The initial events were not delivered strictly in time order
  //
  // I'm not sure if (b) ever happens but even if it doesn't currently occur,
  // it could come about if we change the way we fetch events.
  //
  // We can tell we have (b) if the event time is a little bit before the first
  // event (i.e. less than half the range) and the timestamp is only a little
  // bit past the first event timestamp (again less than half the range).
  // In that case we should adjust the first event time so it really is the
  // first event time.
  if (eventTime < sFirstEventTime &&
      sFirstEventTime - eventTime < kEventTimeHalfRange &&
      roughlyNow - sFirstEventTimeStamp <
        TimeDuration::FromMilliseconds(kEventTimeHalfRange)) {
    UpdateFirstEventTime(eventTime);
  }

  double timeSinceFirstEvent =
    sFirstEventTime <= eventTime
      ? eventTime - sFirstEventTime
      : static_cast<double>(kEventTimeRange) + eventTime - sFirstEventTime;
  TimeStamp eventTimeStamp =
    sFirstEventTimeStamp + TimeDuration::FromMilliseconds(timeSinceFirstEvent);

  // Event time may have wrapped several times since we recorded the first
  // event time (it wraps every ~50 days) so we extend eventTimeStamp as
  // needed.
  double timesWrapped =
    (roughlyNow - sFirstEventTimeStamp).ToMilliseconds() / kEventTimeRange;
  int32_t cyclesToAdd = static_cast<int32_t>(timesWrapped); // floor

  // There is some imprecision in the above calculation since we are using
  // TimeStamp::NowLoRes and sFirstEventTime and sFirstEventTimeStamp may not
  // be *exactly* the same moment. It is possible we think that time
  // has *just* wrapped based on comparing timestamps, but actually the
  // event time is *just about* to wrap; or vice versa.
  // In the following, we detect this situation and adjust cyclesToAdd as
  // necessary.
  double intervalFraction = fmod(timesWrapped, 1.0);

  // If our rough calculation of how many times we've wrapped based on comparing
  // timestamps says we've just wrapped (specifically, less 10% past the wrap
  // point), but the event time is just before the wrap point (again, within
  // 10%), then we need to reduce the number of wraps by 1.
  if (intervalFraction < 0.1 && timeSinceFirstEvent > kEventTimeRange * 0.9) {
    cyclesToAdd--;
  // Likewise, if our rough calculation says we've just wrapped but actually the
  // event time is just after the wrap point, we need to add an extra wrap.
  } else if (intervalFraction > 0.9 &&
             timeSinceFirstEvent < kEventTimeRange * 0.1) {
    cyclesToAdd++;
  }

  if (cyclesToAdd > 0) {
    eventTimeStamp +=
      TimeDuration::FromMilliseconds(kEventTimeRange * cyclesToAdd);
  }

  return eventTimeStamp;
}

void
nsWindow::UpdateFirstEventTime(DWORD aEventTime)
{
  sFirstEventTime = aEventTime;
  DWORD currentTime = ::GetTickCount();
  TimeStamp currentTimeStamp = TimeStamp::Now();
  double timeSinceFirstEvent =
    aEventTime <= currentTime
      ? currentTime - aEventTime
      : static_cast<double>(kEventTimeRange) + currentTime - aEventTime;
  sFirstEventTimeStamp =
    currentTimeStamp - TimeDuration::FromMilliseconds(timeSinceFirstEvent);
}

void nsWindow::PostSleepWakeNotification(const bool aIsSleepMode)
{
  if (aIsSleepMode == gIsSleepMode)
    return;

  gIsSleepMode = aIsSleepMode;

  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  if (observerService)
    observerService->NotifyObservers(nullptr,
      aIsSleepMode ? NS_WIDGET_SLEEP_OBSERVER_TOPIC :
                     NS_WIDGET_WAKE_OBSERVER_TOPIC, nullptr);
}

LRESULT nsWindow::ProcessCharMessage(const MSG &aMsg, bool *aEventDispatched)
{
  if (IMEHandler::IsComposingOn(this)) {
    IMEHandler::NotifyIME(this, REQUEST_TO_COMMIT_COMPOSITION);
  }
  // These must be checked here too as a lone WM_CHAR could be received
  // if a child window didn't handle it (for example Alt+Space in a content
  // window)
  ModifierKeyState modKeyState;
  NativeKey nativeKey(this, aMsg, modKeyState);
  return static_cast<LRESULT>(nativeKey.HandleCharMessage(aMsg,
                                                          aEventDispatched));
}

LRESULT nsWindow::ProcessKeyUpMessage(const MSG &aMsg, bool *aEventDispatched)
{
  if (IMEHandler::IsComposingOn(this)) {
    return 0;
  }

  ModifierKeyState modKeyState;
  NativeKey nativeKey(this, aMsg, modKeyState);
  return static_cast<LRESULT>(nativeKey.HandleKeyUpMessage(aEventDispatched));
}

LRESULT nsWindow::ProcessKeyDownMessage(const MSG &aMsg,
                                        bool *aEventDispatched)
{
  // If this method doesn't call NativeKey::HandleKeyDownMessage(), this method
  // must clean up the redirected message information itself.  For more
  // information, see above comment of
  // RedirectedKeyDownMessageManager::AutoFlusher class definition in
  // KeyboardLayout.h.
  RedirectedKeyDownMessageManager::AutoFlusher redirectedMsgFlusher(this, aMsg);

  ModifierKeyState modKeyState;

  LRESULT result = 0;
  if (!IMEHandler::IsComposingOn(this)) {
    NativeKey nativeKey(this, aMsg, modKeyState);
    result =
      static_cast<LRESULT>(nativeKey.HandleKeyDownMessage(aEventDispatched));
    // HandleKeyDownMessage cleaned up the redirected message information
    // itself, so, we should do nothing.
    redirectedMsgFlusher.Cancel();
  }

  if (aMsg.wParam == VK_MENU ||
      (aMsg.wParam == VK_F10 && !modKeyState.IsShift())) {
    // We need to let Windows handle this keypress,
    // by returning false, if there's a native menu
    // bar somewhere in our containing window hierarchy.
    // Otherwise we handle the keypress and don't pass
    // it on to Windows, by returning true.
    bool hasNativeMenu = false;
    HWND hWnd = mWnd;
    while (hWnd) {
      if (::GetMenu(hWnd)) {
        hasNativeMenu = true;
        break;
      }
      hWnd = ::GetParent(hWnd);
    }
    result = !hasNativeMenu;
  }

  return result;
}

nsresult
nsWindow::SynthesizeNativeKeyEvent(int32_t aNativeKeyboardLayout,
                                   int32_t aNativeKeyCode,
                                   uint32_t aModifierFlags,
                                   const nsAString& aCharacters,
                                   const nsAString& aUnmodifiedCharacters,
                                   nsIObserver* aObserver)
{
  AutoObserverNotifier notifier(aObserver, "keyevent");

  KeyboardLayout* keyboardLayout = KeyboardLayout::GetInstance();
  return keyboardLayout->SynthesizeNativeKeyEvent(
           this, aNativeKeyboardLayout, aNativeKeyCode, aModifierFlags,
           aCharacters, aUnmodifiedCharacters);
}

nsresult
nsWindow::SynthesizeNativeMouseEvent(LayoutDeviceIntPoint aPoint,
                                     uint32_t aNativeMessage,
                                     uint32_t aModifierFlags,
                                     nsIObserver* aObserver)
{
  AutoObserverNotifier notifier(aObserver, "mouseevent");

  ::SetCursorPos(aPoint.x, aPoint.y);

  INPUT input;
  memset(&input, 0, sizeof(input));

  input.type = INPUT_MOUSE;
  input.mi.dwFlags = aNativeMessage;
  ::SendInput(1, &input, sizeof(INPUT));

  return NS_OK;
}

nsresult
nsWindow::SynthesizeNativeMouseScrollEvent(LayoutDeviceIntPoint aPoint,
                                           uint32_t aNativeMessage,
                                           double aDeltaX,
                                           double aDeltaY,
                                           double aDeltaZ,
                                           uint32_t aModifierFlags,
                                           uint32_t aAdditionalFlags,
                                           nsIObserver* aObserver)
{
  AutoObserverNotifier notifier(aObserver, "mousescrollevent");
  return MouseScrollHandler::SynthesizeNativeMouseScrollEvent(
           this, aPoint, aNativeMessage,
           (aNativeMessage == WM_MOUSEWHEEL || aNativeMessage == WM_VSCROLL) ?
             static_cast<int32_t>(aDeltaY) : static_cast<int32_t>(aDeltaX),
           aModifierFlags, aAdditionalFlags);
}

/**************************************************************
 *
 * SECTION: OnXXX message handlers
 *
 * For message handlers that need to be broken out or
 * implemented in specific platform code.
 *
 **************************************************************/

void nsWindow::OnWindowPosChanged(WINDOWPOS* wp)
{
  if (wp == nullptr)
    return;

#ifdef WINSTATE_DEBUG_OUTPUT
  if (mWnd == WinUtils::GetTopLevelHWND(mWnd)) {
    PR_LOG(gWindowsLog, PR_LOG_ALWAYS, ("*** OnWindowPosChanged: [  top] "));
  } else {
    PR_LOG(gWindowsLog, PR_LOG_ALWAYS, ("*** OnWindowPosChanged: [child] "));
  }
  PR_LOG(gWindowsLog, PR_LOG_ALWAYS, ("WINDOWPOS flags:"));
  if (wp->flags & SWP_FRAMECHANGED) {
    PR_LOG(gWindowsLog, PR_LOG_ALWAYS, ("SWP_FRAMECHANGED "));
  }
  if (wp->flags & SWP_SHOWWINDOW) {
    PR_LOG(gWindowsLog, PR_LOG_ALWAYS, ("SWP_SHOWWINDOW "));
  }
  if (wp->flags & SWP_NOSIZE) {
    PR_LOG(gWindowsLog, PR_LOG_ALWAYS, ("SWP_NOSIZE "));
  }
  if (wp->flags & SWP_HIDEWINDOW) {
    PR_LOG(gWindowsLog, PR_LOG_ALWAYS, ("SWP_HIDEWINDOW "));
  }
  if (wp->flags & SWP_NOZORDER) {
    PR_LOG(gWindowsLog, PR_LOG_ALWAYS, ("SWP_NOZORDER "));
  }
  if (wp->flags & SWP_NOACTIVATE) {
    PR_LOG(gWindowsLog, PR_LOG_ALWAYS, ("SWP_NOACTIVATE "));
  }
  PR_LOG(gWindowsLog, PR_LOG_ALWAYS, ("\n"));
#endif

  // Handle window size mode changes
  if (wp->flags & SWP_FRAMECHANGED && mSizeMode != nsSizeMode_Fullscreen) {

    // Bug 566135 - Windows theme code calls show window on SW_SHOWMINIMIZED
    // windows when fullscreen games disable desktop composition. If we're
    // minimized and not being activated, ignore the event and let windows
    // handle it.
    if (mSizeMode == nsSizeMode_Minimized && (wp->flags & SWP_NOACTIVATE))
      return;

    WINDOWPLACEMENT pl;
    pl.length = sizeof(pl);
    ::GetWindowPlacement(mWnd, &pl);

    // Windows has just changed the size mode of this window. The call to
    // SizeModeChanged will trigger a call into SetSizeMode where we will
    // set the min/max window state again or for nsSizeMode_Normal, call
    // SetWindow with a parameter of SW_RESTORE. There's no need however as
    // this window's mode has already changed. Updating mSizeMode here
    // insures the SetSizeMode call is a no-op. Addresses a bug on Win7 related
    // to window docking. (bug 489258)
    if (pl.showCmd == SW_SHOWMAXIMIZED)
      mSizeMode = (mFullscreenMode ? nsSizeMode_Fullscreen : nsSizeMode_Maximized);
    else if (pl.showCmd == SW_SHOWMINIMIZED)
      mSizeMode = nsSizeMode_Minimized;
    else if (mFullscreenMode)
      mSizeMode = nsSizeMode_Fullscreen;
    else
      mSizeMode = nsSizeMode_Normal;

    // If !sTrimOnMinimize, we minimize windows using SW_SHOWMINIMIZED (See
    // SetSizeMode for internal calls, and WM_SYSCOMMAND for external). This
    // prevents the working set from being trimmed but keeps the window active.
    // After the window is minimized, we need to do some touch up work on the
    // active window. (bugs 76831 & 499816)
    if (!sTrimOnMinimize && nsSizeMode_Minimized == mSizeMode)
      ActivateOtherWindowHelper(mWnd);

#ifdef WINSTATE_DEBUG_OUTPUT
    switch (mSizeMode) {
      case nsSizeMode_Normal:
          PR_LOG(gWindowsLog, PR_LOG_ALWAYS, 
                 ("*** mSizeMode: nsSizeMode_Normal\n"));
        break;
      case nsSizeMode_Minimized:
        PR_LOG(gWindowsLog, PR_LOG_ALWAYS, 
               ("*** mSizeMode: nsSizeMode_Minimized\n"));
        break;
      case nsSizeMode_Maximized:
          PR_LOG(gWindowsLog, PR_LOG_ALWAYS, 
                 ("*** mSizeMode: nsSizeMode_Maximized\n"));
        break;
      default:
          PR_LOG(gWindowsLog, PR_LOG_ALWAYS, ("*** mSizeMode: ??????\n"));
        break;
    };
#endif

    if (mWidgetListener)
      mWidgetListener->SizeModeChanged(mSizeMode);

    // If window was restored, window activation was bypassed during the 
    // SetSizeMode call originating from OnWindowPosChanging to avoid saving
    // pre-restore attributes. Force activation now to get correct attributes.
    if (mLastSizeMode != nsSizeMode_Normal && mSizeMode == nsSizeMode_Normal)
      DispatchFocusToTopLevelWindow(true);

    // Skip window size change events below on minimization.
    if (mSizeMode == nsSizeMode_Minimized)
      return;
  }

  // Handle window position changes
  if (!(wp->flags & SWP_NOMOVE)) {
    mBounds.x = wp->x;
    mBounds.y = wp->y;

    NotifyWindowMoved(wp->x, wp->y);
  }

  // Handle window size changes
  if (!(wp->flags & SWP_NOSIZE)) {
    RECT r;
    int32_t newWidth, newHeight;

    ::GetWindowRect(mWnd, &r);

    newWidth  = r.right - r.left;
    newHeight = r.bottom - r.top;
    nsIntRect rect(wp->x, wp->y, newWidth, newHeight);

#ifdef MOZ_XUL
    if (eTransparencyTransparent == mTransparencyMode)
      ResizeTranslucentWindow(newWidth, newHeight);
#endif

    if (newWidth > mLastSize.width)
    {
      RECT drect;

      // getting wider
      drect.left   = wp->x + mLastSize.width;
      drect.top    = wp->y;
      drect.right  = drect.left + (newWidth - mLastSize.width);
      drect.bottom = drect.top + newHeight;

      ::RedrawWindow(mWnd, &drect, nullptr,
                     RDW_INVALIDATE |
                     RDW_NOERASE |
                     RDW_NOINTERNALPAINT |
                     RDW_ERASENOW |
                     RDW_ALLCHILDREN);
    }
    if (newHeight > mLastSize.height)
    {
      RECT drect;

      // getting taller
      drect.left   = wp->x;
      drect.top    = wp->y + mLastSize.height;
      drect.right  = drect.left + newWidth;
      drect.bottom = drect.top + (newHeight - mLastSize.height);

      ::RedrawWindow(mWnd, &drect, nullptr,
                     RDW_INVALIDATE |
                     RDW_NOERASE |
                     RDW_NOINTERNALPAINT |
                     RDW_ERASENOW |
                     RDW_ALLCHILDREN);
    }

    mBounds.width    = newWidth;
    mBounds.height   = newHeight;
    mLastSize.width  = newWidth;
    mLastSize.height = newHeight;

#ifdef WINSTATE_DEBUG_OUTPUT
    PR_LOG(gWindowsLog, PR_LOG_ALWAYS, 
           ("*** Resize window: %d x %d x %d x %d\n", wp->x, wp->y, 
            newWidth, newHeight));
#endif

    // If a maximized window is resized, recalculate the non-client margins.
    if (mSizeMode == nsSizeMode_Maximized) {
      if (UpdateNonClientMargins(nsSizeMode_Maximized, true)) {
        // gecko resize event already sent by UpdateNonClientMargins.
        return;
      }
    }

    // Recalculate the width and height based on the client area for gecko events.
    if (::GetClientRect(mWnd, &r)) {
      rect.width  = r.right - r.left;
      rect.height = r.bottom - r.top;
    }
    
    // Send a gecko resize event
    OnResize(rect);
  }
}

// static
void nsWindow::ActivateOtherWindowHelper(HWND aWnd)
{
  // Find the next window that is enabled, visible, and not minimized.
  HWND hwndBelow = ::GetNextWindow(aWnd, GW_HWNDNEXT);
  while (hwndBelow && (!::IsWindowEnabled(hwndBelow) || !::IsWindowVisible(hwndBelow) ||
                       ::IsIconic(hwndBelow))) {
    hwndBelow = ::GetNextWindow(hwndBelow, GW_HWNDNEXT);
  }

  // Push ourselves to the bottom of the stack, then activate the
  // next window.
  ::SetWindowPos(aWnd, HWND_BOTTOM, 0, 0, 0, 0,
                 SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
  if (hwndBelow)
    ::SetForegroundWindow(hwndBelow);

  // Play the minimize sound while we're here, since that is also
  // forgotten when we use SW_SHOWMINIMIZED.
  nsCOMPtr<nsISound> sound(do_CreateInstance("@mozilla.org/sound;1"));
  if (sound) {
    sound->PlaySystemSound(NS_LITERAL_STRING("Minimize"));
  }
}

void nsWindow::OnWindowPosChanging(LPWINDOWPOS& info)
{
  // Update non-client margins if the frame size is changing, and let the
  // browser know we are changing size modes, so alternative css can kick in.
  // If we're going into fullscreen mode, ignore this, since it'll reset
  // margins to normal mode. 
  if ((info->flags & SWP_FRAMECHANGED && !(info->flags & SWP_NOSIZE)) &&
      mSizeMode != nsSizeMode_Fullscreen) {
    WINDOWPLACEMENT pl;
    pl.length = sizeof(pl);
    ::GetWindowPlacement(mWnd, &pl);
    nsSizeMode sizeMode;
    if (pl.showCmd == SW_SHOWMAXIMIZED)
      sizeMode = (mFullscreenMode ? nsSizeMode_Fullscreen : nsSizeMode_Maximized);
    else if (pl.showCmd == SW_SHOWMINIMIZED)
      sizeMode = nsSizeMode_Minimized;
    else if (mFullscreenMode)
      sizeMode = nsSizeMode_Fullscreen;
    else
      sizeMode = nsSizeMode_Normal;

    if (mWidgetListener)
      mWidgetListener->SizeModeChanged(sizeMode);

    UpdateNonClientMargins(sizeMode, false);
  }

  // enforce local z-order rules
  if (!(info->flags & SWP_NOZORDER)) {
    HWND hwndAfter = info->hwndInsertAfter;

    nsWindow *aboveWindow = 0;
    nsWindowZ placement;

    if (hwndAfter == HWND_BOTTOM)
      placement = nsWindowZBottom;
    else if (hwndAfter == HWND_TOP || hwndAfter == HWND_TOPMOST || hwndAfter == HWND_NOTOPMOST)
      placement = nsWindowZTop;
    else {
      placement = nsWindowZRelative;
      aboveWindow = WinUtils::GetNSWindowPtr(hwndAfter);
    }

    if (mWidgetListener) {
      nsCOMPtr<nsIWidget> actualBelow = nullptr;
      if (mWidgetListener->ZLevelChanged(false, &placement,
                                         aboveWindow, getter_AddRefs(actualBelow))) {
        if (placement == nsWindowZBottom)
          info->hwndInsertAfter = HWND_BOTTOM;
        else if (placement == nsWindowZTop)
          info->hwndInsertAfter = HWND_TOP;
        else {
          info->hwndInsertAfter = (HWND)actualBelow->GetNativeData(NS_NATIVE_WINDOW);
        }
      }
    }
  }
  // prevent rude external programs from making hidden window visible
  if (mWindowType == eWindowType_invisible)
    info->flags &= ~SWP_SHOWWINDOW;
}

void nsWindow::UserActivity()
{
  // Check if we have the idle service, if not we try to get it.
  if (!mIdleService) {
    mIdleService = do_GetService("@mozilla.org/widget/idleservice;1");
  }

  // Check that we now have the idle service.
  if (mIdleService) {
    mIdleService->ResetIdleTimeOut(0);
  }
}

bool nsWindow::OnTouch(WPARAM wParam, LPARAM lParam)
{
  uint32_t cInputs = LOWORD(wParam);
  PTOUCHINPUT pInputs = new TOUCHINPUT[cInputs];

  if (mGesture.GetTouchInputInfo((HTOUCHINPUT)lParam, cInputs, pInputs)) {
    MultiTouchInput touchInput, touchEndInput;

    // Walk across the touch point array processing each contact point.
    for (uint32_t i = 0; i < cInputs; i++) {
      bool addToEvent = false, addToEndEvent = false;

      // N.B.: According with MS documentation
      // https://msdn.microsoft.com/en-us/library/windows/desktop/dd317334(v=vs.85).aspx
      // TOUCHEVENTF_DOWN cannot be combined with TOUCHEVENTF_MOVE or TOUCHEVENTF_UP.
      // Possibly, it means that TOUCHEVENTF_MOVE and TOUCHEVENTF_UP can be combined together.

      if (pInputs[i].dwFlags & (TOUCHEVENTF_DOWN | TOUCHEVENTF_MOVE)) {
        if (touchInput.mTimeStamp.IsNull()) {
          // Initialize a touch event to send.
          touchInput.mType = MultiTouchInput::MULTITOUCH_MOVE;
          touchInput.mTime = ::GetMessageTime();
          touchInput.mTimeStamp = GetMessageTimeStamp(touchInput.mTime);
          ModifierKeyState modifierKeyState;
          touchInput.modifiers = modifierKeyState.GetModifiers();
        }
        // Pres shell expects this event to be a NS_TOUCH_START
        // if any new contact points have been added since the last event sent.
        if (pInputs[i].dwFlags & TOUCHEVENTF_DOWN) {
          touchInput.mType = MultiTouchInput::MULTITOUCH_START;
        }
        addToEvent = true;
      }
      if (pInputs[i].dwFlags & TOUCHEVENTF_UP) {
        // Pres shell expects removed contacts points to be delivered in a separate
        // NS_TOUCH_END event containing only the contact points that were removed.
        if (touchEndInput.mTimeStamp.IsNull()) {
          // Initialize a touch event to send.
          touchEndInput.mType = MultiTouchInput::MULTITOUCH_END;
          touchEndInput.mTime = ::GetMessageTime();
          touchEndInput.mTimeStamp = GetMessageTimeStamp(touchEndInput.mTime);
          ModifierKeyState modifierKeyState;
          touchEndInput.modifiers = modifierKeyState.GetModifiers();
        }
        addToEndEvent = true;
      }
      if (!addToEvent && !addToEndEvent) {
        // Filter out spurious Windows events we don't understand, like palm contact.
        continue;
      }

      // Setup the touch point we'll append to the touch event array.
      nsPointWin touchPoint;
      touchPoint.x = TOUCH_COORD_TO_PIXEL(pInputs[i].x);
      touchPoint.y = TOUCH_COORD_TO_PIXEL(pInputs[i].y);
      touchPoint.ScreenToClient(mWnd);

      // Initialize the touch data.
      SingleTouchData touchData(pInputs[i].dwID,                                      // aIdentifier
                                ScreenIntPoint::FromUntyped(touchPoint),              // aScreenPoint
                                /* radius, if known */
                                pInputs[i].dwFlags & TOUCHINPUTMASKF_CONTACTAREA
                                  ? ScreenSize(
                                      TOUCH_COORD_TO_PIXEL(pInputs[i].cxContact) / 2,
                                      TOUCH_COORD_TO_PIXEL(pInputs[i].cyContact) / 2)
                                  : ScreenSize(1, 1),                                 // aRadius
                                0.0f,                                                 // aRotationAngle
                                0.0f);                                                // aForce

      // Append touch data to the appropriate event.
      if (addToEvent) {
        touchInput.mTouches.AppendElement(touchData);
      }
      if (addToEndEvent) {
        touchEndInput.mTouches.AppendElement(touchData);
      }
    }

    // Dispatch touch start and touch move event if we have one.
    if (!touchInput.mTimeStamp.IsNull()) {
      // Convert MultiTouchInput to WidgetTouchEvent interface.
      WidgetTouchEvent widgetTouchEvent = touchInput.ToWidgetTouchEvent(this);
      DispatchAPZAwareEvent(&widgetTouchEvent);
    }
    // Dispatch touch end event if we have one.
    if (!touchEndInput.mTimeStamp.IsNull()) {
      // Convert MultiTouchInput to WidgetTouchEvent interface.
      WidgetTouchEvent widgetTouchEvent = touchEndInput.ToWidgetTouchEvent(this);
      DispatchAPZAwareEvent(&widgetTouchEvent);
    }
  }

  delete [] pInputs;
  mGesture.CloseTouchInputHandle((HTOUCHINPUT)lParam);
  return true;
}

static int32_t RoundDown(double aDouble)
{
  return aDouble > 0 ? static_cast<int32_t>(floor(aDouble)) :
                       static_cast<int32_t>(ceil(aDouble));
}

// Gesture event processing. Handles WM_GESTURE events.
bool nsWindow::OnGesture(WPARAM wParam, LPARAM lParam)
{
  if (gIsPointerEventsEnabled) {
    return false;
  }

  // Treatment for pan events which translate into scroll events:
  if (mGesture.IsPanEvent(lParam)) {
    if ( !mGesture.ProcessPanMessage(mWnd, wParam, lParam) )
      return false; // ignore

    nsEventStatus status;

    WidgetWheelEvent wheelEvent(true, NS_WHEEL_WHEEL, this);

    ModifierKeyState modifierKeyState;
    modifierKeyState.InitInputEvent(wheelEvent);

    wheelEvent.button      = 0;
    wheelEvent.time        = ::GetMessageTime();
    wheelEvent.timeStamp   = GetMessageTimeStamp(wheelEvent.time);
    wheelEvent.inputSource = nsIDOMMouseEvent::MOZ_SOURCE_TOUCH;

    bool endFeedback = true;

    if (mGesture.PanDeltaToPixelScroll(wheelEvent)) {
      DispatchEvent(&wheelEvent, status);
    }

    if (mDisplayPanFeedback) {
      mGesture.UpdatePanFeedbackX(mWnd,
                                  DeprecatedAbs(RoundDown(wheelEvent.overflowDeltaX)),
                                  endFeedback);
      mGesture.UpdatePanFeedbackY(mWnd,
                                  DeprecatedAbs(RoundDown(wheelEvent.overflowDeltaY)),
                                  endFeedback);
      mGesture.PanFeedbackFinalize(mWnd, endFeedback);
    }

    mGesture.CloseGestureInfoHandle((HGESTUREINFO)lParam);

    return true;
  }

  // Other gestures translate into simple gesture events:
  WidgetSimpleGestureEvent event(true, 0, this);
  if ( !mGesture.ProcessGestureMessage(mWnd, wParam, lParam, event) ) {
    return false; // fall through to DefWndProc
  }
  
  // Polish up and send off the new event
  ModifierKeyState modifierKeyState;
  modifierKeyState.InitInputEvent(event);
  event.button    = 0;
  event.time      = ::GetMessageTime();
  event.timeStamp = GetMessageTimeStamp(event.time);
  event.inputSource = nsIDOMMouseEvent::MOZ_SOURCE_TOUCH;

  nsEventStatus status;
  DispatchEvent(&event, status);
  if (status == nsEventStatus_eIgnore) {
    return false; // Ignored, fall through
  }

  // Only close this if we process and return true.
  mGesture.CloseGestureInfoHandle((HGESTUREINFO)lParam);

  return true; // Handled
}

nsresult
nsWindow::ConfigureChildren(const nsTArray<Configuration>& aConfigurations)
{
  // If this is a remotely updated widget we receive clipping, position, and
  // size information from a source other than our owner. Don't let our parent
  // update this information.
  if (mWindowType == eWindowType_plugin_ipc_chrome) {
    return NS_OK;
  }

  // XXXroc we could use BeginDeferWindowPos/DeferWindowPos/EndDeferWindowPos
  // here, if that helps in some situations. So far I haven't seen a
  // need.
  for (uint32_t i = 0; i < aConfigurations.Length(); ++i) {
    const Configuration& configuration = aConfigurations[i];
    nsWindow* w = static_cast<nsWindow*>(configuration.mChild);
    NS_ASSERTION(w->GetParent() == this,
                 "Configured widget is not a child");
    nsresult rv = w->SetWindowClipRegion(configuration.mClipRegion, true);
    NS_ENSURE_SUCCESS(rv, rv);
    nsIntRect bounds;
    w->GetBounds(bounds);
    if (bounds.Size() != configuration.mBounds.Size()) {
      w->Resize(configuration.mBounds.x, configuration.mBounds.y,
                configuration.mBounds.width, configuration.mBounds.height,
                true);
    } else if (bounds.TopLeft() != configuration.mBounds.TopLeft()) {
      w->Move(configuration.mBounds.x, configuration.mBounds.y);


      if (gfxWindowsPlatform::GetPlatform()->GetRenderMode() ==
          gfxWindowsPlatform::RENDER_DIRECT2D ||
          GetLayerManager()->GetBackendType() != LayersBackend::LAYERS_BASIC) {
        // XXX - Workaround for Bug 587508. This will invalidate the part of the
        // plugin window that might be touched by moving content somehow. The
        // underlying problem should be found and fixed!
        nsIntRegion r;
        r.Sub(bounds, configuration.mBounds);
        r.MoveBy(-bounds.x,
                 -bounds.y);
        nsIntRect toInvalidate = r.GetBounds();

        WinUtils::InvalidatePluginAsWorkaround(w, toInvalidate);
      }
    }
    rv = w->SetWindowClipRegion(configuration.mClipRegion, false);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

static HRGN
CreateHRGNFromArray(const nsTArray<nsIntRect>& aRects)
{
  int32_t size = sizeof(RGNDATAHEADER) + sizeof(RECT)*aRects.Length();
  nsAutoTArray<uint8_t,100> buf;
  buf.SetLength(size);
  RGNDATA* data = reinterpret_cast<RGNDATA*>(buf.Elements());
  RECT* rects = reinterpret_cast<RECT*>(data->Buffer);
  data->rdh.dwSize = sizeof(data->rdh);
  data->rdh.iType = RDH_RECTANGLES;
  data->rdh.nCount = aRects.Length();
  nsIntRect bounds;
  for (uint32_t i = 0; i < aRects.Length(); ++i) {
    const nsIntRect& r = aRects[i];
    bounds.UnionRect(bounds, r);
    ::SetRect(&rects[i], r.x, r.y, r.XMost(), r.YMost());
  }
  ::SetRect(&data->rdh.rcBound, bounds.x, bounds.y, bounds.XMost(), bounds.YMost());
  return ::ExtCreateRegion(nullptr, buf.Length(), data);
}

nsresult
nsWindow::SetWindowClipRegion(const nsTArray<nsIntRect>& aRects,
                              bool aIntersectWithExisting)
{
  if (IsWindowClipRegionEqual(aRects)) {
    return NS_OK;
  }

  nsBaseWidget::SetWindowClipRegion(aRects, aIntersectWithExisting);

  HRGN dest = CreateHRGNFromArray(aRects);
  if (!dest)
    return NS_ERROR_OUT_OF_MEMORY;

  if (aIntersectWithExisting) {
    HRGN current = ::CreateRectRgn(0, 0, 0, 0);
    if (current) {
      if (::GetWindowRgn(mWnd, current) != 0 /*ERROR*/) {
        ::CombineRgn(dest, dest, current, RGN_AND);
      }
      ::DeleteObject(current);
    }
  }

  // If a plugin is not visible, especially if it is in a background tab,
  // it should not be able to steal keyboard focus.  This code checks whether
  // the region that the plugin is being clipped to is NULLREGION.  If it is,
  // the plugin window gets disabled.
  if (IsPlugin()) {
    if (NULLREGION == ::CombineRgn(dest, dest, dest, RGN_OR)) {
      ::ShowWindow(mWnd, SW_HIDE);
      ::EnableWindow(mWnd, FALSE);
    } else {
      ::EnableWindow(mWnd, TRUE);
      ::ShowWindow(mWnd, SW_SHOW);
    }
  }
  if (!::SetWindowRgn(mWnd, dest, TRUE)) {
    ::DeleteObject(dest);
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

// WM_DESTROY event handler
void nsWindow::OnDestroy()
{
  mOnDestroyCalled = true;

  // Make sure we don't get destroyed in the process of tearing down.
  nsCOMPtr<nsIWidget> kungFuDeathGrip(this);
  
  // Dispatch the destroy notification.
  if (!mInDtor)
    NotifyWindowDestroyed();

  // Prevent the widget from sending additional events.
  mWidgetListener = nullptr;
  mAttachedWidgetListener = nullptr;

  // Unregister notifications from terminal services
  ::WTSUnRegisterSessionNotification(mWnd);

  // Free our subclass and clear |this| stored in the window props. We will no longer
  // receive events from Windows after this point.
  SubclassWindow(FALSE);

  // Once mWidgetListener is cleared and the subclass is reset, sCurrentWindow can be
  // cleared. (It's used in tracking windows for mouse events.)
  if (sCurrentWindow == this)
    sCurrentWindow = nullptr;

  // Disconnects us from our parent, will call our GetParent().
  nsBaseWidget::Destroy();

  // Release references to children, device context, toolkit, and app shell.
  nsBaseWidget::OnDestroy();
  
  // Clear our native parent handle.
  // XXX Windows will take care of this in the proper order, and SetParent(nullptr)'s
  // remove child on the parent already took place in nsBaseWidget's Destroy call above.
  //SetParent(nullptr);
  mParent = nullptr;

  // We have to destroy the native drag target before we null out our window pointer.
  EnableDragDrop(false);

  // If we're going away and for some reason we're still the rollup widget, rollup and
  // turn off capture.
  nsIRollupListener* rollupListener = nsBaseWidget::GetActiveRollupListener();
  nsCOMPtr<nsIWidget> rollupWidget;
  if (rollupListener) {
    rollupWidget = rollupListener->GetRollupWidget();
  }
  if (this == rollupWidget) {
    if ( rollupListener )
      rollupListener->Rollup(0, false, nullptr, nullptr);
    CaptureRollupEvents(nullptr, false);
  }

  IMEHandler::OnDestroyWindow(this);

  // Turn off mouse trails if enabled.
  MouseTrailer* mtrailer = nsToolkit::gMouseTrailer;
  if (mtrailer) {
    if (mtrailer->GetMouseTrailerWindow() == mWnd)
      mtrailer->DestroyTimer();

    if (mtrailer->GetCaptureWindow() == mWnd)
      mtrailer->SetCaptureWindow(nullptr);
  }

  // Free GDI window class objects
  if (mBrush) {
    VERIFY(::DeleteObject(mBrush));
    mBrush = nullptr;
  }


  // Destroy any custom cursor resources.
  if (mCursor == -1)
    SetCursor(eCursor_standard);

#ifdef MOZ_XUL
  // Reset transparency
  if (eTransparencyTransparent == mTransparencyMode)
    SetupTranslucentWindowMemoryBitmap(eTransparencyOpaque);
#endif

  // Finalize panning feedback to possibly restore window displacement
  mGesture.PanFeedbackFinalize(mWnd, true);

  // Clear the main HWND.
  mWnd = nullptr;
}

// Send a resize message to the listener
bool nsWindow::OnResize(nsIntRect &aWindowRect)
{
  bool result = mWidgetListener ?
                mWidgetListener->WindowResized(this, aWindowRect.width, aWindowRect.height) : false;

  // If there is an attached view, inform it as well as the normal widget listener.
  if (mAttachedWidgetListener) {
    return mAttachedWidgetListener->WindowResized(this, aWindowRect.width, aWindowRect.height);
  }

  return result;
}

bool nsWindow::OnHotKey(WPARAM wParam, LPARAM lParam)
{
  return true;
}

// Can be overriden. Controls auto-erase of background.
bool nsWindow::AutoErase(HDC dc)
{
  return false;
}

void
nsWindow::ClearCompositor(nsWindow* aWindow)
{
  aWindow->DestroyLayerManager();
}

bool
nsWindow::IsPopup()
{
  return mWindowType == eWindowType_popup;
}

bool
nsWindow::ShouldUseOffMainThreadCompositing()
{
  // We don't currently support using an accelerated layer manager with
  // transparent windows so don't even try. I'm also not sure if we even
  // want to support this case. See bug 593471
  if (mTransparencyMode == eTransparencyTransparent) {
    return false;
  }

  return nsBaseWidget::ShouldUseOffMainThreadCompositing();
}

void
nsWindow::GetPreferredCompositorBackends(nsTArray<LayersBackend>& aHints)
{
  LayerManagerPrefs prefs;
  GetLayerManagerPrefs(&prefs);

  // We don't currently support using an accelerated layer manager with
  // transparent windows so don't even try. I'm also not sure if we even
  // want to support this case. See bug 593471
  if (!(prefs.mDisableAcceleration ||
        mTransparencyMode == eTransparencyTransparent ||
        (IsPopup() && gfxWindowsPlatform::GetPlatform()->IsWARP()))) {
    // See bug 1150376, D3D11 composition can cause issues on some devices
    // on windows 7 where presentation fails randomly for windows with drop
    // shadows.
    if (prefs.mPreferOpenGL) {
      aHints.AppendElement(LayersBackend::LAYERS_OPENGL);
    }

    if (!prefs.mPreferD3D9) {
      aHints.AppendElement(LayersBackend::LAYERS_D3D11);
    }
    if (prefs.mPreferD3D9 || !mozilla::IsVistaOrLater()) {
      // We don't want D3D9 except on Windows XP
      aHints.AppendElement(LayersBackend::LAYERS_D3D9);
    }
  }
  aHints.AppendElement(LayersBackend::LAYERS_BASIC);
}

void
nsWindow::WindowUsesOMTC()
{
  ULONG_PTR style = ::GetClassLongPtr(mWnd, GCL_STYLE);
  if (!style) {
    NS_WARNING("Could not get window class style");
    return;
  }
  style |= CS_HREDRAW | CS_VREDRAW;
  DebugOnly<ULONG_PTR> result = ::SetClassLongPtr(mWnd, GCL_STYLE, style);
  NS_WARN_IF_FALSE(result, "Could not reset window class style");
}

bool
nsWindow::HasBogusPopupsDropShadowOnMultiMonitor() {
  if (sHasBogusPopupsDropShadowOnMultiMonitor == TRI_UNKNOWN) {
    // Since any change in the preferences requires a restart, this can be
    // done just once.
    // Check for Direct2D first.
    sHasBogusPopupsDropShadowOnMultiMonitor =
      gfxWindowsPlatform::GetPlatform()->GetRenderMode() ==
        gfxWindowsPlatform::RENDER_DIRECT2D ? TRI_TRUE : TRI_FALSE;
    if (!sHasBogusPopupsDropShadowOnMultiMonitor) {
      // Otherwise check if Direct3D 9 may be used.
      LayerManagerPrefs prefs;
      GetLayerManagerPrefs(&prefs);
      if (!prefs.mDisableAcceleration && !prefs.mPreferOpenGL) {
        nsCOMPtr<nsIGfxInfo> gfxInfo = do_GetService("@mozilla.org/gfx/info;1");
        if (gfxInfo) {
          int32_t status;
          if (NS_SUCCEEDED(gfxInfo->GetFeatureStatus(nsIGfxInfo::FEATURE_DIRECT3D_9_LAYERS, &status))) {
            if (status == nsIGfxInfo::FEATURE_STATUS_OK || prefs.mForceAcceleration)
            {
              sHasBogusPopupsDropShadowOnMultiMonitor = TRI_TRUE;
            }
          }
        }
      }
    }
  }
  return !!sHasBogusPopupsDropShadowOnMultiMonitor;
}

void
nsWindow::OnSysColorChanged()
{
  if (mWindowType == eWindowType_invisible) {
    ::EnumThreadWindows(GetCurrentThreadId(), nsWindow::BroadcastMsg, WM_SYSCOLORCHANGE);
  }
  else {
    // Note: This is sent for child windows as well as top-level windows.
    // The Win32 toolkit normally only sends these events to top-level windows.
    // But we cycle through all of the childwindows and send it to them as well
    // so all presentations get notified properly.
    // See nsWindow::GlobalMsgWindowProc.
    NotifySysColorChanged();
  }
}

/**************************************************************
 **************************************************************
 **
 ** BLOCK: IME management and accessibility
 **
 ** Handles managing IME input and accessibility.
 **
 **************************************************************
 **************************************************************/

nsresult
nsWindow::NotifyIMEInternal(const IMENotification& aIMENotification)
{
  return IMEHandler::NotifyIME(this, aIMENotification);
}

NS_IMETHODIMP_(void)
nsWindow::SetInputContext(const InputContext& aContext,
                          const InputContextAction& aAction)
{
  InputContext newInputContext = aContext;
  IMEHandler::SetInputContext(this, newInputContext, aAction);
  mInputContext = newInputContext;
}

NS_IMETHODIMP_(InputContext)
nsWindow::GetInputContext()
{
  mInputContext.mIMEState.mOpen = IMEState::CLOSED;
  if (WinUtils::IsIMEEnabled(mInputContext) && IMEHandler::GetOpenState(this)) {
    mInputContext.mIMEState.mOpen = IMEState::OPEN;
  } else {
    mInputContext.mIMEState.mOpen = IMEState::CLOSED;
  }
  return mInputContext;
}

NS_IMETHODIMP
nsWindow::GetToggledKeyState(uint32_t aKeyCode, bool* aLEDState)
{
#ifdef DEBUG_KBSTATE
  PR_LOG(gWindowsLog, PR_LOG_ALWAYS, ("GetToggledKeyState\n"));
#endif 
  NS_ENSURE_ARG_POINTER(aLEDState);
  *aLEDState = (::GetKeyState(aKeyCode) & 1) != 0;
  return NS_OK;
}

nsIMEUpdatePreference
nsWindow::GetIMEUpdatePreference()
{
  return IMEHandler::GetUpdatePreference();
}

#ifdef ACCESSIBILITY
#ifdef DEBUG
#define NS_LOG_WMGETOBJECT(aWnd, aHwnd, aAcc)                                  \
  if (a11y::logging::IsEnabled(a11y::logging::ePlatforms)) {                   \
    printf("Get the window:\n  {\n     HWND: %p, parent HWND: %p, wndobj: %p,\n",\
           aHwnd, ::GetParent(aHwnd), aWnd);                                   \
    printf("     acc: %p", aAcc);                                              \
    if (aAcc) {                                                                \
      nsAutoString name;                                                       \
      aAcc->Name(name);                                                        \
      printf(", accname: %s", NS_ConvertUTF16toUTF8(name).get());              \
    }                                                                          \
    printf("\n }\n");                                                          \
  }

#else
#define NS_LOG_WMGETOBJECT(aWnd, aHwnd, aAcc)
#endif

a11y::Accessible*
nsWindow::GetAccessible()
{
  // If the pref was ePlatformIsDisabled, return null here, disabling a11y.
  if (a11y::PlatformDisabledState() == a11y::ePlatformIsDisabled)
    return nullptr;

  if (mInDtor || mOnDestroyCalled || mWindowType == eWindowType_invisible) {
    return nullptr;
  }

  // In case of popup window return a popup accessible.
  nsView* view = nsView::GetViewFor(this);
  if (view) {
    nsIFrame* frame = view->GetFrame();
    if (frame && nsLayoutUtils::IsPopup(frame)) {
      nsCOMPtr<nsIAccessibilityService> accService =
        services::GetAccessibilityService();
      if (accService) {
        a11y::DocAccessible* docAcc =
          GetAccService()->GetDocAccessible(frame->PresContext()->PresShell());
        if (docAcc) {
          NS_LOG_WMGETOBJECT(this, mWnd,
                             docAcc->GetAccessibleOrDescendant(frame->GetContent()));
          return docAcc->GetAccessibleOrDescendant(frame->GetContent());
        }
      }
    }
  }

  // otherwise root document accessible.
  NS_LOG_WMGETOBJECT(this, mWnd, GetRootAccessible());
  return GetRootAccessible();
}
#endif

/**************************************************************
 **************************************************************
 **
 ** BLOCK: Transparency
 **
 ** Window transparency helpers.
 **
 **************************************************************
 **************************************************************/

#ifdef MOZ_XUL

void nsWindow::ResizeTranslucentWindow(int32_t aNewWidth, int32_t aNewHeight, bool force)
{
  if (!force && aNewWidth == mBounds.width && aNewHeight == mBounds.height)
    return;

  nsRefPtr<gfxWindowsSurface> newSurface =
    new gfxWindowsSurface(gfxIntSize(aNewWidth, aNewHeight), gfxImageFormat::ARGB32);
  mTransparentSurface = newSurface;
  mMemoryDC = newSurface->GetDC();
}

void nsWindow::SetWindowTranslucencyInner(nsTransparencyMode aMode)
{
  if (aMode == mTransparencyMode)
    return;

  // stop on dialogs and popups!
  HWND hWnd = WinUtils::GetTopLevelHWND(mWnd, true);
  nsWindow* parent = WinUtils::GetNSWindowPtr(hWnd);

  if (!parent)
  {
    NS_WARNING("Trying to use transparent chrome in an embedded context");
    return;
  }

  if (parent != this) {
    NS_WARNING("Setting SetWindowTranslucencyInner on a parent this is not us!");
  }

  if (aMode == eTransparencyTransparent) {
    // If we're switching to the use of a transparent window, hide the chrome
    // on our parent.
    HideWindowChrome(true);
  } else if (mHideChrome && mTransparencyMode == eTransparencyTransparent) {
    // if we're switching out of transparent, re-enable our parent's chrome.
    HideWindowChrome(false);
  }

  LONG_PTR style = ::GetWindowLongPtrW(hWnd, GWL_STYLE),
    exStyle = ::GetWindowLongPtr(hWnd, GWL_EXSTYLE);
 
   if (parent->mIsVisible)
     style |= WS_VISIBLE;
   if (parent->mSizeMode == nsSizeMode_Maximized)
     style |= WS_MAXIMIZE;
   else if (parent->mSizeMode == nsSizeMode_Minimized)
     style |= WS_MINIMIZE;

   if (aMode == eTransparencyTransparent)
     exStyle |= WS_EX_LAYERED;
   else
     exStyle &= ~WS_EX_LAYERED;

  VERIFY_WINDOW_STYLE(style);
  ::SetWindowLongPtrW(hWnd, GWL_STYLE, style);
  ::SetWindowLongPtrW(hWnd, GWL_EXSTYLE, exStyle);

  if (HasGlass())
    memset(&mGlassMargins, 0, sizeof mGlassMargins);
  mTransparencyMode = aMode;

  SetupTranslucentWindowMemoryBitmap(aMode);
  UpdateGlass();
}

void nsWindow::SetupTranslucentWindowMemoryBitmap(nsTransparencyMode aMode)
{
  if (eTransparencyTransparent == aMode) {
    ResizeTranslucentWindow(mBounds.width, mBounds.height, true);
  } else {
    mTransparentSurface = nullptr;
    mMemoryDC = nullptr;
  }
}

void nsWindow::ClearTranslucentWindow()
{
  if (mTransparentSurface) {
    IntSize size = mTransparentSurface->GetSize();
    RefPtr<DrawTarget> drawTarget = gfxPlatform::GetPlatform()->
      CreateDrawTargetForSurface(mTransparentSurface, size);
    drawTarget->ClearRect(Rect(0, 0, size.width, size.height));
    UpdateTranslucentWindow();
 }
}

nsresult nsWindow::UpdateTranslucentWindow()
{
  if (mBounds.IsEmpty())
    return NS_OK;

  ::GdiFlush();

  BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
  SIZE winSize = { mBounds.width, mBounds.height };
  POINT srcPos = { 0, 0 };
  HWND hWnd = WinUtils::GetTopLevelHWND(mWnd, true);
  RECT winRect;
  ::GetWindowRect(hWnd, &winRect);

  // perform the alpha blend
  bool updateSuccesful = 
    ::UpdateLayeredWindow(hWnd, nullptr, (POINT*)&winRect, &winSize, mMemoryDC,
                          &srcPos, 0, &bf, ULW_ALPHA);

  if (!updateSuccesful) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

#endif //MOZ_XUL

/**************************************************************
 **************************************************************
 **
 ** BLOCK: Popup rollup hooks
 **
 ** Deals with CaptureRollup on popup windows.
 **
 **************************************************************
 **************************************************************/

// Schedules a timer for a window, so we can rollup after processing the hook event
void nsWindow::ScheduleHookTimer(HWND aWnd, UINT aMsgId)
{
  // In some cases multiple hooks may be scheduled
  // so ignore any other requests once one timer is scheduled
  if (sHookTimerId == 0) {
    // Remember the window handle and the message ID to be used later
    sRollupMsgId = aMsgId;
    sRollupMsgWnd = aWnd;
    // Schedule native timer for doing the rollup after
    // this event is done being processed
    sHookTimerId = ::SetTimer(nullptr, 0, 0, (TIMERPROC)HookTimerForPopups);
    NS_ASSERTION(sHookTimerId, "Timer couldn't be created.");
  }
}

#ifdef POPUP_ROLLUP_DEBUG_OUTPUT
int gLastMsgCode = 0;
extern MSGFEventMsgInfo gMSGFEvents[];
#endif

// Process Menu messages, rollup when popup is clicked.
LRESULT CALLBACK nsWindow::MozSpecialMsgFilter(int code, WPARAM wParam, LPARAM lParam)
{
#ifdef POPUP_ROLLUP_DEBUG_OUTPUT
  if (sProcessHook) {
    MSG* pMsg = (MSG*)lParam;

    int inx = 0;
    while (gMSGFEvents[inx].mId != code && gMSGFEvents[inx].mStr != nullptr) {
      inx++;
    }
    if (code != gLastMsgCode) {
      if (gMSGFEvents[inx].mId == code) {
#ifdef DEBUG
        PR_LOG(gWindowsLog, PR_LOG_ALWAYS, 
               ("MozSpecialMessageProc - code: 0x%X  - %s  hw: %p\n", 
                code, gMSGFEvents[inx].mStr, pMsg->hwnd));
#endif
      } else {
#ifdef DEBUG
        PR_LOG(gWindowsLog, PR_LOG_ALWAYS, 
               ("MozSpecialMessageProc - code: 0x%X  - %d  hw: %p\n", 
                code, gMSGFEvents[inx].mId, pMsg->hwnd));
#endif
      }
      gLastMsgCode = code;
    }
    PrintEvent(pMsg->message, FALSE, FALSE);
  }
#endif // #ifdef POPUP_ROLLUP_DEBUG_OUTPUT

  if (sProcessHook && code == MSGF_MENU) {
    MSG* pMsg = (MSG*)lParam;
    ScheduleHookTimer( pMsg->hwnd, pMsg->message);
  }

  return ::CallNextHookEx(sMsgFilterHook, code, wParam, lParam);
}

// Process all mouse messages. Roll up when a click is in a native window
// that doesn't have an nsIWidget.
LRESULT CALLBACK nsWindow::MozSpecialMouseProc(int code, WPARAM wParam, LPARAM lParam)
{
  if (sProcessHook) {
    switch (WinUtils::GetNativeMessage(wParam)) {
      case WM_LBUTTONDOWN:
      case WM_RBUTTONDOWN:
      case WM_MBUTTONDOWN:
      case WM_MOUSEWHEEL:
      case WM_MOUSEHWHEEL:
      {
        MOUSEHOOKSTRUCT* ms = (MOUSEHOOKSTRUCT*)lParam;
        nsIWidget* mozWin = WinUtils::GetNSWindowPtr(ms->hwnd);
        if (mozWin) {
          // If this window is windowed plugin window, the mouse events are not
          // sent to us.
          if (static_cast<nsWindow*>(mozWin)->IsPlugin())
            ScheduleHookTimer(ms->hwnd, (UINT)wParam);
        } else {
          ScheduleHookTimer(ms->hwnd, (UINT)wParam);
        }
        break;
      }
    }
  }
  return ::CallNextHookEx(sCallMouseHook, code, wParam, lParam);
}

// Process all messages. Roll up when the window is moving, or
// is resizing or when maximized or mininized.
LRESULT CALLBACK nsWindow::MozSpecialWndProc(int code, WPARAM wParam, LPARAM lParam)
{
#ifdef POPUP_ROLLUP_DEBUG_OUTPUT
  if (sProcessHook) {
    CWPSTRUCT* cwpt = (CWPSTRUCT*)lParam;
    PrintEvent(cwpt->message, FALSE, FALSE);
  }
#endif

  if (sProcessHook) {
    CWPSTRUCT* cwpt = (CWPSTRUCT*)lParam;
    if (cwpt->message == WM_MOVING ||
        cwpt->message == WM_SIZING ||
        cwpt->message == WM_GETMINMAXINFO) {
      ScheduleHookTimer(cwpt->hwnd, (UINT)cwpt->message);
    }
  }

  return ::CallNextHookEx(sCallProcHook, code, wParam, lParam);
}

// Register the special "hooks" for dropdown processing.
void nsWindow::RegisterSpecialDropdownHooks()
{
  NS_ASSERTION(!sMsgFilterHook, "sMsgFilterHook must be NULL!");
  NS_ASSERTION(!sCallProcHook,  "sCallProcHook must be NULL!");

  DISPLAY_NMM_PRT("***************** Installing Msg Hooks ***************\n");

  // Install msg hook for moving the window and resizing
  if (!sMsgFilterHook) {
    DISPLAY_NMM_PRT("***** Hooking sMsgFilterHook!\n");
    sMsgFilterHook = SetWindowsHookEx(WH_MSGFILTER, MozSpecialMsgFilter,
                                      nullptr, GetCurrentThreadId());
#ifdef POPUP_ROLLUP_DEBUG_OUTPUT
    if (!sMsgFilterHook) {
      PR_LOG(gWindowsLog, PR_LOG_ALWAYS, 
             ("***** SetWindowsHookEx is NOT installed for WH_MSGFILTER!\n"));
    }
#endif
  }

  // Install msg hook for menus
  if (!sCallProcHook) {
    DISPLAY_NMM_PRT("***** Hooking sCallProcHook!\n");
    sCallProcHook  = SetWindowsHookEx(WH_CALLWNDPROC, MozSpecialWndProc,
                                      nullptr, GetCurrentThreadId());
#ifdef POPUP_ROLLUP_DEBUG_OUTPUT
    if (!sCallProcHook) {
      PR_LOG(gWindowsLog, PR_LOG_ALWAYS, 
             ("***** SetWindowsHookEx is NOT installed for WH_CALLWNDPROC!\n"));
    }
#endif
  }

  // Install msg hook for the mouse
  if (!sCallMouseHook) {
    DISPLAY_NMM_PRT("***** Hooking sCallMouseHook!\n");
    sCallMouseHook  = SetWindowsHookEx(WH_MOUSE, MozSpecialMouseProc,
                                       nullptr, GetCurrentThreadId());
#ifdef POPUP_ROLLUP_DEBUG_OUTPUT
    if (!sCallMouseHook) {
      PR_LOG(gWindowsLog, PR_LOG_ALWAYS, 
             ("***** SetWindowsHookEx is NOT installed for WH_MOUSE!\n"));
    }
#endif
  }
}

// Unhook special message hooks for dropdowns.
void nsWindow::UnregisterSpecialDropdownHooks()
{
  DISPLAY_NMM_PRT("***************** De-installing Msg Hooks ***************\n");

  if (sCallProcHook) {
    DISPLAY_NMM_PRT("***** Unhooking sCallProcHook!\n");
    if (!::UnhookWindowsHookEx(sCallProcHook)) {
      DISPLAY_NMM_PRT("***** UnhookWindowsHookEx failed for sCallProcHook!\n");
    }
    sCallProcHook = nullptr;
  }

  if (sMsgFilterHook) {
    DISPLAY_NMM_PRT("***** Unhooking sMsgFilterHook!\n");
    if (!::UnhookWindowsHookEx(sMsgFilterHook)) {
      DISPLAY_NMM_PRT("***** UnhookWindowsHookEx failed for sMsgFilterHook!\n");
    }
    sMsgFilterHook = nullptr;
  }

  if (sCallMouseHook) {
    DISPLAY_NMM_PRT("***** Unhooking sCallMouseHook!\n");
    if (!::UnhookWindowsHookEx(sCallMouseHook)) {
      DISPLAY_NMM_PRT("***** UnhookWindowsHookEx failed for sCallMouseHook!\n");
    }
    sCallMouseHook = nullptr;
  }
}

// This timer is designed to only fire one time at most each time a "hook" function
// is used to rollup the dropdown. In some cases, the timer may be scheduled from the
// hook, but that hook event or a subsequent event may roll up the dropdown before
// this timer function is executed.
//
// For example, if an MFC control takes focus, the combobox will lose focus and rollup
// before this function fires.
VOID CALLBACK nsWindow::HookTimerForPopups(HWND hwnd, UINT uMsg, UINT idEvent, DWORD dwTime)
{
  if (sHookTimerId != 0) {
    // if the window is nullptr then we need to use the ID to kill the timer
    BOOL status = ::KillTimer(nullptr, sHookTimerId);
    NS_ASSERTION(status, "Hook Timer was not killed.");
    sHookTimerId = 0;
  }

  if (sRollupMsgId != 0) {
    // Note: DealWithPopups does the check to make sure that the rollup widget is set.
    LRESULT popupHandlingResult;
    nsAutoRollup autoRollup;
    DealWithPopups(sRollupMsgWnd, sRollupMsgId, 0, 0, &popupHandlingResult);
    sRollupMsgId = 0;
    sRollupMsgWnd = nullptr;
  }
}

BOOL CALLBACK nsWindow::ClearResourcesCallback(HWND aWnd, LPARAM aMsg)
{
    nsWindow *window = WinUtils::GetNSWindowPtr(aWnd);
    if (window) {
        window->ClearCachedResources();
    }  
    return TRUE;
}

void
nsWindow::ClearCachedResources()
{
    if (mLayerManager &&
        mLayerManager->GetBackendType() == LayersBackend::LAYERS_BASIC) {
      mLayerManager->ClearCachedResources();
    }
    ::EnumChildWindows(mWnd, nsWindow::ClearResourcesCallback, 0);
}

static bool IsDifferentThreadWindow(HWND aWnd)
{
  return ::GetCurrentThreadId() != ::GetWindowThreadProcessId(aWnd, nullptr);
}

// static
bool
nsWindow::EventIsInsideWindow(nsWindow* aWindow)
{
  RECT r;
  ::GetWindowRect(aWindow->mWnd, &r);
  DWORD pos = ::GetMessagePos();
  POINT mp;
  mp.x = GET_X_LPARAM(pos);
  mp.y = GET_Y_LPARAM(pos);

  // was the event inside this window?
  return static_cast<bool>(::PtInRect(&r, mp));
}

// static
bool
nsWindow::GetPopupsToRollup(nsIRollupListener* aRollupListener,
                            uint32_t* aPopupsToRollup)
{
  // If we're dealing with menus, we probably have submenus and we don't want
  // to rollup some of them if the click is in a parent menu of the current
  // submenu.
  *aPopupsToRollup = UINT32_MAX;
  nsAutoTArray<nsIWidget*, 5> widgetChain;
  uint32_t sameTypeCount =
    aRollupListener->GetSubmenuWidgetChain(&widgetChain);
  for (uint32_t i = 0; i < widgetChain.Length(); ++i) {
    nsIWidget* widget = widgetChain[i];
    if (EventIsInsideWindow(static_cast<nsWindow*>(widget))) {
      // Don't roll up if the mouse event occurred within a menu of the
      // same type. If the mouse event occurred in a menu higher than that,
      // roll up, but pass the number of popups to Rollup so that only those
      // of the same type close up.
      if (i < sameTypeCount) {
        return false;
      }

      *aPopupsToRollup = sameTypeCount;
      break;
    }
  }
  return true;
}

// static
bool
nsWindow::NeedsToHandleNCActivateDelayed(HWND aWnd)
{
  // While popup is open, popup window might be activated by other application.
  // At this time, we need to take back focus to the previous window but it
  // causes flickering its nonclient area because WM_NCACTIVATE comes before
  // WM_ACTIVATE and we cannot know which window will take focus at receiving
  // WM_NCACTIVATE. Therefore, we need a hack for preventing the flickerling.
  //
  // If non-popup window receives WM_NCACTIVATE at deactivating, default
  // wndproc shouldn't handle it as deactivating. Instead, at receiving
  // WM_ACTIVIATE after that, WM_NCACTIVATE should be sent again manually.
  // This returns true if the window needs to handle WM_NCACTIVATE later.

  nsWindow* window = WinUtils::GetNSWindowPtr(aWnd);
  return window && !window->IsPopup();
}

// static
bool
nsWindow::DealWithPopups(HWND aWnd, UINT aMessage,
                         WPARAM aWParam, LPARAM aLParam, LRESULT* aResult)
{
  NS_ASSERTION(aResult, "Bad outResult");

  // XXX Why do we use the return value of WM_MOUSEACTIVATE for all messages?
  *aResult = MA_NOACTIVATE;

  if (!::IsWindowVisible(aWnd)) {
    return false;
  }

  nsIRollupListener* rollupListener = nsBaseWidget::GetActiveRollupListener();
  NS_ENSURE_TRUE(rollupListener, false);

  nsCOMPtr<nsIWidget> popup = rollupListener->GetRollupWidget();
  if (!popup) {
    return false;
  }

  static bool sSendingNCACTIVATE = false;
  static bool sPendingNCACTIVATE = false;
  uint32_t popupsToRollup = UINT32_MAX;

  nsWindow* popupWindow = static_cast<nsWindow*>(popup.get());
  UINT nativeMessage = WinUtils::GetNativeMessage(aMessage);
  switch (nativeMessage) {
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_NCLBUTTONDOWN:
    case WM_NCRBUTTONDOWN:
    case WM_NCMBUTTONDOWN:
      if (!EventIsInsideWindow(popupWindow) &&
          GetPopupsToRollup(rollupListener, &popupsToRollup)) {
        break;
      }
      return false;

    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
      // We need to check if the popup thinks that it should cause closing
      // itself when mouse wheel events are fired outside the rollup widget.
      if (!EventIsInsideWindow(popupWindow)) {
        *aResult = MA_ACTIVATE;
        if (rollupListener->ShouldRollupOnMouseWheelEvent() &&
            GetPopupsToRollup(rollupListener, &popupsToRollup)) {
          break;
        }
      }
      return false;

    case WM_ACTIVATEAPP:
      break;

    case WM_ACTIVATE:
      // NOTE: Don't handle WA_INACTIVE for preventing popup taking focus
      // because we cannot distinguish it's caused by mouse or not.
      if (LOWORD(aWParam) == WA_ACTIVE && aLParam) {
        nsWindow* window = WinUtils::GetNSWindowPtr(aWnd);
        if (window && window->IsPopup()) {
          // Cancel notifying widget listeners of deactivating the previous
          // active window (see WM_KILLFOCUS case in ProcessMessage()).
          sJustGotDeactivate = false;
          // Reactivate the window later.
          ::PostMessageW(aWnd, MOZ_WM_REACTIVATE, aWParam, aLParam);
          return true;
        }
        // Don't rollup the popup when focus moves back to the parent window
        // from a popup because such case is caused by strange mouse drivers.
        nsWindow* prevWindow =
          WinUtils::GetNSWindowPtr(reinterpret_cast<HWND>(aLParam));
        if (prevWindow && prevWindow->IsPopup()) {
          return false;
        }
      } else if (LOWORD(aWParam) == WA_INACTIVE) {
        nsWindow* activeWindow =
          WinUtils::GetNSWindowPtr(reinterpret_cast<HWND>(aLParam));
        if (sPendingNCACTIVATE && NeedsToHandleNCActivateDelayed(aWnd)) {
          // If focus moves to non-popup widget or focusable popup, the window
          // needs to update its nonclient area.
          if (!activeWindow || !activeWindow->IsPopup()) {
            sSendingNCACTIVATE = true;
            ::SendMessageW(aWnd, WM_NCACTIVATE, false, 0);
            sSendingNCACTIVATE = false;
          }
          sPendingNCACTIVATE = false;
        }
        // If focus moves from/to popup, we don't need to rollup the popup
        // because such case is caused by strange mouse drivers.
        if (activeWindow) {
          if (activeWindow->IsPopup()) {
            return false;
          }
          nsWindow* deactiveWindow = WinUtils::GetNSWindowPtr(aWnd);
          if (deactiveWindow && deactiveWindow->IsPopup()) {
            return false;
          }
        }
      } else if (LOWORD(aWParam) == WA_CLICKACTIVE) {
        // If the WM_ACTIVATE message is caused by a click in a popup,
        // we should not rollup any popups.
        if (EventIsInsideWindow(popupWindow) ||
            !GetPopupsToRollup(rollupListener, &popupsToRollup)) {
          return false;
        }
      }
      break;

    case MOZ_WM_REACTIVATE:
      // The previous active window should take back focus.
      if (::IsWindow(reinterpret_cast<HWND>(aLParam))) {
        ::SetForegroundWindow(reinterpret_cast<HWND>(aLParam));
      }
      return true;

    case WM_NCACTIVATE:
      if (!aWParam && !sSendingNCACTIVATE &&
          NeedsToHandleNCActivateDelayed(aWnd)) {
        // Don't just consume WM_NCACTIVATE. It doesn't handle only the
        // nonclient area state change.
        ::DefWindowProcW(aWnd, aMessage, TRUE, aLParam);
        // Accept the deactivating because it's necessary to receive following
        // WM_ACTIVATE.
        *aResult = TRUE;
        sPendingNCACTIVATE = true;
        return true;
      }
      return false;

    case WM_MOUSEACTIVATE:
      if (!EventIsInsideWindow(popupWindow) &&
          GetPopupsToRollup(rollupListener, &popupsToRollup)) {
        // WM_MOUSEACTIVATE may be caused by moving the mouse (e.g., X-mouse
        // of TweakUI is enabled. Then, check if the popup should be rolled up
        // with rollup listener. If not, just consume the message.
        if (HIWORD(aLParam) == WM_MOUSEMOVE &&
            !rollupListener->ShouldRollupOnMouseActivate()) {
          return true;
        }
        // Otherwise, it should be handled by wndproc.
        return false;
      }

      // Prevent the click inside the popup from causing a change in window
      // activation. Since the popup is shown non-activated, we need to eat any
      // requests to activate the window while it is displayed. Windows will
      // automatically activate the popup on the mousedown otherwise.
      return true;

    case WM_KILLFOCUS:
      // If focus moves to other window created in different process/thread,
      // e.g., a plugin window, popups should be rolled up.
      if (IsDifferentThreadWindow(reinterpret_cast<HWND>(aWParam))) {
        break;
      }
      return false;

    case WM_MOVING:
    case WM_SIZING:
    case WM_MENUSELECT:
      break;

    default:
      return false;
  }

  // Only need to deal with the last rollup for left mouse down events.
  NS_ASSERTION(!mLastRollup, "mLastRollup is null");

  bool consumeRollupEvent;
  if (nativeMessage == WM_LBUTTONDOWN) {
    POINT pt;
    pt.x = GET_X_LPARAM(aLParam);
    pt.y = GET_Y_LPARAM(aLParam);
    ::ClientToScreen(aWnd, &pt);
    nsIntPoint pos(pt.x, pt.y);

    consumeRollupEvent =
      rollupListener->Rollup(popupsToRollup, true, &pos, &mLastRollup);
    NS_IF_ADDREF(mLastRollup);
  } else {
    consumeRollupEvent =
      rollupListener->Rollup(popupsToRollup, true, nullptr, nullptr);
  }

  // Tell hook to stop processing messages
  sProcessHook = false;
  sRollupMsgId = 0;
  sRollupMsgWnd = nullptr;

  // If we are NOT supposed to be consuming events, let it go through
  if (consumeRollupEvent && nativeMessage != WM_RBUTTONDOWN) {
    *aResult = MA_ACTIVATE;
    return true;
  }

  return false;
}

/**************************************************************
 **************************************************************
 **
 ** BLOCK: Misc. utility methods and functions.
 **
 ** General use.
 **
 **************************************************************
 **************************************************************/

// Note that the result of GetTopLevelWindow method can be different from the
// result of WinUtils::GetTopLevelHWND().  The result can be non-floating
// window.  Because our top level window may be contained in another window
// which is not managed by us.
nsWindow* nsWindow::GetTopLevelWindow(bool aStopOnDialogOrPopup)
{
  nsWindow* curWindow = this;

  while (true) {
    if (aStopOnDialogOrPopup) {
      switch (curWindow->mWindowType) {
        case eWindowType_dialog:
        case eWindowType_popup:
          return curWindow;
        default:
          break;
      }
    }

    // Retrieve the top level parent or owner window
    nsWindow* parentWindow = curWindow->GetParentWindow(true);

    if (!parentWindow)
      return curWindow;

    curWindow = parentWindow;
  }
}

static BOOL CALLBACK gEnumWindowsProc(HWND hwnd, LPARAM lParam)
{
  DWORD pid;
  ::GetWindowThreadProcessId(hwnd, &pid);
  if (pid == GetCurrentProcessId() && ::IsWindowVisible(hwnd))
  {
    gWindowsVisible = true;
    return FALSE;
  }
  return TRUE;
}

bool nsWindow::CanTakeFocus()
{
  gWindowsVisible = false;
  EnumWindows(gEnumWindowsProc, 0);
  if (!gWindowsVisible) {
    return true;
  } else {
    HWND fgWnd = ::GetForegroundWindow();
    if (!fgWnd) {
      return true;
    }
    DWORD pid;
    GetWindowThreadProcessId(fgWnd, &pid);
    if (pid == GetCurrentProcessId()) {
      return true;
    }
  }
  return false;
}

void nsWindow::GetMainWindowClass(nsAString& aClass)
{
  NS_PRECONDITION(aClass.IsEmpty(), "aClass should be empty string");
  nsresult rv = Preferences::GetString("ui.window_class_override", &aClass);
  if (NS_FAILED(rv) || aClass.IsEmpty()) {
    aClass.AssignASCII(sDefaultMainWindowClass);
  }
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

void nsWindow::PickerOpen()
{
  mPickerDisplayCount++;
}

void nsWindow::PickerClosed()
{
  NS_ASSERTION(mPickerDisplayCount > 0, "mPickerDisplayCount out of sync!");
  if (!mPickerDisplayCount)
    return;
  mPickerDisplayCount--;
  if (!mPickerDisplayCount && mDestroyCalled) {
    Destroy();
  }
}

/**************************************************************
 **************************************************************
 **
 ** BLOCK: ChildWindow impl.
 **
 ** Child window overrides.
 **
 **************************************************************
 **************************************************************/

// return the style for a child nsWindow
DWORD ChildWindow::WindowStyle()
{
  DWORD style = WS_CLIPCHILDREN | nsWindow::WindowStyle();
  if (!(style & WS_POPUP))
    style |= WS_CHILD; // WS_POPUP and WS_CHILD are mutually exclusive.
  VERIFY_WINDOW_STYLE(style);
  return style;
}
