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

#include "mozilla/ipc/RPCChannel.h"

/* This must occur *after* ipc/RPCChannel.h to avoid typedefs conflicts. */
#include "mozilla/Util.h"

#include "nsWindow.h"

#include <shellapi.h>
#include <windows.h>
#include <process.h>
#include <commctrl.h>
#include <unknwn.h>
#include <psapi.h>

#include "prlog.h"
#include "prtime.h"
#include "prprf.h"
#include "prmem.h"

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
#include "nsGUIEvent.h"
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
#include "nsDOMTouchEvent.h"
#include <cstdlib> // for std::abs(int/long)
#include <cmath> // for std::abs(float/double)

#ifdef MOZ_ENABLE_D3D9_LAYER
#include "LayerManagerD3D9.h"
#endif

#ifdef MOZ_ENABLE_D3D10_LAYER
#include "LayerManagerD3D10.h"
#endif

#include "LayerManagerOGL.h"
#include "nsIGfxInfo.h"
#include "BasicLayers.h"
#include "nsUXThemeConstants.h"
#include "KeyboardLayout.h"
#include "nsNativeDragTarget.h"
#include <mmsystem.h> // needed for WIN32_LEAN_AND_MEAN
#include <zmouse.h>
#include <richedit.h>

#if defined(ACCESSIBILITY)
#include "oleidl.h"
#include <winuser.h>
#include "nsAccessibilityService.h"
#include "mozilla/a11y/Platform.h"
#if !defined(WINABLEAPI)
#include <winable.h>
#endif // !defined(WINABLEAPI)
#endif // defined(ACCESSIBILITY)

#include "nsIWinTaskbar.h"
#define NS_TASKBAR_CONTRACTID "@mozilla.org/windows-taskbar;1"

#if defined(NS_ENABLE_TSF)
#include "nsTextStore.h"
#endif // defined(NS_ENABLE_TSF)

// Windowless plugin support
#include "npapi.h"

#include "nsWindowDefs.h"

#include "nsCrashOnException.h"
#include "nsIXULRuntime.h"

#include "nsIContent.h"

#include "mozilla/HangMonitor.h"
#include "nsIMM32Handler.h"

using namespace mozilla::widget;
using namespace mozilla::layers;
using namespace mozilla;

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
HCURSOR         nsWindow::sHCursor                = NULL;
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
HHOOK           nsWindow::sMsgFilterHook          = NULL;
HHOOK           nsWindow::sCallProcHook           = NULL;
HHOOK           nsWindow::sCallMouseHook          = NULL;
bool            nsWindow::sProcessHook            = false;
UINT            nsWindow::sRollupMsgId            = 0;
HWND            nsWindow::sRollupMsgWnd           = NULL;
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

// If we're using D3D9, this will not be allowed during initial 5 seconds.
bool            nsWindow::sAllowD3D9              = false;

TriStateBool nsWindow::sHasBogusPopupsDropShadowOnMultiMonitor = TRI_UNKNOWN;

// Used in OOPP plugin focus processing.
const PRUnichar* kOOPPPluginFocusEventId   = L"OOPP Plugin Focus Widget Event";
uint32_t        nsWindow::sOOPPPluginFocusEvent   =
                  RegisterWindowMessageW(kOOPPPluginFocusEventId);

MSG             nsWindow::sRedirectedKeyDown;

/**************************************************************
 *
 * SECTION: globals variables
 *
 **************************************************************/

static const char *sScreenManagerContractID       = "@mozilla.org/gfx/screenmanager;1";

#ifdef PR_LOGGING
PRLogModuleInfo* gWindowsLog                      = nullptr;
#endif

// Kbd layout. Used throughout character processing.
static KeyboardLayout gKbdLayout;

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

// We should never really try to accelerate windows bigger than this. In some
// cases this might lead to no D3D9 acceleration where we could have had it
// but D3D9 does not reliably report when it supports bigger windows. 8192
// is as safe as we can get, we know at least D3D10 hardware always supports
// this, other hardware we expect to report correctly in D3D9.
#define MAX_ACCELERATED_DIMENSION 8192


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

nsWindow::nsWindow() : nsBaseWidget()
{
#ifdef PR_LOGGING
  if (!gWindowsLog) {
    gWindowsLog = PR_NewLogModule("nsWindow");
  }
#endif

  mIconSmall            = nullptr;
  mIconBig              = nullptr;
  mWnd                  = nullptr;
  mPaintDC              = nullptr;
  mPrevWndProc          = nullptr;
  mNativeDragTarget     = nullptr;
  mInDtor               = false;
  mIsVisible            = false;
  mIsTopWidgetWindow    = false;
  mUnicodeWidget        = true;
  mDisplayPanFeedback   = false;
  mTouchWindow          = false;
  mCustomNonClient      = false;
  mHideChrome           = false;
  mFullscreenMode       = false;
  mMousePresent         = false;
  mDestroyCalled        = false;
  mPickerDisplayCount   = 0;
  mWindowType           = eWindowType_child;
  mBorderStyle          = eBorderStyle_default;
  mOldSizeMode          = nsSizeMode_Normal;
  mLastSizeMode         = nsSizeMode_Normal;
  mLastPoint.x          = 0;
  mLastPoint.y          = 0;
  mLastSize.width       = 0;
  mLastSize.height      = 0;
  mOldStyle             = 0;
  mOldExStyle           = 0;
  mPainting             = 0;
  mLastKeyboardLayout   = 0;
  mBlurSuppressLevel    = 0;
  mLastPaintEndTime     = TimeStamp::Now();
#ifdef MOZ_XUL
  mTransparentSurface   = nullptr;
  mMemoryDC             = nullptr;
  mTransparencyMode     = eTransparencyOpaque;
  memset(&mGlassMargins, 0, sizeof mGlassMargins);
#endif
  mBackground           = ::GetSysColor(COLOR_BTNFACE);
  mBrush                = ::CreateSolidBrush(NSRGB_2_COLOREF(mBackground));
  mForeground           = ::GetSysColor(COLOR_WINDOWTEXT);

  mTaskbarPreview = nullptr;
  mHasTaskbarIconBeenCreated = false;

  // Global initialization
  if (!sInstanceCount) {
    // Global app registration id for Win7 and up. See
    // WinTaskbar.cpp for details.
    mozilla::widget::WinTaskbar::RegisterAppUserModelID();
    gKbdLayout.LoadLayout(::GetKeyboardLayout(0));
    // Init IME handler
    nsIMM32Handler::Initialize();
#ifdef NS_ENABLE_TSF
    nsTextStore::Initialize();
#endif
    if (SUCCEEDED(::OleInitialize(NULL))) {
      sIsOleInitialized = TRUE;
    }
    NS_ASSERTION(sIsOleInitialized, "***** OLE is not initialized!\n");
    MouseScrollHandler::Initialize();
    // Init titlebar button info for custom frames.
    nsUXThemeData::InitTitlebarInfo();
    // Init theme data
    nsUXThemeData::UpdateNativeThemeInfo();
    ForgetRedirectedKeyDownMessage();
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
  if (NULL != mWnd)
    Destroy();

  // Free app icon resources.  This must happen after `OnDestroy` (see bug 708033).
  if (mIconSmall)
    ::DestroyIcon(mIconSmall);

  if (mIconBig)
    ::DestroyIcon(mIconBig);

  sInstanceCount--;

  // Global shutdown
  if (sInstanceCount == 0) {
#ifdef NS_ENABLE_TSF
    nsTextStore::Terminate();
#endif
    NS_IF_RELEASE(sCursorImgContainer);
    if (sIsOleInitialized) {
      ::OleFlushClipboard();
      ::OleUninitialize();
      sIsOleInitialized = FALSE;
    }
    // delete any of the IME structures that we allocated
    nsIMM32Handler::Terminate();
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
                 nsDeviceContext *aContext,
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

  BaseCreate(baseParent, aRect, aContext, aInitData);

  HWND parent;
  if (aParent) { // has a nsIWidget parent
    parent = aParent ? (HWND)aParent->GetNativeData(NS_NATIVE_WINDOW) : NULL;
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
      parent = NULL;
    }

    if (WinUtils::GetWindowsVersion() >= WinUtils::VISTA_VERSION &&
        WinUtils::GetWindowsVersion() <= WinUtils::WIN7_VERSION) {
      extendedStyle |= WS_EX_COMPOSITED;
    }

    if (aInitData->mIsDragPopup) {
      // This flag makes the window transparent to mouse events
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
  if(aInitData->mWindowType == eWindowType_plugin) {
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
                           NULL,
                           nsToolkit::mDllInstance,
                           NULL);

  if (!mWnd) {
    NS_WARNING("nsWindow CreateWindowEx failed.");
    return NS_ERROR_FAILURE;
  }

  if (mIsRTL && nsUXThemeData::dwmSetWindowAttributePtr) {
    DWORD dwAttribute = TRUE;    
    nsUXThemeData::dwmSetWindowAttributePtr(mWnd, DWMWA_NONCLIENT_RTL_LAYOUT, &dwAttribute, sizeof dwAttribute);
  }

  if (mWindowType != eWindowType_plugin &&
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
       0, 0, 0, 0, mWnd, NULL, nsToolkit::mDllInstance, NULL);
    HWND scrollableWnd = ::CreateWindowW
      (className.get(), L"FAKETRACKPOINTSCROLLABLE",
       WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP | 0x30,
       0, 0, 0, 0, scrollContainerWnd, NULL, nsToolkit::mDllInstance, NULL);

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

  // NOTE: mNativeIMEContext may be null if IMM module isn't installed.
  nsIMEContext IMEContext(mWnd);
  mInputContext.mNativeIMEContext = static_cast<void*>(IMEContext.get());
  MOZ_ASSERT(mInputContext.mNativeIMEContext ||
             !nsIMM32Handler::IsIMEAvailable());
  // If no IME context is available, we should set this widget's pointer since
  // nullptr indicates there is only one context per process on the platform.
  if (!mInputContext.mNativeIMEContext) {
    mInputContext.mNativeIMEContext = this;
  }

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
        (WinUtils::GetWindowsVersion() >= WinUtils::VISTA_VERSION)) ? 1 : 0;
    sSwitchKeyboardLayout =
      Preferences::GetBool("intl.keyboard.per_window_layout", false);
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
   * cleanup. It also would like to have the HWND intact, so we NULL it here.
   */
  if (mLayerManager) {
    mLayerManager->Destroy();
  }
  mLayerManager = nullptr;

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
    LRESULT result;
    mWindowHook.Notify(mWnd, WM_DESTROY, 0, 0, &result);
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
  wc.hIcon         = aIconID ? ::LoadIconW(::GetModuleHandleW(NULL), aIconID) : NULL;
  wc.hCursor       = NULL;
  wc.hbrBackground = mBrush;
  wc.lpszMenuName  = NULL;
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
    WinUtils::SetNSWindowPtr(mWnd, this);
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
    WinUtils::SetNSWindowPtr(mWnd, NULL);
    mPrevWndProc = NULL;
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
  HDC dc = ::GetDC(mWnd);
  if (!dc)
    return 1.0;

  // LOGPIXELSY returns the number of logical pixels per inch. This is based
  // on font DPI settings rather than the actual screen DPI.
  double pixelsPerInch = ::GetDeviceCaps(dc, LOGPIXELSY);
  ::ReleaseDC(mWnd, dc);
  return pixelsPerInch/96.0;
}

nsWindow* nsWindow::GetParentWindow(bool aIncludeOwner)
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

  return widget;
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
    if (syncInvalidate) {
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
  if (WinUtils::GetWindowsVersion() >= WinUtils::VISTA_VERSION &&
      !HasGlass() &&
      (mWindowType == eWindowType_popup && !IsPopupWithTitleBar() &&
       (mPopupType == ePopupTypeTooltip || mPopupType == ePopupTypePanel))) {
    SetWindowRgn(mWnd, NULL, false);
  }
}

void nsWindow::SetThemeRegion()
{
  // Popup types that have a visual styles region applied (bug 376408). This can be expanded
  // for other window types as needed. The regions are applied generically to the base window
  // so default constants are used for part and state. At some point we might need part and
  // state values from nsNativeThemeWin's GetThemePartAndState, but currently windows that
  // change shape based on state haven't come up.
  if (WinUtils::GetWindowsVersion() >= WinUtils::VISTA_VERSION &&
      !HasGlass() &&
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
 * SECTION: nsIWidget::RegisterTouchWindow,
 * nsIWidget::UnregisterTouchWindow, and helper functions
 *
 * Used to register the native window to receive touch events
 *
 **************************************************************/

NS_METHOD nsWindow::RegisterTouchWindow() {
  mTouchWindow = true;
  mGesture.RegisterTouchWindow(mWnd);
  ::EnumChildWindows(mWnd, nsWindow::RegisterTouchForDescendants, 0);
  return NS_OK;
}

NS_METHOD nsWindow::UnregisterTouchWindow() {
  mTouchWindow = false;
  mGesture.UnregisterTouchWindow(mWnd);
  ::EnumChildWindows(mWnd, nsWindow::UnregisterTouchForDescendants, 0);
  return NS_OK;
}

BOOL CALLBACK nsWindow::RegisterTouchForDescendants(HWND aWnd, LPARAM aMsg) {
  nsWindow* win = WinUtils::GetNSWindowPtr(aWnd);
  if (win)
    win->mGesture.RegisterTouchWindow(aWnd);
  return TRUE;
}

BOOL CALLBACK nsWindow::UnregisterTouchForDescendants(HWND aWnd, LPARAM aMsg) {
  nsWindow* win = WinUtils::GetNSWindowPtr(aWnd);
  if (win)
    win->mGesture.UnregisterTouchWindow(aWnd);
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
    c.mMinSize.width = NS_MAX(int32_t(::GetSystemMetrics(SM_CXMINTRACK)), c.mMinSize.width);
    c.mMinSize.height = NS_MAX(int32_t(::GetSystemMetrics(SM_CYMINTRACK)), c.mMinSize.height);
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

  // for top-level windows only, convert coordinates from global display pixels
  // (the "parent" coordinate space) to the window's device pixel space
  double scale =
    (mWindowType <= eWindowType_popup) ? GetDefaultScale() : 1.0;
  int32_t x = NSToIntRound(aX * scale);
  int32_t y = NSToIntRound(aY * scale);

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
    if (mWindowType == eWindowType_plugin &&
        (!mLayerManager || mLayerManager->GetBackendType() == LAYERS_D3D9) &&
        mClipRects &&
        (mClipRectCount != 1 || !mClipRects[0].IsEqualInterior(nsIntRect(0, 0, mBounds.width, mBounds.height)))) {
      flags |= SWP_NOCOPYBITS;
    }
    VERIFY(::SetWindowPos(mWnd, NULL, x, y, 0, 0, flags));

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
  double scale =
    (mWindowType <= eWindowType_popup) ? GetDefaultScale() : 1.0;
  int32_t width = NSToIntRound(aWidth * scale);
  int32_t height = NSToIntRound(aHeight * scale);

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
    VERIFY(::SetWindowPos(mWnd, NULL, 0, 0, width, GetHeight(height), flags));
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
  double scale =
    (mWindowType <= eWindowType_popup) ? GetDefaultScale() : 1.0;
  int32_t x = NSToIntRound(aX * scale);
  int32_t y = NSToIntRound(aY * scale);
  int32_t width = NSToIntRound(aWidth * scale);
  int32_t height = NSToIntRound(aHeight * scale);

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
    VERIFY(::SetWindowPos(mWnd, NULL, x, y, width, GetHeight(height), flags));
    SetThemeRegion();
  }

  if (aRepaint)
    Invalidate();

  NotifyRollupGeometryChange();
  return NS_OK;
}

NS_IMETHODIMP
nsWindow::BeginResizeDrag(nsGUIEvent* aEvent, int32_t aHorizontal, int32_t aVertical)
{
  NS_ENSURE_ARG_POINTER(aEvent);

  if (aEvent->eventStructType != NS_MOUSE_EVENT) {
    // you can only begin a resize drag with a mouse event
    return NS_ERROR_INVALID_ARG;
  }

  nsMouseEvent* mouseEvent = static_cast<nsMouseEvent*>(aEvent);
  if (mouseEvent->button != nsMouseEvent::eLeftButton) {
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
NS_METHOD nsWindow::ConstrainPosition(bool aAllowSlop,
                                      int32_t *aX, int32_t *aY)
{
  if (!mIsTopWidgetWindow) // only a problem for top-level windows
    return NS_OK;

  bool doConstrain = false; // whether we have enough info to do anything

  /* get our playing field. use the current screen, or failing that
    for any reason, use device caps for the default screen. */
  RECT screenRect;

  nsCOMPtr<nsIScreenManager> screenmgr = do_GetService(sScreenManagerContractID);
  if (screenmgr) {
    nsCOMPtr<nsIScreen> screen;
    int32_t left, top, width, height;

    // zero size rects confuse the screen manager
    width = mBounds.width > 0 ? mBounds.width : 1;
    height = mBounds.height > 0 ? mBounds.height : 1;
    screenmgr->ScreenForRect(*aX, *aY, width, height,
                             getter_AddRefs(screen));
    if (screen) {
      if (mSizeMode != nsSizeMode_Fullscreen) {
        // For normalized windows, use the desktop work area.
        screen->GetAvailRect(&left, &top, &width, &height);
      } else {
        // For full screen windows, use the desktop.
        screen->GetRect(&left, &top, &width, &height);
      }
      screenRect.left = left;
      screenRect.right = left+width;
      screenRect.top = top;
      screenRect.bottom = top+height;
      doConstrain = true;
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
          doConstrain = true;
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
 * GetBounds, GetClientBounds, GetScreenBounds, GetClientOffset
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

// return the x,y offset of the client area from the origin
// of the window. If the window is borderless returns (0,0).
nsIntPoint nsWindow::GetClientOffset()
{
  if (!mWnd) {
    return nsIntPoint(0, 0);
  }

  RECT r1;
  GetWindowRect(mWnd, &r1);
  nsIntPoint pt = WidgetToScreenOffset();
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
     // left, top, right, bottom for nsIntMargin
    nsIntMargin margins(-1, 0, -1, -1);
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
static const PRUnichar kManageWindowInfoProperty[] = L"ManageWindowInfoProperty";
typedef BOOL (WINAPI *GetWindowInfoPtr)(HWND hwnd, PWINDOWINFO pwi);
static GetWindowInfoPtr sGetWindowInfoPtrStub = NULL;

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
      appBarData.hWnd = FindWindow(L"Shell_TrayWnd", NULL);
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
      mNonClientOffset.top = NS_MIN(mCaptionHeight, mNonClientMargins.top);
    } else if (mNonClientMargins.top == 0) {
      mNonClientOffset.top = mCaptionHeight;
    } else {
      mNonClientOffset.top = 0;
    }

    if (mNonClientMargins.bottom > 0 && glass) {
      mNonClientOffset.bottom = NS_MIN(mVertResizeMargin, mNonClientMargins.bottom);
    } else if (mNonClientMargins.bottom == 0) {
      mNonClientOffset.bottom = mVertResizeMargin;
    } else {
      mNonClientOffset.bottom = 0;
    }

    if (mNonClientMargins.left > 0 && glass) {
      mNonClientOffset.left = NS_MIN(mHorResizeMargin, mNonClientMargins.left);
    } else if (mNonClientMargins.left == 0) {
      mNonClientOffset.left = mHorResizeMargin;
    } else {
      mNonClientOffset.left = 0;
    }

    if (mNonClientMargins.right > 0 && glass) {
      mNonClientOffset.right = NS_MIN(mHorResizeMargin, mNonClientMargins.right);
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
      mBorderStyle & eBorderStyle_none ||
      mHideChrome)
    return NS_ERROR_INVALID_ARG;

  // Request for a reset
  if (margins.top == -1 && margins.left == -1 &&
      margins.right == -1 && margins.bottom == -1) {
    mCustomNonClient = false;
    mNonClientMargins = margins;
    RemovePropW(mWnd, kManageWindowInfoProperty);
    // Force a reflow of content based on the new client
    // dimensions.
    ResetLayout();
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
  MapWindowPoints(NULL, mWnd, (LPPOINT)&rect, 2);
  HRGN winRgn = CreateRectRgnIndirect(&rect);

  // Subtract app client chrome and app content leaving
  // windows non-client chrome and app non-client chrome
  // in winRgn.
  GetWindowRect(mWnd, &rect);
  rect.top += mCaptionHeight;
  rect.right -= mHorResizeMargin;
  rect.bottom -= mHorResizeMargin;
  rect.left += mVertResizeMargin;
  MapWindowPoints(NULL, mWnd, (LPPOINT)&rect, 2);
  HRGN clientRgn = CreateRectRgnIndirect(&rect);
  CombineRgn(winRgn, winRgn, clientRgn, RGN_DIFF);
  DeleteObject(clientRgn);

  // triggers ncpaint and paint events for the two areas
  RedrawWindow(mWnd, NULL, winRgn, RDW_FRAME|RDW_INVALIDATE);
  DeleteObject(winRgn);
}

HRGN
nsWindow::ExcludeNonClientFromPaintRegion(HRGN aRegion)
{
  RECT rect;
  HRGN rgn = NULL;
  if (aRegion == (HRGN)1) { // undocumented value indicating a full refresh
    GetWindowRect(mWnd, &rect);
    rgn = CreateRectRgnIndirect(&rect);
  } else {
    rgn = aRegion;
  }
  GetClientRect(mWnd, &rect);
  MapWindowPoints(mWnd, NULL, (LPPOINT)&rect, 2);
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

NS_METHOD nsWindow::SetBackgroundColor(const nscolor &aColor)
{
  nsBaseWidget::SetBackgroundColor(aColor);

  if (mBrush)
    ::DeleteObject(mBrush);

  mBrush = ::CreateSolidBrush(NSRGB_2_COLOREF(mBackground));
  if (mWnd != NULL) {
    ::SetClassLongPtrW(mWnd, GCLP_HBRBACKGROUND, (LONG_PTR)mBrush);
  }
  return NS_OK;
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

    case eCursor_none:
      newCursor = ::LoadCursor(nsToolkit::mDllInstance, MAKEINTRESOURCE(IDC_NONE));
      break;

    default:
      NS_ERROR("Invalid cursor type");
      break;
  }

  if (NULL != newCursor) {
    mCursor = aCursor;
    HCURSOR oldCursor = ::SetCursor(newCursor);
    
    if (sHCursor == oldCursor) {
      NS_IF_RELEASE(sCursorImgContainer);
      if (sHCursor != NULL)
        ::DestroyIcon(sHCursor);
      sHCursor = NULL;
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

  if (sHCursor != NULL)
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

static const nsIntRegion
RegionFromArray(const nsTArray<nsIntRect>& aRects)
{
  nsIntRegion region;
  for (uint32_t i = 0; i < aRects.Length(); ++i) {
    region.Or(region, aRects[i]);
  }
  return region;
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
      nsWindowType type;
      child->GetWindowType(type);
      if (type == eWindowType_plugin) {
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
      largest.y = NS_MAX<uint32_t>(largest.y,
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
  }

  PR_LOG(gWindowsLog, PR_LOG_ALWAYS,
         ("glass margins: left:%d top:%d right:%d bottom:%d\n",
          margins.cxLeftWidth, margins.cyTopHeight,
          margins.cxRightWidth, margins.cyBottomHeight));

  // Extends the window frame behind the client area
  if(nsUXThemeData::CheckForCompositor()) {
    nsUXThemeData::dwmExtendFrameIntoClientAreaPtr(mWnd, &margins);
    nsUXThemeData::dwmSetWindowAttributePtr(mWnd, DWMWA_NCRENDERING_POLICY, &policy, sizeof policy);
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

  VERIFY(::RedrawWindow(mWnd, NULL, NULL, flags));
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
nsWindow::MakeFullScreen(bool aFullScreen)
{
  // taskbarInfo will be NULL pre Windows 7 until Bug 680227 is resolved.
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
    SetSizeMode(mOldSizeMode);
  }

  UpdateNonClientMargins();

  bool visible = mIsVisible;
  if (mOldSizeMode == nsSizeMode_Normal)
    Show(false);
  
  // Will call hide chrome, reposition window. Note this will
  // also cache dimensions for restoration, so it should only
  // be called once per fullscreen request.
  nsresult rv = nsBaseWidget::MakeFullScreen(aFullScreen);

  if (visible) {
    Show(true);
    Invalidate();
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
                                      NULL,
                                      nsToolkit::mDllInstance,
                                      NULL);
    case NS_NATIVE_PLUGIN_PORT:
    case NS_NATIVE_WIDGET:
    case NS_NATIVE_WINDOW:
    case NS_NATIVE_SHAREABLE_WINDOW:
      return (void*)mWnd;
    case NS_NATIVE_GRAPHIC:
      // XXX:  This is sleezy!!  Remember to Release the DC after using it!
#ifdef MOZ_XUL
      return (void*)(eTransparencyTransparent == mTransparencyMode) ?
        mMemoryDC : ::GetDC(mWnd);
#else
      return (void*)::GetDC(mWnd);
#endif

#ifdef NS_ENABLE_TSF
    case NS_NATIVE_TSF_THREAD_MGR:
      return nsTextStore::GetThreadMgr();
    case NS_NATIVE_TSF_CATEGORY_MGR:
      return nsTextStore::GetCategoryMgr();
    case NS_NATIVE_TSF_DISPLAY_ATTR_MGR:
      return nsTextStore::GetDisplayAttrMgr();
#endif //NS_ENABLE_TSF

    default:
      break;
  }

  return NULL;
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

nsIntPoint nsWindow::WidgetToScreenOffset()
{
  POINT point;
  point.x = 0;
  point.y = 0;
  ::ClientToScreen(mWnd, &point);
  return nsIntPoint(point.x, point.y);
}

nsIntSize nsWindow::ClientToWindowSize(const nsIntSize& aClientSize)
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

  return nsIntSize(r.right - r.left, r.bottom - r.top);
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
    if (nullptr != mWnd && NULL != mNativeDragTarget) {
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
    nsToolkit::gMouseTrailer->SetCaptureWindow(NULL);
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

bool
nsWindow::UseOffMainThreadCompositing()
{
  // OMTC doesn't work on Windows right now.
  return false;
}

LayerManager*
nsWindow::GetLayerManager(PLayersChild* aShadowManager,
                          LayersBackend aBackendHint,
                          LayerManagerPersistence aPersistence,
                          bool* aAllowRetaining)
{
  if (aAllowRetaining) {
    *aAllowRetaining = true;
  }

#ifdef MOZ_ENABLE_D3D10_LAYER
  if (mLayerManager) {
    if (mLayerManager->GetBackendType() == LAYERS_D3D10)
    {
      LayerManagerD3D10 *layerManagerD3D10 =
        static_cast<LayerManagerD3D10*>(mLayerManager.get());
      if (layerManagerD3D10->device() !=
          gfxWindowsPlatform::GetPlatform()->GetD3D10Device())
      {
        MOZ_ASSERT(!mLayerManager->IsInTransaction());

        mLayerManager->Destroy();
        mLayerManager = nullptr;
      }
    }
  }
#endif

  RECT windowRect;
  ::GetClientRect(mWnd, &windowRect);

  if (!mLayerManager ||
      (!sAllowD3D9 && aPersistence == LAYER_MANAGER_PERSISTENT &&
        mLayerManager->GetBackendType() == LAYERS_BASIC)) {
    // If D3D9 is not currently allowed but the permanent manager is required,
    // -and- we're currently using basic layers, run through this check.
    LayerManagerPrefs prefs;
    GetLayerManagerPrefs(&prefs);

    /* We don't currently support using an accelerated layer manager with
     * transparent windows so don't even try. I'm also not sure if we even
     * want to support this case. See bug #593471 */
    if (eTransparencyTransparent == mTransparencyMode ||
        prefs.mDisableAcceleration ||
        windowRect.right - windowRect.left > MAX_ACCELERATED_DIMENSION ||
        windowRect.bottom - windowRect.top > MAX_ACCELERATED_DIMENSION)
      mUseLayersAcceleration = false;
    else if (prefs.mAccelerateByDefault)
      mUseLayersAcceleration = true;

    if (mUseLayersAcceleration) {
      if (aPersistence == LAYER_MANAGER_PERSISTENT && !sAllowD3D9) {
        MOZ_ASSERT(!mLayerManager || !mLayerManager->IsInTransaction());

        // This will clear out our existing layer manager if we have one since
        // if we hit this with a LayerManager we're always using BasicLayers.
        nsToolkit::StartAllowingD3D9();
      }

#ifdef MOZ_ENABLE_D3D10_LAYER
      if (!prefs.mPreferD3D9 && !prefs.mPreferOpenGL) {
        nsRefPtr<LayerManagerD3D10> layerManager =
          new LayerManagerD3D10(this);
        if (layerManager->Initialize(prefs.mForceAcceleration)) {
          mLayerManager = layerManager;
        }
      }
#endif
#ifdef MOZ_ENABLE_D3D9_LAYER
      if (!prefs.mPreferOpenGL && !mLayerManager && sAllowD3D9) {
        nsRefPtr<LayerManagerD3D9> layerManager =
          new LayerManagerD3D9(this);
        if (layerManager->Initialize(prefs.mForceAcceleration)) {
          mLayerManager = layerManager;
        }
      }
#endif
      if (!mLayerManager && prefs.mPreferOpenGL) {
        nsCOMPtr<nsIGfxInfo> gfxInfo = do_GetService("@mozilla.org/gfx/info;1");
        int32_t status = nsIGfxInfo::FEATURE_NO_INFO;

        if (gfxInfo && !prefs.mForceAcceleration) {
          gfxInfo->GetFeatureStatus(nsIGfxInfo::FEATURE_OPENGL_LAYERS, &status);
        }

        if (status == nsIGfxInfo::FEATURE_NO_INFO) {
          nsRefPtr<LayerManagerOGL> layerManager =
            new LayerManagerOGL(this);
          if (layerManager->Initialize()) {
            mLayerManager = layerManager;
          }

        } else {
          NS_WARNING("OpenGL accelerated layers are not supported on this system.");
        }
      }
    }

    // Fall back to software if we couldn't use any hardware backends.
    if (!mLayerManager) {
      // Try to use an async compositor first, if possible
      if (UseOffMainThreadCompositing()) {
        // e10s uses the parameter to pass in the shadow manager from the TabChild
        // so we don't expect to see it there since this doesn't support e10s.
        NS_ASSERTION(aShadowManager == nullptr, "Async Compositor not supported with e10s");
        CreateCompositor();
      }

      if (!mLayerManager)
        mLayerManager = CreateBasicLayerManager();
    }
  }

  NS_ASSERTION(mLayerManager, "Couldn't provide a valid layer manager.");

  return mLayerManager;
}

/**************************************************************
 *
 * SECTION: nsIWidget::GetThebesSurface
 *
 * Get the Thebes surface associated with this widget.
 *
 **************************************************************/

gfxASurface *nsWindow::GetThebesSurface()
{
#ifdef CAIRO_HAS_D2D_SURFACE
  if (mD2DWindowSurface) {
    return mD2DWindowSurface;
  }
#endif
  if (mPaintDC)
    return (new gfxWindowsSurface(mPaintDC));

#ifdef CAIRO_HAS_D2D_SURFACE
  if (gfxWindowsPlatform::GetPlatform()->GetRenderMode() ==
      gfxWindowsPlatform::RENDER_DIRECT2D) {
    gfxASurface::gfxContentType content = gfxASurface::CONTENT_COLOR;
#if defined(MOZ_XUL)
    if (mTransparencyMode != eTransparencyOpaque) {
      content = gfxASurface::CONTENT_COLOR_ALPHA;
    }
#endif
    return (new gfxD2DSurface(mWnd, content));
  } else {
#endif
    uint32_t flags = gfxWindowsSurface::FLAG_TAKE_DC;
    if (mTransparencyMode != eTransparencyOpaque) {
        flags |= gfxWindowsSurface::FLAG_IS_TRANSPARENT;
    }
    return (new gfxWindowsSurface(mWnd, flags));
#ifdef CAIRO_HAS_D2D_SURFACE
  }
#endif
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
nsWindow::OverrideSystemMouseScrollSpeed(int32_t aOriginalDelta,
                                         bool aIsHorizontal,
                                         int32_t &aOverriddenDelta)
{
  // The default vertical and horizontal scrolling speed is 3, this is defined
  // on the document of SystemParametersInfo in MSDN.
  const uint32_t kSystemDefaultScrollingSpeed = 3;

  int32_t absOriginDelta = std::abs(aOriginalDelta);

  // Compute the simple overridden speed.
  int32_t absComputedOverriddenDelta;
  nsresult rv =
    nsBaseWidget::OverrideSystemMouseScrollSpeed(absOriginDelta, aIsHorizontal,
                                                 absComputedOverriddenDelta);
  NS_ENSURE_SUCCESS(rv, rv);

  aOverriddenDelta = aOriginalDelta;

  if (absComputedOverriddenDelta == absOriginDelta) {
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
  if (WinUtils::GetWindowsVersion() >= WinUtils::VISTA_VERSION) {
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
  int32_t absDeltaLimit;
  rv =
    nsBaseWidget::OverrideSystemMouseScrollSpeed(kSystemDefaultScrollingSpeed,
                                                 aIsHorizontal, absDeltaLimit);
  NS_ENSURE_SUCCESS(rv, rv);

  // If the given delta is larger than our computed limitation value, the delta
  // was accelerated by the mouse driver.  So, we should do nothing here.
  if (absDeltaLimit <= absOriginDelta) {
    return NS_OK;
  }

  absComputedOverriddenDelta =
    NS_MIN(absComputedOverriddenDelta, absDeltaLimit);

  aOverriddenDelta = (aOriginalDelta > 0) ? absComputedOverriddenDelta :
                                            -absComputedOverriddenDelta;
  return NS_OK;
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
void nsWindow::InitEvent(nsGUIEvent& event, nsIntPoint* aPoint)
{
  if (nullptr == aPoint) {     // use the point from the event
    // get the message position in client coordinates
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
  else {  
    // use the point override if provided
    event.refPoint.x = aPoint->x;
    event.refPoint.y = aPoint->y;
  }

  event.time = ::GetMessageTime();
  mLastPoint = event.refPoint;
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
NS_IMETHODIMP nsWindow::DispatchEvent(nsGUIEvent* event, nsEventStatus & aStatus)
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
  nsGUIEvent event(true, aMsg, this);
  InitEvent(event);

  bool result = DispatchWindowEvent(&event);
  return result;
}

bool nsWindow::DispatchWindowEvent(nsGUIEvent* event)
{
  nsEventStatus status;
  DispatchEvent(event, status);
  return ConvertStatus(status);
}

bool nsWindow::DispatchWindowEvent(nsGUIEvent* event, nsEventStatus &aStatus) {
  DispatchEvent(event, aStatus);
  return ConvertStatus(aStatus);
}

void nsWindow::InitKeyEvent(nsKeyEvent& aKeyEvent,
                            const NativeKey& aNativeKey,
                            const ModifierKeyState &aModKeyState)
{
  nsIntPoint point(0, 0);
  InitEvent(aKeyEvent, &point);
  aKeyEvent.location = aNativeKey.GetKeyLocation();
  aModKeyState.InitInputEvent(aKeyEvent);
}

bool nsWindow::DispatchKeyEvent(nsKeyEvent& aKeyEvent,
                                const MSG *aMsgSentToPlugin)
{
  UserActivity();

  NPEvent pluginEvent;
  if (aMsgSentToPlugin && PluginHasFocus()) {
    pluginEvent.event = aMsgSentToPlugin->message;
    pluginEvent.wParam = aMsgSentToPlugin->wParam;
    pluginEvent.lParam = aMsgSentToPlugin->lParam;
    aKeyEvent.pluginEvent = (void *)&pluginEvent;
  }

  return DispatchWindowEvent(&aKeyEvent);
}

bool nsWindow::DispatchCommandEvent(uint32_t aEventCommand)
{
  nsCOMPtr<nsIAtom> command;
  switch (aEventCommand) {
    case APPCOMMAND_BROWSER_BACKWARD:
      command = nsGkAtoms::Back;
      break;
    case APPCOMMAND_BROWSER_FORWARD:
      command = nsGkAtoms::Forward;
      break;
    case APPCOMMAND_BROWSER_REFRESH:
      command = nsGkAtoms::Reload;
      break;
    case APPCOMMAND_BROWSER_STOP:
      command = nsGkAtoms::Stop;
      break;
    case APPCOMMAND_BROWSER_SEARCH:
      command = nsGkAtoms::Search;
      break;
    case APPCOMMAND_BROWSER_FAVORITES:
      command = nsGkAtoms::Bookmarks;
      break;
    case APPCOMMAND_BROWSER_HOME:
      command = nsGkAtoms::Home;
      break;
    default:
      return false;
  }
  nsCommandEvent event(true, nsGkAtoms::onAppCommand, command, this);

  InitEvent(event);
  DispatchWindowEvent(&event);

  return true;
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
    if (GetUpdateRect(aWnd, NULL, FALSE))
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

// Deal with plugin events
bool nsWindow::DispatchPluginEvent(const MSG &aMsg)
{
  if (!PluginHasFocus())
    return false;

  nsPluginEvent event(true, NS_PLUGIN_INPUT_EVENT, this);
  nsIntPoint point(0, 0);
  InitEvent(event, &point);
  NPEvent pluginEvent;
  pluginEvent.event = aMsg.message;
  pluginEvent.wParam = aMsg.wParam;
  pluginEvent.lParam = aMsg.lParam;
  event.pluginEvent = (void *)&pluginEvent;
  event.retargetToFocusedDocument = true;
  return DispatchWindowEvent(&event);
}

bool nsWindow::DispatchPluginEvent(UINT aMessage,
                                     WPARAM aWParam,
                                     LPARAM aLParam,
                                     bool aDispatchPendingEvents)
{
  bool ret = DispatchPluginEvent(WinUtils::InitMSG(aMessage, aWParam, aLParam));
  if (aDispatchPendingEvents) {
    DispatchPendingEvents();
  }
  return ret;
}

void nsWindow::RemoveMessageAndDispatchPluginEvent(UINT aFirstMsg,
                 UINT aLastMsg, nsFakeCharMessage* aFakeCharMessage)
{
  MSG msg;
  if (aFakeCharMessage) {
    if (aFirstMsg > WM_CHAR || aLastMsg < WM_CHAR) {
      return;
    }
    msg = aFakeCharMessage->GetCharMessage(mWnd);
  } else {
    ::GetMessageW(&msg, mWnd, aFirstMsg, aLastMsg);
  }
  DispatchPluginEvent(msg);
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

  nsMouseEvent event(true, aEventType, this, nsMouseEvent::eReal,
                     aIsContextMenuKey
                     ? nsMouseEvent::eContextMenuKey
                     : nsMouseEvent::eNormal);
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

  nsIntPoint mpScreen = eventPoint + WidgetToScreenOffset();

  // Suppress mouse moves caused by widget creation
  if (aEventType == NS_MOUSE_MOVE) 
  {
    if ((sLastMouseMovePoint.x == mpScreen.x) && (sLastMouseMovePoint.y == mpScreen.y))
      return result;
    sLastMouseMovePoint.x = mpScreen.x;
    sLastMouseMovePoint.y = mpScreen.y;
  }

  bool insideMovementThreshold = (abs(sLastMousePoint.x - eventPoint.x) < (short)::GetSystemMetrics(SM_CXDOUBLECLK)) &&
                                   (abs(sLastMousePoint.y - eventPoint.y) < (short)::GetSystemMetrics(SM_CYDOUBLECLK));

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
    sLastClickCount = 2;
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
    event.exit = IsTopLevelMouseExit(mWnd) ? nsMouseEvent::eTopLevel : nsMouseEvent::eChild;
  }
  else if (aEventType == NS_MOUSE_MOZHITTEST)
  {
    event.flags |= NS_EVENT_FLAG_ONLY_CHROME_DISPATCH;
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
    case NS_MOUSE_EXIT:
      pluginEvent.event = WM_MOUSELEAVE;
      break;
    default:
      pluginEvent.event = WM_NULL;
      break;
  }

  pluginEvent.wParam = wParam;     // plugins NEED raw OS event flags!
  pluginEvent.lParam = lParam;

  event.pluginEvent = (void *)&pluginEvent;

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

      if (rect.Contains(event.refPoint)) {
        if (sCurrentWindow == NULL || sCurrentWindow != this) {
          if ((nullptr != sCurrentWindow) && (!sCurrentWindow->mInDtor)) {
            LPARAM pos = sCurrentWindow->lParamToClient(lParamToScreen(lParam));
            sCurrentWindow->DispatchMouseEvent(NS_MOUSE_EXIT, wParam, pos, false, 
                                               nsMouseEvent::eLeftButton, aInputSource);
          }
          sCurrentWindow = this;
          if (!mInDtor) {
            LPARAM pos = sCurrentWindow->lParamToClient(lParamToScreen(lParam));
            sCurrentWindow->DispatchMouseEvent(NS_MOUSE_ENTER, wParam, pos, false,
                                               nsMouseEvent::eLeftButton, aInputSource);
          }
        }
      }
    } else if (aEventType == NS_MOUSE_EXIT) {
      if (sCurrentWindow == this) {
        sCurrentWindow = nullptr;
      }
    }

    result = DispatchWindowEvent(&event);

    if (nsToolkit::gMouseTrailer)
      nsToolkit::gMouseTrailer->Enable();

    // Release the widget with NS_IF_RELEASE() just in case
    // the context menu key code in nsEventListenerManager::HandleEvent()
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
  HWND toplevelWnd = NULL;
  while (curWnd) {
    toplevelWnd = curWnd;

    nsWindow *win = WinUtils::GetNSWindowPtr(curWnd);
    if (win) {
      nsWindowType wintype;
      win->GetWindowType(wintype);
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
  NS_ASSERTION(!mozilla::ipc::SyncChannel::IsPumpingMessages(),
               "Failed to prevent a nonqueued message from running!");

  // Modal UI being displayed in windowless plugins.
  if (mozilla::ipc::RPCChannel::IsSpinLoopActive() &&
      (InSendMessageEx(NULL)&(ISMEX_REPLIED|ISMEX_SEND)) == ISMEX_SEND) {
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
        if ((InSendMessageEx(NULL)&(ISMEX_REPLIED|ISMEX_SEND)) == ISMEX_SEND) {
          PRUnichar szClass[10];
          HWND focusWnd = (HWND)lParam;
          if (IsWindowVisible(focusWnd) &&
              GetClassNameW(focusWnd, szClass,
                            sizeof(szClass)/sizeof(PRUnichar)) &&
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
      (InSendMessageEx(NULL)&(ISMEX_REPLIED|ISMEX_SEND)) == ISMEX_SEND) {
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
        SetMenuItemInfo(hMenu, SC_RESTORE, FALSE, &mii);
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
                     x, y, 0, hWnd, NULL);
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
  nsCOMPtr<nsISupports> kungFuDeathGrip;
  if (!targetWindow->mInDtor)
    kungFuDeathGrip = do_QueryInterface((nsBaseWidget*)targetWindow);

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
                                  LRESULT *aResult,
                                  bool &aCallDefWndProc)
{
  NS_PRECONDITION(aResult, "aResult must be non-null.");
  *aResult = 0;

  aCallDefWndProc = false;
  bool eventDispatched = false;
  switch (aMsg.message) {
    case WM_CHAR:
    case WM_SYSCHAR:
      *aResult = ProcessCharMessage(aMsg, &eventDispatched);
      break;

    case WM_KEYUP:
    case WM_SYSKEYUP:
      *aResult = ProcessKeyUpMessage(aMsg, &eventDispatched);
      break;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
      *aResult = ProcessKeyDownMessage(aMsg, &eventDispatched);
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

  if (!eventDispatched)
    aCallDefWndProc = !DispatchPluginEvent(aMsg);
  DispatchPendingEvents();
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

// The main windows message processing method.
bool nsWindow::ProcessMessage(UINT msg, WPARAM &wParam, LPARAM &lParam,
                                LRESULT *aRetValue)
{
  // (Large blocks of code should be broken out into OnEvent handlers.)
  if (mWindowHook.Notify(mWnd, msg, wParam, lParam, aRetValue))
    return true;

#if defined(EVENT_DEBUG_OUTPUT)
  // First param shows all events, second param indicates whether
  // to show mouse move events. See nsWindowDbg for details.
  PrintEvent(msg, SHOW_REPEAT_EVENTS, SHOW_MOUSEMOVE_EVENTS);
#endif

  bool eatMessage;
  if (nsIMM32Handler::ProcessMessage(this, msg, wParam, lParam, aRetValue,
                                     eatMessage)) {
    return mWnd ? eatMessage : true;
  }

  if (MouseScrollHandler::ProcessMessage(this, msg, wParam, lParam, aRetValue,
                                         eatMessage)) {
    return mWnd ? eatMessage : true;
  }

  if (PluginHasFocus()) {
    bool callDefaultWndProc;
    MSG nativeMsg = WinUtils::InitMSG(msg, wParam, lParam);
    if (ProcessMessageForPlugin(nativeMsg, aRetValue, callDefaultWndProc)) {
      return mWnd ? !callDefaultWndProc : true;
    }
  }

  bool result = false;    // call the default nsWindow proc
  *aRetValue = 0;

  // Glass hit testing w/custom transparent margins
  LRESULT dwmHitResult;
  if (mCustomNonClient &&
      nsUXThemeData::CheckForCompositor() &&
      nsUXThemeData::dwmDwmDefWindowProcPtr(mWnd, msg, wParam, lParam, &dwmHitResult)) {
    *aRetValue = dwmHitResult;
    return true;
  }

  switch (msg) {
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

    case WM_FONTCHANGE:
    {
      nsresult rv;
      bool didChange = false;

      // update the global font list
      nsCOMPtr<nsIFontEnumerator> fontEnum = do_GetService("@mozilla.org/gfx/fontenumerator;1", &rv);
      if (NS_SUCCEEDED(rv)) {
        fontEnum->UpdateFontList(&didChange);
        //didChange is TRUE only if new font langGroup is added to the list.
        if (didChange)  {
          ForceFontUpdate();
        }
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

      if (!mCustomNonClient)
        break;

      // let the dwm handle nc painting on glass
      if(nsUXThemeData::CheckForCompositor())
        break;

      if (wParam == TRUE) {
        // going active
        *aRetValue = FALSE; // ignored
        result = true;
        UpdateGetWindowInfoCaptionStatus(true);
        // invalidate to trigger a paint
        InvalidateNonClientRegion();
        break;
      } else {
        // going inactive
        *aRetValue = TRUE; // go ahead and deactive
        result = true;
        UpdateGetWindowInfoCaptionStatus(false);
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

    case WM_MOVE: // Window moved
    {
      RECT rect;
      ::GetWindowRect(mWnd, &rect);
      result = OnMove(rect.left, rect.top);
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
      *aRetValue = (int) OnPaint(NULL, 0);
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
      MSG nativeMsg = WinUtils::InitMSG(msg, wParam, lParam);
      result = ProcessCharMessage(nativeMsg, nullptr);
      DispatchPendingEvents();
    }
    break;

    case WM_SYSKEYUP:
    case WM_KEYUP:
    {
      MSG nativeMsg = WinUtils::InitMSG(msg, wParam, lParam);
      nativeMsg.time = ::GetMessageTime();
      result = ProcessKeyUpMessage(nativeMsg, nullptr);
      DispatchPendingEvents();
    }
    break;

    case WM_SYSKEYDOWN:
    case WM_KEYDOWN:
    {
      MSG nativeMsg = WinUtils::InitMSG(msg, wParam, lParam);
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
                                  false, nsMouseEvent::eLeftButton, MOUSE_INPUT_SOURCE());
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
                                  false, nsMouseEvent::eLeftButton, MOUSE_INPUT_SOURCE());
      DispatchPendingEvents();
    }
    break;

    case WM_LBUTTONUP:
    {
      result = DispatchMouseEvent(NS_MOUSE_BUTTON_UP, wParam, lParam,
                                  false, nsMouseEvent::eLeftButton, MOUSE_INPUT_SOURCE());
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
                         nsMouseEvent::eLeftButton, MOUSE_INPUT_SOURCE());
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
                                    nsMouseEvent::eLeftButton :
                                    nsMouseEvent::eRightButton, MOUSE_INPUT_SOURCE());
      if (lParam != -1 && !result && mCustomNonClient &&
          DispatchMouseEvent(NS_MOUSE_MOZHITTEST, wParam, pos,
                             false, nsMouseEvent::eLeftButton,
                             MOUSE_INPUT_SOURCE())) {
        // Blank area hit, throw up the system menu.
        DisplaySystemMenu(mWnd, mSizeMode, mIsRTL, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        result = true;
      }
    }
    break;

    case WM_LBUTTONDBLCLK:
      result = DispatchMouseEvent(NS_MOUSE_DOUBLECLICK, wParam, lParam, false,
                                  nsMouseEvent::eLeftButton, MOUSE_INPUT_SOURCE());
      DispatchPendingEvents();
      break;

    case WM_MBUTTONDOWN:
      result = DispatchMouseEvent(NS_MOUSE_BUTTON_DOWN, wParam, lParam, false,
                                  nsMouseEvent::eMiddleButton, MOUSE_INPUT_SOURCE());
      DispatchPendingEvents();
      break;

    case WM_MBUTTONUP:
      result = DispatchMouseEvent(NS_MOUSE_BUTTON_UP, wParam, lParam, false,
                                  nsMouseEvent::eMiddleButton, MOUSE_INPUT_SOURCE());
      DispatchPendingEvents();
      break;

    case WM_MBUTTONDBLCLK:
      result = DispatchMouseEvent(NS_MOUSE_DOUBLECLICK, wParam, lParam, false,
                                  nsMouseEvent::eMiddleButton, MOUSE_INPUT_SOURCE());
      DispatchPendingEvents();
      break;

    case WM_NCMBUTTONDOWN:
      result = DispatchMouseEvent(NS_MOUSE_BUTTON_DOWN, 0, lParamToClient(lParam), false,
                                  nsMouseEvent::eMiddleButton, MOUSE_INPUT_SOURCE());
      DispatchPendingEvents();
      break;

    case WM_NCMBUTTONUP:
      result = DispatchMouseEvent(NS_MOUSE_BUTTON_UP, 0, lParamToClient(lParam), false,
                                  nsMouseEvent::eMiddleButton, MOUSE_INPUT_SOURCE());
      DispatchPendingEvents();
      break;

    case WM_NCMBUTTONDBLCLK:
      result = DispatchMouseEvent(NS_MOUSE_DOUBLECLICK, 0, lParamToClient(lParam), false,
                                  nsMouseEvent::eMiddleButton, MOUSE_INPUT_SOURCE());
      DispatchPendingEvents();
      break;

    case WM_RBUTTONDOWN:
      result = DispatchMouseEvent(NS_MOUSE_BUTTON_DOWN, wParam, lParam, false,
                                  nsMouseEvent::eRightButton, MOUSE_INPUT_SOURCE());
      DispatchPendingEvents();
      break;

    case WM_RBUTTONUP:
      result = DispatchMouseEvent(NS_MOUSE_BUTTON_UP, wParam, lParam, false,
                                  nsMouseEvent::eRightButton, MOUSE_INPUT_SOURCE());
      DispatchPendingEvents();
      break;

    case WM_RBUTTONDBLCLK:
      result = DispatchMouseEvent(NS_MOUSE_DOUBLECLICK, wParam, lParam, false,
                                  nsMouseEvent::eRightButton, MOUSE_INPUT_SOURCE());
      DispatchPendingEvents();
      break;

    case WM_NCRBUTTONDOWN:
      result = DispatchMouseEvent(NS_MOUSE_BUTTON_DOWN, 0, lParamToClient(lParam), 
                                  false, nsMouseEvent::eRightButton,
                                  MOUSE_INPUT_SOURCE());
      DispatchPendingEvents();
      break;

    case WM_NCRBUTTONUP:
      result = DispatchMouseEvent(NS_MOUSE_BUTTON_UP, 0, lParamToClient(lParam),
                                  false, nsMouseEvent::eRightButton,
                                  MOUSE_INPUT_SOURCE());
      DispatchPendingEvents();
      break;

    case WM_NCRBUTTONDBLCLK:
      result = DispatchMouseEvent(NS_MOUSE_DOUBLECLICK, 0, lParamToClient(lParam),
                                  false, nsMouseEvent::eRightButton,
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
                         false, nsMouseEvent::eLeftButton,
                         MOUSE_INPUT_SOURCE());
      result = 
        DispatchMouseEvent(NS_MOUSE_BUTTON_UP, 0, lParamToClient(lParam),
                           false, nsMouseEvent::eLeftButton,
                           MOUSE_INPUT_SOURCE());
      DispatchPendingEvents();
      break;

    case WM_APPCOMMAND:
    {
      uint32_t appCommand = GET_APPCOMMAND_LPARAM(lParam);

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
          result = true;
          break;
      }
      // default = false - tell the driver that the event was not handled
    }
    break;

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
            mLastKeyboardLayout = gKbdLayout.GetLayout();

        } else {
          StopFlashing();

          sJustGotActivate = true;
          nsMouseEvent event(true, NS_MOUSE_ACTIVATE, this,
                             nsMouseEvent::eReal);
          InitEvent(event);
          ModifierKeyState modifierKeyState;
          modifierKeyState.InitInputEvent(event);
          DispatchWindowEvent(&event);
          if (sSwitchKeyboardLayout && mLastKeyboardLayout)
            ActivateKeyboardLayout(mLastKeyboardLayout, 0);
        }
      }
      break;
      
    case WM_MOUSEACTIVATE:
      if (mWindowType == eWindowType_popup) {
        // a popup with a parent owner should not be activated when clicked
        // but should still allow the mouse event to be fired, so the return
        // value is set to MA_NOACTIVATE. But if the owner isn't the frontmost
        // window, just use default processing so that the window is activated.
        HWND owner = ::GetWindow(mWnd, GW_OWNER);
        if (owner && owner == ::GetForegroundWindow()) {
          *aRetValue = MA_NOACTIVATE;
          result = true;
        }
      }
      break;

    case WM_WINDOWPOSCHANGING:
    {
      LPWINDOWPOS info = (LPWINDOWPOS) lParam;
      OnWindowPosChanging(info);
    }
    break;

    case WM_GETMINMAXINFO:
    {
      MINMAXINFO* mmi = (MINMAXINFO*)lParam;
      // Set the constraints. The minimum size should also be constrained to the
      // default window maximum size so that it fits on screen.
      mmi->ptMinTrackSize.x =
        NS_MIN((int32_t)mmi->ptMaxTrackSize.x,
               NS_MAX((int32_t)mmi->ptMinTrackSize.x, mSizeConstraints.mMinSize.width));
      mmi->ptMinTrackSize.y =
        NS_MIN((int32_t)mmi->ptMaxTrackSize.y,
        NS_MAX((int32_t)mmi->ptMinTrackSize.y, mSizeConstraints.mMinSize.height));
      mmi->ptMaxTrackSize.x = NS_MIN((int32_t)mmi->ptMaxTrackSize.x, mSizeConstraints.mMaxSize.width);
      mmi->ptMaxTrackSize.y = NS_MIN((int32_t)mmi->ptMaxTrackSize.y, mSizeConstraints.mMaxSize.height);
    }
    break;

    case WM_SETFOCUS:
      // If previous focused window isn't ours, it must have received the
      // redirected message.  So, we should forget it.
      if (!WinUtils::IsOurProcessWindow(HWND(wParam))) {
        ForgetRedirectedKeyDownMessage();
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
      WINDOWPOS *wp = (LPWINDOWPOS)lParam;
      OnWindowPosChanged(wp, result);
    }
    break;

    case WM_INPUTLANGCHANGEREQUEST:
      *aRetValue = TRUE;
      result = false;
      break;

    case WM_INPUTLANGCHANGE:
      result = OnInputLangChange((HKL)lParam);
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
      DWORD objId = static_cast<DWORD>(lParam);
      if (objId == OBJID_CLIENT) { // oleacc.dll will be loaded dynamically
        a11y::Accessible* rootAccessible = GetRootAccessible(); // Held by a11y cache
        if (rootAccessible) {
          IAccessible *msaaAccessible = NULL;
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
    RemovePropW(mWnd, kManageWindowInfoProperty);
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
          mWindowType != eWindowType_plugin) {
        // A GestureNotify event is dispatched to decide which single-finger panning
        // direction should be active (including none) and if pan feedback should
        // be displayed. Java and plugin windows can make their own calls.
        GESTURENOTIFYSTRUCT * gestureinfo = (GESTURENOTIFYSTRUCT*)lParam;
        nsPointWin touchPoint;
        touchPoint = gestureinfo->ptsLocation;
        touchPoint.ScreenToClient(mWnd);
        nsGestureNotifyEvent gestureNotifyEvent(true, NS_GESTURENOTIFY_EVENT_START, this);
        gestureNotifyEvent.refPoint = touchPoint;
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
      nsContentCommandEvent command(true, NS_CONTENT_COMMAND_DELETE, this);
      DispatchWindowEvent(&command);
      result = true;
    }
    break;

    case WM_CUT:
    {
      nsContentCommandEvent command(true, NS_CONTENT_COMMAND_CUT, this);
      DispatchWindowEvent(&command);
      result = true;
    }
    break;

    case WM_COPY:
    {
      nsContentCommandEvent command(true, NS_CONTENT_COMMAND_COPY, this);
      DispatchWindowEvent(&command);
      result = true;
    }
    break;

    case WM_PASTE:
    {
      nsContentCommandEvent command(true, NS_CONTENT_COMMAND_PASTE, this);
      DispatchWindowEvent(&command);
      result = true;
    }
    break;

    case EM_UNDO:
    {
      nsContentCommandEvent command(true, NS_CONTENT_COMMAND_UNDO, this);
      DispatchWindowEvent(&command);
      *aRetValue = (LRESULT)(command.mSucceeded && command.mIsEnabled);
      result = true;
    }
    break;

    case EM_REDO:
    {
      nsContentCommandEvent command(true, NS_CONTENT_COMMAND_REDO, this);
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
        nsContentCommandEvent command(true, NS_CONTENT_COMMAND_PASTE,
                                      this, true);
        DispatchWindowEvent(&command);
        *aRetValue = (LRESULT)(command.mSucceeded && command.mIsEnabled);
        result = true;
      }
    }
    break;

    case EM_CANUNDO:
    {
      nsContentCommandEvent command(true, NS_CONTENT_COMMAND_UNDO,
                                    this, true);
      DispatchWindowEvent(&command);
      *aRetValue = (LRESULT)(command.mSucceeded && command.mIsEnabled);
      result = true;
    }
    break;

    case EM_CANREDO:
    {
      nsContentCommandEvent command(true, NS_CONTENT_COMMAND_REDO,
                                    this, true);
      DispatchWindowEvent(&command);
      *aRetValue = (LRESULT)(command.mSucceeded && command.mIsEnabled);
      result = true;
    }
    break;

    default:
    {
#ifdef NS_ENABLE_TSF
      if (msg == WM_USER_TSF_TEXTCHANGE) {
        nsTextStore::OnTextChangeMsg();
      }
#endif //NS_ENABLE_TSF
      if (msg == nsAppShell::GetTaskbarButtonCreatedMessage())
        SetHasTaskbarIconBeenCreated();
      if (msg == sOOPPPluginFocusEvent) {
        if (wParam == 1) {
          // With OOPP, the plugin window exists in another process and is a child of
          // this window. This window is a placeholder plugin window for the dom. We
          // receive this event when the child window receives focus. (sent from
          // PluginInstanceParent.cpp)
          ::SendMessage(mWnd, WM_MOUSEACTIVATE, 0, 0); // See nsPluginNativeWindowWin.cpp
        } else {
          // WM_KILLFOCUS was received by the child process.
          if (sJustGotDeactivate) {
            DispatchFocusToTopLevelWindow(false);
          }
        }
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
  nsIntMargin nonClientSize(NS_MAX(mHorResizeMargin - mNonClientOffset.left,
                                   kResizableBorderMinSize),
                            NS_MAX(mCaptionHeight - mNonClientOffset.top,
                                   kResizableBorderMinSize),
                            NS_MAX(mHorResizeMargin - mNonClientOffset.right,
                                   kResizableBorderMinSize),
                            NS_MAX(mVertResizeMargin - mNonClientOffset.bottom,
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
  nsIntMargin borderSize(NS_MAX(nonClientSize.left, mHorResizeMargin),
                         NS_MAX(nonClientSize.top, mVertResizeMargin),
                         NS_MAX(nonClientSize.right, mHorResizeMargin),
                         NS_MAX(nonClientSize.bottom, mVertResizeMargin));

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
    LPARAM lParam = MAKELPARAM(mx, my);
    LPARAM lParamClient = lParamToClient(lParam);
    bool result = DispatchMouseEvent(NS_MOUSE_MOZHITTEST, 0, lParamClient,
                                     false, nsMouseEvent::eLeftButton, MOUSE_INPUT_SOURCE());
    if (result) {
      // The mouse is over a blank area
      testResult = testResult == HTCLIENT ? HTCAPTION : testResult;

    } else {
      // There's content over the mouse pointer. Set HTCLIENT
      // to possibly override a resizer border.
      testResult = HTCLIENT;
    }
  }

  return testResult;
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

// RemoveNextCharMessage() should be called by WM_KEYDOWN or WM_SYSKEYDOWM
// message handler.  If there is no WM_(SYS)CHAR message for it, this
// method does nothing.
// NOTE: WM_(SYS)CHAR message is posted by TranslateMessage() API which is
// called in message loop.  So, WM_(SYS)KEYDOWN message should have
// WM_(SYS)CHAR message in the queue if the keydown event causes character
// input.

/* static */
void nsWindow::RemoveNextCharMessage(HWND aWnd)
{
  MSG msg;
  if (::PeekMessageW(&msg, aWnd,
                     WM_KEYFIRST, WM_KEYLAST, PM_NOREMOVE | PM_NOYIELD) &&
      (msg.message == WM_CHAR || msg.message == WM_SYSCHAR)) {
    ::GetMessageW(&msg, aWnd, msg.message, msg.message);
  }
}

LRESULT nsWindow::ProcessCharMessage(const MSG &aMsg, bool *aEventDispatched)
{
  NS_PRECONDITION(aMsg.message == WM_CHAR || aMsg.message == WM_SYSCHAR,
                  "message is not keydown event");
  PR_LOG(gWindowsLog, PR_LOG_ALWAYS,
         ("%s charCode=%d scanCode=%d\n",
          aMsg.message == WM_SYSCHAR ? "WM_SYSCHAR" : "WM_CHAR",
          aMsg.wParam, HIWORD(aMsg.lParam) & 0xFF));

  // These must be checked here too as a lone WM_CHAR could be received
  // if a child window didn't handle it (for example Alt+Space in a content window)
  ModifierKeyState modKeyState;
  NativeKey nativeKey(gKbdLayout, this, aMsg);
  return OnChar(aMsg, nativeKey, modKeyState, aEventDispatched);
}

LRESULT nsWindow::ProcessKeyUpMessage(const MSG &aMsg, bool *aEventDispatched)
{
  NS_PRECONDITION(aMsg.message == WM_KEYUP || aMsg.message == WM_SYSKEYUP,
                  "message is not keydown event");
  PR_LOG(gWindowsLog, PR_LOG_ALWAYS,
         ("%s VK=%d\n", aMsg.message == WM_SYSKEYDOWN ?
                        "WM_SYSKEYUP" : "WM_KEYUP", aMsg.wParam));

  ModifierKeyState modKeyState;

  // Note: the original code passed (HIWORD(lParam)) to OnKeyUp as
  // scan code. However, this breaks Alt+Num pad input.
  // MSDN states the following:
  //  Typically, ToAscii performs the translation based on the
  //  virtual-key code. In some cases, however, bit 15 of the
  //  uScanCode parameter may be used to distinguish between a key
  //  press and a key release. The scan code is used for
  //  translating ALT+number key combinations.

  // ignore [shift+]alt+space so the OS can handle it
  if (modKeyState.IsAlt() && !modKeyState.IsControl() &&
      IS_VK_DOWN(NS_VK_SPACE)) {
    return FALSE;
  }

  if (!nsIMM32Handler::IsComposingOn(this) &&
      (aMsg.message != WM_KEYUP || aMsg.wParam != VK_MENU)) {
    // Ignore VK_MENU if it's not a system key release, so that the menu bar does not trigger
    // This helps avoid triggering the menu bar for ALT key accelerators used in
    // assistive technologies such as Window-Eyes and ZoomText, and when using Alt+Tab
    // to switch back to Mozilla in Windows 95 and Windows 98
    return OnKeyUp(aMsg, modKeyState, aEventDispatched);
  }

  return 0;
}

LRESULT nsWindow::ProcessKeyDownMessage(const MSG &aMsg,
                                        bool *aEventDispatched)
{
  PR_LOG(gWindowsLog, PR_LOG_ALWAYS,
         ("%s VK=%d\n", aMsg.message == WM_SYSKEYDOWN ?
                        "WM_SYSKEYDOWN" : "WM_KEYDOWN", aMsg.wParam));
  NS_PRECONDITION(aMsg.message == WM_KEYDOWN || aMsg.message == WM_SYSKEYDOWN,
                  "message is not keydown event");

  // If this method doesn't call OnKeyDown(), this method must clean up the
  // redirected message information itself.  For more information, see above
  // comment of AutoForgetRedirectedKeyDownMessage struct definition in
  // nsWindow.h.
  AutoForgetRedirectedKeyDownMessage forgetRedirectedMessage(this, aMsg);

  ModifierKeyState modKeyState;

  // Note: the original code passed (HIWORD(lParam)) to OnKeyDown as
  // scan code. However, this breaks Alt+Num pad input.
  // MSDN states the following:
  //  Typically, ToAscii performs the translation based on the
  //  virtual-key code. In some cases, however, bit 15 of the
  //  uScanCode parameter may be used to distinguish between a key
  //  press and a key release. The scan code is used for
  //  translating ALT+number key combinations.

  // ignore [shift+]alt+space so the OS can handle it
  if (modKeyState.IsAlt() && !modKeyState.IsControl() &&
      IS_VK_DOWN(NS_VK_SPACE))
    return FALSE;

  LRESULT result = 0;
  if (modKeyState.IsAlt() && nsIMM32Handler::IsStatusChanged()) {
    nsIMM32Handler::NotifyEndStatusChange();
  } else if (!nsIMM32Handler::IsComposingOn(this)) {
    result = OnKeyDown(aMsg, modKeyState, aEventDispatched, nullptr);
    // OnKeyDown cleaned up the redirected message information itself, so,
    // we should do nothing.
    forgetRedirectedMessage.mCancel = true;
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
                                   const nsAString& aUnmodifiedCharacters)
{
  UINT keyboardLayoutListCount = ::GetKeyboardLayoutList(0, NULL);
  NS_ASSERTION(keyboardLayoutListCount > 0,
               "One keyboard layout must be installed at least");
  HKL keyboardLayoutListBuff[50];
  HKL* keyboardLayoutList =
    keyboardLayoutListCount < 50 ? keyboardLayoutListBuff :
                                   new HKL[keyboardLayoutListCount];
  keyboardLayoutListCount =
    ::GetKeyboardLayoutList(keyboardLayoutListCount, keyboardLayoutList);
  NS_ASSERTION(keyboardLayoutListCount > 0,
               "Failed to get all keyboard layouts installed on the system");

  nsPrintfCString layoutName("%08x", aNativeKeyboardLayout);
  HKL loadedLayout = LoadKeyboardLayoutA(layoutName.get(), KLF_NOTELLSHELL);
  if (loadedLayout == NULL) {
    if (keyboardLayoutListBuff != keyboardLayoutList) {
      delete [] keyboardLayoutList;
    }
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Setup clean key state and load desired layout
  BYTE originalKbdState[256];
  ::GetKeyboardState(originalKbdState);
  BYTE kbdState[256];
  memset(kbdState, 0, sizeof(kbdState));
  // This changes the state of the keyboard for the current thread only,
  // and we'll restore it soon, so this should be OK.
  ::SetKeyboardState(kbdState);
  HKL oldLayout = gKbdLayout.GetLayout();
  gKbdLayout.LoadLayout(loadedLayout);

  uint8_t argumentKeySpecific = 0;
  switch (aNativeKeyCode) {
    case VK_SHIFT:
      aModifierFlags &= ~(nsIWidget::SHIFT_L | nsIWidget::SHIFT_R);
      argumentKeySpecific = VK_LSHIFT;
      break;
    case VK_LSHIFT:
      aModifierFlags &= ~nsIWidget::SHIFT_L;
      argumentKeySpecific = aNativeKeyCode;
      aNativeKeyCode = VK_SHIFT;
      break;
    case VK_RSHIFT:
      aModifierFlags &= ~nsIWidget::SHIFT_R;
      argumentKeySpecific = aNativeKeyCode;
      aNativeKeyCode = VK_SHIFT;
      break;
    case VK_CONTROL:
      aModifierFlags &= ~(nsIWidget::CTRL_L | nsIWidget::CTRL_R);
      argumentKeySpecific = VK_LCONTROL;
      break;
    case VK_LCONTROL:
      aModifierFlags &= ~nsIWidget::CTRL_L;
      argumentKeySpecific = aNativeKeyCode;
      aNativeKeyCode = VK_CONTROL;
      break;
    case VK_RCONTROL:
      aModifierFlags &= ~nsIWidget::CTRL_R;
      argumentKeySpecific = aNativeKeyCode;
      aNativeKeyCode = VK_CONTROL;
      break;
    case VK_MENU:
      aModifierFlags &= ~(nsIWidget::ALT_L | nsIWidget::ALT_R);
      argumentKeySpecific = VK_LMENU;
      break;
    case VK_LMENU:
      aModifierFlags &= ~nsIWidget::ALT_L;
      argumentKeySpecific = aNativeKeyCode;
      aNativeKeyCode = VK_MENU;
      break;
    case VK_RMENU:
      aModifierFlags &= ~nsIWidget::ALT_R;
      argumentKeySpecific = aNativeKeyCode;
      aNativeKeyCode = VK_MENU;
      break;
    case VK_CAPITAL:
      aModifierFlags &= ~nsIWidget::CAPS_LOCK;
      argumentKeySpecific = VK_CAPITAL;
      break;
    case VK_NUMLOCK:
      aModifierFlags &= ~nsIWidget::NUM_LOCK;
      argumentKeySpecific = VK_NUMLOCK;
      break;
  }

  nsAutoTArray<KeyPair,10> keySequence;
  SetupKeyModifiersSequence(&keySequence, aModifierFlags);
  NS_ASSERTION(aNativeKeyCode >= 0 && aNativeKeyCode < 256,
               "Native VK key code out of range");
  keySequence.AppendElement(KeyPair(aNativeKeyCode, argumentKeySpecific));

  // Simulate the pressing of each modifier key and then the real key
  for (uint32_t i = 0; i < keySequence.Length(); ++i) {
    uint8_t key = keySequence[i].mGeneral;
    uint8_t keySpecific = keySequence[i].mSpecific;
    kbdState[key] = 0x81; // key is down and toggled on if appropriate
    if (keySpecific) {
      kbdState[keySpecific] = 0x81;
    }
    ::SetKeyboardState(kbdState);
    ModifierKeyState modKeyState;
    UINT scanCode = ::MapVirtualKeyEx(argumentKeySpecific ?
                                        argumentKeySpecific : aNativeKeyCode,
                                      MAPVK_VK_TO_VSC, gKbdLayout.GetLayout());
    LPARAM lParam = static_cast<LPARAM>(scanCode << 16);
    // Add extended key flag to the lParam for right control key and right alt
    // key.
    if (keySpecific == VK_RCONTROL || keySpecific == VK_RMENU) {
      lParam |= 0x1000000;
    }
    MSG msg = WinUtils::InitMSG(WM_KEYDOWN, key, lParam);
    if (i == keySequence.Length() - 1) {
      bool makeDeadCharMessage =
        gKbdLayout.IsDeadKey(key, modKeyState) && aCharacters.IsEmpty();
      nsAutoString chars(aCharacters);
      if (makeDeadCharMessage) {
        UniCharsAndModifiers deadChars =
          gKbdLayout.GetUniCharsAndModifiers(key, modKeyState);
        chars = deadChars.ToString();
        NS_ASSERTION(chars.Length() == 1,
                     "Dead char must be only one character");
      }
      if (chars.IsEmpty()) {
        OnKeyDown(msg, modKeyState, nullptr, nullptr);
      } else {
        nsFakeCharMessage fakeMsg = { chars.CharAt(0), scanCode,
                                      makeDeadCharMessage };
        OnKeyDown(msg, modKeyState, nullptr, &fakeMsg);
        for (uint32_t j = 1; j < chars.Length(); j++) {
          nsFakeCharMessage fakeMsg = { chars.CharAt(j), scanCode, false };
          MSG msg = fakeMsg.GetCharMessage(mWnd);
          NativeKey nativeKey(gKbdLayout, this, msg);
          OnChar(msg, nativeKey, modKeyState, nullptr);
        }
      }
    } else {
      OnKeyDown(msg, modKeyState, nullptr, nullptr);
    }
  }
  for (uint32_t i = keySequence.Length(); i > 0; --i) {
    uint8_t key = keySequence[i - 1].mGeneral;
    uint8_t keySpecific = keySequence[i - 1].mSpecific;
    kbdState[key] = 0; // key is up and toggled off if appropriate
    if (keySpecific) {
      kbdState[keySpecific] = 0;
    }
    ::SetKeyboardState(kbdState);
    ModifierKeyState modKeyState;
    UINT scanCode = ::MapVirtualKeyEx(argumentKeySpecific ?
                                        argumentKeySpecific : aNativeKeyCode,
                                      MAPVK_VK_TO_VSC, gKbdLayout.GetLayout());
    LPARAM lParam = static_cast<LPARAM>(scanCode << 16);
    // Add extended key flag to the lParam for right control key and right alt
    // key.
    if (keySpecific == VK_RCONTROL || keySpecific == VK_RMENU) {
      lParam |= 0x1000000;
    }
    MSG msg = WinUtils::InitMSG(WM_KEYUP, key, lParam);
    OnKeyUp(msg, modKeyState, nullptr);
  }

  // Restore old key state and layout
  ::SetKeyboardState(originalKbdState);
  gKbdLayout.LoadLayout(oldLayout, true);

  // Don't unload the layout if it's installed actually.
  for (uint32_t i = 0; i < keyboardLayoutListCount; i++) {
    if (keyboardLayoutList[i] == loadedLayout) {
      loadedLayout = 0;
      break;
    }
  }
  if (keyboardLayoutListBuff != keyboardLayoutList) {
    delete [] keyboardLayoutList;
  }
  if (loadedLayout) {
    ::UnloadKeyboardLayout(loadedLayout);
  }
  return NS_OK;
}

nsresult
nsWindow::SynthesizeNativeMouseEvent(nsIntPoint aPoint,
                                     uint32_t aNativeMessage,
                                     uint32_t aModifierFlags)
{
  ::SetCursorPos(aPoint.x, aPoint.y);

  INPUT input;
  memset(&input, 0, sizeof(input));

  input.type = INPUT_MOUSE;
  input.mi.dwFlags = aNativeMessage;
  ::SendInput(1, &input, sizeof(INPUT));

  return NS_OK;
}

nsresult
nsWindow::SynthesizeNativeMouseScrollEvent(nsIntPoint aPoint,
                                           uint32_t aNativeMessage,
                                           double aDeltaX,
                                           double aDeltaY,
                                           double aDeltaZ,
                                           uint32_t aModifierFlags,
                                           uint32_t aAdditionalFlags)
{
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

BOOL nsWindow::OnInputLangChange(HKL aHKL)
{
#ifdef KE_DEBUG
  PR_LOG(gWindowsLog, PR_LOG_ALWAYS, ("OnInputLanguageChange\n"));
#endif
  gKbdLayout.LoadLayout(aHKL);
  return false;   // always pass to child window
}

void nsWindow::OnWindowPosChanged(WINDOWPOS *wp, bool& result)
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
                 ("*** mSizeMode: nsSizeMode_Maximized\n");
        break;
      default:
          PR_LOG(gWindowsLog, PR_LOG_ALWAYS, ("*** mSizeMode: ??????\n");
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

      ::RedrawWindow(mWnd, &drect, NULL,
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

      ::RedrawWindow(mWnd, &drect, NULL,
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
        result = true;
        return;
      }
    }

    // Recalculate the width and height based on the client area for gecko events.
    if (::GetClientRect(mWnd, &r)) {
      rect.width  = r.right - r.left;
      rect.height = r.bottom - r.top;
    }
    
    // Send a gecko resize event
    result = OnResize(rect);
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
    nsTouchEvent* touchEventToSend = nullptr;
    nsTouchEvent* touchEndEventToSend = nullptr;
    nsEventStatus status;

    // Walk across the touch point array processing each contact point
    for (uint32_t i = 0; i < cInputs; i++) {
      uint32_t msg;

      if (pInputs[i].dwFlags & (TOUCHEVENTF_DOWN | TOUCHEVENTF_MOVE)) {
        // Create a standard touch event to send
        if (!touchEventToSend) {
          touchEventToSend = new nsTouchEvent(true, NS_TOUCH_MOVE, this);
          touchEventToSend->time = ::GetMessageTime();
          ModifierKeyState modifierKeyState;
          modifierKeyState.InitInputEvent(*touchEventToSend);
        }

        // Pres shell expects this event to be a NS_TOUCH_START if new contact
        // points have been added since the last event sent.
        if (pInputs[i].dwFlags & TOUCHEVENTF_DOWN) {
          touchEventToSend->message = msg = NS_TOUCH_START;
        } else {
          msg = NS_TOUCH_MOVE;
        }
      } else if (pInputs[i].dwFlags & TOUCHEVENTF_UP) {
        // Pres shell expects removed contacts points to be delivered in a
        // separate NS_TOUCH_END event containing only the contact points
        // that were removed.
        if (!touchEndEventToSend) {
          touchEndEventToSend = new nsTouchEvent(true, NS_TOUCH_END, this);
          touchEndEventToSend->time = ::GetMessageTime();
          ModifierKeyState modifierKeyState;
          modifierKeyState.InitInputEvent(*touchEndEventToSend);
        }
        msg = NS_TOUCH_END;
      } else {
        // Filter out spurious Windows events we don't understand, like palm
        // contact.
        continue;
      }

      // Setup the touch point we'll append to the touch event array
      nsPointWin touchPoint;
      touchPoint.x = TOUCH_COORD_TO_PIXEL(pInputs[i].x);
      touchPoint.y = TOUCH_COORD_TO_PIXEL(pInputs[i].y);
      touchPoint.ScreenToClient(mWnd);
      nsCOMPtr<nsIDOMTouch> touch =
        new nsDOMTouch(pInputs[i].dwID,
                       touchPoint,
                       /* radius, if known */
                       pInputs[i].dwFlags & TOUCHINPUTMASKF_CONTACTAREA ?
                         nsIntPoint(
                           TOUCH_COORD_TO_PIXEL(pInputs[i].cxContact) / 2,
                           TOUCH_COORD_TO_PIXEL(pInputs[i].cyContact) / 2) :
                         nsIntPoint(1,1),
                       /* rotation angle and force */
                       0.0f, 0.0f);

      // Append to the appropriate event
      if (msg == NS_TOUCH_START || msg == NS_TOUCH_MOVE) {
        touchEventToSend->touches.AppendElement(touch);
      } else {
        touchEndEventToSend->touches.AppendElement(touch);
      }
    }

    // Dispatch touch start and move event if we have one.
    if (touchEventToSend) {
      DispatchEvent(touchEventToSend, status);
      delete touchEventToSend;
    }

    // Dispatch touch end event if we have one.
    if (touchEndEventToSend) {
      DispatchEvent(touchEndEventToSend, status);
      delete touchEndEventToSend;
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
  // Treatment for pan events which translate into scroll events:
  if (mGesture.IsPanEvent(lParam)) {
    if ( !mGesture.ProcessPanMessage(mWnd, wParam, lParam) )
      return false; // ignore

    nsEventStatus status;

    WheelEvent wheelEvent(true, NS_WHEEL_WHEEL, this);

    ModifierKeyState modifierKeyState;
    modifierKeyState.InitInputEvent(wheelEvent);

    wheelEvent.button      = 0;
    wheelEvent.time        = ::GetMessageTime();
    wheelEvent.inputSource = nsIDOMMouseEvent::MOZ_SOURCE_TOUCH;

    bool endFeedback = true;

    if (mGesture.PanDeltaToPixelScroll(wheelEvent)) {
      DispatchEvent(&wheelEvent, status);
    }

    if (mDisplayPanFeedback) {
      mGesture.UpdatePanFeedbackX(mWnd,
                                  std::abs(RoundDown(wheelEvent.overflowDeltaX)),
                                  endFeedback);
      mGesture.UpdatePanFeedbackY(mWnd,
                                  std::abs(RoundDown(wheelEvent.overflowDeltaY)),
                                  endFeedback);
      mGesture.PanFeedbackFinalize(mWnd, endFeedback);
    }

    mGesture.CloseGestureInfoHandle((HGESTUREINFO)lParam);

    return true;
  }

  // Other gestures translate into simple gesture events:
  nsSimpleGestureEvent event(true, 0, this, 0, 0.0);
  if ( !mGesture.ProcessGestureMessage(mWnd, wParam, lParam, event) ) {
    return false; // fall through to DefWndProc
  }
  
  // Polish up and send off the new event
  ModifierKeyState modifierKeyState;
  modifierKeyState.InitInputEvent(event);
  event.button    = 0;
  event.time      = ::GetMessageTime();
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

/* static */
bool nsWindow::IsRedirectedKeyDownMessage(const MSG &aMsg)
{
  return (aMsg.message == WM_KEYDOWN || aMsg.message == WM_SYSKEYDOWN) &&
         (sRedirectedKeyDown.message == aMsg.message &&
          WinUtils::GetScanCode(sRedirectedKeyDown.lParam) ==
            WinUtils::GetScanCode(aMsg.lParam));
}

/**
 * nsWindow::OnKeyDown peeks into the message queue and pulls out
 * WM_CHAR messages for processing. During testing we don't want to
 * mess with the real message queue. Instead we pass a
 * pseudo-WM_CHAR-message using this structure, and OnKeyDown will use
 * that as if it was in the message queue, and refrain from actually
 * looking at or touching the message queue.
 */
LRESULT nsWindow::OnKeyDown(const MSG &aMsg,
                            const ModifierKeyState &aModKeyState,
                            bool *aEventDispatched,
                            nsFakeCharMessage* aFakeCharMessage)
{
  NativeKey nativeKey(gKbdLayout, this, aMsg);
  UINT virtualKeyCode = nativeKey.GetOriginalVirtualKeyCode();
  UniCharsAndModifiers inputtingChars =
    gKbdLayout.OnKeyDown(virtualKeyCode, aModKeyState);

  // Use only DOMKeyCode for XP processing.
  // Use virtualKeyCode for gKbdLayout and native processing.
  uint32_t DOMKeyCode = nativeKey.GetDOMKeyCode();

#ifdef DEBUG
  //PR_LOG(gWindowsLog, PR_LOG_ALWAYS, ("In OnKeyDown virt: %d\n", DOMKeyCode));
#endif

  static bool sRedirectedKeyDownEventPreventedDefault = false;
  bool noDefault;
  if (aFakeCharMessage || !IsRedirectedKeyDownMessage(aMsg)) {
    nsIMEContext IMEContext(mWnd);
    nsKeyEvent keydownEvent(true, NS_KEY_DOWN, this);
    keydownEvent.keyCode = DOMKeyCode;
    InitKeyEvent(keydownEvent, nativeKey, aModKeyState);
    noDefault = DispatchKeyEvent(keydownEvent, &aMsg);
    if (aEventDispatched) {
      *aEventDispatched = true;
    }

    // If IMC wasn't associated to the window but is associated it now (i.e.,
    // focus is moved from a non-editable editor to an editor by keydown
    // event handler), WM_CHAR and WM_SYSCHAR shouldn't cause first character
    // inputting if IME is opened.  But then, we should redirect the native
    // keydown message to IME.
    // However, note that if focus has been already moved to another
    // application, we shouldn't redirect the message to it because the keydown
    // message is processed by us, so, nobody shouldn't process it.
    HWND focusedWnd = ::GetFocus();
    nsIMEContext newIMEContext(mWnd);
    if (!noDefault && !aFakeCharMessage && focusedWnd && !PluginHasFocus() &&
        !IMEContext.get() && newIMEContext.get()) {
      RemoveNextCharMessage(focusedWnd);

      INPUT keyinput;
      keyinput.type = INPUT_KEYBOARD;
      keyinput.ki.wVk = aMsg.wParam;
      keyinput.ki.wScan = WinUtils::GetScanCode(aMsg.lParam);
      keyinput.ki.dwFlags = KEYEVENTF_SCANCODE;
      if (WinUtils::IsExtendedScanCode(aMsg.lParam)) {
        keyinput.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
      }
      keyinput.ki.time = 0;
      keyinput.ki.dwExtraInfo = 0;

      sRedirectedKeyDownEventPreventedDefault = noDefault;
      sRedirectedKeyDown = aMsg;

      ::SendInput(1, &keyinput, sizeof(keyinput));

      // Return here.  We shouldn't dispatch keypress event for this WM_KEYDOWN.
      // If it's needed, it will be dispatched after next (redirected)
      // WM_KEYDOWN.
      return true;
    }

    if (mOnDestroyCalled) {
      // If this was destroyed by the keydown event handler, we shouldn't
      // dispatch keypress event on this window.
      return true;
    }
  } else {
    noDefault = sRedirectedKeyDownEventPreventedDefault;
    // If this is redirected keydown message, we have dispatched the keydown
    // event already.
    if (aEventDispatched) {
      *aEventDispatched = true;
    }
  }

  ForgetRedirectedKeyDownMessage();

  // If the key was processed by IME, we shouldn't dispatch keypress event.
  if (aMsg.wParam == VK_PROCESSKEY) {
    return noDefault;
  }

  // If we won't be getting a WM_CHAR, WM_SYSCHAR or WM_DEADCHAR, synthesize a keypress
  // for almost all keys
  switch (DOMKeyCode) {
    case NS_VK_SHIFT:
    case NS_VK_CONTROL:
    case NS_VK_ALT:
    case NS_VK_CAPS_LOCK:
    case NS_VK_NUM_LOCK:
    case NS_VK_SCROLL_LOCK:
    case NS_VK_WIN:
      return noDefault;
  }

  bool isDeadKey = gKbdLayout.IsDeadKey(virtualKeyCode, aModKeyState);
  uint32_t extraFlags = (noDefault ? NS_EVENT_FLAG_NO_DEFAULT : 0);
  MSG msg;
  BOOL gotMsg = aFakeCharMessage ||
    ::PeekMessageW(&msg, mWnd, WM_KEYFIRST, WM_KEYLAST, PM_NOREMOVE | PM_NOYIELD);
  // Enter and backspace are always handled here to avoid for example the
  // confusion between ctrl-enter and ctrl-J.
  if (DOMKeyCode == NS_VK_RETURN || DOMKeyCode == NS_VK_BACK ||
      ((aModKeyState.IsControl() || aModKeyState.IsAlt() || aModKeyState.IsWin())
       && !isDeadKey && KeyboardLayout::IsPrintableCharKey(virtualKeyCode)))
  {
    // Remove a possible WM_CHAR or WM_SYSCHAR messages from the message queue.
    // They can be more than one because of:
    //  * Dead-keys not pairing with base character
    //  * Some keyboard layouts may map up to 4 characters to the single key
    bool anyCharMessagesRemoved = false;

    if (aFakeCharMessage) {
      RemoveMessageAndDispatchPluginEvent(WM_KEYFIRST, WM_KEYLAST,
                                          aFakeCharMessage);
      anyCharMessagesRemoved = true;
    } else {
      while (gotMsg && (msg.message == WM_CHAR || msg.message == WM_SYSCHAR))
      {
        PR_LOG(gWindowsLog, PR_LOG_ALWAYS,
               ("%s charCode=%d scanCode=%d\n", msg.message == WM_SYSCHAR ? 
                                                "WM_SYSCHAR" : "WM_CHAR",
                msg.wParam, HIWORD(msg.lParam) & 0xFF));
        RemoveMessageAndDispatchPluginEvent(WM_KEYFIRST, WM_KEYLAST);
        anyCharMessagesRemoved = true;

        gotMsg = ::PeekMessageW (&msg, mWnd, WM_KEYFIRST, WM_KEYLAST, PM_NOREMOVE | PM_NOYIELD);
      }
    }

    if (!anyCharMessagesRemoved && DOMKeyCode == NS_VK_BACK &&
        nsIMM32Handler::IsDoingKakuteiUndo(mWnd)) {
      NS_ASSERTION(!aFakeCharMessage,
                   "We shouldn't be touching the real msg queue");
      RemoveMessageAndDispatchPluginEvent(WM_CHAR, WM_CHAR);
    }
  }
  else if (gotMsg &&
           (aFakeCharMessage ||
            msg.message == WM_CHAR || msg.message == WM_SYSCHAR || msg.message == WM_DEADCHAR)) {
    if (aFakeCharMessage) {
      MSG msg = aFakeCharMessage->GetCharMessage(mWnd);
      if (msg.message == WM_DEADCHAR) {
        return false;
      }
#ifdef DEBUG
      if (KeyboardLayout::IsPrintableCharKey(virtualKeyCode)) {
        nsPrintfCString log(
          "virtualKeyCode=0x%02X, inputtingChar={ mChars=[ 0x%04X, 0x%04X, "
          "0x%04X, 0x%04X, 0x%04X ], mLength=%d }, wParam=0x%04X",
          virtualKeyCode, inputtingChars.mChars[0], inputtingChars.mChars[1],
          inputtingChars.mChars[2], inputtingChars.mChars[3],
          inputtingChars.mChars[4], inputtingChars.mLength, msg.wParam);
        if (!inputtingChars.mLength) {
          log.Insert("length is zero: ", 0);
          NS_ERROR(log.get());
          NS_ABORT();
        } else if (inputtingChars.mChars[0] != msg.wParam) {
          log.Insert("character mismatch: ", 0);
          NS_ERROR(log.get());
          NS_ABORT();
        }
      }
#endif // #ifdef DEBUG
      return OnChar(msg, nativeKey, aModKeyState, nullptr, extraFlags);
    }

    // If prevent default set for keydown, do same for keypress
    ::GetMessageW(&msg, mWnd, msg.message, msg.message);

    if (msg.message == WM_DEADCHAR) {
      if (!PluginHasFocus())
        return false;

      // We need to send the removed message to focused plug-in.
      DispatchPluginEvent(msg);
      return noDefault;
    }

    PR_LOG(gWindowsLog, PR_LOG_ALWAYS,
           ("%s charCode=%d scanCode=%d\n",
            msg.message == WM_SYSCHAR ? "WM_SYSCHAR" : "WM_CHAR",
            msg.wParam, HIWORD(msg.lParam) & 0xFF));

    BOOL result = OnChar(msg, nativeKey, aModKeyState, nullptr, extraFlags);
    // If a syschar keypress wasn't processed, Windows may want to
    // handle it to activate a native menu.
    if (!result && msg.message == WM_SYSCHAR)
      ::DefWindowProcW(mWnd, msg.message, msg.wParam, msg.lParam);
    return result;
  }
  else if (!aModKeyState.IsControl() && !aModKeyState.IsAlt() &&
            !aModKeyState.IsWin() &&
            KeyboardLayout::IsPrintableCharKey(virtualKeyCode)) {
    // If this is simple KeyDown event but next message is not WM_CHAR,
    // this event may not input text, so we should ignore this event.
    // See bug 314130.
    return PluginHasFocus() && noDefault;
  }

  if (isDeadKey) {
    return PluginHasFocus() && noDefault;
  }

  UniCharsAndModifiers shiftedChars;
  UniCharsAndModifiers unshiftedChars;
  uint32_t shiftedLatinChar = 0;
  uint32_t unshiftedLatinChar = 0;

  if (!KeyboardLayout::IsPrintableCharKey(virtualKeyCode)) {
    inputtingChars.Clear();
  }

  if (aModKeyState.IsControl() ^ aModKeyState.IsAlt()) {
    widget::ModifierKeyState capsLockState(
      aModKeyState.GetModifiers() & MODIFIER_CAPSLOCK);
    unshiftedChars =
      gKbdLayout.GetUniCharsAndModifiers(virtualKeyCode, capsLockState);
    capsLockState.Set(MODIFIER_SHIFT);
    shiftedChars =
      gKbdLayout.GetUniCharsAndModifiers(virtualKeyCode, capsLockState);

    // The current keyboard cannot input alphabets or numerics,
    // we should append them for Shortcut/Access keys.
    // E.g., for Cyrillic keyboard layout.
    capsLockState.Unset(MODIFIER_SHIFT);
    WidgetUtils::GetLatinCharCodeForKeyCode(DOMKeyCode,
                                            capsLockState.GetModifiers(),
                                            &unshiftedLatinChar,
                                            &shiftedLatinChar);

    // If the shiftedLatinChar isn't 0, the key code is NS_VK_[A-Z].
    if (shiftedLatinChar) {
      // If the produced characters of the key on current keyboard layout
      // are same as computed Latin characters, we shouldn't append the
      // Latin characters to alternativeCharCode.
      if (unshiftedLatinChar == unshiftedChars.mChars[0] &&
          shiftedLatinChar == shiftedChars.mChars[0]) {
        shiftedLatinChar = unshiftedLatinChar = 0;
      }
    } else if (unshiftedLatinChar) {
      // If the shiftedLatinChar is 0, the keyCode doesn't produce
      // alphabet character.  At that time, the character may be produced
      // with Shift key.  E.g., on French keyboard layout, NS_VK_PERCENT
      // key produces LATIN SMALL LETTER U WITH GRAVE (U+00F9) without
      // Shift key but with Shift key, it produces '%'.
      // If the unshiftedLatinChar is produced by the key on current
      // keyboard layout, we shouldn't append it to alternativeCharCode.
      if (unshiftedLatinChar == unshiftedChars.mChars[0] ||
          unshiftedLatinChar == shiftedChars.mChars[0]) {
        unshiftedLatinChar = 0;
      }
    }

    // If the charCode is not ASCII character, we should replace the
    // charCode with ASCII character only when Ctrl is pressed.
    // But don't replace the charCode when the charCode is not same as
    // unmodified characters. In such case, Ctrl is sometimes used for a
    // part of character inputting key combination like Shift.
    if (aModKeyState.IsControl()) {
      uint32_t ch =
        aModKeyState.IsShift() ? shiftedLatinChar : unshiftedLatinChar;
      if (ch &&
          (!inputtingChars.mLength ||
           inputtingChars.UniCharsCaseInsensitiveEqual(
             aModKeyState.IsShift() ? shiftedChars : unshiftedChars))) {
        inputtingChars.Clear();
        inputtingChars.Append(ch, aModKeyState.GetModifiers());
      }
    }
  }

  if (inputtingChars.mLength ||
      shiftedChars.mLength || unshiftedChars.mLength) {
    uint32_t num = NS_MAX(inputtingChars.mLength,
                          NS_MAX(shiftedChars.mLength, unshiftedChars.mLength));
    uint32_t skipUniChars = num - inputtingChars.mLength;
    uint32_t skipShiftedChars = num - shiftedChars.mLength;
    uint32_t skipUnshiftedChars = num - unshiftedChars.mLength;
    UINT keyCode = !inputtingChars.mLength ? DOMKeyCode : 0;
    for (uint32_t cnt = 0; cnt < num; cnt++) {
      uint16_t uniChar, shiftedChar, unshiftedChar;
      uniChar = shiftedChar = unshiftedChar = 0;
      ModifierKeyState modKeyState(aModKeyState);
      if (skipUniChars <= cnt) {
        if (cnt - skipUniChars  < inputtingChars.mLength) {
          // If key in combination with Alt and/or Ctrl produces a different
          // character than without them then do not report these flags
          // because it is separate keyboard layout shift state. If dead-key
          // and base character does not produce a valid composite character
          // then both produced dead-key character and following base
          // character may have different modifier flags, too.
          modKeyState.Unset(MODIFIER_SHIFT | MODIFIER_CONTROL | MODIFIER_ALT |
                            MODIFIER_ALTGRAPH | MODIFIER_CAPSLOCK);
          modKeyState.Set(inputtingChars.mModifiers[cnt - skipUniChars]);
        }
        uniChar = inputtingChars.mChars[cnt - skipUniChars];
      }
      if (skipShiftedChars <= cnt)
        shiftedChar = shiftedChars.mChars[cnt - skipShiftedChars];
      if (skipUnshiftedChars <= cnt)
        unshiftedChar = unshiftedChars.mChars[cnt - skipUnshiftedChars];
      nsAutoTArray<nsAlternativeCharCode, 5> altArray;

      if (shiftedChar || unshiftedChar) {
        nsAlternativeCharCode chars(unshiftedChar, shiftedChar);
        altArray.AppendElement(chars);
      }
      if (cnt == num - 1) {
        if (unshiftedLatinChar || shiftedLatinChar) {
          nsAlternativeCharCode chars(unshiftedLatinChar, shiftedLatinChar);
          altArray.AppendElement(chars);
        }

        // Typically, following virtual keycodes are used for a key which can
        // input the character.  However, these keycodes are also used for
        // other keys on some keyboard layout.  E.g., in spite of Shift+'1'
        // inputs '+' on Thai keyboard layout, a key which is at '=/+'
        // key on ANSI keyboard layout is VK_OEM_PLUS.  Native applications
        // handle it as '+' key if Ctrl key is pressed.
        PRUnichar charForOEMKeyCode = 0;
        switch (virtualKeyCode) {
          case VK_OEM_PLUS:   charForOEMKeyCode = '+'; break;
          case VK_OEM_COMMA:  charForOEMKeyCode = ','; break;
          case VK_OEM_MINUS:  charForOEMKeyCode = '-'; break;
          case VK_OEM_PERIOD: charForOEMKeyCode = '.'; break;
        }
        if (charForOEMKeyCode &&
            charForOEMKeyCode != unshiftedChars.mChars[0] &&
            charForOEMKeyCode != shiftedChars.mChars[0] &&
            charForOEMKeyCode != unshiftedLatinChar &&
            charForOEMKeyCode != shiftedLatinChar) {
          nsAlternativeCharCode OEMChars(charForOEMKeyCode, charForOEMKeyCode);
          altArray.AppendElement(OEMChars);
        }
      }

      nsKeyEvent keypressEvent(true, NS_KEY_PRESS, this);
      keypressEvent.flags |= extraFlags;
      keypressEvent.charCode = uniChar;
      keypressEvent.alternativeCharCodes.AppendElements(altArray);
      InitKeyEvent(keypressEvent, nativeKey, modKeyState);
      DispatchKeyEvent(keypressEvent, nullptr);
    }
  } else {
    nsKeyEvent keypressEvent(true, NS_KEY_PRESS, this);
    keypressEvent.flags |= extraFlags;
    keypressEvent.keyCode = DOMKeyCode;
    InitKeyEvent(keypressEvent, nativeKey, aModKeyState);
    DispatchKeyEvent(keypressEvent, nullptr);
  }

  return noDefault;
}

// OnKeyUp
LRESULT nsWindow::OnKeyUp(const MSG &aMsg,
                          const ModifierKeyState &aModKeyState,
                          bool *aEventDispatched)
{
  // NOTE: VK_PROCESSKEY never comes with WM_KEYUP
  PR_LOG(gWindowsLog, PR_LOG_ALWAYS,
         ("nsWindow::OnKeyUp wParam(VK)=%d\n", aMsg.wParam));

  if (aEventDispatched)
    *aEventDispatched = true;
  nsKeyEvent keyupEvent(true, NS_KEY_UP, this);
  NativeKey nativeKey(gKbdLayout, this, aMsg);
  keyupEvent.keyCode = nativeKey.GetDOMKeyCode();
  InitKeyEvent(keyupEvent, nativeKey, aModKeyState);
  return DispatchKeyEvent(keyupEvent, &aMsg);
}

// OnChar
LRESULT nsWindow::OnChar(const MSG &aMsg,
                         const NativeKey& aNativeKey,
                         const ModifierKeyState &aModKeyState,
                         bool *aEventDispatched, uint32_t aFlags)
{
  // ignore [shift+]alt+space so the OS can handle it
  if (aModKeyState.IsAlt() && !aModKeyState.IsControl() &&
      IS_VK_DOWN(NS_VK_SPACE)) {
    return FALSE;
  }

  uint32_t charCode = aMsg.wParam;
  // Ignore Ctrl+Enter (bug 318235)
  if (aModKeyState.IsControl() && charCode == 0xA) {
    return FALSE;
  }

  // WM_CHAR with Control and Alt (== AltGr) down really means a normal character
  ModifierKeyState modKeyState(aModKeyState);
  if (modKeyState.IsAlt() && modKeyState.IsControl()) {
    modKeyState.Unset(MODIFIER_ALT | MODIFIER_CONTROL);
  }

  if (nsIMM32Handler::IsComposingOn(this)) {
    ResetInputState();
  }

  wchar_t uniChar;
  // Ctrl+A Ctrl+Z, see Programming Windows 3.1 page 110 for details
  if (modKeyState.IsControl() && charCode <= 0x1A) {
    // need to account for shift here.  bug 16486
    if (modKeyState.IsShift()) {
      uniChar = charCode - 1 + 'A';
    } else {
      uniChar = charCode - 1 + 'a';
    }
  } else if (modKeyState.IsControl() && charCode <= 0x1F) {
    // Fix for 50255 - <ctrl><[> and <ctrl><]> are not being processed.
    // also fixes ctrl+\ (x1c), ctrl+^ (x1e) and ctrl+_ (x1f)
    // for some reason the keypress handler need to have the uniChar code set
    // with the addition of a upper case A not the lower case.
    uniChar = charCode - 1 + 'A';
  } else { // 0x20 - SPACE, 0x3D - EQUALS
    if (charCode < 0x20 || (charCode == 0x3D && modKeyState.IsControl())) {
      uniChar = 0;
    } else {
      uniChar = charCode;
    }
  }

  // Keep the characters unshifted for shortcuts and accesskeys and make sure
  // that numbers are always passed as such (among others: bugs 50255 and 351310)
  if (uniChar && (modKeyState.IsControl() || modKeyState.IsAlt())) {
    UINT virtualKeyCode = ::MapVirtualKeyEx(aNativeKey.GetScanCode(),
                                            MAPVK_VSC_TO_VK,
                                            gKbdLayout.GetLayout());
    UINT unshiftedCharCode =
      virtualKeyCode >= '0' && virtualKeyCode <= '9' ? virtualKeyCode :
        modKeyState.IsShift() ? ::MapVirtualKeyEx(virtualKeyCode,
                                                  MAPVK_VK_TO_CHAR,
                                                  gKbdLayout.GetLayout()) : 0;
    // ignore diacritics (top bit set) and key mapping errors (char code 0)
    if ((INT)unshiftedCharCode > 0)
      uniChar = unshiftedCharCode;
  }

  // Fix for bug 285161 (and 295095) which was caused by the initial fix for bug 178110.
  // When pressing (alt|ctrl)+char, the char must be lowercase unless shift is
  // pressed too.
  if (!modKeyState.IsShift() &&
      (aModKeyState.IsAlt() || aModKeyState.IsControl())) {
    uniChar = towlower(uniChar);
  }

  nsKeyEvent keypressEvent(true, NS_KEY_PRESS, this);
  keypressEvent.flags |= aFlags;
  keypressEvent.charCode = uniChar;
  if (!keypressEvent.charCode) {
    keypressEvent.keyCode = aNativeKey.GetDOMKeyCode();
  }
  InitKeyEvent(keypressEvent, aNativeKey, modKeyState);
  bool result = DispatchKeyEvent(keypressEvent, &aMsg);
  if (aEventDispatched)
    *aEventDispatched = true;
  return result;
}

void
nsWindow::SetupKeyModifiersSequence(nsTArray<KeyPair>* aArray, uint32_t aModifiers)
{
  for (uint32_t i = 0; i < ArrayLength(sModifierKeyMap); ++i) {
    const uint32_t* map = sModifierKeyMap[i];
    if (aModifiers & map[0]) {
      aArray->AppendElement(KeyPair(map[1], map[2]));
    }
  }
}

static BOOL WINAPI EnumFirstChild(HWND hwnd, LPARAM lParam)
{
  *((HWND*)lParam) = hwnd;
  return FALSE;
}

static void InvalidatePluginAsWorkaround(nsWindow *aWindow, const nsIntRect &aRect)
{
  aWindow->Invalidate(aRect);

  // XXX - Even more evil workaround!! See bug 762948, flash's bottom
  // level sandboxed window doesn't seem to get our invalidate. We send
  // an invalidate to it manually. This is totally specialized for this
  // bug, for other child window structures this will just be a more or
  // less bogus invalidate but since that should not have any bad
  // side-effects this will have to do for now.
  HWND current = (HWND)aWindow->GetNativeData(NS_NATIVE_WINDOW);

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
    rect.left   = aRect.x;
    rect.top    = aRect.y;
    rect.right  = aRect.XMost();
    rect.bottom = aRect.YMost();

    ::InvalidateRect(next, &rect, FALSE);
  }
}

nsresult
nsWindow::ConfigureChildren(const nsTArray<Configuration>& aConfigurations)
{
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
          GetLayerManager()->GetBackendType() != LAYERS_BASIC) {
        // XXX - Workaround for Bug 587508. This will invalidate the part of the
        // plugin window that might be touched by moving content somehow. The
        // underlying problem should be found and fixed!
        nsIntRegion r;
        r.Sub(bounds, configuration.mBounds);
        r.MoveBy(-bounds.x,
                 -bounds.y);
        nsIntRect toInvalidate = r.GetBounds();

        InvalidatePluginAsWorkaround(w, toInvalidate);
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
  if (!buf.SetLength(size))
    return NULL;
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
  return ::ExtCreateRegion(NULL, buf.Length(), data);
}

static void
ArrayFromRegion(const nsIntRegion& aRegion, nsTArray<nsIntRect>& aRects)
{
  const nsIntRect* r;
  for (nsIntRegionRectIterator iter(aRegion); (r = iter.Next());) {
    aRects.AppendElement(*r);
  }
}

nsresult
nsWindow::SetWindowClipRegion(const nsTArray<nsIntRect>& aRects,
                              bool aIntersectWithExisting)
{
  if (!aIntersectWithExisting) {
    if (!StoreWindowClipRegion(aRects))
      return NS_OK;
  } else {
    // In this case still early return if nothing changed.
    if (mClipRects && mClipRectCount == aRects.Length() &&
        memcmp(mClipRects,
               aRects.Elements(),
               sizeof(nsIntRect)*mClipRectCount) == 0) {
      return NS_OK;
    }

    // get current rects
    nsTArray<nsIntRect> currentRects;
    GetWindowClipRegion(&currentRects);
    // create region from them
    nsIntRegion currentRegion = RegionFromArray(currentRects);
    // create region from new rects
    nsIntRegion newRegion = RegionFromArray(aRects);
    // intersect regions
    nsIntRegion intersection;
    intersection.And(currentRegion, newRegion);
    // create int rect array from intersection
    nsTArray<nsIntRect> rects;
    ArrayFromRegion(intersection, rects);
    // store
    if (!StoreWindowClipRegion(rects))
      return NS_OK;
  }

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
  if(mWindowType == eWindowType_plugin) {
    if(NULLREGION == ::CombineRgn(dest, dest, dest, RGN_OR)) {
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
  nsCOMPtr<nsIWidget> rollupWidget = rollupListener->GetRollupWidget();
  if ( this == rollupWidget ) {
    if ( rollupListener )
      rollupListener->Rollup(0, nullptr);
    CaptureRollupEvents(nullptr, false);
  }

  // Restore the IM context.
  AssociateDefaultIMC(true);

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
    mBrush = NULL;
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
  mWnd = NULL;
}

// OnMove
bool nsWindow::OnMove(int32_t aX, int32_t aY)
{
  mBounds.x = aX;
  mBounds.y = aY;

  return mWidgetListener ? mWidgetListener->WindowMoved(this, aX, aY) : false;
}

// Send a resize message to the listener
bool nsWindow::OnResize(nsIntRect &aWindowRect)
{
#ifdef CAIRO_HAS_D2D_SURFACE
  if (mD2DWindowSurface) {
    mD2DWindowSurface = NULL;
    Invalidate();
  }
#endif

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
nsWindow::AllowD3D9Callback(nsWindow *aWindow)
{
  if (aWindow->mLayerManager) {
    aWindow->mLayerManager->Destroy();
    aWindow->mLayerManager = NULL;
  }
}

void
nsWindow::AllowD3D9WithReinitializeCallback(nsWindow *aWindow)
{
  if (aWindow->mLayerManager) {
    aWindow->mLayerManager->Destroy();
    aWindow->mLayerManager = NULL;
    (void) aWindow->GetLayerManager();
  }
}

void
nsWindow::StartAllowingD3D9(bool aReinitialize)
{
  sAllowD3D9 = true;

  LayerManagerPrefs prefs;
  GetLayerManagerPrefs(&prefs);
  if (prefs.mDisableAcceleration) {
    // The guarantee here is, if there's *any* chance that after we
    // throw out our layer managers we'd create at least one new,
    // accelerated one, we *will* throw out all the current layer
    // managers.  We early-return here because currently, if
    // |disableAcceleration|, we will always use basic managers and
    // it's a waste to recreate them.
    //
    // NB: the above implies that it's eminently possible for us to
    // skip this early return but still recreate basic managers.
    // That's OK.  It's *not* OK to take this early return when we
    // *might* have created an accelerated manager.
    return;
  }

  if (aReinitialize) {
    EnumAllWindows(AllowD3D9WithReinitializeCallback);
  } else {
    EnumAllWindows(AllowD3D9Callback);
  }
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
            if (status == nsIGfxInfo::FEATURE_NO_INFO || prefs.mForceAcceleration)
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

NS_IMETHODIMP nsWindow::ResetInputState()
{
#ifdef DEBUG_KBSTATE
  PR_LOG(gWindowsLog, PR_LOG_ALWAYS, ("ResetInputState\n"));
#endif

#ifdef NS_ENABLE_TSF
  nsTextStore::CommitComposition(false);
#endif //NS_ENABLE_TSF

  nsIMM32Handler::CommitComposition(this);
  return NS_OK;
}

NS_IMETHODIMP_(void)
nsWindow::SetInputContext(const InputContext& aContext,
                          const InputContextAction& aAction)
{
#ifdef NS_ENABLE_TSF
  nsTextStore::SetInputContext(aContext);
#endif //NS_ENABLE_TSF
  if (nsIMM32Handler::IsComposing()) {
    ResetInputState();
  }
  void* nativeIMEContext = mInputContext.mNativeIMEContext;
  mInputContext = aContext;
  mInputContext.mNativeIMEContext = nullptr;
  bool enable = (mInputContext.mIMEState.mEnabled == IMEState::ENABLED ||
                 mInputContext.mIMEState.mEnabled == IMEState::PLUGIN);

  AssociateDefaultIMC(enable);

  if (enable) {
    nsIMEContext IMEContext(mWnd);
    mInputContext.mNativeIMEContext = static_cast<void*>(IMEContext.get());
  }
  // Restore the latest associated context when we cannot get actual context.
  if (!mInputContext.mNativeIMEContext) {
    mInputContext.mNativeIMEContext = nativeIMEContext;
  }

  if (enable &&
      mInputContext.mIMEState.mOpen != IMEState::DONT_CHANGE_OPEN_STATE) {
    bool open = (mInputContext.mIMEState.mOpen == IMEState::OPEN);
#ifdef NS_ENABLE_TSF
    nsTextStore::SetIMEOpenState(open);
#endif //NS_ENABLE_TSF
    nsIMEContext IMEContext(mWnd);
    if (IMEContext.IsValid()) {
      ::ImmSetOpenStatus(IMEContext.get(), open);
    }
  }
}

NS_IMETHODIMP_(InputContext)
nsWindow::GetInputContext()
{
  mInputContext.mIMEState.mOpen = IMEState::CLOSED;
  switch (mInputContext.mIMEState.mEnabled) {
    case IMEState::ENABLED:
    case IMEState::PLUGIN: {
      nsIMEContext IMEContext(mWnd);
      if (IMEContext.IsValid()) {
        mInputContext.mIMEState.mOpen =
          ::ImmGetOpenStatus(IMEContext.get()) ? IMEState::OPEN :
                                                 IMEState::CLOSED;
      }
#ifdef NS_ENABLE_TSF
      if (mInputContext.mIMEState.mOpen == IMEState::CLOSED &&
          nsTextStore::GetIMEOpenState()) {
        mInputContext.mIMEState.mOpen = IMEState::OPEN;
      }
#endif //NS_ENABLE_TSF
    }
  }
  return mInputContext;
}

NS_IMETHODIMP nsWindow::CancelIMEComposition()
{
#ifdef DEBUG_KBSTATE
  PR_LOG(gWindowsLog, PR_LOG_ALWAYS, ("CancelIMEComposition\n"));
#endif 

#ifdef NS_ENABLE_TSF
  nsTextStore::CommitComposition(true);
#endif //NS_ENABLE_TSF

  nsIMM32Handler::CancelComposition(this);
  return NS_OK;
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

#ifdef NS_ENABLE_TSF
NS_IMETHODIMP
nsWindow::OnIMEFocusChange(bool aFocus)
{
  nsresult rv = nsTextStore::OnFocusChange(aFocus, this,
                                           mInputContext.mIMEState.mEnabled);
  if (rv == NS_ERROR_NOT_AVAILABLE)
    rv = NS_OK; // TSF is not enabled, maybe.
  return rv;
}

NS_IMETHODIMP
nsWindow::OnIMETextChange(uint32_t aStart,
                          uint32_t aOldEnd,
                          uint32_t aNewEnd)
{
  return nsTextStore::OnTextChange(aStart, aOldEnd, aNewEnd);
}

NS_IMETHODIMP
nsWindow::OnIMESelectionChange(void)
{
  return nsTextStore::OnSelectionChange();
}

nsIMEUpdatePreference
nsWindow::GetIMEUpdatePreference()
{
  return nsTextStore::GetIMEUpdatePreference();
}

#endif //NS_ENABLE_TSF

bool nsWindow::AssociateDefaultIMC(bool aAssociate)
{
  nsIMEContext IMEContext(mWnd);

  if (aAssociate) {
    BOOL ret = ::ImmAssociateContextEx(mWnd, NULL, IACE_DEFAULT);
#ifdef DEBUG
    // Note that if IME isn't available with current keyboard layout,
    // IMM might not be installed on the system such as English Windows.
    // On such system, IMM APIs always fail.
    NS_ASSERTION(ret || !nsIMM32Handler::IsIMEAvailable(),
                 "ImmAssociateContextEx failed to restore default IMC");
    if (ret) {
      nsIMEContext newIMEContext(mWnd);
      NS_ASSERTION(!IMEContext.get() || newIMEContext.get() == IMEContext.get(),
                   "Unknown IMC had been associated");
    }
#endif
    return ret && !IMEContext.get();
  }

  if (mOnDestroyCalled) {
    // If OnDestroy() has been called, we shouldn't disassociate the default
    // IMC at destroying the window.
    return false;
  }

  if (!IMEContext.get()) {
    return false; // already disassociated
  }

  BOOL ret = ::ImmAssociateContextEx(mWnd, NULL, 0);
  NS_ASSERTION(ret, "ImmAssociateContextEx failed to disassociate the IMC");
  return ret != FALSE;
}

#ifdef ACCESSIBILITY

#ifdef DEBUG_WMGETOBJECT
#define NS_LOG_WMGETOBJECT_WNDACC(aWnd)                                        \
  a11y::Accessible* acc = aWnd ? aWind->GetAccessible() : nullptr;             \
  PR_LOG(gWindowsLog, PR_LOG_ALWAYS, ("     acc: %p", acc));                   \
  if (acc) {                                                                   \
    nsAutoString name;                                                         \
    acc->GetName(name);                                                        \
    PR_LOG(gWindowsLog, PR_LOG_ALWAYS,                                         \
           (", accname: %s", NS_ConvertUTF16toUTF8(name).get()));              \
    nsCOMPtr<nsIAccessibleDocument> doc = do_QueryObject(acc);                 \
    void *hwnd = nullptr;                                                      \
    doc->GetWindowHandle(&hwnd);                                               \
    PR_LOG(gWindowsLog, PR_LOG_ALWAYS, (", acc hwnd: %d", hwnd));              \
  }

#define NS_LOG_WMGETOBJECT_THISWND                                             \
{                                                                              \
  PR_LOG(gWindowsLog, PR_LOG_ALWAYS,                                           \
         ("\n*******Get Doc Accessible*******\nOrig Window: "));               \
  PR_LOG(gWindowsLog, PR_LOG_ALWAYS,                                           \
         ("\n  {\n     HWND: %d, parent HWND: %d, wndobj: %p,\n",              \
          mWnd, ::GetParent(mWnd), this));                                     \
  NS_LOG_WMGETOBJECT_WNDACC(this)                                              \
  PR_LOG(gWindowsLog, PR_LOG_ALWAYS, ("\n  }\n"));                             \
}

#define NS_LOG_WMGETOBJECT_WND(aMsg, aHwnd)                                    \
{                                                                              \
  nsWindow* wnd = WinUtils::GetNSWindowPtr(aHwnd);                             \
  PR_LOG(gWindowsLog, PR_LOG_ALWAYS,                                           \
         ("Get " aMsg ":\n  {\n     HWND: %d, parent HWND: %d, wndobj: %p,\n", \
          aHwnd, ::GetParent(aHwnd), wnd));                                    \
  NS_LOG_WMGETOBJECT_WNDACC(wnd);                                              \
  PR_LOG(gWindowsLog, PR_LOG_ALWAYS, ("\n }\n"));                              \
}
#else
#define NS_LOG_WMGETOBJECT_THISWND
#define NS_LOG_WMGETOBJECT_WND(aMsg, aHwnd)
#endif // DEBUG_WMGETOBJECT

a11y::Accessible*
nsWindow::GetRootAccessible()
{
  // If the pref was ePlatformIsDisabled, return null here, disabling a11y.
  if (a11y::PlatformDisabledState() == a11y::ePlatformIsDisabled)
    return nullptr;

  if (mInDtor || mOnDestroyCalled || mWindowType == eWindowType_invisible) {
    return nullptr;
  }

  NS_LOG_WMGETOBJECT_THISWND
  NS_LOG_WMGETOBJECT_WND("This Window", mWnd);

  return GetAccessible();
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

#ifdef CAIRO_HAS_D2D_SURFACE
  if (gfxWindowsPlatform::GetPlatform()->GetRenderMode() ==
      gfxWindowsPlatform::RENDER_DIRECT2D) {
    nsRefPtr<gfxD2DSurface> newSurface =
      new gfxD2DSurface(gfxIntSize(aNewWidth, aNewHeight), gfxASurface::ImageFormatARGB32);
    mTransparentSurface = newSurface;
    mMemoryDC = nullptr;
  } else
#endif
  {
    nsRefPtr<gfxWindowsSurface> newSurface =
      new gfxWindowsSurface(gfxIntSize(aNewWidth, aNewHeight), gfxASurface::ImageFormatARGB32);
    mTransparentSurface = newSurface;
    mMemoryDC = newSurface->GetDC();
  }
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
    mMemoryDC = NULL;
  }
}

void nsWindow::ClearTranslucentWindow()
{
  if (mTransparentSurface) {
    nsRefPtr<gfxContext> thebesContext = new gfxContext(mTransparentSurface);
    thebesContext->SetOperator(gfxContext::OPERATOR_CLEAR);
    thebesContext->Paint();
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

#ifdef CAIRO_HAS_D2D_SURFACE
  if (gfxWindowsPlatform::GetPlatform()->GetRenderMode() ==
      gfxWindowsPlatform::RENDER_DIRECT2D) {
    mMemoryDC = static_cast<gfxD2DSurface*>(mTransparentSurface.get())->
      GetDC(true);
  }
#endif
  // perform the alpha blend
  bool updateSuccesful = 
    ::UpdateLayeredWindow(hWnd, NULL, (POINT*)&winRect, &winSize, mMemoryDC, &srcPos, 0, &bf, ULW_ALPHA);

#ifdef CAIRO_HAS_D2D_SURFACE
  if (gfxWindowsPlatform::GetPlatform()->GetRenderMode() ==
      gfxWindowsPlatform::RENDER_DIRECT2D) {
    nsIntRect r(0, 0, 0, 0);
    static_cast<gfxD2DSurface*>(mTransparentSurface.get())->ReleaseDC(&r);
  }
#endif

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
    sHookTimerId = ::SetTimer(NULL, 0, 0, (TIMERPROC)HookTimerForPopups);
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
    while (gMSGFEvents[inx].mId != code && gMSGFEvents[inx].mStr != NULL) {
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
          if (static_cast<nsWindow*>(mozWin)->mWindowType == eWindowType_plugin)
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
    sMsgFilterHook = SetWindowsHookEx(WH_MSGFILTER, MozSpecialMsgFilter, NULL, GetCurrentThreadId());
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
    sCallProcHook  = SetWindowsHookEx(WH_CALLWNDPROC, MozSpecialWndProc, NULL, GetCurrentThreadId());
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
    sCallMouseHook  = SetWindowsHookEx(WH_MOUSE, MozSpecialMouseProc, NULL, GetCurrentThreadId());
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
    sCallProcHook = NULL;
  }

  if (sMsgFilterHook) {
    DISPLAY_NMM_PRT("***** Unhooking sMsgFilterHook!\n");
    if (!::UnhookWindowsHookEx(sMsgFilterHook)) {
      DISPLAY_NMM_PRT("***** UnhookWindowsHookEx failed for sMsgFilterHook!\n");
    }
    sMsgFilterHook = NULL;
  }

  if (sCallMouseHook) {
    DISPLAY_NMM_PRT("***** Unhooking sCallMouseHook!\n");
    if (!::UnhookWindowsHookEx(sCallMouseHook)) {
      DISPLAY_NMM_PRT("***** UnhookWindowsHookEx failed for sCallMouseHook!\n");
    }
    sCallMouseHook = NULL;
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
    // if the window is NULL then we need to use the ID to kill the timer
    BOOL status = ::KillTimer(NULL, sHookTimerId);
    NS_ASSERTION(status, "Hook Timer was not killed.");
    sHookTimerId = 0;
  }

  if (sRollupMsgId != 0) {
    // Note: DealWithPopups does the check to make sure that the rollup widget is set.
    LRESULT popupHandlingResult;
    nsAutoRollup autoRollup;
    DealWithPopups(sRollupMsgWnd, sRollupMsgId, 0, 0, &popupHandlingResult);
    sRollupMsgId = 0;
    sRollupMsgWnd = NULL;
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
#ifdef CAIRO_HAS_D2D_SURFACE
    mD2DWindowSurface = nullptr;
#endif
    if (mLayerManager &&
        mLayerManager->GetBackendType() == LAYERS_BASIC) {
      static_cast<BasicLayerManager*>(mLayerManager.get())->
        ClearCachedResources();
    }
    ::EnumChildWindows(mWnd, nsWindow::ClearResourcesCallback, 0);
}

static bool IsDifferentThreadWindow(HWND aWnd)
{
  return ::GetCurrentThreadId() != ::GetWindowThreadProcessId(aWnd, NULL);
}

bool
nsWindow::EventIsInsideWindow(UINT Msg, nsWindow* aWindow)
{
  RECT r;

  if (Msg == WM_ACTIVATEAPP)
    // don't care about activation/deactivation
    return false;

  ::GetWindowRect(aWindow->mWnd, &r);
  DWORD pos = ::GetMessagePos();
  POINT mp;
  mp.x = GET_X_LPARAM(pos);
  mp.y = GET_Y_LPARAM(pos);

  // was the event inside this window?
  return (bool) PtInRect(&r, mp);
}

// Handle events that may cause a popup (combobox, XPMenu, etc) to need to rollup.
BOOL
nsWindow::DealWithPopups(HWND inWnd, UINT inMsg, WPARAM inWParam, LPARAM inLParam, LRESULT* outResult)
{
  nsIRollupListener* rollupListener = nsBaseWidget::GetActiveRollupListener();
  nsCOMPtr<nsIWidget> rollupWidget = rollupListener->GetRollupWidget();
  if (rollupWidget && ::IsWindowVisible(inWnd)) {

    inMsg = WinUtils::GetNativeMessage(inMsg);
    if (inMsg == WM_LBUTTONDOWN || inMsg == WM_RBUTTONDOWN || inMsg == WM_MBUTTONDOWN ||
        inMsg == WM_MOUSEWHEEL || inMsg == WM_MOUSEHWHEEL || inMsg == WM_ACTIVATE ||
        (inMsg == WM_KILLFOCUS && IsDifferentThreadWindow((HWND)inWParam)) ||
        inMsg == WM_NCRBUTTONDOWN ||
        inMsg == WM_MOVING ||
        inMsg == WM_SIZING ||
        inMsg == WM_NCLBUTTONDOWN ||
        inMsg == WM_NCMBUTTONDOWN ||
        inMsg == WM_MOUSEACTIVATE ||
        inMsg == WM_ACTIVATEAPP ||
        inMsg == WM_MENUSELECT)
    {
      // Rollup if the event is outside the popup.
      bool rollup = !nsWindow::EventIsInsideWindow(inMsg, (nsWindow*)(rollupWidget.get()));

      if (rollup && (inMsg == WM_MOUSEWHEEL || inMsg == WM_MOUSEHWHEEL))
      {
        rollup = rollupListener->ShouldRollupOnMouseWheelEvent();
        *outResult = true;
      }

      // If we're dealing with menus, we probably have submenus and we don't
      // want to rollup if the click is in a parent menu of the current submenu.
      uint32_t popupsToRollup = UINT32_MAX;
      if (rollup) {
        nsAutoTArray<nsIWidget*, 5> widgetChain;
        uint32_t sameTypeCount = rollupListener->GetSubmenuWidgetChain(&widgetChain);
        for ( uint32_t i = 0; i < widgetChain.Length(); ++i ) {
          nsIWidget* widget = widgetChain[i];
          if ( nsWindow::EventIsInsideWindow(inMsg, (nsWindow*)widget) ) {
            // don't roll up if the mouse event occurred within a menu of the
            // same type. If the mouse event occurred in a menu higher than
            // that, roll up, but pass the number of popups to Rollup so
            // that only those of the same type close up.
            if (i < sameTypeCount) {
              rollup = false;
            }
            else {
              popupsToRollup = sameTypeCount;
            }
            break;
          }
        } // foreach parent menu widget
      }

      if (inMsg == WM_MOUSEACTIVATE && popupsToRollup == UINT32_MAX) {
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
            rollup = rollupListener->ShouldRollupOnMouseActivate();
            if (!rollup)
            {
              *outResult = MA_NOACTIVATE;
              return true;
            }
          }
        }
      }
      // if we've still determined that we should still rollup everything, do it.
      else if (rollup) {
        // only need to deal with the last rollup for left mouse down events.
        NS_ASSERTION(!mLastRollup, "mLastRollup is null");
        bool consumeRollupEvent =
          rollupListener->Rollup(popupsToRollup, inMsg == WM_LBUTTONDOWN ? &mLastRollup : nullptr);
        NS_IF_ADDREF(mLastRollup);

        // Tell hook to stop processing messages
        sProcessHook = false;
        sRollupMsgId = 0;
        sRollupMsgWnd = NULL;

        // return TRUE tells Windows that the event is consumed,
        // false allows the event to be dispatched
        //
        // So if we are NOT supposed to be consuming events, let it go through
        if (consumeRollupEvent && inMsg != WM_RBUTTONDOWN) {
          *outResult = MA_ACTIVATE;

          // However, don't activate panels
          if (inMsg == WM_MOUSEACTIVATE) {
            nsWindow* activateWindow = WinUtils::GetNSWindowPtr(inWnd);
            if (activateWindow) {
              nsWindowType wintype;
              activateWindow->GetWindowType(wintype);
              if (wintype == eWindowType_popup && activateWindow->PopupType() == ePopupTypePanel) {
                *outResult = popupsToRollup != UINT32_MAX ? MA_NOACTIVATEANDEAT : MA_NOACTIVATE;
              }
            }
          }
          return TRUE;
        }
        // if we are only rolling up some popups, don't activate and don't let
        // the event go through. This prevents clicks menus higher in the
        // chain from opening when a context menu is open
        if (popupsToRollup != UINT32_MAX && inMsg == WM_MOUSEACTIVATE) {
          *outResult = MA_NOACTIVATEANDEAT;
          return TRUE;
        }
      }
    } // if event that might trigger a popup to rollup
  } // if rollup listeners registered

  return FALSE;
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
