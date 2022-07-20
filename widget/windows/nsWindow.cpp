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
#include "gfxEnv.h"
#include "gfxPlatform.h"

#include "mozilla/AppShutdown.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/PreXULSkeletonUI.h"
#include "mozilla/Logging.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/MiscEvents.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/PresShell.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/SwipeTracker.h"
#include "mozilla/TouchEvents.h"
#include "mozilla/TimeStamp.h"

#include "mozilla/ipc/MessageChannel.h"
#include <algorithm>
#include <limits>

#include "nsWindow.h"
#include "nsAppRunner.h"

#include <shellapi.h>
#include <windows.h>
#include <wtsapi32.h>
#include <process.h>
#include <commctrl.h>
#include <dbt.h>
#include <unknwn.h>
#include <psapi.h>
#include <rpc.h>
#include <propvarutil.h>
#include <propkey.h>

#include "mozilla/Logging.h"
#include "prtime.h"
#include "prenv.h"

#include "mozilla/WidgetTraceEvent.h"
#include "nsContentUtils.h"
#include "nsISupportsPrimitives.h"
#include "nsITheme.h"
#include "nsIObserverService.h"
#include "nsIScreenManager.h"
#include "imgIContainer.h"
#include "nsIFile.h"
#include "nsIRollupListener.h"
#include "nsIClipboard.h"
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
#include "nsWidgetsCID.h"
#include "nsTHashtable.h"
#include "nsHashKeys.h"
#include "nsString.h"
#include "mozilla/Components.h"
#include "nsNativeThemeWin.h"
#include "nsWindowsDllInterceptor.h"
#include "nsLayoutUtils.h"
#include "nsView.h"
#include "nsWindowGfx.h"
#include "gfxWindowsPlatform.h"
#include "gfxDWriteFonts.h"
#include "Layers.h"
#include "nsPrintfCString.h"
#include "mozilla/Preferences.h"
#include "SystemTimeConverter.h"
#include "WinTaskbar.h"
#include "WidgetUtils.h"
#include "WinContentSystemParameters.h"
#include "WinWindowOcclusionTracker.h"
#include "nsIWidgetListener.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/MouseEventBinding.h"
#include "mozilla/dom/Touch.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/GPUProcessManager.h"
#include "mozilla/intl/LocaleService.h"
#include "mozilla/layers/WebRenderLayerManager.h"
#include "mozilla/WindowsVersion.h"
#include "mozilla/TextEvents.h"  // For WidgetKeyboardEvent
#include "mozilla/TextEventDispatcherListener.h"
#include "mozilla/widget/nsAutoRollup.h"
#include "mozilla/widget/PlatformWidgetTypes.h"
#include "nsStyleConsts.h"
#include "nsBidiKeyboard.h"
#include "nsStyleConsts.h"
#include "gfxConfig.h"
#include "InProcessWinCompositorWidget.h"
#include "InputDeviceUtils.h"
#include "ScreenHelperWin.h"
#include "mozilla/StaticPrefs_apz.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/StaticPrefs_gfx.h"
#include "mozilla/StaticPrefs_layout.h"
#include "mozilla/StaticPrefs_widget.h"
#include "nsNativeAppSupportWin.h"

#include "nsIGfxInfo.h"
#include "nsUXThemeConstants.h"
#include "KeyboardLayout.h"
#include "nsNativeDragTarget.h"
#include <mmsystem.h>  // needed for WIN32_LEAN_AND_MEAN
#include <zmouse.h>
#include <richedit.h>

#if defined(ACCESSIBILITY)

#  ifdef DEBUG
#    include "mozilla/a11y/Logging.h"
#  endif

#  include "oleidl.h"
#  include <winuser.h>
#  include "nsAccessibilityService.h"
#  include "mozilla/a11y/DocAccessible.h"
#  include "mozilla/a11y/LazyInstantiator.h"
#  include "mozilla/a11y/Platform.h"
#  if !defined(WINABLEAPI)
#    include <winable.h>
#  endif  // !defined(WINABLEAPI)
#endif    // defined(ACCESSIBILITY)

#include "nsIWinTaskbar.h"
#define NS_TASKBAR_CONTRACTID "@mozilla.org/windows-taskbar;1"

#include "WindowsUIUtils.h"

#include "nsWindowDefs.h"

#include "nsCrashOnException.h"

#include "nsIContent.h"

#include "mozilla/BackgroundHangMonitor.h"
#include "WinIMEHandler.h"

#include "npapi.h"

#include <d3d11.h>

#include "InkCollector.h"

// ERROR from wingdi.h (below) gets undefined by some code.
// #define ERROR               0
// #define RGN_ERROR ERROR
#define ERROR 0

#if !defined(SM_CONVERTIBLESLATEMODE)
#  define SM_CONVERTIBLESLATEMODE 0x2003
#endif

#if !defined(WM_DPICHANGED)
#  define WM_DPICHANGED 0x02E0
#endif

#include "mozilla/gfx/DeviceManagerDx.h"
#include "mozilla/layers/APZInputBridge.h"
#include "mozilla/layers/InputAPZContext.h"
#include "mozilla/layers/KnowsCompositor.h"
#include "InputData.h"

#include "mozilla/TaskController.h"
#include "mozilla/Telemetry.h"
#include "mozilla/webrender/WebRenderAPI.h"
#include "mozilla/layers/IAPZCTreeManager.h"

#include "DirectManipulationOwner.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::gfx;
using namespace mozilla::layers;
using namespace mozilla::widget;
using namespace mozilla::plugins;

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
static const wchar_t kUser32LibName[] = L"user32.dll";

uint32_t nsWindow::sInstanceCount = 0;
bool nsWindow::sSwitchKeyboardLayout = false;
BOOL nsWindow::sIsOleInitialized = FALSE;
nsIWidget::Cursor nsWindow::sCurrentCursor = {};
nsWindow* nsWindow::sCurrentWindow = nullptr;
bool nsWindow::sJustGotDeactivate = false;
bool nsWindow::sJustGotActivate = false;
bool nsWindow::sIsInMouseCapture = false;

// imported in nsWidgetFactory.cpp
TriStateBool nsWindow::sCanQuit = TRI_UNKNOWN;

// Hook Data Memebers for Dropdowns. sProcessHook Tells the
// hook methods whether they should be processing the hook
// messages.
HHOOK nsWindow::sMsgFilterHook = nullptr;
HHOOK nsWindow::sCallProcHook = nullptr;
HHOOK nsWindow::sCallMouseHook = nullptr;
bool nsWindow::sProcessHook = false;
UINT nsWindow::sRollupMsgId = 0;
HWND nsWindow::sRollupMsgWnd = nullptr;
UINT nsWindow::sHookTimerId = 0;

// Mouse Clicks - static variable definitions for figuring
// out 1 - 3 Clicks.
POINT nsWindow::sLastMousePoint = {0};
POINT nsWindow::sLastMouseMovePoint = {0};
LONG nsWindow::sLastMouseDownTime = 0L;
LONG nsWindow::sLastClickCount = 0L;
BYTE nsWindow::sLastMouseButton = 0;

bool nsWindow::sHaveInitializedPrefs = false;
bool nsWindow::sIsRestoringSession = false;

bool nsWindow::sFirstTopLevelWindowCreated = false;

TriStateBool nsWindow::sHasBogusPopupsDropShadowOnMultiMonitor = TRI_UNKNOWN;

bool nsWindow::sTouchInjectInitialized = false;
InjectTouchInputPtr nsWindow::sInjectTouchFuncPtr;

static SystemTimeConverter<DWORD>& TimeConverter() {
  static SystemTimeConverter<DWORD> timeConverterSingleton;
  return timeConverterSingleton;
}

namespace mozilla {

class CurrentWindowsTimeGetter {
 public:
  explicit CurrentWindowsTimeGetter(HWND aWnd) : mWnd(aWnd) {}

  DWORD GetCurrentTime() const { return ::GetTickCount(); }

  void GetTimeAsyncForPossibleBackwardsSkew(const TimeStamp& aNow) {
    DWORD currentTime = GetCurrentTime();
    if (sBackwardsSkewStamp && currentTime == sLastPostTime) {
      // There's already one inflight with this timestamp. Don't
      // send a duplicate.
      return;
    }
    sBackwardsSkewStamp = Some(aNow);
    sLastPostTime = currentTime;
    static_assert(sizeof(WPARAM) >= sizeof(DWORD),
                  "Can't fit a DWORD in a WPARAM");
    ::PostMessage(mWnd, MOZ_WM_SKEWFIX, sLastPostTime, 0);
  }

  static bool GetAndClearBackwardsSkewStamp(DWORD aPostTime,
                                            TimeStamp* aOutSkewStamp) {
    if (aPostTime != sLastPostTime) {
      // The SKEWFIX message is stale; we've sent a new one since then.
      // Ignore this one.
      return false;
    }
    MOZ_ASSERT(sBackwardsSkewStamp);
    *aOutSkewStamp = sBackwardsSkewStamp.value();
    sBackwardsSkewStamp = Nothing();
    return true;
  }

 private:
  static Maybe<TimeStamp> sBackwardsSkewStamp;
  static DWORD sLastPostTime;
  HWND mWnd;
};

Maybe<TimeStamp> CurrentWindowsTimeGetter::sBackwardsSkewStamp;
DWORD CurrentWindowsTimeGetter::sLastPostTime = 0;

}  // namespace mozilla

/**************************************************************
 *
 * SECTION: globals variables
 *
 **************************************************************/

static const char* sScreenManagerContractID =
    "@mozilla.org/gfx/screenmanager;1";

extern mozilla::LazyLogModule gWindowsLog;

// True if we have sent a notification that we are suspending/sleeping.
static bool gIsSleepMode = false;

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

// Getting this object from the window server can be expensive. Keep it
// around, also get it off the main thread. (See bug 1640852)
StaticRefPtr<IVirtualDesktopManager> gVirtualDesktopManager;
static bool gInitializedVirtualDesktopManager = false;

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

#if defined(ACCESSIBILITY)

namespace mozilla {

/**
 * Windows touchscreen code works by setting a global WH_GETMESSAGE hook and
 * injecting tiptsf.dll. The touchscreen process then posts registered messages
 * to our main thread. The tiptsf hook picks up those registered messages and
 * uses them as commands, some of which call into UIA, which then calls into
 * MSAA, which then sends WM_GETOBJECT to us.
 *
 * We can get ahead of this by installing our own thread-local WH_GETMESSAGE
 * hook. Since thread-local hooks are called ahead of global hooks, we will
 * see these registered messages before tiptsf does. At this point we can then
 * raise a flag that blocks a11y before invoking CallNextHookEx which will then
 * invoke the global tiptsf hook. Then when we see WM_GETOBJECT, we check the
 * flag by calling TIPMessageHandler::IsA11yBlocked().
 *
 * For Windows 8, we also hook tiptsf!ProcessCaretEvents, which is an a11y hook
 * function that also calls into UIA.
 */
class TIPMessageHandler {
 public:
  ~TIPMessageHandler() {
    if (mHook) {
      ::UnhookWindowsHookEx(mHook);
    }
  }

  static void Initialize() {
    if (!IsWin8OrLater()) {
      return;
    }

    if (sInstance) {
      return;
    }

    sInstance = new TIPMessageHandler();
    ClearOnShutdown(&sInstance);
  }

  static bool IsA11yBlocked() {
    if (!sInstance) {
      return false;
    }

    return sInstance->mA11yBlockCount > 0;
  }

 private:
  TIPMessageHandler() : mHook(nullptr), mA11yBlockCount(0) {
    MOZ_ASSERT(NS_IsMainThread());

    // Registered messages used by tiptsf
    mMessages[0] = ::RegisterWindowMessage(L"ImmersiveFocusNotification");
    mMessages[1] = ::RegisterWindowMessage(L"TipCloseMenus");
    mMessages[2] = ::RegisterWindowMessage(L"TabletInputPanelOpening");
    mMessages[3] = ::RegisterWindowMessage(L"IHM Pen or Touch Event noticed");
    mMessages[4] = ::RegisterWindowMessage(L"ProgrammabilityCaretVisibility");
    mMessages[5] = ::RegisterWindowMessage(L"CaretTrackingUpdateIPHidden");
    mMessages[6] = ::RegisterWindowMessage(L"CaretTrackingUpdateIPInfo");

    mHook = ::SetWindowsHookEx(WH_GETMESSAGE, &TIPHook, nullptr,
                               ::GetCurrentThreadId());
    MOZ_ASSERT(mHook);

    // On touchscreen devices, tiptsf.dll will have been loaded when STA COM was
    // first initialized.
    if (!IsWin10OrLater() && GetModuleHandle(L"tiptsf.dll") &&
        !sProcessCaretEventsStub) {
      sTipTsfInterceptor.Init("tiptsf.dll");
      DebugOnly<bool> ok = sProcessCaretEventsStub.Set(
          sTipTsfInterceptor, "ProcessCaretEvents", &ProcessCaretEventsHook);
      MOZ_ASSERT(ok);
    }

    if (!sSendMessageTimeoutWStub) {
      sUser32Intercept.Init("user32.dll");
      DebugOnly<bool> hooked = sSendMessageTimeoutWStub.Set(
          sUser32Intercept, "SendMessageTimeoutW", &SendMessageTimeoutWHook);
      MOZ_ASSERT(hooked);
    }
  }

  class MOZ_RAII A11yInstantiationBlocker {
   public:
    A11yInstantiationBlocker() {
      if (!TIPMessageHandler::sInstance) {
        return;
      }
      ++TIPMessageHandler::sInstance->mA11yBlockCount;
    }

    ~A11yInstantiationBlocker() {
      if (!TIPMessageHandler::sInstance) {
        return;
      }
      MOZ_ASSERT(TIPMessageHandler::sInstance->mA11yBlockCount > 0);
      --TIPMessageHandler::sInstance->mA11yBlockCount;
    }
  };

  friend class A11yInstantiationBlocker;

  static LRESULT CALLBACK TIPHook(int aCode, WPARAM aWParam, LPARAM aLParam) {
    if (aCode < 0 || !sInstance) {
      return ::CallNextHookEx(nullptr, aCode, aWParam, aLParam);
    }

    MSG* msg = reinterpret_cast<MSG*>(aLParam);
    UINT& msgCode = msg->message;

    for (uint32_t i = 0; i < ArrayLength(sInstance->mMessages); ++i) {
      if (msgCode == sInstance->mMessages[i]) {
        A11yInstantiationBlocker block;
        return ::CallNextHookEx(nullptr, aCode, aWParam, aLParam);
      }
    }

    return ::CallNextHookEx(nullptr, aCode, aWParam, aLParam);
  }

  static void CALLBACK ProcessCaretEventsHook(HWINEVENTHOOK aWinEventHook,
                                              DWORD aEvent, HWND aHwnd,
                                              LONG aObjectId, LONG aChildId,
                                              DWORD aGeneratingTid,
                                              DWORD aEventTime) {
    A11yInstantiationBlocker block;
    sProcessCaretEventsStub(aWinEventHook, aEvent, aHwnd, aObjectId, aChildId,
                            aGeneratingTid, aEventTime);
  }

  static LRESULT WINAPI SendMessageTimeoutWHook(HWND aHwnd, UINT aMsgCode,
                                                WPARAM aWParam, LPARAM aLParam,
                                                UINT aFlags, UINT aTimeout,
                                                PDWORD_PTR aMsgResult) {
    // We don't want to handle this unless the message is a WM_GETOBJECT that we
    // want to block, and the aHwnd is a nsWindow that belongs to the current
    // thread.
    if (!aMsgResult || aMsgCode != WM_GETOBJECT ||
        static_cast<LONG>(aLParam) != OBJID_CLIENT ||
        !WinUtils::GetNSWindowPtr(aHwnd) ||
        ::GetWindowThreadProcessId(aHwnd, nullptr) != ::GetCurrentThreadId() ||
        !IsA11yBlocked()) {
      return sSendMessageTimeoutWStub(aHwnd, aMsgCode, aWParam, aLParam, aFlags,
                                      aTimeout, aMsgResult);
    }

    // In this case we want to fake the result that would happen if we had
    // decided not to handle WM_GETOBJECT in our WndProc. We hand the message
    // off to DefWindowProc to accomplish this.
    *aMsgResult = static_cast<DWORD_PTR>(
        ::DefWindowProcW(aHwnd, aMsgCode, aWParam, aLParam));

    return static_cast<LRESULT>(TRUE);
  }

  static WindowsDllInterceptor sTipTsfInterceptor;
  static WindowsDllInterceptor::FuncHookType<WINEVENTPROC>
      sProcessCaretEventsStub;
  static WindowsDllInterceptor::FuncHookType<decltype(&SendMessageTimeoutW)>
      sSendMessageTimeoutWStub;
  static StaticAutoPtr<TIPMessageHandler> sInstance;

  HHOOK mHook;
  UINT mMessages[7];
  uint32_t mA11yBlockCount;
};

WindowsDllInterceptor TIPMessageHandler::sTipTsfInterceptor;
WindowsDllInterceptor::FuncHookType<WINEVENTPROC>
    TIPMessageHandler::sProcessCaretEventsStub;
WindowsDllInterceptor::FuncHookType<decltype(&SendMessageTimeoutW)>
    TIPMessageHandler::sSendMessageTimeoutWStub;
StaticAutoPtr<TIPMessageHandler> TIPMessageHandler::sInstance;

}  // namespace mozilla

#endif  // defined(ACCESSIBILITY)

namespace mozilla {

// This task will get the VirtualDesktopManager from the generic thread pool
// since doing this on the main thread on startup causes performance issues.
//
// See bug 1640852.
//
// This should be fine and should not require any locking, as when the main
// thread will access it, if it races with this function it will either find
// it to be null or to have a valid value.
class InitializeVirtualDesktopManagerTask : public Task {
 public:
  InitializeVirtualDesktopManagerTask() : Task(false, kDefaultPriorityValue) {}

#ifdef MOZ_COLLECTING_RUNNABLE_TELEMETRY
  bool GetName(nsACString& aName) override {
    aName.AssignLiteral("InitializeVirtualDesktopManagerTask");
    return true;
  }
#endif

  virtual bool Run() override {
#ifndef __MINGW32__
    if (!IsWin10OrLater()) {
      return true;
    }

    RefPtr<IVirtualDesktopManager> desktopManager;
    HRESULT hr = ::CoCreateInstance(
        CLSID_VirtualDesktopManager, NULL, CLSCTX_INPROC_SERVER,
        __uuidof(IVirtualDesktopManager), getter_AddRefs(desktopManager));
    if (FAILED(hr)) {
      return true;
    }

    gVirtualDesktopManager = desktopManager;
#endif
    return true;
  }
};

static BOOL GetMouseVanishSystemPref(bool aShouldUpdate) {
  static bool sInitialized = false;
  static BOOL sMouseVanishSystemPref = FALSE;

  if (!sInitialized || aShouldUpdate) {
    BOOL ok = ::SystemParametersInfo(SPI_GETMOUSEVANISH, 0,
                                     &sMouseVanishSystemPref, 0);
    if (!ok) {
      // Getting system pref failed so just use user pref.
      sMouseVanishSystemPref =
          StaticPrefs::widget_windows_hide_cursor_when_typing();
    }
    sInitialized = true;
  }

  return sMouseVanishSystemPref;
}

static bool IsMouseVanishKey(WPARAM aVirtKey) {
  switch (aVirtKey) {
    case VK_SHIFT:
    case VK_LSHIFT:
    case VK_RSHIFT:
    case VK_CONTROL:
    case VK_LCONTROL:
    case VK_RCONTROL:
    case VK_MENU:
    case VK_LMENU:
    case VK_RMENU:
    case VK_LWIN:
    case VK_RWIN:
    case VK_INSERT:
    case VK_DELETE:
    case VK_HOME:
    case VK_END:
    case VK_ESCAPE:
    case VK_PRINT:
    case VK_UP:
    case VK_DOWN:
    case VK_LEFT:
    case VK_RIGHT:
    case VK_PRIOR:  // PgUp
    case VK_NEXT:   // PgDn
      return false;
    default:
      // Return true unless Ctrl is pressed
      return (GetKeyState(VK_CONTROL) & 0x8000) != 0x8000;
  }
}

/**
 * Hide/unhide the cursor if the correct Windows and Firefox settings are set.
 */
static void MaybeHideCursor(bool aShouldHide) {
  static bool sMouseExists =
      []{
        // Before the first call to ShowCursor, the visibility count is 0
        // if there is a mouse installed and -1 if not.
        int count = ::ShowCursor(FALSE);
        ::ShowCursor(TRUE);
        return count == -1;
      }();

  if (!sMouseExists) {
    return;
  }

  static bool sIsHidden = false;
  bool shouldHide = aShouldHide &&
                    StaticPrefs::widget_windows_hide_cursor_when_typing() &&
                    GetMouseVanishSystemPref(false);

  if (shouldHide != sIsHidden) {
    [[maybe_unused]] int count = ::ShowCursor(aShouldHide ? FALSE : TRUE);
    MOZ_ASSERT(count == (aShouldHide ? -1 : 0));
    sIsHidden = shouldHide;
  }
}

}  // namespace mozilla

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

nsWindow::nsWindow(bool aIsChildWindow)
    : nsBaseWidget(eBorderStyle_default),
      mBrush(::CreateSolidBrush(NSRGB_2_COLOREF(::GetSysColor(COLOR_BTNFACE)))),
      mFrameState(std::in_place, this),
      mIsChildWindow(aIsChildWindow),
      mLastPaintEndTime(TimeStamp::Now()),
      mCachedHitTestTime(TimeStamp::Now()),
      mSizeConstraintsScale(GetDefaultScale().scale),
      mDesktopId("DesktopIdMutex") {
  MOZ_ASSERT(mWindowType == eWindowType_child);

  if (!gInitializedVirtualDesktopManager) {
    TaskController::Get()->AddTask(
        MakeAndAddRef<InitializeVirtualDesktopManagerTask>());
    gInitializedVirtualDesktopManager = true;
  }

  // Global initialization
  if (!sInstanceCount) {
    // Global app registration id for Win7 and up. See
    // WinTaskbar.cpp for details.
    // MSIX packages explicitly do not support setting the appid from within
    // the app, as it is set in the package manifest instead.
    if (!WinUtils::HasPackageIdentity()) {
      mozilla::widget::WinTaskbar::RegisterAppUserModelID();
    }
    KeyboardLayout::GetInstance()->OnLayoutChange(::GetKeyboardLayout(0));
#if defined(ACCESSIBILITY)
    mozilla::TIPMessageHandler::Initialize();
#endif  // defined(ACCESSIBILITY)
    if (SUCCEEDED(::OleInitialize(nullptr))) {
      sIsOleInitialized = TRUE;
    }
    NS_ASSERTION(sIsOleInitialized, "***** OLE is not initialized!\n");
    MouseScrollHandler::Initialize();
    // Init theme data
    nsUXThemeData::UpdateNativeThemeInfo();
    RedirectedKeyDownMessageManager::Forget();
    if (mPointerEvents.ShouldEnableInkCollector()) {
      InkCollector::sInkCollector = new InkCollector();
    }
  }  // !sInstanceCount

  sInstanceCount++;
}

nsWindow::~nsWindow() {
  mInDtor = true;

  // If the widget was released without calling Destroy() then the native window
  // still exists, and we need to destroy it. Destroy() will early-return if it
  // was already called. In any case it is important to call it before
  // destroying mPresentLock (cf. 1156182).
  Destroy();

  // Free app icon resources.  This must happen after `OnDestroy` (see bug
  // 708033).
  if (mIconSmall) ::DestroyIcon(mIconSmall);

  if (mIconBig) ::DestroyIcon(mIconBig);

  sInstanceCount--;

  // Global shutdown
  if (sInstanceCount == 0) {
    if (InkCollector::sInkCollector) {
      InkCollector::sInkCollector->Shutdown();
      InkCollector::sInkCollector = nullptr;
    }
    IMEHandler::Terminate();
    sCurrentCursor = {};
    if (sIsOleInitialized) {
      ::OleFlushClipboard();
      ::OleUninitialize();
      sIsOleInitialized = FALSE;
    }
  }

  NS_IF_RELEASE(mNativeDragTarget);
}

/**************************************************************
 *
 * SECTION: nsIWidget::Create, nsIWidget::Destroy
 *
 * Creating and destroying windows for this widget.
 *
 **************************************************************/

// Allow Derived classes to modify the height that is passed
// when the window is created or resized.
int32_t nsWindow::GetHeight(int32_t aProposedHeight) { return aProposedHeight; }

static bool ShouldCacheTitleBarInfo(nsWindowType aWindowType,
                                    nsBorderStyle aBorderStyle) {
  return (aWindowType == eWindowType_toplevel) &&
         (aBorderStyle == eBorderStyle_default ||
          aBorderStyle == eBorderStyle_all) &&
         (!nsUXThemeData::sTitlebarInfoPopulatedThemed ||
          !nsUXThemeData::sTitlebarInfoPopulatedAero);
}

void nsWindow::SendAnAPZEvent(InputData& aEvent) {
  LRESULT popupHandlingResult;
  if (DealWithPopups(mWnd, MOZ_WM_DMANIP, 0, 0, &popupHandlingResult)) {
    // We need to consume the event after using it to roll up the popup(s).
    return;
  }

  if (mSwipeTracker && aEvent.mInputType == PANGESTURE_INPUT) {
    // Give the swipe tracker a first pass at the event. If a new pan gesture
    // has been started since the beginning of the swipe, the swipe tracker
    // will know to ignore the event.
    nsEventStatus status =
        mSwipeTracker->ProcessEvent(aEvent.AsPanGestureInput());
    if (status == nsEventStatus_eConsumeNoDefault) {
      return;
    }
  }

  APZEventResult result;
  if (mAPZC) {
    result = mAPZC->InputBridge()->ReceiveInputEvent(aEvent);
  }
  if (result.GetStatus() == nsEventStatus_eConsumeNoDefault) {
    return;
  }

  MOZ_ASSERT(aEvent.mInputType == PANGESTURE_INPUT ||
             aEvent.mInputType == PINCHGESTURE_INPUT);

  if (aEvent.mInputType == PANGESTURE_INPUT) {
    PanGestureInput& panInput = aEvent.AsPanGestureInput();
    WidgetWheelEvent event = panInput.ToWidgetEvent(this);
    bool canTriggerSwipe = SwipeTracker::CanTriggerSwipe(panInput);
    if (!mAPZC) {
      if (MayStartSwipeForNonAPZ(panInput, CanTriggerSwipe{canTriggerSwipe})) {
        return;
      }
    } else {
      event = MayStartSwipeForAPZ(panInput, result,
                                  CanTriggerSwipe{canTriggerSwipe});
    }

    ProcessUntransformedAPZEvent(&event, result);

    return;
  }

  PinchGestureInput& pinchInput = aEvent.AsPinchGestureInput();
  WidgetWheelEvent event = pinchInput.ToWidgetEvent(this);
  ProcessUntransformedAPZEvent(&event, result);
}

void nsWindow::RecreateDirectManipulationIfNeeded() {
  DestroyDirectManipulation();

  if (mWindowType != eWindowType_toplevel && mWindowType != eWindowType_popup) {
    return;
  }

  if (!(StaticPrefs::apz_allow_zooming() ||
        StaticPrefs::apz_windows_use_direct_manipulation()) ||
      StaticPrefs::apz_windows_force_disable_direct_manipulation()) {
    return;
  }

  if (!IsWin10OrLater()) {
    // Chrome source said the Windows Direct Manipulation implementation had
    // important bugs until Windows 10 (although IE on Windows 8.1 seems to use
    // Direct Manipulation).
    return;
  }

  mDmOwner = MakeUnique<DirectManipulationOwner>(this);

  LayoutDeviceIntRect bounds(mBounds.X(), mBounds.Y(), mBounds.Width(),
                             GetHeight(mBounds.Height()));
  mDmOwner->Init(bounds);
}

void nsWindow::ResizeDirectManipulationViewport() {
  if (mDmOwner) {
    LayoutDeviceIntRect bounds(mBounds.X(), mBounds.Y(), mBounds.Width(),
                               GetHeight(mBounds.Height()));
    mDmOwner->ResizeViewport(bounds);
  }
}

void nsWindow::DestroyDirectManipulation() {
  if (mDmOwner) {
    mDmOwner->Destroy();
    mDmOwner.reset();
  }
}

// Create the proper widget
nsresult nsWindow::Create(nsIWidget* aParent, nsNativeWidget aNativeParent,
                          const LayoutDeviceIntRect& aRect,
                          nsWidgetInitData* aInitData) {
  nsWidgetInitData defaultInitData;
  if (!aInitData) aInitData = &defaultInitData;

  nsIWidget* baseParent =
      aInitData->mWindowType == eWindowType_dialog ||
              aInitData->mWindowType == eWindowType_toplevel ||
              aInitData->mWindowType == eWindowType_invisible
          ? nullptr
          : aParent;

  mIsTopWidgetWindow = (nullptr == baseParent);
  mBounds = aRect;

  // Ensure that the toolkit is created.
  nsToolkit::GetToolkit();

  BaseCreate(baseParent, aInitData);

  HWND parent;
  if (aParent) {  // has a nsIWidget parent
    parent = aParent ? (HWND)aParent->GetNativeData(NS_NATIVE_WINDOW) : nullptr;
    mParent = aParent;
  } else {  // has a nsNative parent
    parent = (HWND)aNativeParent;
    mParent =
        aNativeParent ? WinUtils::GetNSWindowPtr((HWND)aNativeParent) : nullptr;
  }

  mIsRTL = aInitData->mRTL;
  mOpeningAnimationSuppressed = aInitData->mIsAnimationSuppressed;
  mAlwaysOnTop = aInitData->mAlwaysOnTop;
  mResizable = aInitData->mResizable;

  DWORD style = WindowStyle();
  DWORD extendedStyle = WindowExStyle();

  // When window is PiP window on Windows7, WS_EX_COMPOSITED is set to suppress
  // flickering during resizing with hardware acceleration.
  bool isPIPWindow = aInitData && aInitData->mPIPWindow;
  if (isPIPWindow && !IsWin8OrLater() &&
      gfxConfig::IsEnabled(gfx::Feature::HW_COMPOSITING) &&
      WidgetTypeSupportsAcceleration()) {
    extendedStyle |= WS_EX_COMPOSITED;
  }

  if (mWindowType == eWindowType_popup) {
    if (!aParent) {
      parent = nullptr;
    }

    if (!IsWin8OrLater() && HasBogusPopupsDropShadowOnMultiMonitor() &&
        ShouldUseOffMainThreadCompositing()) {
      extendedStyle |= WS_EX_COMPOSITED;
    }

    if (aInitData->mMouseTransparent) {
      // This flag makes the window transparent to mouse events
      mMouseTransparent = true;
      extendedStyle |= WS_EX_TRANSPARENT;
    }
  } else if (mWindowType == eWindowType_invisible) {
    // Make sure CreateWindowEx succeeds at creating a toplevel window
    style &= ~0x40000000;  // WS_CHILDWINDOW
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

  const wchar_t* className = GetWindowClass();
  if (aInitData->mWindowType == eWindowType_toplevel && !aParent &&
      !sFirstTopLevelWindowCreated) {
    sFirstTopLevelWindowCreated = true;
    mWnd = ConsumePreXULSkeletonUIHandle();
    auto skeletonUIError = GetPreXULSkeletonUIErrorReason();
    if (skeletonUIError) {
      nsAutoString errorString(
          GetPreXULSkeletonUIErrorString(skeletonUIError.value()));
      Telemetry::ScalarSet(
          Telemetry::ScalarID::STARTUP_SKELETON_UI_DISABLED_REASON,
          errorString);
    }
    if (mWnd) {
      MOZ_ASSERT(style == kPreXULSkeletonUIWindowStyle,
                 "The skeleton UI window style should match the expected "
                 "style for the first window created");
      MOZ_ASSERT(extendedStyle == kPreXULSkeletonUIWindowStyleEx,
                 "The skeleton UI window extended style should match the "
                 "expected extended style for the first window created");
      mIsShowingPreXULSkeletonUI = true;

      // If we successfully consumed the pre-XUL skeleton UI, just update
      // our internal state to match what is currently being displayed.
      mIsVisible = true;
      mFrameState->ConsumePreXULSkeletonState(WasPreXULSkeletonUIMaximized());

      // These match the margins set in browser-tabsintitlebar.js with
      // default prefs on Windows. Bug 1673092 tracks lining this up with
      // that more correctly instead of hard-coding it.
      LayoutDeviceIntMargin margins(0, 2, 2, 2);
      SetNonClientMargins(margins);

      // Reset the WNDPROC for this window and its whole class, as we had
      // to use our own WNDPROC when creating the the skeleton UI window.
      ::SetWindowLongPtrW(mWnd, GWLP_WNDPROC,
                          reinterpret_cast<LONG_PTR>(
                              WinUtils::NonClientDpiScalingDefWindowProcW));
      ::SetClassLongPtrW(mWnd, GCLP_WNDPROC,
                         reinterpret_cast<LONG_PTR>(
                             WinUtils::NonClientDpiScalingDefWindowProcW));
    }
  }

  if (!mWnd) {
    mWnd =
        ::CreateWindowExW(extendedStyle, className, L"", style, aRect.X(),
                          aRect.Y(), aRect.Width(), GetHeight(aRect.Height()),
                          parent, nullptr, nsToolkit::mDllInstance, nullptr);
  }

  if (!mWnd) {
    NS_WARNING("nsWindow CreateWindowEx failed.");
    return NS_ERROR_FAILURE;
  }

  if (aInitData->mIsPrivate) {
    if (Preferences::GetBool("browser.privacySegmentation.enabled", false)) {
      RefPtr<IPropertyStore> pPropStore;
      if (!FAILED(SHGetPropertyStoreForWindow(mWnd, IID_IPropertyStore,
                                              getter_AddRefs(pPropStore)))) {
        PROPVARIANT pv;
        nsAutoString aumid;
        // make sure we're using the private browsing AUMID so that taskbar
        // grouping works properly
        Unused << NS_WARN_IF(
            !mozilla::widget::WinTaskbar::GenerateAppUserModelID(aumid, true));
        if (!FAILED(InitPropVariantFromString(aumid.get(), &pv))) {
          if (!FAILED(pPropStore->SetValue(PKEY_AppUserModel_ID, pv))) {
            pPropStore->Commit();
          }

          PropVariantClear(&pv);
        }
      }
      HICON icon = ::LoadIconW(::GetModuleHandleW(nullptr),
                               MAKEINTRESOURCEW(IDI_PBMODE));
      SetBigIcon(icon);
      SetSmallIcon(icon);
    }
  }

  mDeviceNotifyHandle = InputDeviceUtils::RegisterNotification(mWnd);

  // If mDefaultScale is set before mWnd has been set, it will have the scale of
  // the primary monitor, rather than the monitor that the window is actually
  // on. For non-popup windows this gets corrected by the WM_DPICHANGED message
  // which resets mDefaultScale, but for popup windows we don't reset
  // mDefaultScale on that message. In order to ensure that popup windows
  // spawned on a non-primary monitor end up with the correct scale, we reset
  // mDefaultScale here so that it gets recomputed using the correct monitor now
  // that we have a mWnd.
  mDefaultScale = -1.0;

  if (mIsRTL) {
    DWORD dwAttribute = TRUE;
    DwmSetWindowAttribute(mWnd, DWMWA_NONCLIENT_RTL_LAYOUT, &dwAttribute,
                          sizeof dwAttribute);
  }

  UpdateDarkModeToolbar();

  if (mOpeningAnimationSuppressed) {
    SuppressAnimation(true);
  }

  if (mAlwaysOnTop) {
    ::SetWindowPos(mWnd, HWND_TOPMOST, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
  }

  if (mWindowType != eWindowType_invisible &&
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
    HWND scrollContainerWnd = ::CreateWindowW(
        className, L"FAKETRACKPOINTSCROLLCONTAINER", WS_CHILD | WS_VISIBLE, 0,
        0, 0, 0, mWnd, nullptr, nsToolkit::mDllInstance, nullptr);
    HWND scrollableWnd = ::CreateWindowW(
        className, L"FAKETRACKPOINTSCROLLABLE",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP | 0x30, 0, 0, 0, 0,
        scrollContainerWnd, nullptr, nsToolkit::mDllInstance, nullptr);

    // Give the FAKETRACKPOINTSCROLLABLE window a specific ID so that
    // WindowProcInternal can distinguish it from the top-level window
    // easily.
    ::SetWindowLongPtrW(scrollableWnd, GWLP_ID, eFakeTrackPointScrollableID);

    // Make FAKETRACKPOINTSCROLLABLE use nsWindow::WindowProc, and store the
    // old window procedure in its "user data".
    WNDPROC oldWndProc = (WNDPROC)::SetWindowLongPtrW(
        scrollableWnd, GWLP_WNDPROC, (LONG_PTR)nsWindow::WindowProc);
    ::SetWindowLongPtrW(scrollableWnd, GWLP_USERDATA, (LONG_PTR)oldWndProc);
  }

  SubclassWindow(TRUE);

  // Starting with Windows XP, a process always runs within a terminal services
  // session. In order to play nicely with RDP, fast user switching, and the
  // lock screen, we should be handling WM_WTSSESSION_CHANGE. We must register
  // our HWND in order to receive this message.
  DebugOnly<BOOL> wtsRegistered =
      ::WTSRegisterSessionNotification(mWnd, NOTIFY_FOR_THIS_SESSION);
  NS_ASSERTION(wtsRegistered, "WTSRegisterSessionNotification failed!\n");

  mDefaultIMC.Init(this);
  IMEHandler::InitInputContext(this, mInputContext);

  // Do some initialization work, but only if (a) it hasn't already been done,
  // and (b) this is the hidden window (which is conveniently created before
  // any visible windows but after the profile has been initialized).
  if (!sHaveInitializedPrefs && mWindowType == eWindowType_invisible) {
    sSwitchKeyboardLayout =
        Preferences::GetBool("intl.keyboard.per_window_layout", false);
    sHaveInitializedPrefs = true;
  }

  // Query for command button metric data for rendering the titlebar. We
  // only do this once on the first window that has an actual titlebar
  if (ShouldCacheTitleBarInfo(mWindowType, mBorderStyle)) {
    nsUXThemeData::UpdateTitlebarInfo(mWnd);
  }

  static bool a11yPrimed = false;
  if (!a11yPrimed && mWindowType == eWindowType_toplevel) {
    a11yPrimed = true;
    if (Preferences::GetInt("accessibility.force_disabled", 0) == -1) {
      ::PostMessage(mWnd, MOZ_WM_STARTA11Y, 0, 0);
    }
  }

  RecreateDirectManipulationIfNeeded();

  return NS_OK;
}

void nsWindow::LocalesChanged() {
  bool isRTL = intl::LocaleService::GetInstance()->IsAppLocaleRTL();
  if (mIsRTL != isRTL) {
    DWORD dwAttribute = isRTL;
    DwmSetWindowAttribute(mWnd, DWMWA_NONCLIENT_RTL_LAYOUT, &dwAttribute,
                          sizeof dwAttribute);
    mIsRTL = isRTL;
  }
}

// Close this nsWindow
void nsWindow::Destroy() {
  // WM_DESTROY has already fired, avoid calling it twice
  if (mOnDestroyCalled) return;

  // Don't destroy windows that have file pickers open, we'll tear these down
  // later once the picker is closed.
  mDestroyCalled = true;
  if (mPickerDisplayCount) return;

  // During the destruction of all of our children, make sure we don't get
  // deleted.
  nsCOMPtr<nsIWidget> kungFuDeathGrip(this);

  DestroyDirectManipulation();

  /**
   * On windows the LayerManagerOGL destructor wants the widget to be around for
   * cleanup. It also would like to have the HWND intact, so we nullptr it here.
   */
  DestroyLayerManager();

  InputDeviceUtils::UnregisterNotification(mDeviceNotifyHandle);
  mDeviceNotifyHandle = nullptr;

  // The DestroyWindow function destroys the specified window. The function
  // sends WM_DESTROY and WM_NCDESTROY messages to the window to deactivate it
  // and remove the keyboard focus from it. The function also destroys the
  // window's menu, flushes the thread message queue, destroys timers, removes
  // clipboard ownership, and breaks the clipboard viewer chain (if the window
  // is at the top of the viewer chain).
  //
  // If the specified window is a parent or owner window, DestroyWindow
  // automatically destroys the associated child or owned windows when it
  // destroys the parent or owner window. The function first destroys child or
  // owned windows, and then it destroys the parent or owner window.
  VERIFY(::DestroyWindow(mWnd));

  // Our windows can be subclassed which may prevent us receiving WM_DESTROY. If
  // OnDestroy() didn't get called, call it now.
  if (false == mOnDestroyCalled) {
    MSGResult msgResult;
    mWindowHook.Notify(mWnd, WM_DESTROY, 0, 0, msgResult);
    OnDestroy();
  }
}

/**************************************************************
 *
 * SECTION: Window class utilities
 *
 * Utilities for calculating the proper window class name for
 * Create window.
 *
 **************************************************************/

const wchar_t* nsWindow::RegisterWindowClass(const wchar_t* aClassName,
                                             UINT aExtraStyle,
                                             LPWSTR aIconID) const {
  WNDCLASSW wc;
  if (::GetClassInfoW(nsToolkit::mDllInstance, aClassName, &wc)) {
    // already registered
    return aClassName;
  }

  wc.style = CS_DBLCLKS | aExtraStyle;
  wc.lpfnWndProc = WinUtils::NonClientDpiScalingDefWindowProcW;
  wc.cbClsExtra = 0;
  wc.cbWndExtra = 0;
  wc.hInstance = nsToolkit::mDllInstance;
  wc.hIcon =
      aIconID ? ::LoadIconW(::GetModuleHandleW(nullptr), aIconID) : nullptr;
  wc.hCursor = nullptr;
  wc.hbrBackground = mBrush;
  wc.lpszMenuName = nullptr;
  wc.lpszClassName = aClassName;

  if (!::RegisterClassW(&wc)) {
    // For older versions of Win32 (i.e., not XP), the registration may
    // fail with aExtraStyle, so we have to re-register without it.
    wc.style = CS_DBLCLKS;
    ::RegisterClassW(&wc);
  }
  return aClassName;
}

static LPWSTR const gStockApplicationIcon = MAKEINTRESOURCEW(32512);

// Return the proper window class for everything except popups.
const wchar_t* nsWindow::GetWindowClass() const {
  switch (mWindowType) {
    case eWindowType_invisible:
      return RegisterWindowClass(kClassNameHidden, 0, gStockApplicationIcon);
    case eWindowType_dialog:
      return RegisterWindowClass(kClassNameDialog, 0, 0);
    case eWindowType_popup:
      return RegisterWindowClass(kClassNameDropShadow, CS_DROPSHADOW,
                                 gStockApplicationIcon);
    default:
      return RegisterWindowClass(GetMainWindowClass(), 0,
                                 gStockApplicationIcon);
  }
}

/**************************************************************
 *
 * SECTION: Window styles utilities
 *
 * Return the proper windows styles and extended styles.
 *
 **************************************************************/

// Return nsWindow styles
DWORD nsWindow::WindowStyle() {
  DWORD style;

  switch (mWindowType) {
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
      [[fallthrough]];

    case eWindowType_toplevel:
    case eWindowType_invisible:
      style = WS_OVERLAPPED | WS_BORDER | WS_DLGFRAME | WS_SYSMENU |
              WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_CLIPCHILDREN;
      break;
  }

  if (mBorderStyle != eBorderStyle_default &&
      mBorderStyle != eBorderStyle_all) {
    if (mBorderStyle == eBorderStyle_none ||
        !(mBorderStyle & eBorderStyle_border))
      style &= ~WS_BORDER;

    if (mBorderStyle == eBorderStyle_none ||
        !(mBorderStyle & eBorderStyle_title)) {
      style &= ~WS_DLGFRAME;
      style |= WS_POPUP;
      style &= ~WS_CHILD;
    }

    if (mBorderStyle == eBorderStyle_none ||
        !(mBorderStyle & eBorderStyle_close))
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

    if (mBorderStyle == eBorderStyle_none ||
        !(mBorderStyle & eBorderStyle_resizeh))
      style &= ~WS_THICKFRAME;

    if (mBorderStyle == eBorderStyle_none ||
        !(mBorderStyle & eBorderStyle_minimize))
      style &= ~WS_MINIMIZEBOX;

    if (mBorderStyle == eBorderStyle_none ||
        !(mBorderStyle & eBorderStyle_maximize))
      style &= ~WS_MAXIMIZEBOX;

    if (IsPopupWithTitleBar()) {
      style |= WS_CAPTION;
      if (mBorderStyle & eBorderStyle_close) {
        style |= WS_SYSMENU;
      }
    }
  }

  if (mIsChildWindow) {
    style |= WS_CLIPCHILDREN;
    if (!(style & WS_POPUP)) {
      style |= WS_CHILD;  // WS_POPUP and WS_CHILD are mutually exclusive.
    }
  }

  VERIFY_WINDOW_STYLE(style);
  return style;
}

// Return nsWindow extended styles
DWORD nsWindow::WindowExStyle() {
  switch (mWindowType) {
    case eWindowType_child:
      return 0;

    case eWindowType_dialog:
      return WS_EX_WINDOWEDGE | WS_EX_DLGMODALFRAME;

    case eWindowType_popup: {
      DWORD extendedStyle = WS_EX_TOOLWINDOW;
      if (mPopupLevel == ePopupLevelTop) extendedStyle |= WS_EX_TOPMOST;
      return extendedStyle;
    }
    default:
      NS_ERROR("unknown border style");
      [[fallthrough]];

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
void nsWindow::SubclassWindow(BOOL bState) {
  if (bState) {
    if (!mWnd || !IsWindow(mWnd)) {
      NS_ERROR("Invalid window handle");
    }

    mPrevWndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(
        mWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(nsWindow::WindowProc)));
    NS_ASSERTION(mPrevWndProc, "Null standard window procedure");
    // connect the this pointer to the nsWindow handle
    WinUtils::SetNSWindowBasePtr(mWnd, this);
  } else {
    if (IsWindow(mWnd)) {
      SetWindowLongPtrW(mWnd, GWLP_WNDPROC,
                        reinterpret_cast<LONG_PTR>(mPrevWndProc));
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
void nsWindow::SetParent(nsIWidget* aNewParent) {
  nsCOMPtr<nsIWidget> kungFuDeathGrip(this);
  nsIWidget* parent = GetParent();
  if (parent) {
    parent->RemoveChild(this);
  }

  mParent = aNewParent;

  if (aNewParent) {
    ReparentNativeWidget(aNewParent);
    aNewParent->AddChild(this);
    return;
  }
  if (mWnd) {
    // If we have no parent, SetParent should return the desktop.
    VERIFY(::SetParent(mWnd, nullptr));
    RecreateDirectManipulationIfNeeded();
  }
}

void nsWindow::ReparentNativeWidget(nsIWidget* aNewParent) {
  MOZ_ASSERT(aNewParent, "null widget");

  mParent = aNewParent;
  if (mWindowType == eWindowType_popup) {
    return;
  }
  HWND newParent = (HWND)aNewParent->GetNativeData(NS_NATIVE_WINDOW);
  NS_ASSERTION(newParent, "Parent widget has a null native window handle");
  if (newParent && mWnd) {
    ::SetParent(mWnd, newParent);
    RecreateDirectManipulationIfNeeded();
  }
}

nsIWidget* nsWindow::GetParent(void) {
  if (mIsTopWidgetWindow) {
    return nullptr;
  }
  if (mInDtor || mOnDestroyCalled) {
    return nullptr;
  }
  return mParent;
}

static int32_t RoundDown(double aDouble) {
  return aDouble > 0 ? static_cast<int32_t>(floor(aDouble))
                     : static_cast<int32_t>(ceil(aDouble));
}

float nsWindow::GetDPI() {
  float dpi = 96.0f;
  nsCOMPtr<nsIScreen> screen = GetWidgetScreen();
  if (screen) {
    screen->GetDpi(&dpi);
  }
  return dpi;
}

double nsWindow::GetDefaultScaleInternal() {
  if (mDefaultScale <= 0.0) {
    mDefaultScale = WinUtils::LogToPhysFactor(mWnd);
  }
  return mDefaultScale;
}

int32_t nsWindow::LogToPhys(double aValue) {
  return WinUtils::LogToPhys(
      ::MonitorFromWindow(mWnd, MONITOR_DEFAULTTOPRIMARY), aValue);
}

nsWindow* nsWindow::GetParentWindow(bool aIncludeOwner) {
  return static_cast<nsWindow*>(GetParentWindowBase(aIncludeOwner));
}

nsWindow* nsWindow::GetParentWindowBase(bool aIncludeOwner) {
  if (mIsTopWidgetWindow) {
    // Must use a flag instead of mWindowType to tell if the window is the
    // owned by the topmost widget, because a child window can be embedded
    // inside a HWND which is not associated with a nsIWidget.
    return nullptr;
  }

  // If this widget has already been destroyed, pretend we have no parent.
  // This corresponds to code in Destroy which removes the destroyed
  // widget from its parent's child list.
  if (mInDtor || mOnDestroyCalled) return nullptr;

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

BOOL CALLBACK nsWindow::EnumAllChildWindProc(HWND aWnd, LPARAM aParam) {
  nsWindow* wnd = WinUtils::GetNSWindowPtr(aWnd);
  if (wnd) {
    reinterpret_cast<nsTArray<nsWindow*>*>(aParam)->AppendElement(wnd);
  }
  return TRUE;
}

BOOL CALLBACK nsWindow::EnumAllThreadWindowProc(HWND aWnd, LPARAM aParam) {
  nsWindow* wnd = WinUtils::GetNSWindowPtr(aWnd);
  if (wnd) {
    reinterpret_cast<nsTArray<nsWindow*>*>(aParam)->AppendElement(wnd);
  }
  EnumChildWindows(aWnd, EnumAllChildWindProc, aParam);
  return TRUE;
}

/* static*/
nsTArray<nsWindow*> nsWindow::EnumAllWindows() {
  nsTArray<nsWindow*> windows;
  EnumThreadWindows(GetCurrentThreadId(), EnumAllThreadWindowProc,
                    reinterpret_cast<LPARAM>(&windows));
  return windows;
}

/**************************************************************
 *
 * SECTION: nsIWidget::Show
 *
 * Hide or show this component.
 *
 **************************************************************/

void nsWindow::Show(bool bState) {
  if (bState && mIsShowingPreXULSkeletonUI) {
    // The first time we decide to actually show the window is when we decide
    // that we've taken over the window from the skeleton UI, and we should
    // no longer treat resizes / moves specially.
    mIsShowingPreXULSkeletonUI = false;
    // Initialize the UI state - this would normally happen below, but since
    // we're actually already showing, we won't hit it in the normal way.
    ::SendMessageW(mWnd, WM_CHANGEUISTATE,
                   MAKEWPARAM(UIS_SET, UISF_HIDEFOCUS | UISF_HIDEACCEL), 0);
#if defined(ACCESSIBILITY)
    // If our HWND has focus and the a11y engine hasn't started yet, fire a
    // focus win event. Windows already did this when the skeleton UI appeared,
    // but a11y wouldn't have been able to start at that point even if a client
    // responded. Firing this now gives clients the chance to respond with
    // WM_GETOBJECT, which will trigger the a11y engine. We don't want to do
    // this if the a11y engine has already started because it has probably
    // already fired focus on a descendant.
    if (::GetFocus() == mWnd && !GetAccService()) {
      ::NotifyWinEvent(EVENT_OBJECT_FOCUS, mWnd, OBJID_CLIENT, CHILDID_SELF);
    }
#endif  // defined(ACCESSIBILITY)
  }

  if (mWindowType == eWindowType_popup) {
    const bool shouldUseDropShadow = [&] {
      if (mTransparencyMode == eTransparencyTransparent) {
        return false;
      }
      if (HasBogusPopupsDropShadowOnMultiMonitor() &&
          WinUtils::GetMonitorCount() > 1 &&
          !gfxWindowsPlatform::GetPlatform()->DwmCompositionEnabled()) {
        // See bug 603793. When we try to draw D3D9/10 windows with a drop
        // shadow without the DWM on a secondary monitor, windows fails to
        // composite our windows correctly. We therefor switch off the drop
        // shadow for pop-up windows when the DWM is disabled and two monitors
        // are connected.
        return false;
      }
      return true;
    }();

    static bool sShadowEnabled = true;
    if (sShadowEnabled != shouldUseDropShadow) {
      ::SetClassLongA(mWnd, GCL_STYLE, shouldUseDropShadow ? CS_DROPSHADOW : 0);
      sShadowEnabled = shouldUseDropShadow;
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

  if (mWnd) {
    if (bState) {
      if (!wasVisible && mWindowType == eWindowType_toplevel) {
        // speed up the initial paint after show for
        // top level windows:
        syncInvalidate = true;

        // Set the cursor before showing the window to avoid the default wait
        // cursor.
        SetCursor(Cursor{eCursor_standard});

        switch (mFrameState->GetSizeMode()) {
          case nsSizeMode_Fullscreen:
            ::ShowWindow(mWnd, SW_SHOW);
            break;
          case nsSizeMode_Maximized:
            ::ShowWindow(mWnd, SW_SHOWMAXIMIZED);
            break;
          case nsSizeMode_Minimized:
            ::ShowWindow(mWnd, SW_SHOWMINIMIZED);
            break;
          default:
            if (CanTakeFocus() && !mAlwaysOnTop) {
              ::ShowWindow(mWnd, SW_SHOWNORMAL);
            } else {
              ::ShowWindow(mWnd, SW_SHOWNOACTIVATE);
              // Don't flicker the window if we're restoring session
              if (!sIsRestoringSession) {
                Unused << GetAttention(2);
              }
            }
            break;
        }
      } else {
        DWORD flags = SWP_NOSIZE | SWP_NOMOVE | SWP_SHOWWINDOW;
        if (wasVisible) flags |= SWP_NOZORDER;
        if (mAlwaysOnTop) flags |= SWP_NOACTIVATE;

        if (mWindowType == eWindowType_popup) {
          // ensure popups are the topmost of the TOPMOST
          // layer. Remember not to set the SWP_NOZORDER
          // flag as that might allow the taskbar to overlap
          // the popup.
          flags |= SWP_NOACTIVATE;
          HWND owner = ::GetWindow(mWnd, GW_OWNER);
          if (owner) {
            // ePopupLevelTop popups should be above all else.  All other
            // types should be placed in front of their owner, without
            // changing the owner's z-level relative to other windows.
            if (PopupLevel() != ePopupLevelTop) {
              ::SetWindowPos(mWnd, owner, 0, 0, 0, 0, flags);
              ::SetWindowPos(owner, mWnd, 0, 0, 0, 0,
                             SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            } else {
              ::SetWindowPos(mWnd, HWND_TOP, 0, 0, 0, 0, flags);
            }
          } else {
            ::SetWindowPos(mWnd, HWND_TOPMOST, 0, 0, 0, 0, flags);
          }
        } else {
          if (mWindowType == eWindowType_dialog && !CanTakeFocus())
            flags |= SWP_NOACTIVATE;

          ::SetWindowPos(mWnd, HWND_TOP, 0, 0, 0, 0, flags);
        }
      }

      if (!wasVisible && (mWindowType == eWindowType_toplevel ||
                          mWindowType == eWindowType_dialog)) {
        // When a toplevel window or dialog is shown, initialize the UI state
        ::SendMessageW(mWnd, WM_CHANGEUISTATE,
                       MAKEWPARAM(UIS_SET, UISF_HIDEFOCUS | UISF_HIDEACCEL), 0);
      }
    } else {
      // Clear contents to avoid ghosting of old content if we display
      // this window again.
      if (wasVisible && mTransparencyMode == eTransparencyTransparent) {
        if (mCompositorWidgetDelegate) {
          mCompositorWidgetDelegate->ClearTransparentWindow();
        }
      }
      if (mWindowType != eWindowType_dialog) {
        ::ShowWindow(mWnd, SW_HIDE);
      } else {
        ::SetWindowPos(mWnd, 0, 0, 0, 0, 0,
                       SWP_HIDEWINDOW | SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER |
                           SWP_NOACTIVATE);
      }
    }
  }

  if (!wasVisible && bState) {
    Invalidate();
    if (syncInvalidate && !mInDtor && !mOnDestroyCalled) {
      ::UpdateWindow(mWnd);
    }
  }

  if (mOpeningAnimationSuppressed) {
    SuppressAnimation(false);
  }
}

/**************************************************************
 *
 * SECTION: nsIWidget::IsVisible
 *
 * Returns the visibility state.
 *
 **************************************************************/

// Return true if the whether the component is visible, false otherwise
bool nsWindow::IsVisible() const { return mIsVisible; }

/**************************************************************
 *
 * SECTION: Window clipping utilities
 *
 * Used in Size and Move operations for setting the proper
 * window clipping regions for window transparency.
 *
 **************************************************************/

// XP and Vista visual styles sometimes require window clipping regions to be
// applied for proper transparency. These routines are called on size and move
// operations.
// XXX this is apparently still needed in Windows 7 and later
void nsWindow::ClearThemeRegion() {
  if (!HasGlass() &&
      (mWindowType == eWindowType_popup && !IsPopupWithTitleBar() &&
       (mPopupType == ePopupTypeTooltip || mPopupType == ePopupTypePanel))) {
    SetWindowRgn(mWnd, nullptr, false);
  }
}

/**************************************************************
 *
 * SECTION: Touch and APZ-related functions
 *
 **************************************************************/

void nsWindow::RegisterTouchWindow() {
  mTouchWindow = true;
  ::RegisterTouchWindow(mWnd, TWF_WANTPALM);
  ::EnumChildWindows(mWnd, nsWindow::RegisterTouchForDescendants, 0);
}

BOOL CALLBACK nsWindow::RegisterTouchForDescendants(HWND aWnd, LPARAM aMsg) {
  nsWindow* win = WinUtils::GetNSWindowPtr(aWnd);
  if (win) {
    ::RegisterTouchWindow(aWnd, TWF_WANTPALM);
  }
  return TRUE;
}

void nsWindow::LockAspectRatio(bool aShouldLock) {
  if (aShouldLock) {
    mAspectRatio = (float)mBounds.Width() / (float)mBounds.Height();
  } else {
    mAspectRatio = 0.0;
  }
}

/**************************************************************
 *
 * SECTION: nsIWidget::SetWindowMouseTransparent
 *
 * Sets whether the window should ignore mouse events.
 *
 **************************************************************/
void nsWindow::SetWindowMouseTransparent(bool aIsTransparent) {
  if (!mWnd) {
    return;
  }

  LONG_PTR oldStyle = ::GetWindowLongPtrW(mWnd, GWL_EXSTYLE);
  LONG_PTR newStyle = aIsTransparent ? (oldStyle | WS_EX_TRANSPARENT)
                                     : (oldStyle & ~WS_EX_TRANSPARENT);
  ::SetWindowLongPtrW(mWnd, GWL_EXSTYLE, newStyle);
  mMouseTransparent = aIsTransparent;
}

/**************************************************************
 *
 * SECTION: nsIWidget::Move, nsIWidget::Resize,
 * nsIWidget::Size, nsIWidget::BeginResizeDrag
 *
 * Repositioning and sizing a window.
 *
 **************************************************************/

void nsWindow::SetSizeConstraints(const SizeConstraints& aConstraints) {
  SizeConstraints c = aConstraints;

  if (mWindowType != eWindowType_popup && mResizable) {
    c.mMinSize.width =
        std::max(int32_t(::GetSystemMetrics(SM_CXMINTRACK)), c.mMinSize.width);
    c.mMinSize.height =
        std::max(int32_t(::GetSystemMetrics(SM_CYMINTRACK)), c.mMinSize.height);
  }

  if (mMaxTextureSize > 0) {
    // We can't make ThebesLayers bigger than this anyway.. no point it letting
    // a window grow bigger as we won't be able to draw content there in
    // general.
    c.mMaxSize.width = std::min(c.mMaxSize.width, mMaxTextureSize);
    c.mMaxSize.height = std::min(c.mMaxSize.height, mMaxTextureSize);
  }

  mSizeConstraintsScale = GetDefaultScale().scale;

  nsBaseWidget::SetSizeConstraints(c);
}

const SizeConstraints nsWindow::GetSizeConstraints() {
  double scale = GetDefaultScale().scale;
  if (mSizeConstraintsScale == scale || mSizeConstraintsScale == 0.0) {
    return mSizeConstraints;
  }
  scale /= mSizeConstraintsScale;
  SizeConstraints c = mSizeConstraints;
  if (c.mMinSize.width != NS_MAXSIZE) {
    c.mMinSize.width = NSToIntRound(c.mMinSize.width * scale);
  }
  if (c.mMinSize.height != NS_MAXSIZE) {
    c.mMinSize.height = NSToIntRound(c.mMinSize.height * scale);
  }
  if (c.mMaxSize.width != NS_MAXSIZE) {
    c.mMaxSize.width = NSToIntRound(c.mMaxSize.width * scale);
  }
  if (c.mMaxSize.height != NS_MAXSIZE) {
    c.mMaxSize.height = NSToIntRound(c.mMaxSize.height * scale);
  }
  return c;
}

// Move this component
void nsWindow::Move(double aX, double aY) {
  if (mWindowType == eWindowType_toplevel ||
      mWindowType == eWindowType_dialog) {
    SetSizeMode(nsSizeMode_Normal);
  }

  // for top-level windows only, convert coordinates from desktop pixels
  // (the "parent" coordinate space) to the window's device pixel space
  double scale =
      BoundsUseDesktopPixels() ? GetDesktopToDeviceScale().scale : 1.0;
  int32_t x = NSToIntRound(aX * scale);
  int32_t y = NSToIntRound(aY * scale);

  // Check to see if window needs to be moved first
  // to avoid a costly call to SetWindowPos. This check
  // can not be moved to the calling code in nsView, because
  // some platforms do not position child windows correctly

  // Only perform this check for non-popup windows, since the positioning can
  // in fact change even when the x/y do not.  We always need to perform the
  // check. See bug #97805 for details.
  if (mWindowType != eWindowType_popup && mBounds.IsEqualXY(x, y)) {
    // Nothing to do, since it is already positioned correctly.
    return;
  }

  mBounds.MoveTo(x, y);

  if (mWnd) {
#ifdef DEBUG
    // complain if a window is moved offscreen (legal, but potentially
    // worrisome)
    if (mIsTopWidgetWindow) {  // only a problem for top-level windows
      // Make sure this window is actually on the screen before we move it
      // XXX: Needs multiple monitor support
      HDC dc = ::GetDC(mWnd);
      if (dc) {
        if (::GetDeviceCaps(dc, TECHNOLOGY) == DT_RASDISPLAY) {
          RECT workArea;
          ::SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
          // no annoying assertions. just mention the issue.
          if (x < 0 || x >= workArea.right || y < 0 || y >= workArea.bottom) {
            MOZ_LOG(gWindowsLog, LogLevel::Info,
                    ("window moved to offscreen position\n"));
          }
        }
        ::ReleaseDC(mWnd, dc);
      }
    }
#endif

    // Normally, when the skeleton UI is disabled, we resize+move the window
    // before showing it in order to ensure that it restores to the correct
    // position when the user un-maximizes it. However, when we are using the
    // skeleton UI, this results in the skeleton UI window being moved around
    // undesirably before being locked back into the maximized position. To
    // avoid this, we simply set the placement to restore to via
    // SetWindowPlacement. It's a little bit more of a dance, though, since we
    // need to convert the workspace coords that SetWindowPlacement uses to the
    // screen space coordinates we normally use with SetWindowPos.
    if (mIsShowingPreXULSkeletonUI && WasPreXULSkeletonUIMaximized()) {
      WINDOWPLACEMENT pl = {sizeof(WINDOWPLACEMENT)};
      VERIFY(::GetWindowPlacement(mWnd, &pl));

      HMONITOR monitor = ::MonitorFromWindow(mWnd, MONITOR_DEFAULTTONULL);
      if (NS_WARN_IF(!monitor)) {
        return;
      }
      MONITORINFO mi = {sizeof(MONITORINFO)};
      VERIFY(::GetMonitorInfo(monitor, &mi));

      int32_t deltaX =
          x + mi.rcWork.left - mi.rcMonitor.left - pl.rcNormalPosition.left;
      int32_t deltaY =
          y + mi.rcWork.top - mi.rcMonitor.top - pl.rcNormalPosition.top;
      pl.rcNormalPosition.left += deltaX;
      pl.rcNormalPosition.right += deltaX;
      pl.rcNormalPosition.top += deltaY;
      pl.rcNormalPosition.bottom += deltaY;
      VERIFY(::SetWindowPlacement(mWnd, &pl));
    } else {
      ClearThemeRegion();

      UINT flags = SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSIZE;
      double oldScale = mDefaultScale;
      mResizeState = IN_SIZEMOVE;
      VERIFY(::SetWindowPos(mWnd, nullptr, x, y, 0, 0, flags));
      mResizeState = NOT_RESIZING;
      if (WinUtils::LogToPhysFactor(mWnd) != oldScale) {
        ChangedDPI();
      }
    }

    ResizeDirectManipulationViewport();
  }
}

// Resize this component
void nsWindow::Resize(double aWidth, double aHeight, bool aRepaint) {
  // for top-level windows only, convert coordinates from desktop pixels
  // (the "parent" coordinate space) to the window's device pixel space
  double scale =
      BoundsUseDesktopPixels() ? GetDesktopToDeviceScale().scale : 1.0;
  int32_t width = NSToIntRound(aWidth * scale);
  int32_t height = NSToIntRound(aHeight * scale);

  NS_ASSERTION((width >= 0), "Negative width passed to nsWindow::Resize");
  NS_ASSERTION((height >= 0), "Negative height passed to nsWindow::Resize");
  if (width < 0 || height < 0) {
    gfxCriticalNoteOnce << "Negative passed to Resize(" << width << ", "
                        << height << ") repaint: " << aRepaint;
  }

  ConstrainSize(&width, &height);

  // Avoid unnecessary resizing calls
  if (mBounds.IsEqualSize(width, height)) {
    if (aRepaint) {
      Invalidate();
    }
    return;
  }

  // Set cached value for lightweight and printing
  bool wasLocking = mAspectRatio != 0.0;
  mBounds.SizeTo(width, height);
  if (wasLocking) {
    LockAspectRatio(true);  // This causes us to refresh the mAspectRatio value
  }

  if (mWnd) {
    // Refer to the comment above a similar check in nsWindow::Move
    if (mIsShowingPreXULSkeletonUI && WasPreXULSkeletonUIMaximized()) {
      WINDOWPLACEMENT pl = {sizeof(WINDOWPLACEMENT)};
      VERIFY(::GetWindowPlacement(mWnd, &pl));
      pl.rcNormalPosition.right = pl.rcNormalPosition.left + width;
      pl.rcNormalPosition.bottom = pl.rcNormalPosition.top + GetHeight(height);
      mResizeState = RESIZING;
      VERIFY(::SetWindowPlacement(mWnd, &pl));
      mResizeState = NOT_RESIZING;
    } else {
      UINT flags = SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE;

      if (!aRepaint) {
        flags |= SWP_NOREDRAW;
      }

      ClearThemeRegion();
      double oldScale = mDefaultScale;
      mResizeState = RESIZING;
      VERIFY(
          ::SetWindowPos(mWnd, nullptr, 0, 0, width, GetHeight(height), flags));

      mResizeState = NOT_RESIZING;
      if (WinUtils::LogToPhysFactor(mWnd) != oldScale) {
        ChangedDPI();
      }
    }

    ResizeDirectManipulationViewport();
  }

  if (aRepaint) Invalidate();
}

// Resize this component
void nsWindow::Resize(double aX, double aY, double aWidth, double aHeight,
                      bool aRepaint) {
  // for top-level windows only, convert coordinates from desktop pixels
  // (the "parent" coordinate space) to the window's device pixel space
  double scale =
      BoundsUseDesktopPixels() ? GetDesktopToDeviceScale().scale : 1.0;
  int32_t x = NSToIntRound(aX * scale);
  int32_t y = NSToIntRound(aY * scale);
  int32_t width = NSToIntRound(aWidth * scale);
  int32_t height = NSToIntRound(aHeight * scale);

  NS_ASSERTION((width >= 0), "Negative width passed to nsWindow::Resize");
  NS_ASSERTION((height >= 0), "Negative height passed to nsWindow::Resize");
  if (width < 0 || height < 0) {
    gfxCriticalNoteOnce << "Negative passed to Resize(" << x << " ," << y
                        << ", " << width << ", " << height
                        << ") repaint: " << aRepaint;
  }

  ConstrainSize(&width, &height);

  // Avoid unnecessary resizing calls
  if (mBounds.IsEqualRect(x, y, width, height)) {
    if (aRepaint) {
      Invalidate();
    }
    return;
  }

  // Set cached value for lightweight and printing
  mBounds.SetRect(x, y, width, height);

  if (mWnd) {
    // Refer to the comment above a similar check in nsWindow::Move
    if (mIsShowingPreXULSkeletonUI && WasPreXULSkeletonUIMaximized()) {
      WINDOWPLACEMENT pl = {sizeof(WINDOWPLACEMENT)};
      VERIFY(::GetWindowPlacement(mWnd, &pl));

      HMONITOR monitor = ::MonitorFromWindow(mWnd, MONITOR_DEFAULTTONULL);
      if (NS_WARN_IF(!monitor)) {
        return;
      }
      MONITORINFO mi = {sizeof(MONITORINFO)};
      VERIFY(::GetMonitorInfo(monitor, &mi));

      int32_t deltaX =
          x + mi.rcWork.left - mi.rcMonitor.left - pl.rcNormalPosition.left;
      int32_t deltaY =
          y + mi.rcWork.top - mi.rcMonitor.top - pl.rcNormalPosition.top;
      pl.rcNormalPosition.left += deltaX;
      pl.rcNormalPosition.right = pl.rcNormalPosition.left + width;
      pl.rcNormalPosition.top += deltaY;
      pl.rcNormalPosition.bottom = pl.rcNormalPosition.top + GetHeight(height);
      VERIFY(::SetWindowPlacement(mWnd, &pl));
    } else {
      UINT flags = SWP_NOZORDER | SWP_NOACTIVATE;
      if (!aRepaint) {
        flags |= SWP_NOREDRAW;
      }

      ClearThemeRegion();

      double oldScale = mDefaultScale;
      mResizeState = RESIZING;
      VERIFY(
          ::SetWindowPos(mWnd, nullptr, x, y, width, GetHeight(height), flags));
      mResizeState = NOT_RESIZING;
      if (WinUtils::LogToPhysFactor(mWnd) != oldScale) {
        ChangedDPI();
      }

      if (mTransitionWnd) {
        // If we have a fullscreen transition window, we need to make
        // it topmost again, otherwise the taskbar may be raised by
        // the system unexpectedly when we leave fullscreen state.
        ::SetWindowPos(mTransitionWnd, HWND_TOPMOST, 0, 0, 0, 0,
                       SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
      }
    }

    ResizeDirectManipulationViewport();
  }

  if (aRepaint) Invalidate();
}

mozilla::Maybe<bool> nsWindow::IsResizingNativeWidget() {
  if (mResizeState == RESIZING) {
    return Some(true);
  }
  return Some(false);
}

nsresult nsWindow::BeginResizeDrag(WidgetGUIEvent* aEvent, int32_t aHorizontal,
                                   int32_t aVertical) {
  NS_ENSURE_ARG_POINTER(aEvent);

  if (aEvent->mClass != eMouseEventClass) {
    // you can only begin a resize drag with a mouse event
    return NS_ERROR_INVALID_ARG;
  }

  if (aEvent->AsMouseEvent()->mButton != MouseButton::ePrimary) {
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
                POINTTOPOINTS(aEvent->mRefPoint));

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
void nsWindow::PlaceBehind(nsTopLevelWidgetZPlacement aPlacement,
                           nsIWidget* aWidget, bool aActivate) {
  HWND behind = HWND_TOP;
  if (aPlacement == eZPlacementBottom)
    behind = HWND_BOTTOM;
  else if (aPlacement == eZPlacementBelow && aWidget)
    behind = (HWND)aWidget->GetNativeData(NS_NATIVE_WINDOW);
  UINT flags = SWP_NOMOVE | SWP_NOREPOSITION | SWP_NOSIZE;
  if (!aActivate) flags |= SWP_NOACTIVATE;

  if (!CanTakeFocus() && behind == HWND_TOP) {
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
}

static UINT GetCurrentShowCmd(HWND aWnd) {
  WINDOWPLACEMENT pl;
  pl.length = sizeof(pl);
  ::GetWindowPlacement(aWnd, &pl);
  return pl.showCmd;
}

// Maximize, minimize or restore the window.
void nsWindow::SetSizeMode(nsSizeMode aMode) {
  // If we are still displaying a maximized pre-XUL skeleton UI, ignore the
  // noise of sizemode changes. Once we have "shown" the window for the first
  // time (called nsWindow::Show(true), even though the window is already
  // technically displayed), we will again accept sizemode changes.
  if (mIsShowingPreXULSkeletonUI && WasPreXULSkeletonUIMaximized()) {
    return;
  }

  mFrameState->EnsureSizeMode(aMode);
}

nsSizeMode nsWindow::SizeMode() { return mFrameState->GetSizeMode(); }

void DoGetWorkspaceID(HWND aWnd, nsAString* aWorkspaceID) {
  RefPtr<IVirtualDesktopManager> desktopManager = gVirtualDesktopManager;
  if (!desktopManager || !aWnd) {
    return;
  }

  GUID desktop;
  HRESULT hr = desktopManager->GetWindowDesktopId(aWnd, &desktop);
  if (FAILED(hr)) {
    return;
  }

  RPC_WSTR workspaceIDStr = nullptr;
  if (UuidToStringW(&desktop, &workspaceIDStr) == RPC_S_OK) {
    aWorkspaceID->Assign((wchar_t*)workspaceIDStr);
    RpcStringFreeW(&workspaceIDStr);
  }
}

void nsWindow::GetWorkspaceID(nsAString& workspaceID) {
  // If we have a value cached, use that, but also make sure it is
  // scheduled to be updated.  If we don't yet have a value, get
  // one synchronously.
  auto desktop = mDesktopId.Lock();
  if (desktop->mID.IsEmpty()) {
    DoGetWorkspaceID(mWnd, &desktop->mID);
    desktop->mUpdateIsQueued = false;
  } else {
    AsyncUpdateWorkspaceID(*desktop);
  }

  workspaceID = desktop->mID;
}

void nsWindow::AsyncUpdateWorkspaceID(Desktop& aDesktop) {
  struct UpdateWorkspaceIdTask : public Task {
    explicit UpdateWorkspaceIdTask(nsWindow* aSelf)
        : Task(false /* mainThread */, EventQueuePriority::Normal),
          mSelf(aSelf) {}

    bool Run() override {
      auto desktop = mSelf->mDesktopId.Lock();
      if (desktop->mUpdateIsQueued) {
        DoGetWorkspaceID(mSelf->mWnd, &desktop->mID);
        desktop->mUpdateIsQueued = false;
      }
      return true;
    }

#ifdef MOZ_COLLECTING_RUNNABLE_TELEMETRY
    bool GetName(nsACString& aName) override {
      aName.AssignLiteral("UpdateWorkspaceIdTask");
      return true;
    }
#endif

    RefPtr<nsWindow> mSelf;
  };

  if (aDesktop.mUpdateIsQueued) {
    return;
  }

  aDesktop.mUpdateIsQueued = true;
  TaskController::Get()->AddTask(MakeAndAddRef<UpdateWorkspaceIdTask>(this));
}

void nsWindow::MoveToWorkspace(const nsAString& workspaceID) {
  RefPtr<IVirtualDesktopManager> desktopManager = gVirtualDesktopManager;
  if (!desktopManager) {
    return;
  }

  GUID desktop;
  const nsString flat = PromiseFlatString(workspaceID);
  RPC_WSTR workspaceIDStr = reinterpret_cast<RPC_WSTR>((wchar_t*)flat.get());
  if (UuidFromStringW(workspaceIDStr, &desktop) == RPC_S_OK) {
    if (SUCCEEDED(desktopManager->MoveWindowToDesktop(mWnd, desktop))) {
      auto desktop = mDesktopId.Lock();
      desktop->mID = workspaceID;
    }
  }
}

void nsWindow::SuppressAnimation(bool aSuppress) {
  DWORD dwAttribute = aSuppress ? TRUE : FALSE;
  DwmSetWindowAttribute(mWnd, DWMWA_TRANSITIONS_FORCEDISABLED, &dwAttribute,
                        sizeof dwAttribute);
}

// Constrain a potential move to fit onscreen
// Position (aX, aY) is specified in Windows screen (logical) pixels,
// except when using per-monitor DPI, in which case it's device pixels.
void nsWindow::ConstrainPosition(bool aAllowSlop, int32_t* aX, int32_t* aY) {
  if (!mIsTopWidgetWindow)  // only a problem for top-level windows
    return;

  double dpiScale = GetDesktopToDeviceScale().scale;

  // We need to use the window size in the kind of pixels used for window-
  // manipulation APIs.
  int32_t logWidth =
      std::max<int32_t>(NSToIntRound(mBounds.Width() / dpiScale), 1);
  int32_t logHeight =
      std::max<int32_t>(NSToIntRound(mBounds.Height() / dpiScale), 1);

  /* get our playing field. use the current screen, or failing that
  for any reason, use device caps for the default screen. */
  RECT screenRect;

  nsCOMPtr<nsIScreenManager> screenmgr =
      do_GetService(sScreenManagerContractID);
  if (!screenmgr) {
    return;
  }
  nsCOMPtr<nsIScreen> screen;
  int32_t left, top, width, height;

  screenmgr->ScreenForRect(*aX, *aY, logWidth, logHeight,
                           getter_AddRefs(screen));
  if (mFrameState->GetSizeMode() != nsSizeMode_Fullscreen) {
    // For normalized windows, use the desktop work area.
    nsresult rv = screen->GetAvailRectDisplayPix(&left, &top, &width, &height);
    if (NS_FAILED(rv)) {
      return;
    }
  } else {
    // For full screen windows, use the desktop.
    nsresult rv = screen->GetRectDisplayPix(&left, &top, &width, &height);
    if (NS_FAILED(rv)) {
      return;
    }
  }
  screenRect.left = left;
  screenRect.right = left + width;
  screenRect.top = top;
  screenRect.bottom = top + height;

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
}

/**************************************************************
 *
 * SECTION: nsIWidget::Enable, nsIWidget::IsEnabled
 *
 * Enabling and disabling the widget.
 *
 **************************************************************/

// Enable/disable this component
void nsWindow::Enable(bool bState) {
  if (mWnd) {
    ::EnableWindow(mWnd, bState);
  }
}

// Return the current enable state
bool nsWindow::IsEnabled() const {
  return !mWnd || (::IsWindowEnabled(mWnd) &&
                   ::IsWindowEnabled(::GetAncestor(mWnd, GA_ROOT)));
}

/**************************************************************
 *
 * SECTION: nsIWidget::SetFocus
 *
 * Give the focus to this widget.
 *
 **************************************************************/

void nsWindow::SetFocus(Raise aRaise, mozilla::dom::CallerType aCallerType) {
  if (mWnd) {
#ifdef WINSTATE_DEBUG_OUTPUT
    if (mWnd == WinUtils::GetTopLevelHWND(mWnd)) {
      MOZ_LOG(gWindowsLog, LogLevel::Info,
              ("*** SetFocus: [  top] raise=%d\n", aRaise == Raise::Yes));
    } else {
      MOZ_LOG(gWindowsLog, LogLevel::Info,
              ("*** SetFocus: [child] raise=%d\n", aRaise == Raise::Yes));
    }
#endif
    // Uniconify, if necessary
    HWND toplevelWnd = WinUtils::GetTopLevelHWND(mWnd);
    if (aRaise == Raise::Yes && ::IsIconic(toplevelWnd)) {
      ::ShowWindow(toplevelWnd, SW_RESTORE);
    }
    ::SetFocus(mWnd);
  }
}

/**************************************************************
 *
 * SECTION: Bounds
 *
 * GetBounds, GetClientBounds, GetScreenBounds,
 * GetRestoredBounds, GetClientOffset
 * SetDrawsInTitlebar, SetNonClientMargins
 *
 * Bound calculations.
 *
 **************************************************************/

// Return the window's full dimensions in screen coordinates.
// If the window has a parent, converts the origin to an offset
// of the parent's screen origin.
LayoutDeviceIntRect nsWindow::GetBounds() {
  if (!mWnd) {
    return mBounds;
  }

  RECT r;
  VERIFY(::GetWindowRect(mWnd, &r));

  LayoutDeviceIntRect rect;

  // assign size
  rect.SizeTo(r.right - r.left, r.bottom - r.top);

  // popup window bounds' are in screen coordinates, not relative to parent
  // window
  if (mWindowType == eWindowType_popup) {
    rect.MoveTo(r.left, r.top);
    return rect;
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
    r.top -= pr.top;
    // adjust for chrome
    nsWindow* pWidget = static_cast<nsWindow*>(GetParent());
    if (pWidget && pWidget->IsTopLevelWidget()) {
      LayoutDeviceIntPoint clientOffset = pWidget->GetClientOffset();
      r.left -= clientOffset.x;
      r.top -= clientOffset.y;
    }
  }
  rect.MoveTo(r.left, r.top);
  if (mCompositorSession &&
      !wr::WindowSizeSanityCheck(rect.width, rect.height)) {
    gfxCriticalNoteOnce << "Invalid size" << rect << " size mode "
                        << mFrameState->GetSizeMode();
  }

  return rect;
}

// Get this component dimension
LayoutDeviceIntRect nsWindow::GetClientBounds() {
  if (!mWnd) {
    return LayoutDeviceIntRect(0, 0, 0, 0);
  }

  RECT r;
  if (!::GetClientRect(mWnd, &r)) {
    MOZ_ASSERT_UNREACHABLE("unexpected to be called");
    gfxCriticalNoteOnce << "GetClientRect failed " << ::GetLastError();
    return mBounds;
  }

  LayoutDeviceIntRect bounds = GetBounds();
  LayoutDeviceIntRect rect;
  rect.MoveTo(bounds.TopLeft() + GetClientOffset());
  rect.SizeTo(r.right - r.left, r.bottom - r.top);
  return rect;
}

// Like GetBounds, but don't offset by the parent
LayoutDeviceIntRect nsWindow::GetScreenBounds() {
  if (!mWnd) {
    return mBounds;
  }

  RECT r;
  VERIFY(::GetWindowRect(mWnd, &r));

  return LayoutDeviceIntRect(r.left, r.top, r.right - r.left, r.bottom - r.top);
}

nsresult nsWindow::GetRestoredBounds(LayoutDeviceIntRect& aRect) {
  if (SizeMode() == nsSizeMode_Normal) {
    aRect = GetScreenBounds();
    return NS_OK;
  }
  if (!mWnd) {
    return NS_ERROR_FAILURE;
  }

  WINDOWPLACEMENT pl = {sizeof(WINDOWPLACEMENT)};
  VERIFY(::GetWindowPlacement(mWnd, &pl));
  const RECT& r = pl.rcNormalPosition;

  HMONITOR monitor = ::MonitorFromWindow(mWnd, MONITOR_DEFAULTTONULL);
  if (!monitor) {
    return NS_ERROR_FAILURE;
  }
  MONITORINFO mi = {sizeof(MONITORINFO)};
  VERIFY(::GetMonitorInfo(monitor, &mi));

  aRect.SetRect(r.left, r.top, r.right - r.left, r.bottom - r.top);
  aRect.MoveBy(mi.rcWork.left - mi.rcMonitor.left,
               mi.rcWork.top - mi.rcMonitor.top);
  return NS_OK;
}

// Return the x,y offset of the client area from the origin of the window. If
// the window is borderless returns (0,0).
LayoutDeviceIntPoint nsWindow::GetClientOffset() {
  if (!mWnd) {
    return LayoutDeviceIntPoint(0, 0);
  }

  RECT r1;
  GetWindowRect(mWnd, &r1);
  LayoutDeviceIntPoint pt = WidgetToScreenOffset();
  return LayoutDeviceIntPoint(pt.x - r1.left, pt.y - r1.top);
}

void nsWindow::SetDrawsInTitlebar(bool aState) {
  nsWindow* window = GetTopLevelWindow(true);
  if (window && window != this) {
    return window->SetDrawsInTitlebar(aState);
  }

  if (aState) {
    // top, right, bottom, left for nsIntMargin
    LayoutDeviceIntMargin margins(0, -1, -1, -1);
    SetNonClientMargins(margins);
  } else {
    LayoutDeviceIntMargin margins(-1, -1, -1, -1);
    SetNonClientMargins(margins);
  }
}

void nsWindow::ResetLayout() {
  // This will trigger a frame changed event, triggering
  // nc calc size and a sizemode gecko event.
  SetWindowPos(mWnd, 0, 0, 0, 0, 0,
               SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOMOVE |
                   SWP_NOOWNERZORDER | SWP_NOSIZE | SWP_NOZORDER);

  // If hidden, just send the frame changed event for now.
  if (!mIsVisible) return;

  // Send a gecko size event to trigger reflow.
  RECT clientRc = {0};
  GetClientRect(mWnd, &clientRc);
  OnResize(WinUtils::ToIntRect(clientRc).Size());

  // Invalidate and update
  Invalidate();
}

// Internally track the caption status via a window property. Required
// due to our internal handling of WM_NCACTIVATE when custom client
// margins are set.
static const wchar_t kManageWindowInfoProperty[] = L"ManageWindowInfoProperty";
typedef BOOL(WINAPI* GetWindowInfoPtr)(HWND hwnd, PWINDOWINFO pwi);
static WindowsDllInterceptor::FuncHookType<GetWindowInfoPtr>
    sGetWindowInfoPtrStub;

BOOL WINAPI GetWindowInfoHook(HWND hWnd, PWINDOWINFO pwi) {
  if (!sGetWindowInfoPtrStub) {
    NS_ASSERTION(FALSE, "Something is horribly wrong in GetWindowInfoHook!");
    return FALSE;
  }
  int windowStatus =
      reinterpret_cast<LONG_PTR>(GetPropW(hWnd, kManageWindowInfoProperty));
  // No property set, return the default data.
  if (!windowStatus) return sGetWindowInfoPtrStub(hWnd, pwi);
  // Call GetWindowInfo and update dwWindowStatus with our
  // internally tracked value.
  BOOL result = sGetWindowInfoPtrStub(hWnd, pwi);
  if (result && pwi)
    pwi->dwWindowStatus = (windowStatus == 1 ? 0 : WS_ACTIVECAPTION);
  return result;
}

void nsWindow::UpdateGetWindowInfoCaptionStatus(bool aActiveCaption) {
  if (!mWnd) return;

  sUser32Intercept.Init("user32.dll");
  sGetWindowInfoPtrStub.Set(sUser32Intercept, "GetWindowInfo",
                            &GetWindowInfoHook);
  if (!sGetWindowInfoPtrStub) {
    return;
  }

  // Update our internally tracked caption status
  SetPropW(mWnd, kManageWindowInfoProperty,
           reinterpret_cast<HANDLE>(static_cast<INT_PTR>(aActiveCaption) + 1));
}

#define DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1 19
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20

void nsWindow::UpdateDarkModeToolbar() {
  if (!IsWin10OrLater()) {
    return;
  }
  LookAndFeel::EnsureColorSchemesInitialized();
  BOOL dark = LookAndFeel::ColorSchemeForChrome() == ColorScheme::Dark;
  DwmSetWindowAttribute(mWnd, DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1, &dark,
                        sizeof dark);
  DwmSetWindowAttribute(mWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark,
                        sizeof dark);
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
bool nsWindow::UpdateNonClientMargins(int32_t aSizeMode, bool aReflowWindow) {
  if (!mCustomNonClient) return false;

  if (aSizeMode == -1) {
    aSizeMode = mFrameState->GetSizeMode();
  }

  bool hasCaption = (mBorderStyle & (eBorderStyle_all | eBorderStyle_title |
                                     eBorderStyle_menu | eBorderStyle_default));

  float dpi = GetDPI();

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
  // `WinUtils::GetSystemMetricsForDpi(SM_CYFRAME, dpi)`
  mCaptionHeight =
      WinUtils::GetSystemMetricsForDpi(SM_CYFRAME, dpi) +
      (hasCaption ? WinUtils::GetSystemMetricsForDpi(SM_CYCAPTION, dpi) +
                        WinUtils::GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi)
                  : 0);
  if (!mUseResizeMarginOverrides) {
    // mHorResizeMargin is the size of the default NC areas on the
    // left and right sides of our window.  It is calculated as
    // the sum of:
    //      SM_CXFRAME        - The thickness of the sizing border
    //      SM_CXPADDEDBORDER - The amount of border padding
    //                          for captioned windows
    //
    // If the window does not have a caption, mHorResizeMargin will be equal to
    // `WinUtils::GetSystemMetricsForDpi(SM_CXFRAME, dpi)`
    mHorResizeMargin =
        WinUtils::GetSystemMetricsForDpi(SM_CXFRAME, dpi) +
        (hasCaption ? WinUtils::GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi)
                    : 0);

    // mVertResizeMargin is the size of the default NC area at the
    // bottom of the window. It is calculated as the sum of:
    //      SM_CYFRAME        - The thickness of the sizing border
    //      SM_CXPADDEDBORDER - The amount of border padding
    //                          for captioned windows.
    //
    // If the window does not have a caption, mVertResizeMargin will be equal to
    // `WinUtils::GetSystemMetricsForDpi(SM_CYFRAME, dpi)`
    mVertResizeMargin =
        WinUtils::GetSystemMetricsForDpi(SM_CYFRAME, dpi) +
        (hasCaption ? WinUtils::GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi)
                    : 0);
  }

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
    // On Windows 10+, we make the entire frame part of the client area.
    // We leave the default frame sizes for left, right and bottom since
    // Windows will automagically position the edges "offscreen" for maximized
    // windows.
    // On versions prior to Windows 10, we add padding to the widget to
    // circumvent a bug in DwmDefWindowProc (see
    // nsNativeThemeWin::GetWidgetPadding).  We "undo" that padding in
    // WM_NCCALCSIZE by adding the caption (as well as the sizing frame) to the
    // client area.
    // The padding is not needed on Win10+ because we handle window buttons
    // non-natively in the theme.  It also does not work on Win10+ -- it
    // exposes a new issue where widget edges would sometimes appear to bleed
    // into other displays (bug 1614218).
    int verticalResize = 0;
    if (IsWin10OrLater()) {
      verticalResize =
          WinUtils::GetSystemMetricsForDpi(SM_CYFRAME, dpi) +
          (hasCaption ? WinUtils::GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi)
                      : 0);
    }

    mNonClientOffset.top = mCaptionHeight - verticalResize;
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
        HMONITOR taskbarMonitor =
            ::MonitorFromWindow(appBarData.hWnd, MONITOR_DEFAULTTOPRIMARY);
        HMONITOR windowMonitor =
            ::MonitorFromWindow(mWnd, MONITOR_DEFAULTTONEAREST);
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
    bool glass = gfxWindowsPlatform::GetPlatform()->DwmCompositionEnabled();

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
      mNonClientOffset.bottom =
          std::min(mVertResizeMargin, mNonClientMargins.bottom);
    } else if (mNonClientMargins.bottom == 0) {
      mNonClientOffset.bottom = mVertResizeMargin;
    } else {
      mNonClientOffset.bottom = 0;
    }

    if (mNonClientMargins.left > 0 && glass) {
      mNonClientOffset.left =
          std::min(mHorResizeMargin, mNonClientMargins.left);
    } else if (mNonClientMargins.left == 0) {
      mNonClientOffset.left = mHorResizeMargin;
    } else {
      mNonClientOffset.left = 0;
    }

    if (mNonClientMargins.right > 0 && glass) {
      mNonClientOffset.right =
          std::min(mHorResizeMargin, mNonClientMargins.right);
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

nsresult nsWindow::SetNonClientMargins(LayoutDeviceIntMargin& margins) {
  if (!mIsTopWidgetWindow || mBorderStyle == eBorderStyle_none)
    return NS_ERROR_INVALID_ARG;

  if (mHideChrome) {
    mFutureMarginsOnceChromeShows = margins;
    mFutureMarginsToUse = true;
    return NS_OK;
  }
  mFutureMarginsToUse = false;

  // Request for a reset
  if (margins.top == -1 && margins.left == -1 && margins.right == -1 &&
      margins.bottom == -1) {
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

  if (margins.top < -1 || margins.bottom < -1 || margins.left < -1 ||
      margins.right < -1)
    return NS_ERROR_INVALID_ARG;

  mNonClientMargins = margins;
  mCustomNonClient = true;
  if (!UpdateNonClientMargins()) {
    NS_WARNING("UpdateNonClientMargins failed!");
    return NS_OK;
  }

  return NS_OK;
}

void nsWindow::SetResizeMargin(mozilla::LayoutDeviceIntCoord aResizeMargin) {
  mUseResizeMarginOverrides = true;
  mHorResizeMargin = aResizeMargin;
  mVertResizeMargin = aResizeMargin;
  UpdateNonClientMargins();
}

void nsWindow::InvalidateNonClientRegion() {
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
  rect.bottom -= mVertResizeMargin;
  rect.left += mHorResizeMargin;
  MapWindowPoints(nullptr, mWnd, (LPPOINT)&rect, 2);
  HRGN clientRgn = CreateRectRgnIndirect(&rect);
  CombineRgn(winRgn, winRgn, clientRgn, RGN_DIFF);
  DeleteObject(clientRgn);

  // triggers ncpaint and paint events for the two areas
  RedrawWindow(mWnd, nullptr, winRgn, RDW_FRAME | RDW_INVALIDATE);
  DeleteObject(winRgn);
}

HRGN nsWindow::ExcludeNonClientFromPaintRegion(HRGN aRegion) {
  RECT rect;
  HRGN rgn = nullptr;
  if (aRegion == (HRGN)1) {  // undocumented value indicating a full refresh
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

void nsWindow::SetBackgroundColor(const nscolor& aColor) {
  if (mBrush) ::DeleteObject(mBrush);

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
static HCURSOR CursorFor(nsCursor aCursor) {
  switch (aCursor) {
    case eCursor_select:
      return ::LoadCursor(nullptr, IDC_IBEAM);
    case eCursor_wait:
      return ::LoadCursor(nullptr, IDC_WAIT);
    case eCursor_hyperlink:
      return ::LoadCursor(nullptr, IDC_HAND);
    case eCursor_standard:
    case eCursor_context_menu:  // XXX See bug 258960.
      return ::LoadCursor(nullptr, IDC_ARROW);

    case eCursor_n_resize:
    case eCursor_s_resize:
      return ::LoadCursor(nullptr, IDC_SIZENS);

    case eCursor_w_resize:
    case eCursor_e_resize:
      return ::LoadCursor(nullptr, IDC_SIZEWE);

    case eCursor_nw_resize:
    case eCursor_se_resize:
      return ::LoadCursor(nullptr, IDC_SIZENWSE);

    case eCursor_ne_resize:
    case eCursor_sw_resize:
      return ::LoadCursor(nullptr, IDC_SIZENESW);

    case eCursor_crosshair:
      return ::LoadCursor(nullptr, IDC_CROSS);

    case eCursor_move:
      return ::LoadCursor(nullptr, IDC_SIZEALL);

    case eCursor_help:
      return ::LoadCursor(nullptr, IDC_HELP);

    case eCursor_copy:  // CSS3
      return ::LoadCursor(nsToolkit::mDllInstance, MAKEINTRESOURCE(IDC_COPY));

    case eCursor_alias:
      return ::LoadCursor(nsToolkit::mDllInstance, MAKEINTRESOURCE(IDC_ALIAS));

    case eCursor_cell:
      return ::LoadCursor(nsToolkit::mDllInstance, MAKEINTRESOURCE(IDC_CELL));
    case eCursor_grab:
      return ::LoadCursor(nsToolkit::mDllInstance, MAKEINTRESOURCE(IDC_GRAB));

    case eCursor_grabbing:
      return ::LoadCursor(nsToolkit::mDllInstance,
                          MAKEINTRESOURCE(IDC_GRABBING));

    case eCursor_spinning:
      return ::LoadCursor(nullptr, IDC_APPSTARTING);

    case eCursor_zoom_in:
      return ::LoadCursor(nsToolkit::mDllInstance, MAKEINTRESOURCE(IDC_ZOOMIN));

    case eCursor_zoom_out:
      return ::LoadCursor(nsToolkit::mDllInstance,
                          MAKEINTRESOURCE(IDC_ZOOMOUT));

    case eCursor_not_allowed:
    case eCursor_no_drop:
      return ::LoadCursor(nullptr, IDC_NO);

    case eCursor_col_resize:
      return ::LoadCursor(nsToolkit::mDllInstance,
                          MAKEINTRESOURCE(IDC_COLRESIZE));

    case eCursor_row_resize:
      return ::LoadCursor(nsToolkit::mDllInstance,
                          MAKEINTRESOURCE(IDC_ROWRESIZE));

    case eCursor_vertical_text:
      return ::LoadCursor(nsToolkit::mDllInstance,
                          MAKEINTRESOURCE(IDC_VERTICALTEXT));

    case eCursor_all_scroll:
      // XXX not 100% appropriate perhaps
      return ::LoadCursor(nullptr, IDC_SIZEALL);

    case eCursor_nesw_resize:
      return ::LoadCursor(nullptr, IDC_SIZENESW);

    case eCursor_nwse_resize:
      return ::LoadCursor(nullptr, IDC_SIZENWSE);

    case eCursor_ns_resize:
      return ::LoadCursor(nullptr, IDC_SIZENS);

    case eCursor_ew_resize:
      return ::LoadCursor(nullptr, IDC_SIZEWE);

    case eCursor_none:
      return ::LoadCursor(nsToolkit::mDllInstance, MAKEINTRESOURCE(IDC_NONE));

    default:
      NS_ERROR("Invalid cursor type");
      return nullptr;
  }
}

static HCURSOR CursorForImage(const nsIWidget::Cursor& aCursor,
                              CSSToLayoutDeviceScale aScale) {
  if (!aCursor.IsCustom()) {
    return nullptr;
  }

  nsIntSize size = nsIWidget::CustomCursorSize(aCursor);

  // Reject cursors greater than 128 pixels in either direction, to prevent
  // spoofing.
  // XXX ideally we should rescale. Also, we could modify the API to
  // allow trusted content to set larger cursors.
  if (size.width > 128 || size.height > 128) {
    return nullptr;
  }

  LayoutDeviceIntSize layoutSize =
      RoundedToInt(CSSIntSize(size.width, size.height) * aScale);
  LayoutDeviceIntPoint hotspot =
      RoundedToInt(CSSIntPoint(aCursor.mHotspotX, aCursor.mHotspotY) * aScale);
  HCURSOR cursor;
  nsresult rv = nsWindowGfx::CreateIcon(aCursor.mContainer, true, hotspot,
                                        layoutSize, &cursor);
  if (NS_FAILED(rv)) {
    return nullptr;
  }

  return cursor;
}

void nsWindow::SetCursor(const Cursor& aCursor) {
  static HCURSOR sCurrentHCursor = nullptr;
  static bool sCurrentHCursorIsCustom = false;

  mCursor = aCursor;

  if (sCurrentCursor == aCursor && sCurrentHCursor && !mUpdateCursor) {
    // Cursors in windows are global, so even if our mUpdateCursor flag is
    // false we always need to make sure the Windows cursor is up-to-date,
    // since stuff like native drag and drop / resizers code can mutate it
    // outside of this method.
    ::SetCursor(sCurrentHCursor);
    return;
  }

  mUpdateCursor = false;

  if (sCurrentHCursorIsCustom) {
    ::DestroyIcon(sCurrentHCursor);
  }
  sCurrentHCursor = nullptr;
  sCurrentHCursorIsCustom = false;
  sCurrentCursor = aCursor;

  HCURSOR cursor = CursorForImage(aCursor, GetDefaultScale());
  bool custom = false;
  if (cursor) {
    custom = true;
  } else {
    cursor = CursorFor(aCursor.mDefaultCursor);
  }

  if (!cursor) {
    return;
  }

  sCurrentHCursor = cursor;
  sCurrentHCursorIsCustom = custom;
  ::SetCursor(cursor);
}

/**************************************************************
 *
 * SECTION: nsIWidget::Get/SetTransparencyMode
 *
 * Manage the transparency mode of the window containing this
 * widget. Only works for popup and dialog windows when the
 * Desktop Window Manager compositor is not enabled.
 *
 **************************************************************/

nsTransparencyMode nsWindow::GetTransparencyMode() {
  return GetTopLevelWindow(true)->GetWindowTranslucencyInner();
}

void nsWindow::SetTransparencyMode(nsTransparencyMode aMode) {
  nsWindow* window = GetTopLevelWindow(true);
  MOZ_ASSERT(window);

  if (!window || window->DestroyCalled()) {
    return;
  }

  if (nsWindowType::eWindowType_toplevel == window->mWindowType &&
      mTransparencyMode != aMode &&
      !gfxWindowsPlatform::GetPlatform()->DwmCompositionEnabled()) {
    NS_WARNING("Cannot set transparency mode on top-level windows.");
    return;
  }

  window->SetWindowTranslucencyInner(aMode);
}

void nsWindow::UpdateOpaqueRegion(const LayoutDeviceIntRegion& aOpaqueRegion) {
  if (!HasGlass() || GetParent()) return;

  // If there is no opaque region or hidechrome=true, set margins
  // to support a full sheet of glass. Comments in MSDN indicate
  // all values must be set to -1 to get a full sheet of glass.
  MARGINS margins = {-1, -1, -1, -1};
  if (!aOpaqueRegion.IsEmpty()) {
    LayoutDeviceIntRect clientBounds = GetClientBounds();
    // Find the largest rectangle and use that to calculate the inset.
    LayoutDeviceIntRect largest = aOpaqueRegion.GetLargestRectangle();
    margins.cxLeftWidth = largest.X();
    margins.cxRightWidth = clientBounds.Width() - largest.XMost();
    margins.cyBottomHeight = clientBounds.Height() - largest.YMost();
    if (mCustomNonClient) {
      // The minimum glass height must be the caption buttons height,
      // otherwise the buttons are drawn incorrectly.
      largest.MoveToY(std::max<uint32_t>(
          largest.Y(), nsUXThemeData::GetCommandButtonBoxMetrics().cy));
    }
    margins.cyTopHeight = largest.Y();
  }

  // Only update glass area if there are changes
  if (memcmp(&mGlassMargins, &margins, sizeof mGlassMargins)) {
    mGlassMargins = margins;
    UpdateGlass();
  }
}

/**************************************************************
 *
 * SECTION: nsIWidget::UpdateWindowDraggingRegion
 *
 * For setting the draggable titlebar region from CSS
 * with -moz-window-dragging: drag.
 *
 **************************************************************/

void nsWindow::UpdateWindowDraggingRegion(
    const LayoutDeviceIntRegion& aRegion) {
  if (mDraggableRegion != aRegion) {
    mDraggableRegion = aRegion;
  }
}

void nsWindow::UpdateGlass() {
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
      [[fallthrough]];
    case eTransparencyGlass:
      policy = DWMNCRP_ENABLED;
      break;
    default:
      break;
  }

  MOZ_LOG(gWindowsLog, LogLevel::Info,
          ("glass margins: left:%d top:%d right:%d bottom:%d\n",
           margins.cxLeftWidth, margins.cyTopHeight, margins.cxRightWidth,
           margins.cyBottomHeight));

  // Extends the window frame behind the client area
  if (gfxWindowsPlatform::GetPlatform()->DwmCompositionEnabled()) {
    DwmExtendFrameIntoClientArea(mWnd, &margins);
    DwmSetWindowAttribute(mWnd, DWMWA_NCRENDERING_POLICY, &policy,
                          sizeof policy);
  }
}

/**************************************************************
 *
 * SECTION: nsIWidget::HideWindowChrome
 *
 * Show or hide window chrome.
 *
 **************************************************************/

void nsWindow::HideWindowChrome(bool aShouldHide) {
  HWND hwnd = WinUtils::GetTopLevelHWND(mWnd, true);
  if (!WinUtils::GetNSWindowPtr(hwnd)) {
    NS_WARNING("Trying to hide window decorations in an embedded context");
    return;
  }

  if (mHideChrome == aShouldHide) return;

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
  } else {
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
}

/**************************************************************
 *
 * SECTION: nsWindow::Invalidate
 *
 * Invalidate an area of the client for painting.
 *
 **************************************************************/

// Invalidate this component visible area
void nsWindow::Invalidate(bool aEraseBackground, bool aUpdateNCArea,
                          bool aIncludeChildren) {
  if (!mWnd) {
    return;
  }

#ifdef WIDGET_DEBUG_OUTPUT
  debug_DumpInvalidate(stdout, this, nullptr, "noname", (int32_t)mWnd);
#endif  // WIDGET_DEBUG_OUTPUT

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
}

// Invalidate this component visible area
void nsWindow::Invalidate(const LayoutDeviceIntRect& aRect) {
  if (mWnd) {
#ifdef WIDGET_DEBUG_OUTPUT
    debug_DumpInvalidate(stdout, this, &aRect, "noname", (int32_t)mWnd);
#endif  // WIDGET_DEBUG_OUTPUT

    RECT rect;

    rect.left = aRect.X();
    rect.top = aRect.Y();
    rect.right = aRect.XMost();
    rect.bottom = aRect.YMost();

    VERIFY(::InvalidateRect(mWnd, &rect, FALSE));
  }
}

static LRESULT CALLBACK FullscreenTransitionWindowProc(HWND hWnd, UINT uMsg,
                                                       WPARAM wParam,
                                                       LPARAM lParam) {
  switch (uMsg) {
    case WM_FULLSCREEN_TRANSITION_BEFORE:
    case WM_FULLSCREEN_TRANSITION_AFTER: {
      DWORD duration = (DWORD)lParam;
      DWORD flags = AW_BLEND;
      if (uMsg == WM_FULLSCREEN_TRANSITION_AFTER) {
        flags |= AW_HIDE;
      }
      ::AnimateWindow(hWnd, duration, flags);
      // The message sender should have added ref for us.
      NS_DispatchToMainThread(
          already_AddRefed<nsIRunnable>((nsIRunnable*)wParam));
      break;
    }
    case WM_DESTROY:
      ::PostQuitMessage(0);
      break;
    default:
      return ::DefWindowProcW(hWnd, uMsg, wParam, lParam);
  }
  return 0;
}

struct FullscreenTransitionInitData {
  nsIntRect mBounds;
  HANDLE mSemaphore;
  HANDLE mThread;
  HWND mWnd;

  FullscreenTransitionInitData()
      : mSemaphore(nullptr), mThread(nullptr), mWnd(nullptr) {}

  ~FullscreenTransitionInitData() {
    if (mSemaphore) {
      ::CloseHandle(mSemaphore);
    }
    if (mThread) {
      ::CloseHandle(mThread);
    }
  }
};

static DWORD WINAPI FullscreenTransitionThreadProc(LPVOID lpParam) {
  // Initialize window class
  static bool sInitialized = false;
  if (!sInitialized) {
    WNDCLASSW wc = {};
    wc.lpfnWndProc = ::FullscreenTransitionWindowProc;
    wc.hInstance = nsToolkit::mDllInstance;
    wc.hbrBackground = ::CreateSolidBrush(RGB(0, 0, 0));
    wc.lpszClassName = kClassNameTransition;
    ::RegisterClassW(&wc);
    sInitialized = true;
  }

  auto data = static_cast<FullscreenTransitionInitData*>(lpParam);
  HWND wnd = ::CreateWindowW(kClassNameTransition, L"", 0, 0, 0, 0, 0, nullptr,
                             nullptr, nsToolkit::mDllInstance, nullptr);
  if (!wnd) {
    ::ReleaseSemaphore(data->mSemaphore, 1, nullptr);
    return 0;
  }

  // Since AnimateWindow blocks the thread of the transition window,
  // we need to hide the cursor for that window, otherwise the system
  // would show the busy pointer to the user.
  ::ShowCursor(false);
  ::SetWindowLongW(wnd, GWL_STYLE, 0);
  ::SetWindowLongW(
      wnd, GWL_EXSTYLE,
      WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE);
  ::SetWindowPos(wnd, HWND_TOPMOST, data->mBounds.X(), data->mBounds.Y(),
                 data->mBounds.Width(), data->mBounds.Height(), 0);
  data->mWnd = wnd;
  ::ReleaseSemaphore(data->mSemaphore, 1, nullptr);
  // The initialization data may no longer be valid
  // after we release the semaphore.
  data = nullptr;

  MSG msg;
  while (::GetMessageW(&msg, nullptr, 0, 0)) {
    ::TranslateMessage(&msg);
    ::DispatchMessage(&msg);
  }
  ::ShowCursor(true);
  ::DestroyWindow(wnd);
  return 0;
}

class FullscreenTransitionData final : public nsISupports {
 public:
  NS_DECL_ISUPPORTS

  explicit FullscreenTransitionData(HWND aWnd) : mWnd(aWnd) {
    MOZ_ASSERT(NS_IsMainThread(),
               "FullscreenTransitionData "
               "should be constructed in the main thread");
  }

  const HWND mWnd;

 private:
  ~FullscreenTransitionData() {
    MOZ_ASSERT(NS_IsMainThread(),
               "FullscreenTransitionData "
               "should be deconstructed in the main thread");
    ::PostMessageW(mWnd, WM_DESTROY, 0, 0);
  }
};

NS_IMPL_ISUPPORTS0(FullscreenTransitionData)

/* virtual */
bool nsWindow::PrepareForFullscreenTransition(nsISupports** aData) {
  // We don't support fullscreen transition when composition is not
  // enabled, which could make the transition broken and annoying.
  // See bug 1184201.
  if (!gfxWindowsPlatform::GetPlatform()->DwmCompositionEnabled()) {
    return false;
  }

  FullscreenTransitionInitData initData;
  nsCOMPtr<nsIScreen> screen = GetWidgetScreen();
  int32_t x, y, width, height;
  screen->GetRectDisplayPix(&x, &y, &width, &height);
  MOZ_ASSERT(BoundsUseDesktopPixels(),
             "Should only be called on top-level window");
  double scale = GetDesktopToDeviceScale().scale;  // XXX or GetDefaultScale() ?
  initData.mBounds.SetRect(NSToIntRound(x * scale), NSToIntRound(y * scale),
                           NSToIntRound(width * scale),
                           NSToIntRound(height * scale));

  // Create a semaphore for synchronizing the window handle which will
  // be created by the transition thread and used by the main thread for
  // posting the transition messages.
  initData.mSemaphore = ::CreateSemaphore(nullptr, 0, 1, nullptr);
  if (initData.mSemaphore) {
    initData.mThread = ::CreateThread(
        nullptr, 0, FullscreenTransitionThreadProc, &initData, 0, nullptr);
    if (initData.mThread) {
      ::WaitForSingleObject(initData.mSemaphore, INFINITE);
    }
  }
  if (!initData.mWnd) {
    return false;
  }

  mTransitionWnd = initData.mWnd;

  auto data = new FullscreenTransitionData(initData.mWnd);
  *aData = data;
  NS_ADDREF(data);
  return true;
}

/* virtual */
void nsWindow::PerformFullscreenTransition(FullscreenTransitionStage aStage,
                                           uint16_t aDuration,
                                           nsISupports* aData,
                                           nsIRunnable* aCallback) {
  auto data = static_cast<FullscreenTransitionData*>(aData);
  nsCOMPtr<nsIRunnable> callback = aCallback;
  UINT msg = aStage == eBeforeFullscreenToggle ? WM_FULLSCREEN_TRANSITION_BEFORE
                                               : WM_FULLSCREEN_TRANSITION_AFTER;
  WPARAM wparam = (WPARAM)callback.forget().take();
  ::PostMessage(data->mWnd, msg, wparam, (LPARAM)aDuration);
}

/* virtual */
void nsWindow::CleanupFullscreenTransition() {
  MOZ_ASSERT(NS_IsMainThread(),
             "CleanupFullscreenTransition "
             "should only run on the main thread");

  mTransitionWnd = nullptr;
}

void nsWindow::OnFullscreenWillChange(bool aFullScreen) {
  if (mWidgetListener) {
    mWidgetListener->FullscreenWillChange(aFullScreen);
  }
}

void nsWindow::OnFullscreenChanged(bool aFullScreen) {
  // taskbarInfo will be nullptr pre Windows 7 until Bug 680227 is resolved.
  nsCOMPtr<nsIWinTaskbar> taskbarInfo = do_GetService(NS_TASKBAR_CONTRACTID);

  // Notify the taskbar that we will be entering full screen mode.
  if (aFullScreen && taskbarInfo) {
    taskbarInfo->PrepareFullScreenHWND(mWnd, TRUE);
  }

  // If we are going fullscreen, the window size continues to change
  // and the window will be reflow again then.
  UpdateNonClientMargins(mFrameState->GetSizeMode(), /* Reflow */ !aFullScreen);

  // Will call hide chrome, reposition window. Note this will
  // also cache dimensions for restoration, so it should only
  // be called once per fullscreen request.
  nsBaseWidget::InfallibleMakeFullScreen(aFullScreen);

  if (mIsVisible && !aFullScreen &&
      mFrameState->GetSizeMode() == nsSizeMode_Normal) {
    // Ensure the window exiting fullscreen get activated. Window
    // activation might be bypassed in SetSizeMode.
    DispatchFocusToTopLevelWindow(true);
  }

  // Notify the taskbar that we have exited full screen mode.
  if (!aFullScreen && taskbarInfo) {
    taskbarInfo->PrepareFullScreenHWND(mWnd, FALSE);
  }

  OnSizeModeChange(mFrameState->GetSizeMode());

  if (mWidgetListener) {
    mWidgetListener->FullscreenChanged(aFullScreen);
  }
}

nsresult nsWindow::MakeFullScreen(bool aFullScreen) {
  mFrameState->EnsureFullscreenMode(aFullScreen);
  return NS_OK;
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
void* nsWindow::GetNativeData(uint32_t aDataType) {
  switch (aDataType) {
    case NS_NATIVE_TMP_WINDOW:
      return (void*)::CreateWindowExW(
          mIsRTL ? WS_EX_LAYOUTRTL : 0, GetWindowClass(), L"", WS_CHILD,
          CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, mWnd,
          nullptr, nsToolkit::mDllInstance, nullptr);
    case NS_NATIVE_WIDGET:
    case NS_NATIVE_WINDOW:
    case NS_NATIVE_WINDOW_WEBRTC_DEVICE_ID:
      return (void*)mWnd;
    case NS_NATIVE_GRAPHIC:
      MOZ_ASSERT_UNREACHABLE("Not supported on Windows:");
      return nullptr;
    case NS_RAW_NATIVE_IME_CONTEXT: {
      void* pseudoIMEContext = GetPseudoIMEContext();
      if (pseudoIMEContext) {
        return pseudoIMEContext;
      }
      [[fallthrough]];
    }
    case NS_NATIVE_TSF_THREAD_MGR:
    case NS_NATIVE_TSF_CATEGORY_MGR:
    case NS_NATIVE_TSF_DISPLAY_ATTR_MGR:
      return IMEHandler::GetNativeData(this, aDataType);

    default:
      break;
  }

  return nullptr;
}

void nsWindow::SetNativeData(uint32_t aDataType, uintptr_t aVal) {
  NS_ERROR("SetNativeData called with unsupported data type.");
}

// Free some native data according to aDataType
void nsWindow::FreeNativeData(void* data, uint32_t aDataType) {
  switch (aDataType) {
    case NS_NATIVE_GRAPHIC:
    case NS_NATIVE_WIDGET:
    case NS_NATIVE_WINDOW:
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

nsresult nsWindow::SetTitle(const nsAString& aTitle) {
  const nsString& strTitle = PromiseFlatString(aTitle);
  AutoRestore<bool> sendingText(mSendingSetText);
  mSendingSetText = true;
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

void nsWindow::SetBigIcon(HICON aIcon) {
  HICON icon =
      (HICON)::SendMessageW(mWnd, WM_SETICON, (WPARAM)ICON_BIG, (LPARAM)aIcon);
  if (icon) {
    ::DestroyIcon(icon);
  }

  mIconBig = aIcon;
}

void nsWindow::SetSmallIcon(HICON aIcon) {
  HICON icon = (HICON)::SendMessageW(mWnd, WM_SETICON, (WPARAM)ICON_SMALL,
                                     (LPARAM)aIcon);
  if (icon) {
    ::DestroyIcon(icon);
  }

  mIconSmall = aIcon;
}

void nsWindow::SetIcon(const nsAString& aIconSpec) {
  // Assume the given string is a local identifier for an icon file.

  nsCOMPtr<nsIFile> iconFile;
  ResolveIconName(aIconSpec, u".ico"_ns, getter_AddRefs(iconFile));
  if (!iconFile) return;

  nsAutoString iconPath;
  iconFile->GetPath(iconPath);

  // XXX this should use MZLU (see bug 239279)

  ::SetLastError(0);

  HICON bigIcon =
      (HICON)::LoadImageW(nullptr, (LPCWSTR)iconPath.get(), IMAGE_ICON,
                          ::GetSystemMetrics(SM_CXICON),
                          ::GetSystemMetrics(SM_CYICON), LR_LOADFROMFILE);
  HICON smallIcon =
      (HICON)::LoadImageW(nullptr, (LPCWSTR)iconPath.get(), IMAGE_ICON,
                          ::GetSystemMetrics(SM_CXSMICON),
                          ::GetSystemMetrics(SM_CYSMICON), LR_LOADFROMFILE);

  if (bigIcon) {
    SetBigIcon(bigIcon);
  }
#ifdef DEBUG_SetIcon
  else {
    NS_LossyConvertUTF16toASCII cPath(iconPath);
    MOZ_LOG(gWindowsLog, LogLevel::Info,
            ("\nIcon load error; icon=%s, rc=0x%08X\n\n", cPath.get(),
             ::GetLastError()));
  }
#endif
  if (smallIcon) {
    SetSmallIcon(smallIcon);
  }
#ifdef DEBUG_SetIcon
  else {
    NS_LossyConvertUTF16toASCII cPath(iconPath);
    MOZ_LOG(gWindowsLog, LogLevel::Info,
            ("\nSmall icon load error; icon=%s, rc=0x%08X\n\n", cPath.get(),
             ::GetLastError()));
  }
#endif
}

void nsWindow::SetBigIconNoData() {
  HICON bigIcon =
      ::LoadIconW(::GetModuleHandleW(nullptr), gStockApplicationIcon);
  SetBigIcon(bigIcon);
}

void nsWindow::SetSmallIconNoData() {
  HICON smallIcon =
      ::LoadIconW(::GetModuleHandleW(nullptr), gStockApplicationIcon);
  SetSmallIcon(smallIcon);
}

/**************************************************************
 *
 * SECTION: nsIWidget::WidgetToScreenOffset
 *
 * Return this widget's origin in screen coordinates.
 *
 **************************************************************/

LayoutDeviceIntPoint nsWindow::WidgetToScreenOffset() {
  POINT point;
  point.x = 0;
  point.y = 0;
  ::ClientToScreen(mWnd, &point);
  return LayoutDeviceIntPoint(point.x, point.y);
}

LayoutDeviceIntSize nsWindow::ClientToWindowSize(
    const LayoutDeviceIntSize& aClientSize) {
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

void nsWindow::EnableDragDrop(bool aEnable) {
  if (!mWnd) {
    // Return early if the window already closed
    return;
  }

  if (aEnable) {
    if (!mNativeDragTarget) {
      mNativeDragTarget = new nsNativeDragTarget(this);
      mNativeDragTarget->AddRef();
      if (SUCCEEDED(::CoLockObjectExternal((LPUNKNOWN)mNativeDragTarget, TRUE,
                                           FALSE))) {
        ::RegisterDragDrop(mWnd, (LPDROPTARGET)mNativeDragTarget);
      }
    }
  } else {
    if (mWnd && mNativeDragTarget) {
      ::RevokeDragDrop(mWnd);
      ::CoLockObjectExternal((LPUNKNOWN)mNativeDragTarget, FALSE, TRUE);
      mNativeDragTarget->DragCancel();
      NS_RELEASE(mNativeDragTarget);
    }
  }
}

/**************************************************************
 *
 * SECTION: nsIWidget::CaptureMouse
 *
 * Enables/Disables system mouse capture.
 *
 **************************************************************/

void nsWindow::CaptureMouse(bool aCapture) {
  TRACKMOUSEEVENT mTrack;
  mTrack.cbSize = sizeof(TRACKMOUSEEVENT);
  mTrack.dwHoverTime = 0;
  mTrack.hwndTrack = mWnd;
  if (aCapture) {
    mTrack.dwFlags = TME_CANCEL | TME_LEAVE;
    ::SetCapture(mWnd);
  } else {
    mTrack.dwFlags = TME_LEAVE;
    ::ReleaseCapture();
  }
  sIsInMouseCapture = aCapture;
  TrackMouseEvent(&mTrack);
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

void nsWindow::CaptureRollupEvents(nsIRollupListener* aListener,
                                   bool aDoCapture) {
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
}

/**************************************************************
 *
 * SECTION: nsIWidget::GetAttention
 *
 * Bring this window to the user's attention.
 *
 **************************************************************/

// Draw user's attention to this window until it comes to foreground.
nsresult nsWindow::GetAttention(int32_t aCycleCount) {
  // Got window?
  if (!mWnd) return NS_ERROR_NOT_INITIALIZED;

  HWND flashWnd = WinUtils::GetTopLevelHWND(mWnd, false, false);
  HWND fgWnd = ::GetForegroundWindow();
  // Don't flash if the flash count is 0 or if the foreground window is our
  // window handle or that of our owned-most window.
  if (aCycleCount == 0 || flashWnd == fgWnd ||
      flashWnd == WinUtils::GetTopLevelHWND(fgWnd, false, false)) {
    return NS_OK;
  }

  DWORD defaultCycleCount = 0;
  ::SystemParametersInfo(SPI_GETFOREGROUNDFLASHCOUNT, 0, &defaultCycleCount, 0);

  FLASHWINFO flashInfo = {sizeof(FLASHWINFO), flashWnd, FLASHW_ALL,
                          aCycleCount > 0 ? aCycleCount : defaultCycleCount, 0};
  ::FlashWindowEx(&flashInfo);

  return NS_OK;
}

void nsWindow::StopFlashing() {
  HWND flashWnd = mWnd;
  while (HWND ownerWnd = ::GetWindow(flashWnd, GW_OWNER)) {
    flashWnd = ownerWnd;
  }

  FLASHWINFO flashInfo = {sizeof(FLASHWINFO), flashWnd, FLASHW_STOP, 0, 0};
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

bool nsWindow::HasPendingInputEvent() {
  // If there is pending input or the user is currently
  // moving the window then return true.
  // Note: When the user is moving the window WIN32 spins
  // a separate event loop and input events are not
  // reported to the application.
  if (HIWORD(GetQueueStatus(QS_INPUT))) return true;
  GUITHREADINFO guiInfo;
  guiInfo.cbSize = sizeof(GUITHREADINFO);
  if (!GetGUIThreadInfo(GetCurrentThreadId(), &guiInfo)) return false;
  return GUI_INMOVESIZE == (guiInfo.flags & GUI_INMOVESIZE);
}

/**************************************************************
 *
 * SECTION: nsIWidget::GetWindowRenderer
 *
 * Get the window renderer associated with this widget.
 *
 **************************************************************/

WindowRenderer* nsWindow::GetWindowRenderer() {
  if (mWindowRenderer) {
    return mWindowRenderer;
  }

  if (!mLocalesChangedObserver) {
    mLocalesChangedObserver = new LocalesChangedObserver(this);
  }

  // Try OMTC first.
  if (!mWindowRenderer && ShouldUseOffMainThreadCompositing()) {
    gfxWindowsPlatform::GetPlatform()->UpdateRenderMode();
    CreateCompositor();
  }

  if (!mWindowRenderer) {
    MOZ_ASSERT(!mCompositorSession && !mCompositorBridgeChild);
    MOZ_ASSERT(!mCompositorWidgetDelegate);

    // Ensure we have a widget proxy even if we're not using the compositor,
    // since all our transparent window handling lives there.
    WinCompositorWidgetInitData initData(
        reinterpret_cast<uintptr_t>(mWnd),
        reinterpret_cast<uintptr_t>(static_cast<nsIWidget*>(this)),
        mTransparencyMode, mFrameState->GetSizeMode());
    // If we're not using the compositor, the options don't actually matter.
    CompositorOptions options(false, false);
    mBasicLayersSurface =
        new InProcessWinCompositorWidget(initData, options, this);
    mCompositorWidgetDelegate = mBasicLayersSurface;
    mWindowRenderer = CreateFallbackRenderer();
  }

  NS_ASSERTION(mWindowRenderer, "Couldn't provide a valid window renderer.");

  if (mWindowRenderer) {
    // Update the size constraints now that the layer manager has been
    // created.
    KnowsCompositor* knowsCompositor = mWindowRenderer->AsKnowsCompositor();
    if (knowsCompositor) {
      SizeConstraints c = mSizeConstraints;
      mMaxTextureSize = knowsCompositor->GetMaxTextureSize();
      c.mMaxSize.width = std::min(c.mMaxSize.width, mMaxTextureSize);
      c.mMaxSize.height = std::min(c.mMaxSize.height, mMaxTextureSize);
      nsBaseWidget::SetSizeConstraints(c);
    }
  }

  return mWindowRenderer;
}

/**************************************************************
 *
 * SECTION: nsBaseWidget::SetCompositorWidgetDelegate
 *
 * Called to connect the nsWindow to the delegate providing
 * platform compositing API access.
 *
 **************************************************************/

void nsWindow::SetCompositorWidgetDelegate(CompositorWidgetDelegate* delegate) {
  if (delegate) {
    mCompositorWidgetDelegate = delegate->AsPlatformSpecificDelegate();
    MOZ_ASSERT(mCompositorWidgetDelegate,
               "nsWindow::SetCompositorWidgetDelegate called with a "
               "non-PlatformCompositorWidgetDelegate");
  } else {
    mCompositorWidgetDelegate = nullptr;
  }
}

/**************************************************************
 *
 * SECTION: nsIWidget::OnDefaultButtonLoaded
 *
 * Called after the dialog is loaded and it has a default button.
 *
 **************************************************************/

nsresult nsWindow::OnDefaultButtonLoaded(
    const LayoutDeviceIntRect& aButtonRect) {
  if (aButtonRect.IsEmpty()) return NS_OK;

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
    if (!::SystemParametersInfo(SPI_GETSNAPTODEFBUTTON, 0, &snapDefaultButton,
                                0) ||
        !snapDefaultButton)
      return NS_OK;
  }

  LayoutDeviceIntRect widgetRect = GetScreenBounds();
  LayoutDeviceIntRect buttonRect(aButtonRect + widgetRect.TopLeft());

  LayoutDeviceIntPoint centerOfButton(buttonRect.X() + buttonRect.Width() / 2,
                                      buttonRect.Y() + buttonRect.Height() / 2);
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

void nsWindow::UpdateThemeGeometries(
    const nsTArray<ThemeGeometry>& aThemeGeometries) {
  RefPtr<WebRenderLayerManager> layerManager =
      GetWindowRenderer() ? GetWindowRenderer()->AsWebRender() : nullptr;
  if (!layerManager) {
    return;
  }

  if (!HasGlass() ||
      !gfxWindowsPlatform::GetPlatform()->DwmCompositionEnabled()) {
    return;
  }

  mWindowButtonsRect = Nothing();

  if (!IsWin10OrLater()) {
    for (size_t i = 0; i < aThemeGeometries.Length(); i++) {
      if (aThemeGeometries[i].mType ==
          nsNativeThemeWin::eThemeGeometryTypeWindowButtons) {
        LayoutDeviceIntRect bounds = aThemeGeometries[i].mRect;
        // Extend the bounds by one pixel to the right, because that's how much
        // the actual window button shape extends past the client area of the
        // window (and overlaps the right window frame).
        bounds.SetWidth(bounds.Width() + 1);
        if (!mWindowButtonsRect) {
          mWindowButtonsRect = Some(bounds);
        }
      }
    }
  }
}

void nsWindow::AddWindowOverlayWebRenderCommands(
    layers::WebRenderBridgeChild* aWrBridge, wr::DisplayListBuilder& aBuilder,
    wr::IpcResourceUpdateQueue& aResources) {
  if (mWindowButtonsRect) {
    wr::LayoutRect rect = wr::ToLayoutRect(*mWindowButtonsRect);
    aBuilder.PushClearRect(rect);
  }
}

uint32_t nsWindow::GetMaxTouchPoints() const {
  return WinUtils::GetMaxTouchPoints();
}

void nsWindow::SetWindowClass(const nsAString& xulWinType) {
  mIsEarlyBlankWindow = xulWinType.EqualsLiteral("navigator:blank");
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

// Event initialization
void nsWindow::InitEvent(WidgetGUIEvent& event, LayoutDeviceIntPoint* aPoint) {
  if (nullptr == aPoint) {  // use the point from the event
    // get the message position in client coordinates
    if (mWnd != nullptr) {
      DWORD pos = ::GetMessagePos();
      POINT cpos;

      cpos.x = GET_X_LPARAM(pos);
      cpos.y = GET_Y_LPARAM(pos);

      ::ScreenToClient(mWnd, &cpos);
      event.mRefPoint = LayoutDeviceIntPoint(cpos.x, cpos.y);
    } else {
      event.mRefPoint = LayoutDeviceIntPoint(0, 0);
    }
  } else {
    // use the point override if provided
    event.mRefPoint = *aPoint;
  }

  event.AssignEventTime(CurrentMessageWidgetEventTime());
}

WidgetEventTime nsWindow::CurrentMessageWidgetEventTime() const {
  LONG messageTime = ::GetMessageTime();
  return WidgetEventTime(messageTime, GetMessageTimeStamp(messageTime));
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
nsresult nsWindow::DispatchEvent(WidgetGUIEvent* event,
                                 nsEventStatus& aStatus) {
#ifdef WIDGET_DEBUG_OUTPUT
  debug_DumpEvent(stdout, event->mWidget, event, "something", (int32_t)mWnd);
#endif  // WIDGET_DEBUG_OUTPUT

  aStatus = nsEventStatus_eIgnore;

  // Top level windows can have a view attached which requires events be sent
  // to the underlying base window and the view. Added when we combined the
  // base chrome window with the main content child for nc client area (title
  // bar) rendering.
  if (mAttachedWidgetListener) {
    aStatus = mAttachedWidgetListener->HandleEvent(event, mUseAttachedEvents);
  } else if (mWidgetListener) {
    aStatus = mWidgetListener->HandleEvent(event, mUseAttachedEvents);
  }

  // the window can be destroyed during processing of seemingly innocuous events
  // like, say, mousedowns due to the magic of scripting. mousedowns will return
  // nsEventStatus_eIgnore, which causes problems with the deleted window.
  // therefore:
  if (mOnDestroyCalled) aStatus = nsEventStatus_eConsumeNoDefault;
  return NS_OK;
}

bool nsWindow::DispatchStandardEvent(EventMessage aMsg) {
  WidgetGUIEvent event(true, aMsg, this);
  InitEvent(event);

  bool result = DispatchWindowEvent(event);
  return result;
}

bool nsWindow::DispatchKeyboardEvent(WidgetKeyboardEvent* event) {
  nsEventStatus status = DispatchInputEvent(event).mContentStatus;
  return ConvertStatus(status);
}

bool nsWindow::DispatchContentCommandEvent(WidgetContentCommandEvent* aEvent) {
  nsEventStatus status;
  DispatchEvent(aEvent, status);
  return ConvertStatus(status);
}

bool nsWindow::DispatchWheelEvent(WidgetWheelEvent* aEvent) {
  nsEventStatus status =
      DispatchInputEvent(aEvent->AsInputEvent()).mContentStatus;
  return ConvertStatus(status);
}

// Recursively dispatch synchronous paints for nsIWidget
// descendants with invalidated rectangles.
BOOL CALLBACK nsWindow::DispatchStarvedPaints(HWND aWnd, LPARAM aMsg) {
  LONG_PTR proc = ::GetWindowLongPtrW(aWnd, GWLP_WNDPROC);
  if (proc == (LONG_PTR)&nsWindow::WindowProc) {
    // its one of our windows so check to see if it has a
    // invalidated rect. If it does. Dispatch a synchronous
    // paint.
    if (GetUpdateRect(aWnd, nullptr, FALSE)) VERIFY(::UpdateWindow(aWnd));
  }
  return TRUE;
}

// Check for pending paints and dispatch any pending paint
// messages for any nsIWidget which is a descendant of the
// top-level window that *this* window is embedded within.
//
// Note: We do not dispatch pending paint messages for non
// nsIWidget managed windows.
void nsWindow::DispatchPendingEvents() {
  if (mPainting) {
    NS_WARNING(
        "We were asked to dispatch pending events during painting, "
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

void nsWindow::DispatchCustomEvent(const nsString& eventName) {
  if (Document* doc = GetDocument()) {
    if (nsPIDOMWindowOuter* win = doc->GetWindow()) {
      win->DispatchCustomEvent(eventName, ChromeOnlyDispatch::eYes);
    }
  }
}

bool nsWindow::TouchEventShouldStartDrag(EventMessage aEventMessage,
                                         LayoutDeviceIntPoint aEventPoint) {
  // Allow users to start dragging by double-tapping.
  if (aEventMessage == eMouseDoubleClick) {
    return true;
  }

  // In chrome UI, allow touchdownstartsdrag attributes
  // to cause any touchdown event to trigger a drag.
  if (aEventMessage == eMouseDown) {
    WidgetMouseEvent hittest(true, eMouseHitTest, this,
                             WidgetMouseEvent::eReal);
    hittest.mRefPoint = aEventPoint;
    hittest.mIgnoreRootScrollFrame = true;
    hittest.mInputSource = MouseEvent_Binding::MOZ_SOURCE_TOUCH;
    DispatchInputEvent(&hittest);

    if (EventTarget* target = hittest.GetDOMEventTarget()) {
      if (nsIContent* content = nsIContent::FromEventTarget(target)) {
        // Check if the element or any parent element has the
        // attribute we're looking for.
        for (Element* element = content->GetAsElementOrParentElement(); element;
             element = element->GetParentElement()) {
          nsAutoString startDrag;
          element->GetAttribute(u"touchdownstartsdrag"_ns, startDrag);
          if (!startDrag.IsEmpty()) {
            return true;
          }
        }
      }
    }
  }

  return false;
}

// Deal with all sort of mouse event
bool nsWindow::DispatchMouseEvent(EventMessage aEventMessage, WPARAM wParam,
                                  LPARAM lParam, bool aIsContextMenuKey,
                                  int16_t aButton, uint16_t aInputSource,
                                  WinPointerInfo* aPointerInfo,
                                  bool aIgnoreAPZ) {
  bool result = false;

  UserActivity();

  if (!mWidgetListener) {
    return result;
  }

  LayoutDeviceIntPoint eventPoint(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
  LayoutDeviceIntPoint mpScreen = eventPoint + WidgetToScreenOffset();

  // Suppress mouse moves caused by widget creation. Make sure to do this early
  // so that we update sLastMouseMovePoint even for touch-induced mousemove
  // events.
  if (aEventMessage == eMouseMove) {
    if ((sLastMouseMovePoint.x == mpScreen.x) &&
        (sLastMouseMovePoint.y == mpScreen.y)) {
      return result;
    }
    sLastMouseMovePoint.x = mpScreen.x;
    sLastMouseMovePoint.y = mpScreen.y;
  }

  if (!aIgnoreAPZ && WinUtils::GetIsMouseFromTouch(aEventMessage)) {
    if (mTouchWindow) {
      // If mTouchWindow is true, then we must have APZ enabled and be
      // feeding it raw touch events. In that case we only want to
      // send touch-generated mouse events to content if they should
      // start a touch-based drag-and-drop gesture, such as on
      // double-tapping or when tapping elements marked with the
      // touchdownstartsdrag attribute in chrome UI.
      MOZ_ASSERT(mAPZC);
      if (TouchEventShouldStartDrag(aEventMessage, eventPoint)) {
        aEventMessage = eMouseTouchDrag;
      } else {
        return result;
      }
    }
  }

  uint32_t pointerId =
      aPointerInfo ? aPointerInfo->pointerId : MOUSE_POINTERID();

  // Since it is unclear whether a user will use the digitizer,
  // Postpone initialization until first PEN message will be found.
  if (MouseEvent_Binding::MOZ_SOURCE_PEN == aInputSource
      // Messages should be only at topLevel window.
      && nsWindowType::eWindowType_toplevel == mWindowType
      // Currently this scheme is used only when pointer events is enabled.
      && InkCollector::sInkCollector) {
    InkCollector::sInkCollector->SetTarget(mWnd);
    InkCollector::sInkCollector->SetPointerId(pointerId);
  }

  switch (aEventMessage) {
    case eMouseDown:
      CaptureMouse(true);
      break;

    // eMouseMove and eMouseExitFromWidget are here because we need to make
    // sure capture flag isn't left on after a drag where we wouldn't see a
    // button up message (see bug 324131).
    case eMouseUp:
    case eMouseMove:
    case eMouseExitFromWidget:
      if (!(wParam & (MK_LBUTTON | MK_MBUTTON | MK_RBUTTON)) &&
          sIsInMouseCapture)
        CaptureMouse(false);
      break;

    default:
      break;

  }  // switch

  WidgetMouseEvent event(true, aEventMessage, this, WidgetMouseEvent::eReal,
                         aIsContextMenuKey ? WidgetMouseEvent::eContextMenuKey
                                           : WidgetMouseEvent::eNormal);
  if (aEventMessage == eContextMenu && aIsContextMenuKey) {
    LayoutDeviceIntPoint zero(0, 0);
    InitEvent(event, &zero);
  } else {
    InitEvent(event, &eventPoint);
  }

  ModifierKeyState modifierKeyState;
  modifierKeyState.InitInputEvent(event);

  // eContextMenu with Shift state is special.  It won't fire "contextmenu"
  // event in the web content for blocking web content to prevent its default.
  // However, Shift+F10 is a standard shortcut key on Windows.  Therefore,
  // this should not block web page to prevent its default.  I.e., it should
  // behave same as ContextMenu key without Shift key.
  // XXX Should we allow to block web page to prevent its default with
  //     Ctrl+Shift+F10 or Alt+Shift+F10 instead?
  if (aEventMessage == eContextMenu && aIsContextMenuKey && event.IsShift() &&
      NativeKey::LastKeyOrCharMSG().message == WM_SYSKEYDOWN &&
      NativeKey::LastKeyOrCharMSG().wParam == VK_F10) {
    event.mModifiers &= ~MODIFIER_SHIFT;
  }

  event.mButton = aButton;
  event.mInputSource = aInputSource;
  if (aPointerInfo) {
    // Mouse events from Windows WM_POINTER*. Fill more information in
    // WidgetMouseEvent.
    event.AssignPointerHelperData(*aPointerInfo);
    event.mPressure = aPointerInfo->mPressure;
    event.mButtons = aPointerInfo->mButtons;
  } else {
    // If we get here the mouse events must be from non-touch sources, so
    // convert it to pointer events as well
    event.convertToPointer = true;
    event.pointerId = pointerId;
  }

  bool insideMovementThreshold =
      (DeprecatedAbs(sLastMousePoint.x - eventPoint.x) <
       (short)::GetSystemMetrics(SM_CXDOUBLECLK)) &&
      (DeprecatedAbs(sLastMousePoint.y - eventPoint.y) <
       (short)::GetSystemMetrics(SM_CYDOUBLECLK));

  BYTE eventButton;
  switch (aButton) {
    case MouseButton::ePrimary:
      eventButton = VK_LBUTTON;
      break;
    case MouseButton::eMiddle:
      eventButton = VK_MBUTTON;
      break;
    case MouseButton::eSecondary:
      eventButton = VK_RBUTTON;
      break;
    default:
      eventButton = 0;
      break;
  }

  // Doubleclicks are used to set the click count, then changed to mousedowns
  // We're going to time double-clicks from mouse *up* to next mouse *down*
  LONG curMsgTime = ::GetMessageTime();

  switch (aEventMessage) {
    case eMouseDoubleClick:
      event.mMessage = eMouseDown;
      event.mButton = aButton;
      sLastClickCount = 2;
      sLastMouseDownTime = curMsgTime;
      break;
    case eMouseUp:
      // remember when this happened for the next mouse down
      sLastMousePoint.x = eventPoint.x;
      sLastMousePoint.y = eventPoint.y;
      sLastMouseButton = eventButton;
      break;
    case eMouseDown:
      // now look to see if we want to convert this to a double- or triple-click
      if (((curMsgTime - sLastMouseDownTime) < (LONG)::GetDoubleClickTime()) &&
          insideMovementThreshold && eventButton == sLastMouseButton) {
        sLastClickCount++;
      } else {
        // reset the click count, to count *this* click
        sLastClickCount = 1;
      }
      // Set last Click time on MouseDown only
      sLastMouseDownTime = curMsgTime;
      break;
    case eMouseMove:
      if (!insideMovementThreshold) {
        sLastClickCount = 0;
      }
      break;
    case eMouseExitFromWidget:
      event.mExitFrom =
          Some(IsTopLevelMouseExit(mWnd) ? WidgetMouseEvent::ePlatformTopLevel
                                         : WidgetMouseEvent::ePlatformChild);
      break;
    default:
      break;
  }
  event.mClickCount = sLastClickCount;

#ifdef NS_DEBUG_XX
  MOZ_LOG(gWindowsLog, LogLevel::Info,
          ("Msg Time: %d Click Count: %d\n", curMsgTime, event.mClickCount));
#endif

  // call the event callback
  if (mWidgetListener) {
    if (aEventMessage == eMouseMove) {
      LayoutDeviceIntRect rect = GetBounds();
      rect.MoveTo(0, 0);

      if (rect.Contains(event.mRefPoint)) {
        if (sCurrentWindow == nullptr || sCurrentWindow != this) {
          if ((nullptr != sCurrentWindow) && (!sCurrentWindow->mInDtor)) {
            LPARAM pos = sCurrentWindow->lParamToClient(lParamToScreen(lParam));
            sCurrentWindow->DispatchMouseEvent(
                eMouseExitFromWidget, wParam, pos, false, MouseButton::ePrimary,
                aInputSource, aPointerInfo);
          }
          sCurrentWindow = this;
          if (!mInDtor) {
            LPARAM pos = sCurrentWindow->lParamToClient(lParamToScreen(lParam));
            sCurrentWindow->DispatchMouseEvent(
                eMouseEnterIntoWidget, wParam, pos, false,
                MouseButton::ePrimary, aInputSource, aPointerInfo);
          }
        }
      }
    } else if (aEventMessage == eMouseExitFromWidget) {
      if (sCurrentWindow == this) {
        sCurrentWindow = nullptr;
      }
    }

    result = ConvertStatus(DispatchInputEvent(&event).mContentStatus);

    // Release the widget with NS_IF_RELEASE() just in case
    // the context menu key code in EventListenerManager::HandleEvent()
    // released it already.
    return result;
  }

  return result;
}

HWND nsWindow::GetTopLevelForFocus(HWND aCurWnd) {
  // retrieve the toplevel window or dialogue
  HWND toplevelWnd = nullptr;
  while (aCurWnd) {
    toplevelWnd = aCurWnd;
    nsWindow* win = WinUtils::GetNSWindowPtr(aCurWnd);
    if (win) {
      if (win->mWindowType == eWindowType_toplevel ||
          win->mWindowType == eWindowType_dialog) {
        break;
      }
    }

    aCurWnd = ::GetParent(aCurWnd);  // Parent or owner (if has no parent)
  }
  return toplevelWnd;
}

void nsWindow::DispatchFocusToTopLevelWindow(bool aIsActivate) {
  if (aIsActivate) {
    sJustGotActivate = false;
  }
  sJustGotDeactivate = false;
  mLastKillFocusWindow = nullptr;

  HWND toplevelWnd = GetTopLevelForFocus(mWnd);

  if (toplevelWnd) {
    nsWindow* win = WinUtils::GetNSWindowPtr(toplevelWnd);
    if (win && win->mWidgetListener) {
      if (aIsActivate) {
        win->mWidgetListener->WindowActivated();
      } else {
        win->mWidgetListener->WindowDeactivated();
      }
    }
  }
}

HWND nsWindow::WindowAtMouse() {
  DWORD pos = ::GetMessagePos();
  POINT mp;
  mp.x = GET_X_LPARAM(pos);
  mp.y = GET_Y_LPARAM(pos);
  return ::WindowFromPoint(mp);
}

bool nsWindow::IsTopLevelMouseExit(HWND aWnd) {
  HWND mouseWnd = WindowAtMouse();

  // WinUtils::GetTopLevelHWND() will return a HWND for the window frame
  // (which includes the non-client area).  If the mouse has moved into
  // the non-client area, we should treat it as a top-level exit.
  HWND mouseTopLevel = WinUtils::GetTopLevelHWND(mouseWnd);
  if (mouseWnd == mouseTopLevel) return true;

  return WinUtils::GetTopLevelHWND(aWnd) != mouseTopLevel;
}

/**************************************************************
 *
 * SECTION: IPC
 *
 * IPC related helpers.
 *
 **************************************************************/

// static
bool nsWindow::IsAsyncResponseEvent(UINT aMsg, LRESULT& aResult) {
  switch (aMsg) {
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
          "An unhandled ISMEX_SEND message was received during spin loop! (%X)",
          aMsg);
  NS_WARNING(szBuf);
#endif

  return false;
}

void nsWindow::IPCWindowProcHandler(UINT& msg, WPARAM& wParam, LPARAM& lParam) {
  MOZ_ASSERT_IF(
      msg != WM_GETOBJECT,
      !mozilla::ipc::MessageChannel::IsPumpingMessages() ||
          mozilla::ipc::SuppressedNeuteringRegion::IsNeuteringSuppressed());

  // Modal UI being displayed in windowless plugins.
  if (mozilla::ipc::MessageChannel::IsSpinLoopActive() &&
      (InSendMessageEx(nullptr) & (ISMEX_REPLIED | ISMEX_SEND)) == ISMEX_SEND) {
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

  switch (msg) {
    // Windowless flash sending WM_ACTIVATE events to the main window
    // via calls to ShowWindow.
    case WM_ACTIVATE:
      if (lParam != 0 && LOWORD(wParam) == WA_ACTIVE &&
          IsWindow((HWND)lParam)) {
        // Check for Adobe Reader X sync activate message from their
        // helper window and ignore. Fixes an annoying focus problem.
        if ((InSendMessageEx(nullptr) & (ISMEX_REPLIED | ISMEX_SEND)) ==
            ISMEX_SEND) {
          wchar_t szClass[10];
          HWND focusWnd = (HWND)lParam;
          if (IsWindowVisible(focusWnd) &&
              GetClassNameW(focusWnd, szClass,
                            sizeof(szClass) / sizeof(char16_t)) &&
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
      (InSendMessageEx(nullptr) & (ISMEX_REPLIED | ISMEX_SEND)) == ISMEX_SEND) {
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

static bool DisplaySystemMenu(HWND hWnd, nsSizeMode sizeMode, bool isRtl,
                              int32_t x, int32_t y) {
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
    switch (sizeMode) {
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
      case nsSizeMode_Invalid:
        NS_ASSERTION(false, "Did the argument come from invalid IPC?");
        break;
      default:
        MOZ_ASSERT_UNREACHABLE("Unhnalded nsSizeMode value detected");
        break;
    }
    LPARAM cmd = TrackPopupMenu(
        hMenu,
        (TPM_LEFTBUTTON | TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_TOPALIGN |
         (isRtl ? TPM_RIGHTALIGN : TPM_LEFTALIGN)),
        x, y, 0, hWnd, nullptr);
    if (cmd) {
      PostMessage(hWnd, WM_SYSCOMMAND, cmd, 0);
      return true;
    }
  }
  return false;
}

// The WndProc procedure for all nsWindows in this toolkit. This merely catches
// exceptions and passes the real work to WindowProcInternal. See bug 587406
// and http://msdn.microsoft.com/en-us/library/ms633573%28VS.85%29.aspx
LRESULT CALLBACK nsWindow::WindowProc(HWND hWnd, UINT msg, WPARAM wParam,
                                      LPARAM lParam) {
  mozilla::ipc::CancelCPOWs();

  BackgroundHangMonitor().NotifyActivity();

  return mozilla::CallWindowProcCrashProtected(WindowProcInternal, hWnd, msg,
                                               wParam, lParam);
}

LRESULT CALLBACK nsWindow::WindowProcInternal(HWND hWnd, UINT msg,
                                              WPARAM wParam, LPARAM lParam) {
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
  nsWindow* targetWindow = WinUtils::GetNSWindowPtr(hWnd);
  NS_ASSERTION(targetWindow, "nsWindow* is null!");
  if (!targetWindow) return ::DefWindowProcW(hWnd, msg, wParam, lParam);

  // Hold the window for the life of this method, in case it gets
  // destroyed during processing, unless we're in the dtor already.
  nsCOMPtr<nsIWidget> kungFuDeathGrip;
  if (!targetWindow->mInDtor) kungFuDeathGrip = targetWindow;

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

  LRESULT res = ::CallWindowProcW(targetWindow->GetPrevWindowProc(), hWnd, msg,
                                  wParam, lParam);

  return res;
}

const char16_t* GetQuitType() {
  if (Preferences::GetBool(PREF_WIN_REGISTER_APPLICATION_RESTART, false)) {
    DWORD cchCmdLine = 0;
    HRESULT rc = ::GetApplicationRestartSettings(::GetCurrentProcess(), nullptr,
                                                 &cchCmdLine, nullptr);
    if (rc == S_OK) {
      return u"os-restart";
    }
  }
  return nullptr;
}

bool nsWindow::ExternalHandlerProcessMessage(UINT aMessage, WPARAM& aWParam,
                                             LPARAM& aLParam,
                                             MSGResult& aResult) {
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

  return false;
}

// The main windows message processing method. Wraps ProcessMessageInternal so
// we can log aRetValue.
bool nsWindow::ProcessMessage(UINT msg, WPARAM& wParam, LPARAM& lParam,
                              LRESULT* aRetValue) {
  bool result = ProcessMessageInternal(msg, wParam, lParam, aRetValue);

  // SHOW_REPEAT_EVENTS indicates whether to show all (repeating) events,
  // SHOW_MOUSEMOVE_EVENTS indicates whether to show mouse move events.
  // See nsWindowDbg for details.
  PrintEvent(msg, wParam, lParam, *aRetValue, result, SHOW_REPEAT_EVENTS,
             SHOW_MOUSEMOVE_EVENTS);

  return result;
}

// The main windows message processing method. Called by ProcessMessage.
bool nsWindow::ProcessMessageInternal(UINT msg, WPARAM& wParam, LPARAM& lParam,
                                      LRESULT* aRetValue) {
  MSGResult msgResult(aRetValue);
  if (ExternalHandlerProcessMessage(msg, wParam, lParam, msgResult)) {
    return (msgResult.mConsumed || !mWnd);
  }

  bool result = false;  // call the default nsWindow proc
  *aRetValue = 0;

  // Glass hit testing w/custom transparent margins
  LRESULT dwmHitResult;
  if (mCustomNonClient &&
      gfxWindowsPlatform::GetPlatform()->DwmCompositionEnabled() &&
      /* We don't do this for win10 glass with a custom titlebar,
       * in order to avoid the caption buttons breaking. */
      !(IsWin10OrLater() && HasGlass()) &&
      DwmDefWindowProc(mWnd, msg, wParam, lParam, &dwmHitResult)) {
    *aRetValue = dwmHitResult;
    return true;
  }

  // (Large blocks of code should be broken out into OnEvent handlers.)
  switch (msg) {
    // WM_QUERYENDSESSION must be handled by all windows.
    // Otherwise Windows thinks the window can just be killed at will.
    case WM_QUERYENDSESSION:
      if (sCanQuit == TRI_UNKNOWN) {
        // Ask if it's ok to quit, and store the answer until we
        // get WM_ENDSESSION signaling the round is complete.
        nsCOMPtr<nsIObserverService> obsServ =
            mozilla::services::GetObserverService();
        nsCOMPtr<nsISupportsPRBool> cancelQuit =
            do_CreateInstance(NS_SUPPORTS_PRBOOL_CONTRACTID);
        cancelQuit->SetData(false);

        const char16_t* quitType = GetQuitType();
        obsServ->NotifyObservers(cancelQuit, "quit-application-requested",
                                 quitType);

        bool abortQuit;
        cancelQuit->GetData(&abortQuit);
        sCanQuit = abortQuit ? TRI_FALSE : TRI_TRUE;
      }
      *aRetValue = sCanQuit ? TRUE : FALSE;
      result = true;
      break;

    case MOZ_WM_STARTA11Y:
#if defined(ACCESSIBILITY)
      Unused << GetAccessible();
      result = true;
#else
      result = false;
#endif
      break;

    case WM_ENDSESSION:
    case MOZ_WM_APP_QUIT:
      if (msg == MOZ_WM_APP_QUIT || (wParam == TRUE && sCanQuit == TRI_TRUE)) {
        // Let's fake a shutdown sequence without actually closing windows etc.
        // to avoid Windows killing us in the middle. A proper shutdown would
        // require having a chance to pump some messages. Unfortunately
        // Windows won't let us do that. Bug 212316.
        nsCOMPtr<nsIObserverService> obsServ =
            mozilla::services::GetObserverService();
        const char16_t* syncShutdown = u"syncShutdown";
        const char16_t* quitType = GetQuitType();

        AppShutdown::Init(AppShutdownMode::Normal, 0);

        obsServ->NotifyObservers(nullptr, "quit-application-granted",
                                 syncShutdown);
        obsServ->NotifyObservers(nullptr, "quit-application-forced", nullptr);

        AppShutdown::OnShutdownConfirmed();

        AppShutdown::AdvanceShutdownPhase(ShutdownPhase::AppShutdownConfirmed,
                                          quitType);
        AppShutdown::AdvanceShutdownPhase(ShutdownPhase::AppShutdownNetTeardown,
                                          nullptr);
        AppShutdown::AdvanceShutdownPhase(ShutdownPhase::AppShutdownTeardown,
                                          nullptr);
        AppShutdown::AdvanceShutdownPhase(ShutdownPhase::AppShutdown, nullptr);
        AppShutdown::AdvanceShutdownPhase(ShutdownPhase::AppShutdownQM,
                                          nullptr);
        AppShutdown::AdvanceShutdownPhase(ShutdownPhase::AppShutdownTelemetry,
                                          nullptr);

        AppShutdown::DoImmediateExit();
      }
      sCanQuit = TRI_UNKNOWN;
      result = true;
      break;

    case WM_SYSCOLORCHANGE:
      // No need to invalidate layout for system color changes, but we need to
      // invalidate style.
      NotifyThemeChanged(widget::ThemeChangeKind::Style);
      break;

    case WM_THEMECHANGED: {
      // Before anything else, push updates to child processes
      WinContentSystemParameters::GetSingleton()->OnThemeChanged();

      // Update non-client margin offsets
      UpdateNonClientMargins();
      nsUXThemeData::UpdateNativeThemeInfo();

      // We assume pretty much everything could've changed here.
      NotifyThemeChanged(widget::ThemeChangeKind::StyleAndLayout);

      UpdateDarkModeToolbar();

      // Invalidate the window so that the repaint will
      // pick up the new theme.
      Invalidate(true, true, true);
    } break;

    case WM_WTSSESSION_CHANGE: {
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
    } break;

    case WM_FONTCHANGE: {
      // We only handle this message for the hidden window,
      // as we only need to update the (global) font list once
      // for any given change, not once per window!
      if (mWindowType != eWindowType_invisible) {
        break;
      }

      nsresult rv;
      bool didChange = false;

      // update the global font list
      nsCOMPtr<nsIFontEnumerator> fontEnum =
          do_GetService("@mozilla.org/gfx/fontenumerator;1", &rv);
      if (NS_SUCCEEDED(rv)) {
        fontEnum->UpdateFontList(&didChange);
        if (didChange) {
          gfxPlatform::ForceGlobalReflow(gfxPlatform::NeedsReframe::Yes);
        }
      }  // if (NS_SUCCEEDED(rv))
    } break;

    case WM_SETTINGCHANGE: {
      if (wParam == SPI_SETCLIENTAREAANIMATION ||
          // CaretBlinkTime is cached in nsLookAndFeel
          wParam == SPI_SETKEYBOARDDELAY) {
        // This only affects reduced motion settings and and carent blink time,
        // so no need to invalidate style / layout.
        NotifyThemeChanged(widget::ThemeChangeKind::MediaQueriesOnly);
        break;
      }
      if (wParam == SPI_SETFONTSMOOTHING ||
          wParam == SPI_SETFONTSMOOTHINGTYPE) {
        gfxDWriteFont::UpdateSystemTextVars();
        break;
      }
      if (wParam == SPI_SETWORKAREA) {
        // NB: We also refresh screens on WM_DISPLAYCHANGE but the rcWork
        // values are sometimes wrong at that point.  This message then
        // arrives soon afterward, when we can get the right rcWork values.
        ScreenHelperWin::RefreshScreens();
        break;
      }
      if (auto lParamString = reinterpret_cast<const wchar_t*>(lParam)) {
        if (!wcscmp(lParamString, L"ImmersiveColorSet")) {
          // This affects system colors (-moz-win-accentcolor), so gotta pass
          // the style flag.
          NotifyThemeChanged(widget::ThemeChangeKind::Style);
          break;
        }

        // UserInteractionMode, ConvertibleSlateMode, SystemDockMode may cause
        // @media(pointer) queries to change, which layout needs to know about
        //
        // (WM_SETTINGCHANGE will be sent to all top-level windows, so we
        //  only respond to the hidden top-level window to avoid hammering
        //  layout with a bunch of NotifyThemeChanged() calls)
        //
        if (mWindowType == eWindowType_invisible) {
          if (!wcscmp(lParamString, L"UserInteractionMode") ||
              !wcscmp(lParamString, L"ConvertibleSlateMode") ||
              !wcscmp(lParamString, L"SystemDockMode")) {
            NotifyThemeChanged(widget::ThemeChangeKind::MediaQueriesOnly);
            WindowsUIUtils::UpdateInTabletMode();
          }
        }
      }

      // SPI_GETMOUSEVANISH sends WM_SETTINGCHANGE when changed but does
      // not include identifiers in the parameters.  WM_SETTINGCHANGE docs
      // recommend updating all cached settings when this message is received
      // anyway.
      GetMouseVanishSystemPref(true);
    } break;

    case WM_DEVICECHANGE: {
      if (wParam == DBT_DEVICEARRIVAL || wParam == DBT_DEVICEREMOVECOMPLETE) {
        DEV_BROADCAST_HDR* hdr = reinterpret_cast<DEV_BROADCAST_HDR*>(lParam);
        // Check dbch_devicetype explicitly since we will get other device types
        // (e.g. DBT_DEVTYP_VOLUME) for some reasons even if we specify
        // DBT_DEVTYP_DEVICEINTERFACE in the filter for
        // RegisterDeviceNotification.
        if (hdr->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE) {
          // This can only change media queries (any-hover/any-pointer).
          NotifyThemeChanged(widget::ThemeChangeKind::MediaQueriesOnly);
        }
      }
    } break;

    case WM_NCCALCSIZE: {
      // NOTE: the following block is mirrored in PreXULSkeletonUI.cpp, and
      // will need to be kept in sync.
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
        RECT* clientRect =
            wParam ? &(reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam))->rgrc[0]
                   : (reinterpret_cast<RECT*>(lParam));
        clientRect->top += mCaptionHeight - mNonClientOffset.top;
        clientRect->left += mHorResizeMargin - mNonClientOffset.left;
        clientRect->right -= mHorResizeMargin - mNonClientOffset.right;
        clientRect->bottom -= mVertResizeMargin - mNonClientOffset.bottom;
        // Make client rect's width and height more than 0 to
        // avoid problems of webrender and angle.
        clientRect->right = std::max(clientRect->right, clientRect->left + 1);
        clientRect->bottom = std::max(clientRect->bottom, clientRect->top + 1);

        result = true;
        *aRetValue = 0;
      }
      break;
    }

    case WM_NCHITTEST: {
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

      if (!mCustomNonClient) break;

      *aRetValue =
          ClientMarginHitTestPoint(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
      result = true;
      break;
    }

    case WM_SETTEXT:
      /*
       * WM_SETTEXT paints the titlebar area. Avoid this if we have a
       * custom titlebar we paint ourselves, or if we're the ones
       * sending the message with an updated title
       */

      if ((mSendingSetText &&
           gfxWindowsPlatform::GetPlatform()->DwmCompositionEnabled()) ||
          !mCustomNonClient || mNonClientMargins.top == -1)
        break;

      {
        // From msdn, the way around this is to disable the visible state
        // temporarily. We need the text to be set but we don't want the
        // redraw to occur. However, we need to make sure that we don't
        // do this at the same time that a Present is happening.
        //
        // To do this we take mPresentLock in nsWindow::PreRender and
        // if that lock is taken we wait before doing WM_SETTEXT
        if (mCompositorWidgetDelegate) {
          mCompositorWidgetDelegate->EnterPresentLock();
        }
        DWORD style = GetWindowLong(mWnd, GWL_STYLE);
        SetWindowLong(mWnd, GWL_STYLE, style & ~WS_VISIBLE);
        *aRetValue =
            CallWindowProcW(GetPrevWindowProc(), mWnd, msg, wParam, lParam);
        SetWindowLong(mWnd, GWL_STYLE, style);
        if (mCompositorWidgetDelegate) {
          mCompositorWidgetDelegate->LeavePresentLock();
        }

        return true;
      }

    case WM_NCACTIVATE: {
      /*
       * WM_NCACTIVATE paints nc areas. Avoid this and re-route painting
       * through WM_NCPAINT via InvalidateNonClientRegion.
       */
      UpdateGetWindowInfoCaptionStatus(FALSE != wParam);

      if (!mCustomNonClient) break;

      // There is a case that rendered result is not kept. Bug 1237617
      if (wParam == TRUE && !gfxEnv::DisableForcePresent() &&
          gfxWindowsPlatform::GetPlatform()->DwmCompositionEnabled()) {
        NS_DispatchToMainThread(NewRunnableMethod(
            "nsWindow::ForcePresent", this, &nsWindow::ForcePresent));
      }

      // let the dwm handle nc painting on glass
      // Never allow native painting if we are on fullscreen
      if (mFrameState->GetSizeMode() != nsSizeMode_Fullscreen &&
          gfxWindowsPlatform::GetPlatform()->DwmCompositionEnabled())
        break;

      if (wParam == TRUE) {
        // going active
        *aRetValue = FALSE;  // ignored
        result = true;
        // invalidate to trigger a paint
        InvalidateNonClientRegion();
        break;
      } else {
        // going inactive
        *aRetValue = TRUE;  // go ahead and deactive
        result = true;
        // invalidate to trigger a paint
        InvalidateNonClientRegion();
        break;
      }
    }

    case WM_NCPAINT: {
      /*
       * ClearType changes often don't send a WM_SETTINGCHANGE message. But they
       * do seem to always send a WM_NCPAINT message, so let's update on that.
       */
      gfxDWriteFont::UpdateSystemTextVars();

      /*
       * Reset the non-client paint region so that it excludes the
       * non-client areas we paint manually. Then call defwndproc
       * to do the actual painting.
       */

      if (!mCustomNonClient) break;

      // let the dwm handle nc painting on glass
      if (gfxWindowsPlatform::GetPlatform()->DwmCompositionEnabled()) break;

      HRGN paintRgn = ExcludeNonClientFromPaintRegion((HRGN)wParam);
      LRESULT res = CallWindowProcW(GetPrevWindowProc(), mWnd, msg,
                                    (WPARAM)paintRgn, lParam);
      if (paintRgn != (HRGN)wParam) DeleteObject(paintRgn);
      *aRetValue = res;
      result = true;
    } break;

    case WM_POWERBROADCAST:
      switch (wParam) {
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

    case WM_CLOSE:  // close request
      if (mWidgetListener) mWidgetListener->RequestWindowClose(this);
      result = true;  // abort window closure
      break;

    case WM_DESTROY:
      // clean up.
      DestroyLayerManager();
      OnDestroy();
      result = true;
      break;

    case WM_PAINT:
      *aRetValue = (int)OnPaint(nullptr, 0);
      result = true;
      break;

    case WM_PRINTCLIENT:
      result = OnPaint((HDC)wParam, 0);
      break;

    case WM_HOTKEY:
      result = OnHotKey(wParam, lParam);
      break;

    case WM_SYSCHAR:
    case WM_CHAR: {
      MSG nativeMsg = WinUtils::InitMSG(msg, wParam, lParam, mWnd);
      result = ProcessCharMessage(nativeMsg, nullptr);
      DispatchPendingEvents();
    } break;

    case WM_SYSKEYUP:
    case WM_KEYUP: {
      MSG nativeMsg = WinUtils::InitMSG(msg, wParam, lParam, mWnd);
      nativeMsg.time = ::GetMessageTime();
      result = ProcessKeyUpMessage(nativeMsg, nullptr);
      DispatchPendingEvents();
    } break;

    case WM_SYSKEYDOWN:
    case WM_KEYDOWN: {
      if (IsMouseVanishKey(wParam)) {
        MaybeHideCursor(true);
      }

      MSG nativeMsg = WinUtils::InitMSG(msg, wParam, lParam, mWnd);
      result = ProcessKeyDownMessage(nativeMsg, nullptr);
      DispatchPendingEvents();
    } break;

    // say we've dealt with erase background if widget does
    // not need auto-erasing
    case WM_ERASEBKGND:
      if (!AutoErase((HDC)wParam)) {
        *aRetValue = 1;
        result = true;
      }
      break;

    case WM_MOUSEMOVE: {
      MaybeHideCursor(false);

      LPARAM lParamScreen = lParamToScreen(lParam);
      mSimulatedClientArea = IsSimulatedClientArea(GET_X_LPARAM(lParamScreen),
                                                   GET_Y_LPARAM(lParamScreen));

      if (!mMousePresent && !sIsInMouseCapture) {
        // First MOUSEMOVE over the client area. Ask for MOUSELEAVE
        TRACKMOUSEEVENT mTrack;
        mTrack.cbSize = sizeof(TRACKMOUSEEVENT);
        mTrack.dwFlags = TME_LEAVE;
        mTrack.dwHoverTime = 0;
        mTrack.hwndTrack = mWnd;
        TrackMouseEvent(&mTrack);
      }
      mMousePresent = true;

      // Suppress dispatch of pending events
      // when mouse moves are generated by widget
      // creation instead of user input.
      POINT mp;
      mp.x = GET_X_LPARAM(lParamScreen);
      mp.y = GET_Y_LPARAM(lParamScreen);
      bool userMovedMouse = false;
      if ((sLastMouseMovePoint.x != mp.x) || (sLastMouseMovePoint.y != mp.y)) {
        userMovedMouse = true;
      }

      result =
          DispatchMouseEvent(eMouseMove, wParam, lParam, false,
                             MouseButton::ePrimary, MOUSE_INPUT_SOURCE(),
                             mPointerEvents.GetCachedPointerInfo(msg, wParam));
      if (userMovedMouse) {
        DispatchPendingEvents();
      }
    } break;

    case WM_NCMOUSEMOVE: {
      MaybeHideCursor(false);

      LPARAM lParamClient = lParamToClient(lParam);
      if (IsSimulatedClientArea(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam))) {
        if (!sIsInMouseCapture) {
          TRACKMOUSEEVENT mTrack;
          mTrack.cbSize = sizeof(TRACKMOUSEEVENT);
          mTrack.dwFlags = TME_LEAVE | TME_NONCLIENT;
          mTrack.dwHoverTime = 0;
          mTrack.hwndTrack = mWnd;
          TrackMouseEvent(&mTrack);
        }
        // If we noticed the mouse moving in our draggable region, forward the
        // message as a normal WM_MOUSEMOVE.
        SendMessage(mWnd, WM_MOUSEMOVE, 0, lParamClient);
      } else {
        // We've transitioned from a draggable area to somewhere else within
        // the non-client area - perhaps one of the edges of the window for
        // resizing.
        mSimulatedClientArea = false;
      }

      if (mMousePresent && !sIsInMouseCapture && !mSimulatedClientArea) {
        SendMessage(mWnd, WM_MOUSELEAVE, 0, 0);
      }
    } break;

    case WM_LBUTTONDOWN: {
      MaybeHideCursor(false);

      result =
          DispatchMouseEvent(eMouseDown, wParam, lParam, false,
                             MouseButton::ePrimary, MOUSE_INPUT_SOURCE(),
                             mPointerEvents.GetCachedPointerInfo(msg, wParam));
      DispatchPendingEvents();
    } break;

    case WM_LBUTTONUP: {
      result =
          DispatchMouseEvent(eMouseUp, wParam, lParam, false,
                             MouseButton::ePrimary, MOUSE_INPUT_SOURCE(),
                             mPointerEvents.GetCachedPointerInfo(msg, wParam));
      DispatchPendingEvents();
    } break;

    case WM_NCMOUSELEAVE: {
      mSimulatedClientArea = false;

      if (EventIsInsideWindow(this)) {
        // If we're handling WM_NCMOUSELEAVE and the mouse is still over the
        // window, then by process of elimination, the mouse has moved from the
        // non-client to client area, so no need to fall-through to the
        // WM_MOUSELEAVE handler. We also need to re-register for the
        // WM_MOUSELEAVE message, since according to the documentation at [1],
        // all tracking requested via TrackMouseEvent is cleared once
        // WM_NCMOUSELEAVE or WM_MOUSELEAVE fires.
        // [1]:
        // https://docs.microsoft.com/en-us/windows/desktop/api/winuser/nf-winuser-trackmouseevent
        TRACKMOUSEEVENT mTrack;
        mTrack.cbSize = sizeof(TRACKMOUSEEVENT);
        mTrack.dwFlags = TME_LEAVE;
        mTrack.dwHoverTime = 0;
        mTrack.hwndTrack = mWnd;
        TrackMouseEvent(&mTrack);
        break;
      }
      // We've transitioned from non-client to outside of the window, so
      // fall-through to the WM_MOUSELEAVE handler.
      [[fallthrough]];
    }
    case WM_MOUSELEAVE: {
      if (!mMousePresent) break;
      if (mSimulatedClientArea) break;
      mMousePresent = false;

      // Check if the mouse is over the fullscreen transition window, if so
      // clear sLastMouseMovePoint. This way the WM_MOUSEMOVE we get after the
      // transition window disappears will not be ignored, even if the mouse
      // hasn't moved.
      if (mTransitionWnd && WindowAtMouse() == mTransitionWnd) {
        sLastMouseMovePoint = {0};
      }

      // We need to check mouse button states and put them in for
      // wParam.
      WPARAM mouseState = (GetKeyState(VK_LBUTTON) ? MK_LBUTTON : 0) |
                          (GetKeyState(VK_MBUTTON) ? MK_MBUTTON : 0) |
                          (GetKeyState(VK_RBUTTON) ? MK_RBUTTON : 0);
      // Synthesize an event position because we don't get one from
      // WM_MOUSELEAVE.
      LPARAM pos = lParamToClient(::GetMessagePos());
      DispatchMouseEvent(eMouseExitFromWidget, mouseState, pos, false,
                         MouseButton::ePrimary, MOUSE_INPUT_SOURCE());
    } break;

    case MOZ_WM_PEN_LEAVES_HOVER_OF_DIGITIZER: {
      LPARAM pos = lParamToClient(::GetMessagePos());
      MOZ_ASSERT(InkCollector::sInkCollector);
      uint16_t pointerId = InkCollector::sInkCollector->GetPointerId();
      if (pointerId != 0) {
        WinPointerInfo pointerInfo;
        pointerInfo.pointerId = pointerId;
        DispatchMouseEvent(eMouseExitFromWidget, wParam, pos, false,
                           MouseButton::ePrimary,
                           MouseEvent_Binding::MOZ_SOURCE_PEN, &pointerInfo);
        InkCollector::sInkCollector->ClearTarget();
        InkCollector::sInkCollector->ClearPointerId();
      }
    } break;

    case WM_CONTEXTMENU: {
      // If the context menu is brought up by a touch long-press, then
      // the APZ code is responsible for dealing with this, so we don't
      // need to do anything.
      if (mTouchWindow &&
          MOUSE_INPUT_SOURCE() == MouseEvent_Binding::MOZ_SOURCE_TOUCH) {
        MOZ_ASSERT(mAPZC);  // since mTouchWindow is true, APZ must be enabled
        result = true;
        break;
      }

      // if the context menu is brought up from the keyboard, |lParam|
      // will be -1.
      LPARAM pos;
      bool contextMenukey = false;
      if (lParam == -1) {
        contextMenukey = true;
        pos = lParamToClient(GetMessagePos());
      } else {
        pos = lParamToClient(lParam);
      }

      result = DispatchMouseEvent(
          eContextMenu, wParam, pos, contextMenukey,
          contextMenukey ? MouseButton::ePrimary : MouseButton::eSecondary,
          MOUSE_INPUT_SOURCE());
      if (lParam != -1 && !result && mCustomNonClient &&
          mDraggableRegion.Contains(GET_X_LPARAM(pos), GET_Y_LPARAM(pos))) {
        // Blank area hit, throw up the system menu.
        DisplaySystemMenu(mWnd, mFrameState->GetSizeMode(), mIsRTL,
                          GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        result = true;
      }
    } break;

    case WM_POINTERLEAVE:
    case WM_POINTERDOWN:
    case WM_POINTERUP:
    case WM_POINTERUPDATE:
      MaybeHideCursor(false);
      result = OnPointerEvents(msg, wParam, lParam);
      if (result) {
        DispatchPendingEvents();
      }
      break;

    case DM_POINTERHITTEST:
      if (mDmOwner) {
        UINT contactId = GET_POINTERID_WPARAM(wParam);
        POINTER_INPUT_TYPE pointerType;
        if (mPointerEvents.GetPointerType(contactId, &pointerType) &&
            pointerType == PT_TOUCHPAD) {
          mDmOwner->SetContact(contactId);
        }
      }
      break;

    case WM_LBUTTONDBLCLK:
      MaybeHideCursor(false);
      result = DispatchMouseEvent(eMouseDoubleClick, wParam, lParam, false,
                                  MouseButton::ePrimary, MOUSE_INPUT_SOURCE());
      DispatchPendingEvents();
      break;

    case WM_MBUTTONDOWN:
      MaybeHideCursor(false);
      result = DispatchMouseEvent(eMouseDown, wParam, lParam, false,
                                  MouseButton::eMiddle, MOUSE_INPUT_SOURCE());
      DispatchPendingEvents();
      break;

    case WM_MBUTTONUP:
      result = DispatchMouseEvent(eMouseUp, wParam, lParam, false,
                                  MouseButton::eMiddle, MOUSE_INPUT_SOURCE());
      DispatchPendingEvents();
      break;

    case WM_MBUTTONDBLCLK:
      MaybeHideCursor(false);
      result = DispatchMouseEvent(eMouseDoubleClick, wParam, lParam, false,
                                  MouseButton::eMiddle, MOUSE_INPUT_SOURCE());
      DispatchPendingEvents();
      break;

    case WM_NCMBUTTONDOWN:
      MaybeHideCursor(false);
      result = DispatchMouseEvent(eMouseDown, 0, lParamToClient(lParam), false,
                                  MouseButton::eMiddle, MOUSE_INPUT_SOURCE());
      DispatchPendingEvents();
      break;

    case WM_NCMBUTTONUP:
      result = DispatchMouseEvent(eMouseUp, 0, lParamToClient(lParam), false,
                                  MouseButton::eMiddle, MOUSE_INPUT_SOURCE());
      DispatchPendingEvents();
      break;

    case WM_NCMBUTTONDBLCLK:
      MaybeHideCursor(false);
      result =
          DispatchMouseEvent(eMouseDoubleClick, 0, lParamToClient(lParam),
                             false, MouseButton::eMiddle, MOUSE_INPUT_SOURCE());
      DispatchPendingEvents();
      break;

    case WM_RBUTTONDOWN:
      MaybeHideCursor(false);
      result =
          DispatchMouseEvent(eMouseDown, wParam, lParam, false,
                             MouseButton::eSecondary, MOUSE_INPUT_SOURCE(),
                             mPointerEvents.GetCachedPointerInfo(msg, wParam));
      DispatchPendingEvents();
      break;

    case WM_RBUTTONUP:
      result =
          DispatchMouseEvent(eMouseUp, wParam, lParam, false,
                             MouseButton::eSecondary, MOUSE_INPUT_SOURCE(),
                             mPointerEvents.GetCachedPointerInfo(msg, wParam));
      DispatchPendingEvents();
      break;

    case WM_RBUTTONDBLCLK:
      MaybeHideCursor(false);
      result =
          DispatchMouseEvent(eMouseDoubleClick, wParam, lParam, false,
                             MouseButton::eSecondary, MOUSE_INPUT_SOURCE());
      DispatchPendingEvents();
      break;

    case WM_NCRBUTTONDOWN:
      MaybeHideCursor(false);
      result =
          DispatchMouseEvent(eMouseDown, 0, lParamToClient(lParam), false,
                             MouseButton::eSecondary, MOUSE_INPUT_SOURCE());
      DispatchPendingEvents();
      break;

    case WM_NCRBUTTONUP:
      result =
          DispatchMouseEvent(eMouseUp, 0, lParamToClient(lParam), false,
                             MouseButton::eSecondary, MOUSE_INPUT_SOURCE());
      DispatchPendingEvents();
      break;

    case WM_NCRBUTTONDBLCLK:
      MaybeHideCursor(false);
      result = DispatchMouseEvent(eMouseDoubleClick, 0, lParamToClient(lParam),
                                  false, MouseButton::eSecondary,
                                  MOUSE_INPUT_SOURCE());
      DispatchPendingEvents();
      break;

    // Windows doesn't provide to customize the behavior of 4th nor 5th button
    // of mouse.  If 5-button mouse works with standard mouse deriver of
    // Windows, users cannot disable 4th button (browser back) nor 5th button
    // (browser forward).  We should allow to do it with our prefs since we can
    // prevent Windows to generate WM_APPCOMMAND message if WM_XBUTTONUP
    // messages are not sent to DefWindowProc.
    case WM_XBUTTONDOWN:
    case WM_XBUTTONUP:
    case WM_NCXBUTTONDOWN:
    case WM_NCXBUTTONUP:
      MaybeHideCursor(false);

      *aRetValue = TRUE;
      switch (GET_XBUTTON_WPARAM(wParam)) {
        case XBUTTON1:
          result = !Preferences::GetBool("mousebutton.4th.enabled", true);
          break;
        case XBUTTON2:
          result = !Preferences::GetBool("mousebutton.5th.enabled", true);
          break;
        default:
          break;
      }
      break;

    case WM_SIZING: {
      if (mAspectRatio > 0) {
        LPRECT rect = (LPRECT)lParam;
        int32_t newWidth, newHeight;

        // The following conditions and switch statement borrow heavily from the
        // Chromium source code from
        // https://chromium.googlesource.com/chromium/src/+/456d6e533cfb4531995e0ef52c279d4b5aa8a352/ui/views/window/window_resize_utils.cc#45
        if (wParam == WMSZ_LEFT || wParam == WMSZ_RIGHT ||
            wParam == WMSZ_TOPLEFT || wParam == WMSZ_BOTTOMLEFT) {
          newWidth = rect->right - rect->left;
          newHeight = newWidth / mAspectRatio;
          if (newHeight < mSizeConstraints.mMinSize.height) {
            newHeight = mSizeConstraints.mMinSize.height;
            newWidth = newHeight * mAspectRatio;
          } else if (newHeight > mSizeConstraints.mMaxSize.height) {
            newHeight = mSizeConstraints.mMaxSize.height;
            newWidth = newHeight * mAspectRatio;
          }
        } else {
          newHeight = rect->bottom - rect->top;
          newWidth = newHeight * mAspectRatio;
          if (newWidth < mSizeConstraints.mMinSize.width) {
            newWidth = mSizeConstraints.mMinSize.width;
            newHeight = newWidth / mAspectRatio;
          } else if (newWidth > mSizeConstraints.mMaxSize.width) {
            newWidth = mSizeConstraints.mMaxSize.width;
            newHeight = newWidth / mAspectRatio;
          }
        }

        switch (wParam) {
          case WMSZ_RIGHT:
          case WMSZ_BOTTOM:
            rect->right = newWidth + rect->left;
            rect->bottom = rect->top + newHeight;
            break;
          case WMSZ_TOP:
            rect->right = newWidth + rect->left;
            rect->top = rect->bottom - newHeight;
            break;
          case WMSZ_LEFT:
          case WMSZ_TOPLEFT:
            rect->left = rect->right - newWidth;
            rect->top = rect->bottom - newHeight;
            break;
          case WMSZ_TOPRIGHT:
            rect->right = rect->left + newWidth;
            rect->top = rect->bottom - newHeight;
            break;
          case WMSZ_BOTTOMLEFT:
            rect->left = rect->right - newWidth;
            rect->bottom = rect->top + newHeight;
            break;
          case WMSZ_BOTTOMRIGHT:
            rect->right = rect->left + newWidth;
            rect->bottom = rect->top + newHeight;
            break;
        }
      }

      // When we get WM_ENTERSIZEMOVE we don't know yet if we're in a live
      // resize or move event. Instead we wait for first VM_SIZING message
      // within a ENTERSIZEMOVE to consider this a live resize event.
      if (mResizeState == IN_SIZEMOVE) {
        mResizeState = RESIZING;
        NotifyLiveResizeStarted();
      }
      break;
    }

    case WM_MOVING:
      FinishLiveResizing(MOVING);
      if (WinUtils::IsPerMonitorDPIAware()) {
        // Sometimes, we appear to miss a WM_DPICHANGED message while moving
        // a window around. Therefore, call ChangedDPI and ResetLayout here
        // if it appears that the window's scaling is not what we expect.
        // This causes the prescontext and appshell window management code to
        // check the appUnitsPerDevPixel value and current widget size, and
        // refresh them if necessary. If nothing has changed, these calls will
        // return without actually triggering any extra reflow or painting.
        if (WinUtils::LogToPhysFactor(mWnd) != mDefaultScale) {
          ChangedDPI();
          ResetLayout();
          if (mWidgetListener) {
            mWidgetListener->UIResolutionChanged();
          }
        }
      }
      break;

    case WM_ENTERSIZEMOVE: {
      if (mResizeState == NOT_RESIZING) {
        mResizeState = IN_SIZEMOVE;
      }
      break;
    }

    case WM_EXITSIZEMOVE: {
      FinishLiveResizing(NOT_RESIZING);

      if (!sIsInMouseCapture) {
        NotifySizeMoveDone();
      }

      break;
    }

    case WM_DISPLAYCHANGE: {
      ScreenHelperWin::RefreshScreens();
      if (mWidgetListener) {
        mWidgetListener->UIResolutionChanged();
      }
      break;
    }

    case WM_NCLBUTTONDBLCLK:
      DispatchMouseEvent(eMouseDoubleClick, 0, lParamToClient(lParam), false,
                         MouseButton::ePrimary, MOUSE_INPUT_SOURCE());
      result = DispatchMouseEvent(eMouseUp, 0, lParamToClient(lParam), false,
                                  MouseButton::ePrimary, MOUSE_INPUT_SOURCE());
      DispatchPendingEvents();
      break;

    case WM_NCLBUTTONDOWN: {
      // Dispatch a custom event when this happens in the draggable region, so
      // that non-popup-based panels can react to it. This doesn't send an
      // actual mousedown event because that would break dragging or interfere
      // with other mousedown handling in the caption area.
      if (ClientMarginHitTestPoint(GET_X_LPARAM(lParam),
                                   GET_Y_LPARAM(lParam)) == HTCAPTION) {
        DispatchCustomEvent(u"draggableregionleftmousedown"_ns);
      }

      if (IsWindowButton(wParam) && mCustomNonClient && !mWindowButtonsRect) {
        DispatchMouseEvent(eMouseDown, wParamFromGlobalMouseState(),
                           lParamToClient(lParam), false, MouseButton::ePrimary,
                           MOUSE_INPUT_SOURCE(), nullptr, true);
        DispatchPendingEvents();
        result = true;
      }
      break;
    }

    case WM_APPCOMMAND: {
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
    case WM_ACTIVATE: {
      int32_t fActive = LOWORD(wParam);
      if (!fActive) {
        MaybeHideCursor(false);
      }

      if (mWidgetListener) {
        if (WA_INACTIVE == fActive) {
          // when minimizing a window, the deactivation and focus events will
          // be fired in the reverse order. Instead, just deactivate right away.
          // This can also happen when a modal system dialog is opened, so check
          // if the last window to receive the WM_KILLFOCUS message was this one
          // or a child of this one.
          if (HIWORD(wParam) ||
              (mLastKillFocusWindow &&
               (GetTopLevelForFocus(mLastKillFocusWindow) == mWnd))) {
            DispatchFocusToTopLevelWindow(false);
          } else {
            sJustGotDeactivate = true;
          }
          if (mIsTopWidgetWindow) {
            mLastKeyboardLayout = KeyboardLayout::GetInstance()->GetLayout();
          }
        } else {
          StopFlashing();

          sJustGotActivate = true;
          WidgetMouseEvent event(true, eMouseActivate, this,
                                 WidgetMouseEvent::eReal);
          InitEvent(event);
          ModifierKeyState modifierKeyState;
          modifierKeyState.InitInputEvent(event);
          DispatchInputEvent(&event);
          if (sSwitchKeyboardLayout && mLastKeyboardLayout)
            ActivateKeyboardLayout(mLastKeyboardLayout, 0);
        }
      }
    } break;

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

    case WM_WINDOWPOSCHANGING: {
      LPWINDOWPOS info = (LPWINDOWPOS)lParam;
      OnWindowPosChanging(info);
      result = true;
    } break;

    case WM_GETMINMAXINFO: {
      MINMAXINFO* mmi = (MINMAXINFO*)lParam;
      // Set the constraints. The minimum size should also be constrained to the
      // default window maximum size so that it fits on screen.
      mmi->ptMinTrackSize.x =
          std::min((int32_t)mmi->ptMaxTrackSize.x,
                   std::max((int32_t)mmi->ptMinTrackSize.x,
                            mSizeConstraints.mMinSize.width));
      mmi->ptMinTrackSize.y =
          std::min((int32_t)mmi->ptMaxTrackSize.y,
                   std::max((int32_t)mmi->ptMinTrackSize.y,
                            mSizeConstraints.mMinSize.height));
      mmi->ptMaxTrackSize.x = std::min((int32_t)mmi->ptMaxTrackSize.x,
                                       mSizeConstraints.mMaxSize.width);
      mmi->ptMaxTrackSize.y = std::min((int32_t)mmi->ptMaxTrackSize.y,
                                       mSizeConstraints.mMaxSize.height);
    } break;

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
      } else {
        mLastKillFocusWindow = mWnd;
      }
      break;

    case WM_WINDOWPOSCHANGED: {
      WINDOWPOS* wp = (LPWINDOWPOS)lParam;
      OnWindowPosChanged(wp);
      result = true;
    } break;

    case WM_INPUTLANGCHANGEREQUEST:
      *aRetValue = TRUE;
      result = false;
      break;

    case WM_INPUTLANGCHANGE:
      KeyboardLayout::GetInstance()->OnLayoutChange(
          reinterpret_cast<HKL>(lParam));
      nsBidiKeyboard::OnLayoutChange();
      result = false;  // always pass to child window
      break;

    case WM_DESTROYCLIPBOARD: {
      nsIClipboard* clipboard;
      nsresult rv = CallGetService(kCClipboardCID, &clipboard);
      if (NS_SUCCEEDED(rv)) {
        clipboard->EmptyClipboard(nsIClipboard::kGlobalClipboard);
        NS_RELEASE(clipboard);
      }
    } break;

#ifdef ACCESSIBILITY
    case WM_GETOBJECT: {
      *aRetValue = 0;
      // Do explicit casting to make it working on 64bit systems (see bug 649236
      // for details).
      int32_t objId = static_cast<DWORD>(lParam);
      if (objId == OBJID_CLIENT) {  // oleacc.dll will be loaded dynamically
        RefPtr<IAccessible> root(
            a11y::LazyInstantiator::GetRootAccessible(mWnd));
        if (root) {
          *aRetValue = LresultFromObject(IID_IAccessible, wParam, root);
          a11y::LazyInstantiator::EnableBlindAggregation(mWnd);
          result = true;
        }
      }
    } break;
#endif

    case WM_SYSCOMMAND: {
      WPARAM filteredWParam = (wParam & 0xFFF0);
      if (mFrameState->GetSizeMode() == nsSizeMode_Fullscreen &&
          filteredWParam == SC_RESTORE &&
          GetCurrentShowCmd(mWnd) != SW_SHOWMINIMIZED) {
        mFrameState->EnsureFullscreenMode(false);
        result = true;
      }

      // Handle the system menu manually when we're in full screen mode
      // so we can set the appropriate options.
      if (filteredWParam == SC_KEYMENU && lParam == VK_SPACE &&
          mFrameState->GetSizeMode() == nsSizeMode_Fullscreen) {
        DisplaySystemMenu(mWnd, mFrameState->GetSizeMode(), mIsRTL,
                          MOZ_SYSCONTEXT_X_POS, MOZ_SYSCONTEXT_Y_POS);
        result = true;
      }
    } break;

    case WM_DWMCOMPOSITIONCHANGED:
      // Every window will get this message, but gfxVars only broadcasts
      // updates when the value actually changes
      if (XRE_IsParentProcess()) {
        BOOL dwmEnabled = FALSE;
        if (FAILED(::DwmIsCompositionEnabled(&dwmEnabled)) || !dwmEnabled) {
          gfxVars::SetDwmCompositionEnabled(false);
        } else {
          gfxVars::SetDwmCompositionEnabled(true);
        }
      }

      UpdateNonClientMargins();
      BroadcastMsg(mWnd, WM_DWMCOMPOSITIONCHANGED);
      // TODO: Why is NotifyThemeChanged needed, what does it affect? And can we
      // make it more granular by tweaking the ChangeKind we pass?
      NotifyThemeChanged(widget::ThemeChangeKind::StyleAndLayout);
      UpdateGlass();
      Invalidate(true, true, true);
      break;

    case WM_DPICHANGED: {
      LPRECT rect = (LPRECT)lParam;
      OnDPIChanged(rect->left, rect->top, rect->right - rect->left,
                   rect->bottom - rect->top);
      break;
    }

    case WM_UPDATEUISTATE: {
      // If the UI state has changed, fire an event so the UI updates the
      // keyboard cues based on the system setting and how the window was
      // opened. For example, a dialog opened via a keyboard press on a button
      // should enable cues, whereas the same dialog opened via a mouse click of
      // the button should not.
      if (mWindowType == eWindowType_toplevel ||
          mWindowType == eWindowType_dialog) {
        int32_t action = LOWORD(wParam);
        if (action == UIS_SET || action == UIS_CLEAR) {
          int32_t flags = HIWORD(wParam);
          UIStateChangeType showFocusRings = UIStateChangeType_NoChange;
          if (flags & UISF_HIDEFOCUS) {
            showFocusRings = (action == UIS_SET) ? UIStateChangeType_Clear
                                                 : UIStateChangeType_Set;
          }
          NotifyUIStateChanged(showFocusRings);
        }
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

    case WM_GESTURENOTIFY: {
      if (mWindowType != eWindowType_invisible) {
        // A GestureNotify event is dispatched to decide which single-finger
        // panning direction should be active (including none) and if pan
        // feedback should be displayed. Java and plugin windows can make their
        // own calls.

        GESTURENOTIFYSTRUCT* gestureinfo = (GESTURENOTIFYSTRUCT*)lParam;
        nsPointWin touchPoint;
        touchPoint = gestureinfo->ptsLocation;
        touchPoint.ScreenToClient(mWnd);
        WidgetGestureNotifyEvent gestureNotifyEvent(true, eGestureNotify, this);
        gestureNotifyEvent.mRefPoint =
            LayoutDeviceIntPoint::FromUnknownPoint(touchPoint);
        nsEventStatus status;
        DispatchEvent(&gestureNotifyEvent, status);
        mDisplayPanFeedback = gestureNotifyEvent.mDisplayPanFeedback;
        if (!mTouchWindow)
          mGesture.SetWinGestureSupport(mWnd, gestureNotifyEvent.mPanDirection);
      }
      result = false;  // should always bubble to DefWindowProc
    } break;

    case WM_CLEAR: {
      WidgetContentCommandEvent command(true, eContentCommandDelete, this);
      DispatchWindowEvent(command);
      result = true;
    } break;

    case WM_CUT: {
      WidgetContentCommandEvent command(true, eContentCommandCut, this);
      DispatchWindowEvent(command);
      result = true;
    } break;

    case WM_COPY: {
      WidgetContentCommandEvent command(true, eContentCommandCopy, this);
      DispatchWindowEvent(command);
      result = true;
    } break;

    case WM_PASTE: {
      WidgetContentCommandEvent command(true, eContentCommandPaste, this);
      DispatchWindowEvent(command);
      result = true;
    } break;

    case EM_UNDO: {
      WidgetContentCommandEvent command(true, eContentCommandUndo, this);
      DispatchWindowEvent(command);
      *aRetValue = (LRESULT)(command.mSucceeded && command.mIsEnabled);
      result = true;
    } break;

    case EM_REDO: {
      WidgetContentCommandEvent command(true, eContentCommandRedo, this);
      DispatchWindowEvent(command);
      *aRetValue = (LRESULT)(command.mSucceeded && command.mIsEnabled);
      result = true;
    } break;

    case EM_CANPASTE: {
      // Support EM_CANPASTE message only when wParam isn't specified or
      // is plain text format.
      if (wParam == 0 || wParam == CF_TEXT || wParam == CF_UNICODETEXT) {
        WidgetContentCommandEvent command(true, eContentCommandPaste, this,
                                          true);
        DispatchWindowEvent(command);
        *aRetValue = (LRESULT)(command.mSucceeded && command.mIsEnabled);
        result = true;
      }
    } break;

    case EM_CANUNDO: {
      WidgetContentCommandEvent command(true, eContentCommandUndo, this, true);
      DispatchWindowEvent(command);
      *aRetValue = (LRESULT)(command.mSucceeded && command.mIsEnabled);
      result = true;
    } break;

    case EM_CANREDO: {
      WidgetContentCommandEvent command(true, eContentCommandRedo, this, true);
      DispatchWindowEvent(command);
      *aRetValue = (LRESULT)(command.mSucceeded && command.mIsEnabled);
      result = true;
    } break;

    case MOZ_WM_SKEWFIX: {
      TimeStamp skewStamp;
      if (CurrentWindowsTimeGetter::GetAndClearBackwardsSkewStamp(wParam,
                                                                  &skewStamp)) {
        TimeConverter().CompensateForBackwardsSkew(::GetMessageTime(),
                                                   skewStamp);
      }
    } break;

    default: {
      if (msg == nsAppShell::GetTaskbarButtonCreatedMessage()) {
        SetHasTaskbarIconBeenCreated();
      }
    } break;
  }

  //*aRetValue = result;
  if (mWnd) {
    return result;
  } else {
    // Events which caused mWnd destruction and aren't consumed
    // will crash during the Windows default processing.
    return true;
  }
}

void nsWindow::FinishLiveResizing(ResizeState aNewState) {
  if (mResizeState == RESIZING) {
    NotifyLiveResizeStopped();
  }
  mResizeState = aNewState;
  ForcePresent();
}

/**************************************************************
 *
 * SECTION: Broadcast messaging
 *
 * Broadcast messages to all windows.
 *
 **************************************************************/

// Enumerate all child windows sending aMsg to each of them
BOOL CALLBACK nsWindow::BroadcastMsgToChildren(HWND aWnd, LPARAM aMsg) {
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
BOOL CALLBACK nsWindow::BroadcastMsg(HWND aTopWindow, LPARAM aMsg) {
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

int32_t nsWindow::ClientMarginHitTestPoint(int32_t mx, int32_t my) {
  if (mFrameState->GetSizeMode() == nsSizeMode_Minimized ||
      mFrameState->GetSizeMode() == nsSizeMode_Fullscreen) {
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

  bool isResizable = (mBorderStyle & (eBorderStyle_all | eBorderStyle_resizeh |
                                      eBorderStyle_default)) > 0
                         ? true
                         : false;
  if (mFrameState->GetSizeMode() == nsSizeMode_Maximized) isResizable = false;

  // Ensure being accessible to borders of window.  Even if contents are in
  // this area, the area must behave as border.
  nsIntMargin nonClientSize(
      std::max(mCaptionHeight - mNonClientOffset.top, kResizableBorderMinSize),
      std::max(mHorResizeMargin - mNonClientOffset.right,
               kResizableBorderMinSize),
      std::max(mVertResizeMargin - mNonClientOffset.bottom,
               kResizableBorderMinSize),
      std::max(mHorResizeMargin - mNonClientOffset.left,
               kResizableBorderMinSize));

  bool allowContentOverride =
      mFrameState->GetSizeMode() == nsSizeMode_Maximized ||
      (mx >= winRect.left + nonClientSize.left &&
       mx <= winRect.right - nonClientSize.right &&
       my >= winRect.top + nonClientSize.top &&
       my <= winRect.bottom - nonClientSize.bottom);

  // The border size.  If there is no content under mouse cursor, the border
  // size should be larger than the values in system settings.  Otherwise,
  // contents under the mouse cursor should be able to override the behavior.
  // E.g., user must expect that Firefox button always opens the popup menu
  // even when the user clicks on the above edge of it.
  nsIntMargin borderSize(std::max(nonClientSize.top, mVertResizeMargin),
                         std::max(nonClientSize.right, mHorResizeMargin),
                         std::max(nonClientSize.bottom, mVertResizeMargin),
                         std::max(nonClientSize.left, mHorResizeMargin));

  bool top = false;
  bool bottom = false;
  bool left = false;
  bool right = false;

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

  bool inResizeRegion = false;
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
      if (left) testResult = HTLEFT;
      if (right) testResult = HTRIGHT;
    }
    inResizeRegion = (testResult != HTCLIENT);
  } else {
    if (top)
      testResult = HTCAPTION;
    else if (bottom || left || right)
      testResult = HTBORDER;
  }

  if (!sIsInMouseCapture && allowContentOverride) {
    POINT pt = {mx, my};
    ::ScreenToClient(mWnd, &pt);

    if (pt.x == mCachedHitTestPoint.x && pt.y == mCachedHitTestPoint.y &&
        TimeStamp::Now() - mCachedHitTestTime <
            TimeDuration::FromMilliseconds(HITTEST_CACHE_LIFETIME_MS)) {
      return mCachedHitTestResult;
    }

    mCachedHitTestPoint = {pt.x, pt.y};
    mCachedHitTestTime = TimeStamp::Now();

    if (mWindowBtnRect[WindowButtonType::Minimize].Contains(pt.x, pt.y)) {
      testResult = HTMINBUTTON;
    } else if (mWindowBtnRect[WindowButtonType::Maximize].Contains(pt.x,
                                                                   pt.y)) {
      testResult = HTMAXBUTTON;
    } else if (mWindowBtnRect[WindowButtonType::Close].Contains(pt.x, pt.y)) {
      testResult = HTCLOSE;
    } else if (!inResizeRegion) {
      // If we're in the resize region, avoid overriding that with either a
      // drag or a client result; resize takes priority over either (but not
      // over the window controls, which is why we check this after those).
      if (mDraggableRegion.Contains(pt.x, pt.y)) {
        testResult = HTCAPTION;
      } else {
        testResult = HTCLIENT;
      }
    }

    mCachedHitTestResult = testResult;
  }

  return testResult;
}

bool nsWindow::IsSimulatedClientArea(int32_t screenX, int32_t screenY) {
  int32_t testResult = ClientMarginHitTestPoint(screenX, screenY);
  return testResult == HTCAPTION || IsWindowButton(testResult);
}

bool nsWindow::IsWindowButton(int32_t hitTestResult) {
  return hitTestResult == HTMINBUTTON || hitTestResult == HTMAXBUTTON ||
         hitTestResult == HTCLOSE;
}

TimeStamp nsWindow::GetMessageTimeStamp(LONG aEventTime) const {
  CurrentWindowsTimeGetter getCurrentTime(mWnd);
  return TimeConverter().GetTimeStampFromSystemTime(aEventTime, getCurrentTime);
}

void nsWindow::PostSleepWakeNotification(const bool aIsSleepMode) {
  if (aIsSleepMode == gIsSleepMode) return;

  gIsSleepMode = aIsSleepMode;

  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  if (observerService)
    observerService->NotifyObservers(nullptr,
                                     aIsSleepMode
                                         ? NS_WIDGET_SLEEP_OBSERVER_TOPIC
                                         : NS_WIDGET_WAKE_OBSERVER_TOPIC,
                                     nullptr);
}

LRESULT nsWindow::ProcessCharMessage(const MSG& aMsg, bool* aEventDispatched) {
  if (IMEHandler::IsComposingOn(this)) {
    IMEHandler::NotifyIME(this, REQUEST_TO_COMMIT_COMPOSITION);
  }
  // These must be checked here too as a lone WM_CHAR could be received
  // if a child window didn't handle it (for example Alt+Space in a content
  // window)
  ModifierKeyState modKeyState;
  NativeKey nativeKey(this, aMsg, modKeyState);
  return static_cast<LRESULT>(nativeKey.HandleCharMessage(aEventDispatched));
}

LRESULT nsWindow::ProcessKeyUpMessage(const MSG& aMsg, bool* aEventDispatched) {
  ModifierKeyState modKeyState;
  NativeKey nativeKey(this, aMsg, modKeyState);
  bool result = nativeKey.HandleKeyUpMessage(aEventDispatched);
  if (aMsg.wParam == VK_F10) {
    // Bug 1382199: Windows default behavior will trigger the System menu bar
    // when F10 is released. Among other things, this causes the System menu bar
    // to appear when a web page overrides the contextmenu event. We *never*
    // want this default behavior, so eat this key (never pass it to Windows).
    return true;
  }
  return result;
}

LRESULT nsWindow::ProcessKeyDownMessage(const MSG& aMsg,
                                        bool* aEventDispatched) {
  // If this method doesn't call NativeKey::HandleKeyDownMessage(), this method
  // must clean up the redirected message information itself.  For more
  // information, see above comment of
  // RedirectedKeyDownMessageManager::AutoFlusher class definition in
  // KeyboardLayout.h.
  RedirectedKeyDownMessageManager::AutoFlusher redirectedMsgFlusher(this, aMsg);

  ModifierKeyState modKeyState;

  NativeKey nativeKey(this, aMsg, modKeyState);
  LRESULT result =
      static_cast<LRESULT>(nativeKey.HandleKeyDownMessage(aEventDispatched));
  // HandleKeyDownMessage cleaned up the redirected message information
  // itself, so, we should do nothing.
  redirectedMsgFlusher.Cancel();

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

nsresult nsWindow::SynthesizeNativeKeyEvent(
    int32_t aNativeKeyboardLayout, int32_t aNativeKeyCode,
    uint32_t aModifierFlags, const nsAString& aCharacters,
    const nsAString& aUnmodifiedCharacters, nsIObserver* aObserver) {
  AutoObserverNotifier notifier(aObserver, "keyevent");

  KeyboardLayout* keyboardLayout = KeyboardLayout::GetInstance();
  return keyboardLayout->SynthesizeNativeKeyEvent(
      this, aNativeKeyboardLayout, aNativeKeyCode, aModifierFlags, aCharacters,
      aUnmodifiedCharacters);
}

nsresult nsWindow::SynthesizeNativeMouseEvent(
    LayoutDeviceIntPoint aPoint, NativeMouseMessage aNativeMessage,
    MouseButton aButton, nsIWidget::Modifiers aModifierFlags,
    nsIObserver* aObserver) {
  AutoObserverNotifier notifier(aObserver, "mouseevent");

  INPUT input;
  memset(&input, 0, sizeof(input));

  // TODO (bug 1693240):
  // Now, we synthesize native mouse events asynchronously since we want to
  // synthesize the event on the front window at the point. However, Windows
  // does not provide a way to set modifier only while a mouse message is
  // being handled, and MOUSEEVENTF_MOVE may be coalesced by Windows.  So, we
  // need a trick for handling it.

  switch (aNativeMessage) {
    case NativeMouseMessage::Move:
      input.mi.dwFlags = MOUSEEVENTF_MOVE;
      // Reset sLastMouseMovePoint so that even if we're moving the mouse
      // to the position it's already at, we still dispatch a mousemove
      // event, because the callers of this function expect that.
      sLastMouseMovePoint = {0};
      break;
    case NativeMouseMessage::ButtonDown:
    case NativeMouseMessage::ButtonUp: {
      const bool isDown = aNativeMessage == NativeMouseMessage::ButtonDown;
      switch (aButton) {
        case MouseButton::ePrimary:
          input.mi.dwFlags = isDown ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
          break;
        case MouseButton::eMiddle:
          input.mi.dwFlags =
              isDown ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP;
          break;
        case MouseButton::eSecondary:
          input.mi.dwFlags =
              isDown ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
          break;
        case MouseButton::eX1:
          input.mi.dwFlags = isDown ? MOUSEEVENTF_XDOWN : MOUSEEVENTF_XUP;
          input.mi.mouseData = XBUTTON1;
          break;
        case MouseButton::eX2:
          input.mi.dwFlags = isDown ? MOUSEEVENTF_XDOWN : MOUSEEVENTF_XUP;
          input.mi.mouseData = XBUTTON2;
          break;
        default:
          return NS_ERROR_INVALID_ARG;
      }
      break;
    }
    case NativeMouseMessage::EnterWindow:
    case NativeMouseMessage::LeaveWindow:
      MOZ_ASSERT_UNREACHABLE("Non supported mouse event on Windows");
      return NS_ERROR_INVALID_ARG;
  }

  input.type = INPUT_MOUSE;
  ::SetCursorPos(aPoint.x, aPoint.y);
  ::SendInput(1, &input, sizeof(INPUT));

  return NS_OK;
}

nsresult nsWindow::SynthesizeNativeMouseScrollEvent(
    LayoutDeviceIntPoint aPoint, uint32_t aNativeMessage, double aDeltaX,
    double aDeltaY, double aDeltaZ, uint32_t aModifierFlags,
    uint32_t aAdditionalFlags, nsIObserver* aObserver) {
  AutoObserverNotifier notifier(aObserver, "mousescrollevent");
  return MouseScrollHandler::SynthesizeNativeMouseScrollEvent(
      this, aPoint, aNativeMessage,
      (aNativeMessage == WM_MOUSEWHEEL || aNativeMessage == WM_VSCROLL)
          ? static_cast<int32_t>(aDeltaY)
          : static_cast<int32_t>(aDeltaX),
      aModifierFlags, aAdditionalFlags);
}

nsresult nsWindow::SynthesizeNativeTouchpadPan(TouchpadGesturePhase aEventPhase,
                                               LayoutDeviceIntPoint aPoint,
                                               double aDeltaX, double aDeltaY,
                                               int32_t aModifierFlags,
                                               nsIObserver* aObserver) {
  AutoObserverNotifier notifier(aObserver, "touchpadpanevent");
  DirectManipulationOwner::SynthesizeNativeTouchpadPan(
      this, aEventPhase, aPoint, aDeltaX, aDeltaY, aModifierFlags);
  return NS_OK;
}

static void MaybeLogSizeMode(nsSizeMode aMode) {
#ifdef WINSTATE_DEBUG_OUTPUT
  switch (aMode) {
    case nsSizeMode_Normal:
      MOZ_LOG(gWindowsLog, LogLevel::Info,
              ("*** SizeMode: nsSizeMode_Normal\n"));
      break;
    case nsSizeMode_Minimized:
      MOZ_LOG(gWindowsLog, LogLevel::Info,
              ("*** SizeMode: nsSizeMode_Minimized\n"));
      break;
    case nsSizeMode_Maximized:
      MOZ_LOG(gWindowsLog, LogLevel::Info,
              ("*** SizeMode: nsSizeMode_Maximized\n"));
      break;
    default:
      MOZ_LOG(gWindowsLog, LogLevel::Info, ("*** SizeMode: ??????\n"));
      break;
  }
#endif
}

static void MaybeLogPosChanged(HWND aWnd, WINDOWPOS* wp) {
#ifdef WINSTATE_DEBUG_OUTPUT
  if (aWnd == WinUtils::GetTopLevelHWND(aWnd)) {
    MOZ_LOG(gWindowsLog, LogLevel::Info, ("*** OnWindowPosChanged: [  top] "));
  } else {
    MOZ_LOG(gWindowsLog, LogLevel::Info, ("*** OnWindowPosChanged: [child] "));
  }
  MOZ_LOG(gWindowsLog, LogLevel::Info, ("WINDOWPOS flags:"));
  if (wp->flags & SWP_FRAMECHANGED) {
    MOZ_LOG(gWindowsLog, LogLevel::Info, ("SWP_FRAMECHANGED "));
  }
  if (wp->flags & SWP_SHOWWINDOW) {
    MOZ_LOG(gWindowsLog, LogLevel::Info, ("SWP_SHOWWINDOW "));
  }
  if (wp->flags & SWP_NOSIZE) {
    MOZ_LOG(gWindowsLog, LogLevel::Info, ("SWP_NOSIZE "));
  }
  if (wp->flags & SWP_HIDEWINDOW) {
    MOZ_LOG(gWindowsLog, LogLevel::Info, ("SWP_HIDEWINDOW "));
  }
  if (wp->flags & SWP_NOZORDER) {
    MOZ_LOG(gWindowsLog, LogLevel::Info, ("SWP_NOZORDER "));
  }
  if (wp->flags & SWP_NOACTIVATE) {
    MOZ_LOG(gWindowsLog, LogLevel::Info, ("SWP_NOACTIVATE "));
  }
  MOZ_LOG(gWindowsLog, LogLevel::Info, ("\n"));
#endif
}

/**************************************************************
 *
 * SECTION: OnXXX message handlers
 *
 * For message handlers that need to be broken out or
 * implemented in specific platform code.
 *
 **************************************************************/

void nsWindow::OnWindowPosChanged(WINDOWPOS* wp) {
  if (wp == nullptr) return;

  MaybeLogPosChanged(mWnd, wp);

  // Handle window size mode changes
  if (wp->flags & SWP_FRAMECHANGED) {
    // Bug 566135 - Windows theme code calls show window on SW_SHOWMINIMIZED
    // windows when fullscreen games disable desktop composition. If we're
    // minimized and not being activated, ignore the event and let windows
    // handle it.
    if (mFrameState->GetSizeMode() == nsSizeMode_Minimized &&
        (wp->flags & SWP_NOACTIVATE)) {
      return;
    }

    mFrameState->OnFrameChanged();

    if (mFrameState->GetSizeMode() == nsSizeMode_Minimized) {
      // Skip window size change events below on minimization.
      return;
    }
  }

  // Notify visibility change when window is activated.
  if (!(wp->flags & SWP_NOACTIVATE) && NeedsToTrackWindowOcclusionState()) {
    WinWindowOcclusionTracker::Get()->OnWindowVisibilityChanged(
        this, mFrameState->GetSizeMode() != nsSizeMode_Minimized);
  }

  // Handle window position changes
  if (!(wp->flags & SWP_NOMOVE)) {
    mBounds.MoveTo(wp->x, wp->y);
    NotifyWindowMoved(wp->x, wp->y);
  }

  // Handle window size changes
  if (!(wp->flags & SWP_NOSIZE)) {
    RECT r;
    int32_t newWidth, newHeight;

    ::GetWindowRect(mWnd, &r);

    newWidth = r.right - r.left;
    newHeight = r.bottom - r.top;

    if (newWidth > mLastSize.width) {
      RECT drect;

      // getting wider
      drect.left = wp->x + mLastSize.width;
      drect.top = wp->y;
      drect.right = drect.left + (newWidth - mLastSize.width);
      drect.bottom = drect.top + newHeight;

      ::RedrawWindow(mWnd, &drect, nullptr,
                     RDW_INVALIDATE | RDW_NOERASE | RDW_NOINTERNALPAINT |
                         RDW_ERASENOW | RDW_ALLCHILDREN);
    }
    if (newHeight > mLastSize.height) {
      RECT drect;

      // getting taller
      drect.left = wp->x;
      drect.top = wp->y + mLastSize.height;
      drect.right = drect.left + newWidth;
      drect.bottom = drect.top + (newHeight - mLastSize.height);

      ::RedrawWindow(mWnd, &drect, nullptr,
                     RDW_INVALIDATE | RDW_NOERASE | RDW_NOINTERNALPAINT |
                         RDW_ERASENOW | RDW_ALLCHILDREN);
    }

    mBounds.SizeTo(newWidth, newHeight);
    mLastSize.width = newWidth;
    mLastSize.height = newHeight;

#ifdef WINSTATE_DEBUG_OUTPUT
    MOZ_LOG(gWindowsLog, LogLevel::Info,
            ("*** Resize window: %d x %d x %d x %d\n", wp->x, wp->y, newWidth,
             newHeight));
#endif

    if (mAspectRatio > 0) {
      // It's possible (via Windows Aero Snap) that the size of the window
      // has changed such that it violates the aspect ratio constraint. If so,
      // queue up an event to enforce the aspect ratio constraint and repaint.
      // When resized with Windows Aero Snap, we are in the NOT_RESIZING state.
      float newAspectRatio = (float)newWidth / newHeight;
      if (mResizeState == NOT_RESIZING && mAspectRatio != newAspectRatio) {
        // Hold a reference to self alive and pass it into the lambda to make
        // sure this nsIWidget stays alive long enough to run this function.
        nsCOMPtr<nsIWidget> self(this);
        NS_DispatchToMainThread(NS_NewRunnableFunction(
            "EnforceAspectRatio", [self, this, newWidth]() -> void {
              if (mWnd) {
                Resize(newWidth, newWidth / mAspectRatio, true);
              }
            }));
      }
    }

    // If a maximized window is resized, recalculate the non-client margins.
    if (mFrameState->GetSizeMode() == nsSizeMode_Maximized) {
      if (UpdateNonClientMargins(nsSizeMode_Maximized, true)) {
        // gecko resize event already sent by UpdateNonClientMargins.
        return;
      }
    }
  }

  // Notify the widget listener for size change of client area for gecko
  // events. This needs to be done when either window size is changed,
  // or window frame is changed. They may not happen together.
  // However, we don't invoke that for popup when window frame changes,
  // because popups may trigger frame change before size change via
  // {Set,Clear}ThemeRegion they invoke in Resize. That would make the
  // code below call OnResize with a wrong client size first, which can
  // lead to flickerling for some popups.
  if (!(wp->flags & SWP_NOSIZE) ||
      ((wp->flags & SWP_FRAMECHANGED) && !IsPopup())) {
    RECT r;
    LayoutDeviceIntSize clientSize;
    if (::GetClientRect(mWnd, &r)) {
      clientSize = WinUtils::ToIntRect(r).Size();
    } else {
      clientSize = mBounds.Size();
    }
    // Send a gecko resize event
    OnResize(clientSize);
  }
}

void nsWindow::OnWindowPosChanging(LPWINDOWPOS& info) {
  // Update non-client margins if the frame size is changing, and let the
  // browser know we are changing size modes, so alternative css can kick in.
  // If we're going into fullscreen mode, ignore this, since it'll reset
  // margins to normal mode.
  if (info->flags & SWP_FRAMECHANGED && !(info->flags & SWP_NOSIZE)) {
    mFrameState->OnFrameChanging();
  }

  // Force fullscreen. This works around a bug in Windows 10 1809 where
  // using fullscreen when a window is "snapped" causes a spurious resize
  // smaller than the full screen, see bug 1482920.
  if (mFrameState->GetSizeMode() == nsSizeMode_Fullscreen &&
      !(info->flags & SWP_NOMOVE) && !(info->flags & SWP_NOSIZE)) {
    nsCOMPtr<nsIScreenManager> screenmgr =
        do_GetService(sScreenManagerContractID);
    if (screenmgr) {
      LayoutDeviceIntRect bounds(info->x, info->y, info->cx, info->cy);
      DesktopIntRect deskBounds =
          RoundedToInt(bounds / GetDesktopToDeviceScale());
      nsCOMPtr<nsIScreen> screen;
      screenmgr->ScreenForRect(deskBounds.X(), deskBounds.Y(),
                               deskBounds.Width(), deskBounds.Height(),
                               getter_AddRefs(screen));

      if (screen) {
        int32_t x, y, width, height;
        screen->GetRect(&x, &y, &width, &height);

        info->x = x;
        info->y = y;
        info->cx = width;
        info->cy = height;
      }
    }
  }

  // enforce local z-order rules
  if (!(info->flags & SWP_NOZORDER)) {
    HWND hwndAfter = info->hwndInsertAfter;

    nsWindow* aboveWindow = 0;
    nsWindowZ placement;

    if (hwndAfter == HWND_BOTTOM)
      placement = nsWindowZBottom;
    else if (hwndAfter == HWND_TOP || hwndAfter == HWND_TOPMOST ||
             hwndAfter == HWND_NOTOPMOST)
      placement = nsWindowZTop;
    else {
      placement = nsWindowZRelative;
      aboveWindow = WinUtils::GetNSWindowPtr(hwndAfter);
    }

    if (mWidgetListener) {
      nsCOMPtr<nsIWidget> actualBelow = nullptr;
      if (mWidgetListener->ZLevelChanged(false, &placement, aboveWindow,
                                         getter_AddRefs(actualBelow))) {
        if (placement == nsWindowZBottom)
          info->hwndInsertAfter = HWND_BOTTOM;
        else if (placement == nsWindowZTop)
          info->hwndInsertAfter = HWND_TOP;
        else {
          info->hwndInsertAfter =
              (HWND)actualBelow->GetNativeData(NS_NATIVE_WINDOW);
        }
      }
    }
  }
  // prevent rude external programs from making hidden window visible
  if (mWindowType == eWindowType_invisible) info->flags &= ~SWP_SHOWWINDOW;

  // When waking from sleep or switching out of tablet mode, Windows 10
  // Version 1809 will reopen popup windows that should be hidden. Detect
  // this case and refuse to show the window.
  static bool sDWMUnhidesPopups = IsWin10Sep2018UpdateOrLater();
  if (sDWMUnhidesPopups && (info->flags & SWP_SHOWWINDOW) &&
      mWindowType == eWindowType_popup && mWidgetListener &&
      mWidgetListener->ShouldNotBeVisible()) {
    info->flags &= ~SWP_SHOWWINDOW;
  }
}

void nsWindow::UserActivity() {
  // Check if we have the idle service, if not we try to get it.
  if (!mIdleService) {
    mIdleService = do_GetService("@mozilla.org/widget/useridleservice;1");
  }

  // Check that we now have the idle service.
  if (mIdleService) {
    mIdleService->ResetIdleTimeOut(0);
  }
}

// Helper function for TouchDeviceNeedsPanGestureConversion(PTOUCHINPUT,
// uint32_t).
static bool TouchDeviceNeedsPanGestureConversion(HANDLE aSource) {
  std::string deviceName;
  UINT dataSize = 0;
  // The first call just queries how long the name string will be.
  GetRawInputDeviceInfoA(aSource, RIDI_DEVICENAME, nullptr, &dataSize);
  if (!dataSize || dataSize > 0x10000) {
    return false;
  }
  deviceName.resize(dataSize);
  // The second call actually populates the string.
  UINT result = GetRawInputDeviceInfoA(aSource, RIDI_DEVICENAME, &deviceName[0],
                                       &dataSize);
  if (result == UINT_MAX) {
    return false;
  }
  // The affected device name is "\\?\VIRTUAL_DIGITIZER", but each backslash
  // needs to be escaped with another one.
  std::string expectedDeviceName = "\\\\?\\VIRTUAL_DIGITIZER";
  // For some reason, the dataSize returned by the first call is double the
  // actual length of the device name (as if it were returning the size of a
  // wide-character string in bytes) even though we are using the narrow
  // version of the API. For the comparison against the expected device name
  // to pass, we truncate the buffer to be no longer tha the expected device
  // name.
  if (deviceName.substr(0, expectedDeviceName.length()) != expectedDeviceName) {
    return false;
  }

  RID_DEVICE_INFO deviceInfo;
  deviceInfo.cbSize = sizeof(deviceInfo);
  dataSize = sizeof(deviceInfo);
  result =
      GetRawInputDeviceInfoA(aSource, RIDI_DEVICEINFO, &deviceInfo, &dataSize);
  if (result == UINT_MAX) {
    return false;
  }
  // The device identifiers that we check for here come from bug 1355162
  // comment 1 (see also bug 1511901 comment 35).
  return deviceInfo.dwType == RIM_TYPEHID && deviceInfo.hid.dwVendorId == 0 &&
         deviceInfo.hid.dwProductId == 0 &&
         deviceInfo.hid.dwVersionNumber == 1 &&
         deviceInfo.hid.usUsagePage == 13 && deviceInfo.hid.usUsage == 4;
}

// Determine if the touch device that originated |aOSEvent| needs to have
// touch events representing a two-finger gesture converted to pan
// gesture events.
// We only do this for touch devices with a specific name and identifiers.
static bool TouchDeviceNeedsPanGestureConversion(PTOUCHINPUT aOSEvent,
                                                 uint32_t aTouchCount) {
  if (!StaticPrefs::apz_windows_check_for_pan_gesture_conversion()) {
    return false;
  }
  if (aTouchCount == 0) {
    return false;
  }
  HANDLE source = aOSEvent[0].hSource;

  // Cache the result of this computation for each touch device.
  // Touch devices are identified by the HANDLE stored in the hSource
  // field of TOUCHINPUT.
  static std::map<HANDLE, bool> sResultCache;
  auto [iter, inserted] = sResultCache.emplace(source, false);
  if (inserted) {
    iter->second = TouchDeviceNeedsPanGestureConversion(source);
  }
  return iter->second;
}

Maybe<PanGestureInput> nsWindow::ConvertTouchToPanGesture(
    const MultiTouchInput& aTouchInput, PTOUCHINPUT aOSEvent) {
  // Checks if the touch device that originated the touch event is one
  // for which we want to convert the touch events to pang gesture events.
  bool shouldConvert = TouchDeviceNeedsPanGestureConversion(
      aOSEvent, aTouchInput.mTouches.Length());
  if (!shouldConvert) {
    return Nothing();
  }

  // Only two-finger gestures need conversion.
  if (aTouchInput.mTouches.Length() != 2) {
    return Nothing();
  }

  PanGestureInput::PanGestureType eventType = PanGestureInput::PANGESTURE_PAN;
  if (aTouchInput.mType == MultiTouchInput::MULTITOUCH_START) {
    eventType = PanGestureInput::PANGESTURE_START;
  } else if (aTouchInput.mType == MultiTouchInput::MULTITOUCH_END) {
    eventType = PanGestureInput::PANGESTURE_END;
  } else if (aTouchInput.mType == MultiTouchInput::MULTITOUCH_CANCEL) {
    eventType = PanGestureInput::PANGESTURE_CANCELLED;
  }

  // Use the midpoint of the two touches as the start point of the pan gesture.
  ScreenPoint focusPoint = (aTouchInput.mTouches[0].mScreenPoint +
                            aTouchInput.mTouches[1].mScreenPoint) /
                           2;
  // To compute the displacement of the pan gesture, we keep track of the
  // location of the previous event.
  ScreenPoint displacement = (eventType == PanGestureInput::PANGESTURE_START)
                                 ? ScreenPoint(0, 0)
                                 : (focusPoint - mLastPanGestureFocus);
  mLastPanGestureFocus = focusPoint;

  // We need to negate the displacement because for a touch event, moving the
  // fingers down results in scrolling up, but for a touchpad gesture, we want
  // moving the fingers down to result in scrolling down.
  PanGestureInput result(eventType, aTouchInput.mTime, aTouchInput.mTimeStamp,
                         focusPoint, -displacement, aTouchInput.modifiers);
  result.mSimulateMomentum = true;

  return Some(result);
}

// Dispatch an event that originated as an OS touch event.
// Usually, we want to dispatch it as a touch event, but some touchpads
// produce touch events for two-finger scrolling, which need to be converted
// to pan gesture events for correct behaviour.
void nsWindow::DispatchTouchOrPanGestureInput(MultiTouchInput& aTouchInput,
                                              PTOUCHINPUT aOSEvent) {
  if (Maybe<PanGestureInput> panInput =
          ConvertTouchToPanGesture(aTouchInput, aOSEvent)) {
    DispatchPanGestureInput(*panInput);
    return;
  }

  DispatchTouchInput(aTouchInput);
}

bool nsWindow::OnTouch(WPARAM wParam, LPARAM lParam) {
  uint32_t cInputs = LOWORD(wParam);
  PTOUCHINPUT pInputs = new TOUCHINPUT[cInputs];

  if (GetTouchInputInfo((HTOUCHINPUT)lParam, cInputs, pInputs,
                        sizeof(TOUCHINPUT))) {
    MultiTouchInput touchInput, touchEndInput;

    // Walk across the touch point array processing each contact point.
    for (uint32_t i = 0; i < cInputs; i++) {
      bool addToEvent = false, addToEndEvent = false;

      // N.B.: According with MS documentation
      // https://msdn.microsoft.com/en-us/library/windows/desktop/dd317334(v=vs.85).aspx
      // TOUCHEVENTF_DOWN cannot be combined with TOUCHEVENTF_MOVE or
      // TOUCHEVENTF_UP.  Possibly, it means that TOUCHEVENTF_MOVE and
      // TOUCHEVENTF_UP can be combined together.

      if (pInputs[i].dwFlags & (TOUCHEVENTF_DOWN | TOUCHEVENTF_MOVE)) {
        if (touchInput.mTimeStamp.IsNull()) {
          // Initialize a touch event to send.
          touchInput.mType = MultiTouchInput::MULTITOUCH_MOVE;
          touchInput.mTime = ::GetMessageTime();
          touchInput.mTimeStamp = GetMessageTimeStamp(touchInput.mTime);
          ModifierKeyState modifierKeyState;
          touchInput.modifiers = modifierKeyState.GetModifiers();
        }
        // Pres shell expects this event to be a eTouchStart
        // if any new contact points have been added since the last event sent.
        if (pInputs[i].dwFlags & TOUCHEVENTF_DOWN) {
          touchInput.mType = MultiTouchInput::MULTITOUCH_START;
        }
        addToEvent = true;
      }
      if (pInputs[i].dwFlags & TOUCHEVENTF_UP) {
        // Pres shell expects removed contacts points to be delivered in a
        // separate eTouchEnd event containing only the contact points that were
        // removed.
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
        // Filter out spurious Windows events we don't understand, like palm
        // contact.
        continue;
      }

      // Setup the touch point we'll append to the touch event array.
      nsPointWin touchPoint;
      touchPoint.x = TOUCH_COORD_TO_PIXEL(pInputs[i].x);
      touchPoint.y = TOUCH_COORD_TO_PIXEL(pInputs[i].y);
      touchPoint.ScreenToClient(mWnd);

      // Initialize the touch data.
      SingleTouchData touchData(
          pInputs[i].dwID,                               // aIdentifier
          ScreenIntPoint::FromUnknownPoint(touchPoint),  // aScreenPoint
          // The contact area info cannot be trusted even when
          // TOUCHINPUTMASKF_CONTACTAREA is set when the input source is pen,
          // which somehow violates the API docs. (bug 1710509) Ultimately the
          // dwFlags check will become redundant since we want to migrate to
          // WM_POINTER for pens. (bug 1707075)
          (pInputs[i].dwMask & TOUCHINPUTMASKF_CONTACTAREA) &&
                  !(pInputs[i].dwFlags & TOUCHEVENTF_PEN)
              ? ScreenSize(TOUCH_COORD_TO_PIXEL(pInputs[i].cxContact) / 2,
                           TOUCH_COORD_TO_PIXEL(pInputs[i].cyContact) / 2)
              : ScreenSize(1, 1),  // aRadius
          0.0f,                    // aRotationAngle
          0.0f);                   // aForce

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
      DispatchTouchOrPanGestureInput(touchInput, pInputs);
    }
    // Dispatch touch end event if we have one.
    if (!touchEndInput.mTimeStamp.IsNull()) {
      DispatchTouchOrPanGestureInput(touchEndInput, pInputs);
    }
  }

  delete[] pInputs;
  CloseTouchInputHandle((HTOUCHINPUT)lParam);
  return true;
}

// Gesture event processing. Handles WM_GESTURE events.
bool nsWindow::OnGesture(WPARAM wParam, LPARAM lParam) {
  // Treatment for pan events which translate into scroll events:
  if (mGesture.IsPanEvent(lParam)) {
    if (!mGesture.ProcessPanMessage(mWnd, wParam, lParam))
      return false;  // ignore

    nsEventStatus status;

    WidgetWheelEvent wheelEvent(true, eWheel, this);

    ModifierKeyState modifierKeyState;
    modifierKeyState.InitInputEvent(wheelEvent);

    wheelEvent.mButton = 0;
    wheelEvent.mTime = ::GetMessageTime();
    wheelEvent.mTimeStamp = GetMessageTimeStamp(wheelEvent.mTime);
    wheelEvent.mInputSource = MouseEvent_Binding::MOZ_SOURCE_TOUCH;

    bool endFeedback = true;

    if (mGesture.PanDeltaToPixelScroll(wheelEvent)) {
      DispatchEvent(&wheelEvent, status);
    }

    if (mDisplayPanFeedback) {
      mGesture.UpdatePanFeedbackX(
          mWnd, DeprecatedAbs(RoundDown(wheelEvent.mOverflowDeltaX)),
          endFeedback);
      mGesture.UpdatePanFeedbackY(
          mWnd, DeprecatedAbs(RoundDown(wheelEvent.mOverflowDeltaY)),
          endFeedback);
      mGesture.PanFeedbackFinalize(mWnd, endFeedback);
    }

    CloseGestureInfoHandle((HGESTUREINFO)lParam);

    return true;
  }

  // Other gestures translate into simple gesture events:
  WidgetSimpleGestureEvent event(true, eVoidEvent, this);
  if (!mGesture.ProcessGestureMessage(mWnd, wParam, lParam, event)) {
    return false;  // fall through to DefWndProc
  }

  // Polish up and send off the new event
  ModifierKeyState modifierKeyState;
  modifierKeyState.InitInputEvent(event);
  event.mButton = 0;
  event.mTime = ::GetMessageTime();
  event.mTimeStamp = GetMessageTimeStamp(event.mTime);
  event.mInputSource = MouseEvent_Binding::MOZ_SOURCE_TOUCH;

  nsEventStatus status;
  DispatchEvent(&event, status);
  if (status == nsEventStatus_eIgnore) {
    return false;  // Ignored, fall through
  }

  // Only close this if we process and return true.
  CloseGestureInfoHandle((HGESTUREINFO)lParam);

  return true;  // Handled
}

// WM_DESTROY event handler
void nsWindow::OnDestroy() {
  mOnDestroyCalled = true;

  // Make sure we don't get destroyed in the process of tearing down.
  nsCOMPtr<nsIWidget> kungFuDeathGrip(this);

  // Dispatch the destroy notification.
  if (!mInDtor) NotifyWindowDestroyed();

  // Prevent the widget from sending additional events.
  mWidgetListener = nullptr;
  mAttachedWidgetListener = nullptr;

  DestroyDirectManipulation();

  if (mWnd == mLastKillFocusWindow) {
    mLastKillFocusWindow = nullptr;
  }
  // Unregister notifications from terminal services
  ::WTSUnRegisterSessionNotification(mWnd);

  // Free our subclass and clear |this| stored in the window props. We will no
  // longer receive events from Windows after this point.
  SubclassWindow(FALSE);

  // Once mWidgetListener is cleared and the subclass is reset, sCurrentWindow
  // can be cleared. (It's used in tracking windows for mouse events.)
  if (sCurrentWindow == this) sCurrentWindow = nullptr;

  // Disconnects us from our parent, will call our GetParent().
  nsBaseWidget::Destroy();

  // Release references to children, device context, toolkit, and app shell.
  nsBaseWidget::OnDestroy();

  // Clear our native parent handle.
  // XXX Windows will take care of this in the proper order, and
  // SetParent(nullptr)'s remove child on the parent already took place in
  // nsBaseWidget's Destroy call above.
  // SetParent(nullptr);
  mParent = nullptr;

  // We have to destroy the native drag target before we null out our window
  // pointer.
  EnableDragDrop(false);

  // If we're going away and for some reason we're still the rollup widget,
  // rollup and turn off capture.
  nsIRollupListener* rollupListener = nsBaseWidget::GetActiveRollupListener();
  nsCOMPtr<nsIWidget> rollupWidget;
  if (rollupListener) {
    rollupWidget = rollupListener->GetRollupWidget();
  }
  if (this == rollupWidget) {
    if (rollupListener) rollupListener->Rollup(0, false, nullptr, nullptr);
    CaptureRollupEvents(nullptr, false);
  }

  IMEHandler::OnDestroyWindow(this);

  // Free GDI window class objects
  if (mBrush) {
    VERIFY(::DeleteObject(mBrush));
    mBrush = nullptr;
  }

  // Destroy any custom cursor resources.
  if (mCursor.IsCustom()) {
    SetCursor(Cursor{eCursor_standard});
  }

  if (mCompositorWidgetDelegate) {
    mCompositorWidgetDelegate->OnDestroyWindow();
  }
  mBasicLayersSurface = nullptr;

  // Finalize panning feedback to possibly restore window displacement
  mGesture.PanFeedbackFinalize(mWnd, true);

  // Clear the main HWND.
  mWnd = nullptr;
}

// Send a resize message to the listener
bool nsWindow::OnResize(const LayoutDeviceIntSize& aSize) {
  if (mCompositorWidgetDelegate &&
      !mCompositorWidgetDelegate->OnWindowResize(aSize)) {
    return false;
  }

  bool result = false;
  if (mWidgetListener) {
    result = mWidgetListener->WindowResized(this, aSize.width, aSize.height);
  }

  // If there is an attached view, inform it as well as the normal widget
  // listener.
  if (mAttachedWidgetListener) {
    return mAttachedWidgetListener->WindowResized(this, aSize.width,
                                                  aSize.height);
  }

  return result;
}

void nsWindow::OnSizeModeChange(nsSizeMode aSizeMode) {
  MOZ_LOG(gWindowsLog, LogLevel::Info,
          ("nsWindow::OnSizeModeChange() aSizeMode %d", aSizeMode));

  if (NeedsToTrackWindowOcclusionState()) {
    WinWindowOcclusionTracker::Get()->OnWindowVisibilityChanged(
        this, aSizeMode != nsSizeMode_Minimized);

    wr::DebugFlags flags{0};
    flags.bits = gfx::gfxVars::WebRenderDebugFlags();
    bool debugEnabled = bool(flags & wr::DebugFlags::WINDOW_VISIBILITY_DBG);
    if (debugEnabled && mCompositorWidgetDelegate) {
      mCompositorWidgetDelegate->NotifyVisibilityUpdated(aSizeMode,
                                                         mIsFullyOccluded);
    }
  }

  if (mCompositorWidgetDelegate) {
    mCompositorWidgetDelegate->OnWindowModeChange(aSizeMode);
  }

  if (mWidgetListener) {
    mWidgetListener->SizeModeChanged(aSizeMode);
  }
}

bool nsWindow::OnHotKey(WPARAM wParam, LPARAM lParam) { return true; }

// Can be overriden. Controls auto-erase of background.
bool nsWindow::AutoErase(HDC dc) { return false; }

bool nsWindow::IsPopup() { return mWindowType == eWindowType_popup; }

bool nsWindow::ShouldUseOffMainThreadCompositing() {
  if (mWindowType == eWindowType_popup && mPopupType == ePopupTypeTooltip) {
    return false;
  }

  // Content rendering of popup is always done by child window.
  // See nsDocumentViewer::ShouldAttachToTopLevel().
  if (mWindowType == eWindowType_popup && !mIsChildWindow) {
    MOZ_ASSERT(!mParent);
    return false;
  }

  return nsBaseWidget::ShouldUseOffMainThreadCompositing();
}

void nsWindow::WindowUsesOMTC() {
  ULONG_PTR style = ::GetClassLongPtr(mWnd, GCL_STYLE);
  if (!style) {
    NS_WARNING("Could not get window class style");
    return;
  }
  style |= CS_HREDRAW | CS_VREDRAW;
  DebugOnly<ULONG_PTR> result = ::SetClassLongPtr(mWnd, GCL_STYLE, style);
  NS_WARNING_ASSERTION(result, "Could not reset window class style");
}

bool nsWindow::HasBogusPopupsDropShadowOnMultiMonitor() {
  if (sHasBogusPopupsDropShadowOnMultiMonitor == TRI_UNKNOWN) {
    // Since any change in the preferences requires a restart, this can be
    // done just once.
    // Check for Direct2D first.
    sHasBogusPopupsDropShadowOnMultiMonitor =
        gfxWindowsPlatform::GetPlatform()->IsDirect2DBackend() ? TRI_TRUE
                                                               : TRI_FALSE;
    if (!sHasBogusPopupsDropShadowOnMultiMonitor) {
      // Otherwise check if Direct3D 9 may be used.
      if (gfxConfig::IsEnabled(gfx::Feature::HW_COMPOSITING) &&
          !gfxConfig::IsEnabled(gfx::Feature::OPENGL_COMPOSITING)) {
        nsCOMPtr<nsIGfxInfo> gfxInfo = components::GfxInfo::Service();
        if (gfxInfo) {
          int32_t status;
          nsCString discardFailureId;
          if (NS_SUCCEEDED(gfxInfo->GetFeatureStatus(
                  nsIGfxInfo::FEATURE_DIRECT3D_9_LAYERS, discardFailureId,
                  &status))) {
            if (status == nsIGfxInfo::FEATURE_STATUS_OK ||
                gfxConfig::IsForcedOnByUser(gfx::Feature::HW_COMPOSITING)) {
              sHasBogusPopupsDropShadowOnMultiMonitor = TRI_TRUE;
            }
          }
        }
      }
    }
  }
  return !!sHasBogusPopupsDropShadowOnMultiMonitor;
}

void nsWindow::OnDPIChanged(int32_t x, int32_t y, int32_t width,
                            int32_t height) {
  // Don't try to handle WM_DPICHANGED for popup windows (see bug 1239353);
  // they remain tied to their original parent's resolution.
  if (mWindowType == eWindowType_popup) {
    return;
  }
  if (StaticPrefs::layout_css_devPixelsPerPx() > 0.0) {
    return;
  }
  mDefaultScale = -1.0;  // force recomputation of scale factor

  if (mResizeState != RESIZING &&
      mFrameState->GetSizeMode() == nsSizeMode_Normal) {
    // Limit the position (if not in the middle of a drag-move) & size,
    // if it would overflow the destination screen
    nsCOMPtr<nsIScreenManager> sm = do_GetService(sScreenManagerContractID);
    if (sm) {
      nsCOMPtr<nsIScreen> screen;
      sm->ScreenForRect(x, y, width, height, getter_AddRefs(screen));
      if (screen) {
        int32_t availLeft, availTop, availWidth, availHeight;
        screen->GetAvailRect(&availLeft, &availTop, &availWidth, &availHeight);
        if (mResizeState != MOVING) {
          x = std::max(x, availLeft);
          y = std::max(y, availTop);
        }
        width = std::min(width, availWidth);
        height = std::min(height, availHeight);
      }
    }

    Resize(x, y, width, height, true);
  }
  UpdateNonClientMargins();
  ChangedDPI();
  ResetLayout();
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

void nsWindow::SetInputContext(const InputContext& aContext,
                               const InputContextAction& aAction) {
  InputContext newInputContext = aContext;
  IMEHandler::SetInputContext(this, newInputContext, aAction);
  mInputContext = newInputContext;
}

InputContext nsWindow::GetInputContext() {
  mInputContext.mIMEState.mOpen = IMEState::CLOSED;
  if (WinUtils::IsIMEEnabled(mInputContext) && IMEHandler::GetOpenState(this)) {
    mInputContext.mIMEState.mOpen = IMEState::OPEN;
  } else {
    mInputContext.mIMEState.mOpen = IMEState::CLOSED;
  }
  return mInputContext;
}

TextEventDispatcherListener* nsWindow::GetNativeTextEventDispatcherListener() {
  return IMEHandler::GetNativeTextEventDispatcherListener();
}

#ifdef ACCESSIBILITY
#  ifdef DEBUG
#    define NS_LOG_WMGETOBJECT(aWnd, aHwnd, aAcc)                            \
      if (a11y::logging::IsEnabled(a11y::logging::ePlatforms)) {             \
        printf(                                                              \
            "Get the window:\n  {\n     HWND: %p, parent HWND: %p, wndobj: " \
            "%p,\n",                                                         \
            aHwnd, ::GetParent(aHwnd), aWnd);                                \
        printf("     acc: %p", aAcc);                                        \
        if (aAcc) {                                                          \
          nsAutoString name;                                                 \
          aAcc->Name(name);                                                  \
          printf(", accname: %s", NS_ConvertUTF16toUTF8(name).get());        \
        }                                                                    \
        printf("\n }\n");                                                    \
      }

#  else
#    define NS_LOG_WMGETOBJECT(aWnd, aHwnd, aAcc)
#  endif

a11y::LocalAccessible* nsWindow::GetAccessible() {
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
      nsAccessibilityService* accService = GetOrCreateAccService();
      if (accService) {
        a11y::DocAccessible* docAcc =
            GetAccService()->GetDocAccessible(frame->PresShell());
        if (docAcc) {
          NS_LOG_WMGETOBJECT(
              this, mWnd,
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

void nsWindow::SetWindowTranslucencyInner(nsTransparencyMode aMode) {
  if (aMode == mTransparencyMode) return;

  // stop on dialogs and popups!
  HWND hWnd = WinUtils::GetTopLevelHWND(mWnd, true);
  nsWindow* parent = WinUtils::GetNSWindowPtr(hWnd);

  if (!parent) {
    NS_WARNING("Trying to use transparent chrome in an embedded context");
    return;
  }

  if (parent != this) {
    NS_WARNING(
        "Setting SetWindowTranslucencyInner on a parent this is not us!");
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

  if (parent->mIsVisible) {
    style |= WS_VISIBLE;
    if (parent->mFrameState->GetSizeMode() == nsSizeMode_Maximized) {
      style |= WS_MAXIMIZE;
    } else if (parent->mFrameState->GetSizeMode() == nsSizeMode_Minimized) {
      style |= WS_MINIMIZE;
    }
  }

  if (aMode == eTransparencyTransparent)
    exStyle |= WS_EX_LAYERED;
  else
    exStyle &= ~WS_EX_LAYERED;

  VERIFY_WINDOW_STYLE(style);
  ::SetWindowLongPtrW(hWnd, GWL_STYLE, style);
  ::SetWindowLongPtrW(hWnd, GWL_EXSTYLE, exStyle);

  if (HasGlass()) memset(&mGlassMargins, 0, sizeof mGlassMargins);
  mTransparencyMode = aMode;

  if (mCompositorWidgetDelegate) {
    mCompositorWidgetDelegate->UpdateTransparency(aMode);
  }
  UpdateGlass();

  // Clear window by transparent black when compositor window is used in GPU
  // process and non-client area rendering by DWM is enabled.
  // It is for showing non-client area rendering. See nsWindow::UpdateGlass().
  if (HasGlass() && GetWindowRenderer()->AsKnowsCompositor() &&
      GetWindowRenderer()->AsKnowsCompositor()->GetUseCompositorWnd()) {
    HDC hdc;
    RECT rect;
    hdc = ::GetWindowDC(mWnd);
    ::GetWindowRect(mWnd, &rect);
    ::MapWindowPoints(nullptr, mWnd, (LPPOINT)&rect, 2);
    ::FillRect(hdc, &rect,
               reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
    ReleaseDC(mWnd, hdc);
  }

  // Disable double buffering with D3D compositor for disabling compositor
  // window usage.
  if (HasGlass() && !gfxVars::UseWebRender() &&
      gfxVars::UseDoubleBufferingWithCompositor()) {
    gfxVars::SetUseDoubleBufferingWithCompositor(false);
    GPUProcessManager::Get()->ResetCompositors();
  }
}

/**************************************************************
 **************************************************************
 **
 ** BLOCK: Popup rollup hooks
 **
 ** Deals with CaptureRollup on popup windows.
 **
 **************************************************************
 **************************************************************/

// Schedules a timer for a window, so we can rollup after processing the hook
// event
void nsWindow::ScheduleHookTimer(HWND aWnd, UINT aMsgId) {
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
LRESULT CALLBACK nsWindow::MozSpecialMsgFilter(int code, WPARAM wParam,
                                               LPARAM lParam) {
#ifdef POPUP_ROLLUP_DEBUG_OUTPUT
  if (sProcessHook) {
    MSG* pMsg = (MSG*)lParam;

    int inx = 0;
    while (gMSGFEvents[inx].mId != code && gMSGFEvents[inx].mStr != nullptr) {
      inx++;
    }
    if (code != gLastMsgCode) {
      if (gMSGFEvents[inx].mId == code) {
#  ifdef DEBUG
        MOZ_LOG(gWindowsLog, LogLevel::Info,
                ("MozSpecialMessageProc - code: 0x%X  - %s  hw: %p\n", code,
                 gMSGFEvents[inx].mStr, pMsg->hwnd));
#  endif
      } else {
#  ifdef DEBUG
        MOZ_LOG(gWindowsLog, LogLevel::Info,
                ("MozSpecialMessageProc - code: 0x%X  - %d  hw: %p\n", code,
                 gMSGFEvents[inx].mId, pMsg->hwnd));
#  endif
      }
      gLastMsgCode = code;
    }
    PrintEvent(pMsg->message, FALSE, FALSE);
  }
#endif  // #ifdef POPUP_ROLLUP_DEBUG_OUTPUT

  if (sProcessHook && code == MSGF_MENU) {
    MSG* pMsg = (MSG*)lParam;
    ScheduleHookTimer(pMsg->hwnd, pMsg->message);
  }

  return ::CallNextHookEx(sMsgFilterHook, code, wParam, lParam);
}

// Process all mouse messages. Roll up when a click is in a native window
// that doesn't have an nsIWidget.
LRESULT CALLBACK nsWindow::MozSpecialMouseProc(int code, WPARAM wParam,
                                               LPARAM lParam) {
  if (sProcessHook) {
    switch (WinUtils::GetNativeMessage(wParam)) {
      case WM_LBUTTONDOWN:
      case WM_RBUTTONDOWN:
      case WM_MBUTTONDOWN:
      case WM_MOUSEWHEEL:
      case WM_MOUSEHWHEEL: {
        MOUSEHOOKSTRUCT* ms = (MOUSEHOOKSTRUCT*)lParam;
        nsIWidget* mozWin = WinUtils::GetNSWindowPtr(ms->hwnd);
        if (!mozWin) {
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
LRESULT CALLBACK nsWindow::MozSpecialWndProc(int code, WPARAM wParam,
                                             LPARAM lParam) {
#ifdef POPUP_ROLLUP_DEBUG_OUTPUT
  if (sProcessHook) {
    CWPSTRUCT* cwpt = (CWPSTRUCT*)lParam;
    PrintEvent(cwpt->message, FALSE, FALSE);
  }
#endif

  if (sProcessHook) {
    CWPSTRUCT* cwpt = (CWPSTRUCT*)lParam;
    if (cwpt->message == WM_MOVING || cwpt->message == WM_SIZING ||
        cwpt->message == WM_GETMINMAXINFO) {
      ScheduleHookTimer(cwpt->hwnd, (UINT)cwpt->message);
    }
  }

  return ::CallNextHookEx(sCallProcHook, code, wParam, lParam);
}

// Register the special "hooks" for dropdown processing.
void nsWindow::RegisterSpecialDropdownHooks() {
  NS_ASSERTION(!sMsgFilterHook, "sMsgFilterHook must be NULL!");
  NS_ASSERTION(!sCallProcHook, "sCallProcHook must be NULL!");

  DISPLAY_NMM_PRT("***************** Installing Msg Hooks ***************\n");

  // Install msg hook for moving the window and resizing
  if (!sMsgFilterHook) {
    DISPLAY_NMM_PRT("***** Hooking sMsgFilterHook!\n");
    sMsgFilterHook = SetWindowsHookEx(WH_MSGFILTER, MozSpecialMsgFilter,
                                      nullptr, GetCurrentThreadId());
#ifdef POPUP_ROLLUP_DEBUG_OUTPUT
    if (!sMsgFilterHook) {
      MOZ_LOG(gWindowsLog, LogLevel::Info,
              ("***** SetWindowsHookEx is NOT installed for WH_MSGFILTER!\n"));
    }
#endif
  }

  // Install msg hook for menus
  if (!sCallProcHook) {
    DISPLAY_NMM_PRT("***** Hooking sCallProcHook!\n");
    sCallProcHook = SetWindowsHookEx(WH_CALLWNDPROC, MozSpecialWndProc, nullptr,
                                     GetCurrentThreadId());
#ifdef POPUP_ROLLUP_DEBUG_OUTPUT
    if (!sCallProcHook) {
      MOZ_LOG(
          gWindowsLog, LogLevel::Info,
          ("***** SetWindowsHookEx is NOT installed for WH_CALLWNDPROC!\n"));
    }
#endif
  }

  // Install msg hook for the mouse
  if (!sCallMouseHook) {
    DISPLAY_NMM_PRT("***** Hooking sCallMouseHook!\n");
    sCallMouseHook = SetWindowsHookEx(WH_MOUSE, MozSpecialMouseProc, nullptr,
                                      GetCurrentThreadId());
#ifdef POPUP_ROLLUP_DEBUG_OUTPUT
    if (!sCallMouseHook) {
      MOZ_LOG(gWindowsLog, LogLevel::Info,
              ("***** SetWindowsHookEx is NOT installed for WH_MOUSE!\n"));
    }
#endif
  }
}

// Unhook special message hooks for dropdowns.
void nsWindow::UnregisterSpecialDropdownHooks() {
  DISPLAY_NMM_PRT(
      "***************** De-installing Msg Hooks ***************\n");

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

// This timer is designed to only fire one time at most each time a "hook"
// function is used to rollup the dropdown. In some cases, the timer may be
// scheduled from the hook, but that hook event or a subsequent event may roll
// up the dropdown before this timer function is executed.
//
// For example, if an MFC control takes focus, the combobox will lose focus and
// rollup before this function fires.
VOID CALLBACK nsWindow::HookTimerForPopups(HWND hwnd, UINT uMsg, UINT idEvent,
                                           DWORD dwTime) {
  if (sHookTimerId != 0) {
    // if the window is nullptr then we need to use the ID to kill the timer
    DebugOnly<BOOL> status = ::KillTimer(nullptr, sHookTimerId);
    NS_ASSERTION(status, "Hook Timer was not killed.");
    sHookTimerId = 0;
  }

  if (sRollupMsgId != 0) {
    // Note: DealWithPopups does the check to make sure that the rollup widget
    // is set.
    LRESULT popupHandlingResult;
    nsAutoRollup autoRollup;
    DealWithPopups(sRollupMsgWnd, sRollupMsgId, 0, 0, &popupHandlingResult);
    sRollupMsgId = 0;
    sRollupMsgWnd = nullptr;
  }
}

static bool IsDifferentThreadWindow(HWND aWnd) {
  return ::GetCurrentThreadId() != ::GetWindowThreadProcessId(aWnd, nullptr);
}

// static
bool nsWindow::EventIsInsideWindow(nsWindow* aWindow,
                                   Maybe<POINT> aEventPoint) {
  RECT r;
  ::GetWindowRect(aWindow->mWnd, &r);
  POINT mp;
  if (aEventPoint) {
    mp = *aEventPoint;
  } else {
    DWORD pos = ::GetMessagePos();
    mp.x = GET_X_LPARAM(pos);
    mp.y = GET_Y_LPARAM(pos);
  }

  // was the event inside this window?
  return static_cast<bool>(::PtInRect(&r, mp));
}

// static
bool nsWindow::GetPopupsToRollup(nsIRollupListener* aRollupListener,
                                 uint32_t* aPopupsToRollup,
                                 Maybe<POINT> aEventPoint) {
  // If we're dealing with menus, we probably have submenus and we don't want
  // to rollup some of them if the click is in a parent menu of the current
  // submenu.
  *aPopupsToRollup = UINT32_MAX;
  AutoTArray<nsIWidget*, 5> widgetChain;
  uint32_t sameTypeCount = aRollupListener->GetSubmenuWidgetChain(&widgetChain);
  for (uint32_t i = 0; i < widgetChain.Length(); ++i) {
    nsIWidget* widget = widgetChain[i];
    if (EventIsInsideWindow(static_cast<nsWindow*>(widget), aEventPoint)) {
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
bool nsWindow::NeedsToHandleNCActivateDelayed(HWND aWnd) {
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

static bool IsTouchSupportEnabled(HWND aWnd) {
  nsWindow* topWindow =
      WinUtils::GetNSWindowPtr(WinUtils::GetTopLevelHWND(aWnd, true));
  return topWindow ? topWindow->IsTouchWindow() : false;
}

static Maybe<POINT> GetSingleTouch(WPARAM wParam, LPARAM lParam) {
  Maybe<POINT> ret;
  uint32_t cInputs = LOWORD(wParam);
  if (cInputs != 1) {
    return ret;
  }
  TOUCHINPUT input;
  if (GetTouchInputInfo((HTOUCHINPUT)lParam, cInputs, &input,
                        sizeof(TOUCHINPUT))) {
    ret.emplace();
    ret->x = TOUCH_COORD_TO_PIXEL(input.x);
    ret->y = TOUCH_COORD_TO_PIXEL(input.y);
  }
  // Note that we don't call CloseTouchInputHandle here because we need
  // to read the touch input info again in OnTouch later.
  return ret;
}

// static
bool nsWindow::DealWithPopups(HWND aWnd, UINT aMessage, WPARAM aWParam,
                              LPARAM aLParam, LRESULT* aResult) {
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

  bool consumeRollupEvent = false;
  Maybe<POINT> touchPoint;  // In screen coords.

  nsWindow* popupWindow = static_cast<nsWindow*>(popup.get());
  UINT nativeMessage = WinUtils::GetNativeMessage(aMessage);
  switch (nativeMessage) {
    case WM_TOUCH:
      if (!IsTouchSupportEnabled(aWnd)) {
        // If APZ is disabled, don't allow touch inputs to dismiss popups. The
        // compatibility mouse events will do it instead.
        return false;
      }
      touchPoint = GetSingleTouch(aWParam, aLParam);
      if (!touchPoint) {
        return false;
      }
      [[fallthrough]];
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_NCLBUTTONDOWN:
    case WM_NCRBUTTONDOWN:
    case WM_NCMBUTTONDOWN:
      if (nativeMessage != WM_TOUCH && IsTouchSupportEnabled(aWnd) &&
          MOUSE_INPUT_SOURCE() == MouseEvent_Binding::MOZ_SOURCE_TOUCH) {
        // If any of these mouse events are really compatibility events that
        // Windows is sending for touch inputs, then don't allow them to dismiss
        // popups when APZ is enabled (instead we do the dismissing as part of
        // WM_TOUCH handling which is more correct).
        // If we don't do this, then when the user lifts their finger after a
        // long-press, the WM_RBUTTONDOWN compatibility event that Windows sends
        // us will dismiss the contextmenu popup that we displayed as part of
        // handling the long-tap-up.
        return false;
      }
      if (!EventIsInsideWindow(popupWindow, touchPoint) &&
          GetPopupsToRollup(rollupListener, &popupsToRollup, touchPoint)) {
        break;
      }
      return false;
    case WM_POINTERDOWN: {
      WinPointerEvents pointerEvents;
      if (!pointerEvents.ShouldRollupOnPointerEvent(nativeMessage, aWParam)) {
        return false;
      }
      POINT pt;
      pt.x = GET_X_LPARAM(aLParam);
      pt.y = GET_Y_LPARAM(aLParam);
      if (!GetPopupsToRollup(rollupListener, &popupsToRollup, Some(pt))) {
        return false;
      }
      if (EventIsInsideWindow(popupWindow, Some(pt))) {
        // Don't roll up if the event is inside the popup window.
        return false;
      }
    } break;
    case MOZ_WM_DMANIP: {
      POINT pt;
      ::GetCursorPos(&pt);
      if (!GetPopupsToRollup(rollupListener, &popupsToRollup, Some(pt))) {
        return false;
      }
      if (EventIsInsideWindow(popupWindow, Some(pt))) {
        // Don't roll up if the event is inside the popup window
        return false;
      }
    } break;
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
      // We need to check if the popup thinks that it should cause closing
      // itself when mouse wheel events are fired outside the rollup widget.
      if (!EventIsInsideWindow(popupWindow)) {
        // Check if we should consume this event even if we don't roll-up:
        consumeRollupEvent = rollupListener->ShouldConsumeOnMouseWheelEvent();
        *aResult = MA_ACTIVATE;
        if (rollupListener->ShouldRollupOnMouseWheelEvent() &&
            GetPopupsToRollup(rollupListener, &popupsToRollup)) {
          break;
        }
      }
      return consumeRollupEvent;

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
          // Consume this message here since previous window must not have
          // been inactivated since we've already stopped accepting the
          // inactivation below.
          return true;
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
        // because such case is caused by strange mouse drivers.  And in
        // such case, we should consume the message here since we need to
        // hide this odd focus move from our content.  (If we didn't consume
        // the message here, ProcessMessage() will notify widget listener of
        // inactivation and that causes unnecessary reflow for supporting
        // -moz-window-inactive pseudo class.
        if (activeWindow) {
          if (activeWindow->IsPopup()) {
            return true;
          }
          nsWindow* deactiveWindow = WinUtils::GetNSWindowPtr(aWnd);
          if (deactiveWindow && deactiveWindow->IsPopup()) {
            return true;
          }
        }
      } else if (LOWORD(aWParam) == WA_CLICKACTIVE) {
        // If the WM_ACTIVATE message is caused by a click in a popup,
        // we should not rollup any popups.
        nsWindow* window = WinUtils::GetNSWindowPtr(aWnd);
        if ((window && window->IsPopup()) ||
            !GetPopupsToRollup(rollupListener, &popupsToRollup)) {
          return false;
        }
      }
      break;

    case MOZ_WM_REACTIVATE:
      // The previous active window should take back focus.
      if (::IsWindow(reinterpret_cast<HWND>(aLParam))) {
        // FYI: Even without this API call, you see expected result (e.g., the
        //      owner window of the popup keeps active without flickering
        //      the non-client area).  And also this causes initializing
        //      TSF and it causes using CPU time a lot.  However, even if we
        //      consume WM_ACTIVE messages, native focus change has already
        //      been occurred.  I.e., a popup window is active now.  Therefore,
        //      you'll see some odd behavior if we don't reactivate the owner
        //      window here.  For example, if you do:
        //        1. Turn wheel on a bookmark panel.
        //        2. Turn wheel on another window.
        //      then, you'll see that the another window becomes active but the
        //      owner window of the bookmark panel looks still active and the
        //      bookmark panel keeps open.  The reason is that the first wheel
        //      operation gives focus to the bookmark panel.  Therefore, when
        //      the next operation gives focus to the another window, previous
        //      focus window is the bookmark panel (i.e., a popup window).
        //      So, in this case, our hack around here prevents to inactivate
        //      the owner window and roll up the bookmark panel.
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

    case WM_SHOWWINDOW:
      // If the window is being minimized, close popups.
      if (aLParam == SW_PARENTCLOSING) {
        break;
      }
      return false;

    case WM_KILLFOCUS:
      // If focus moves to other window created in different process/thread,
      // e.g., a plugin window, popups should be rolled up.
      if (IsDifferentThreadWindow(reinterpret_cast<HWND>(aWParam))) {
        break;
      }
      return false;

    case WM_MOVING:
    case WM_MENUSELECT:
      break;

    default:
      return false;
  }

  // Only need to deal with the last rollup for left mouse down events.
  NS_ASSERTION(!nsAutoRollup::GetLastRollup(), "last rollup is null");

  if (nativeMessage == WM_TOUCH || nativeMessage == WM_LBUTTONDOWN ||
      nativeMessage == WM_POINTERDOWN) {
    LayoutDeviceIntPoint pos;
    if (nativeMessage == WM_TOUCH) {
      pos.x = touchPoint->x;
      pos.y = touchPoint->y;
    } else {
      POINT pt;
      pt.x = GET_X_LPARAM(aLParam);
      pt.y = GET_Y_LPARAM(aLParam);
      // POINTERDOWN is already in screen coords.
      if (nativeMessage == WM_LBUTTONDOWN) {
        ::ClientToScreen(aWnd, &pt);
      }
      pos = LayoutDeviceIntPoint(pt.x, pt.y);
    }

    nsIContent* lastRollup;
    consumeRollupEvent =
        rollupListener->Rollup(popupsToRollup, true, &pos, &lastRollup);
    nsAutoRollup::SetLastRollup(lastRollup);
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
nsWindow* nsWindow::GetTopLevelWindow(bool aStopOnDialogOrPopup) {
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

    if (!parentWindow) return curWindow;

    curWindow = parentWindow;
  }
}

// Set a flag if hwnd is a (non-popup) visible window from this process,
// and bail out of the enumeration. Otherwise leave the flag unmodified
// and continue the enumeration.
// lParam must be a bool* pointing at the flag to be set.
static BOOL CALLBACK EnumVisibleWindowsProc(HWND hwnd, LPARAM lParam) {
  DWORD pid;
  ::GetWindowThreadProcessId(hwnd, &pid);
  if (pid == ::GetCurrentProcessId() && ::IsWindowVisible(hwnd)) {
    // Don't count popups as visible windows, since they don't take focus,
    // in case we only have a popup visible (see bug 1554490 where the gfx
    // test window is an offscreen popup).
    nsWindow* window = WinUtils::GetNSWindowPtr(hwnd);
    if (!window || !window->IsPopup()) {
      bool* windowsVisible = reinterpret_cast<bool*>(lParam);
      *windowsVisible = true;
      return FALSE;
    }
  }
  return TRUE;
}

// Determine if it would be ok to activate a window, taking focus.
// We want to avoid stealing focus from another app (bug 225305).
bool nsWindow::CanTakeFocus() {
  HWND fgWnd = ::GetForegroundWindow();
  if (!fgWnd) {
    // There is no foreground window, so don't worry about stealing focus.
    return true;
  }
  // We can take focus if the current foreground window is already from
  // this process.
  DWORD pid;
  ::GetWindowThreadProcessId(fgWnd, &pid);
  if (pid == ::GetCurrentProcessId()) {
    return true;
  }

  bool windowsVisible = false;
  ::EnumWindows(EnumVisibleWindowsProc,
                reinterpret_cast<LPARAM>(&windowsVisible));

  if (!windowsVisible) {
    // We're probably creating our first visible window, allow that to
    // take focus.
    return true;
  }
  return false;
}

/* static */ const wchar_t* nsWindow::GetMainWindowClass() {
  static const wchar_t* sMainWindowClass = nullptr;
  if (!sMainWindowClass) {
    nsAutoString className;
    Preferences::GetString("ui.window_class_override", className);
    if (!className.IsEmpty()) {
      sMainWindowClass = wcsdup(className.get());
    } else {
      sMainWindowClass = kClassNameGeneral;
    }
  }
  return sMainWindowClass;
}

LPARAM nsWindow::lParamToScreen(LPARAM lParam) {
  POINT pt;
  pt.x = GET_X_LPARAM(lParam);
  pt.y = GET_Y_LPARAM(lParam);
  ::ClientToScreen(mWnd, &pt);
  return MAKELPARAM(pt.x, pt.y);
}

LPARAM nsWindow::lParamToClient(LPARAM lParam) {
  POINT pt;
  pt.x = GET_X_LPARAM(lParam);
  pt.y = GET_Y_LPARAM(lParam);
  ::ScreenToClient(mWnd, &pt);
  return MAKELPARAM(pt.x, pt.y);
}

WPARAM nsWindow::wParamFromGlobalMouseState() {
  WPARAM result = 0;

  if (!!::GetKeyState(VK_CONTROL)) {
    result |= MK_CONTROL;
  }

  if (!!::GetKeyState(VK_SHIFT)) {
    result |= MK_SHIFT;
  }

  if (!!::GetKeyState(VK_LBUTTON)) {
    result |= MK_LBUTTON;
  }

  if (!!::GetKeyState(VK_MBUTTON)) {
    result |= MK_MBUTTON;
  }

  if (!!::GetKeyState(VK_RBUTTON)) {
    result |= MK_RBUTTON;
  }

  if (!!::GetKeyState(VK_XBUTTON1)) {
    result |= MK_XBUTTON1;
  }

  if (!!::GetKeyState(VK_XBUTTON2)) {
    result |= MK_XBUTTON2;
  }

  return result;
}

void nsWindow::PickerOpen() { mPickerDisplayCount++; }

void nsWindow::PickerClosed() {
  NS_ASSERTION(mPickerDisplayCount > 0, "mPickerDisplayCount out of sync!");
  if (!mPickerDisplayCount) return;
  mPickerDisplayCount--;
  if (!mPickerDisplayCount && mDestroyCalled) {
    Destroy();
  }
}

bool nsWindow::WidgetTypeSupportsAcceleration() {
  // We don't currently support using an accelerated layer manager with
  // transparent windows so don't even try. I'm also not sure if we even
  // want to support this case. See bug 593471.
  //
  // Windows' support for transparent accelerated surfaces isn't great.
  // Some possible approaches:
  //  - Readback the data and update it using
  //  UpdateLayeredWindow/UpdateLayeredWindowIndirect
  //    This is what WPF does. See
  //    CD3DDeviceLevel1::PresentWithGDI/CD3DSwapChainWithSwDC in WpfGfx. The
  //    rationale for not using IDirect3DSurface9::GetDC is explained here:
  //    https://web.archive.org/web/20160521191104/https://blogs.msdn.microsoft.com/dwayneneed/2008/09/08/transparent-windows-in-wpf/
  //  - Use D3D11_RESOURCE_MISC_GDI_COMPATIBLE, IDXGISurface1::GetDC(),
  //    and UpdateLayeredWindowIndirect.
  //    This is suggested here:
  //    https://docs.microsoft.com/en-us/archive/msdn-magazine/2009/december/windows-with-c-layered-windows-with-direct2d
  //    but might have the same problem that IDirect3DSurface9::GetDC has.
  //  - Creating the window with the WS_EX_NOREDIRECTIONBITMAP flag and use
  //  DirectComposition.
  //    Not supported on Win7.
  //  - Using DwmExtendFrameIntoClientArea with negative margins and something
  //  to turn off the glass effect.
  //    This doesn't work when the DWM is not running (Win7)
  //
  // Also see bug 1150376, D3D11 composition can cause issues on some devices
  // on Windows 7 where presentation fails randomly for windows with drop
  // shadows.
  return mTransparencyMode != eTransparencyTransparent &&
         !(IsPopup() && DeviceManagerDx::Get()->IsWARP());
}

bool nsWindow::DispatchTouchEventFromWMPointer(
    UINT msg, LPARAM aLParam, const WinPointerInfo& aPointerInfo,
    mozilla::MouseButton aButton) {
  MultiTouchInput::MultiTouchType touchType;
  switch (msg) {
    case WM_POINTERDOWN:
      touchType = MultiTouchInput::MULTITOUCH_START;
      break;
    case WM_POINTERUPDATE:
      if (aPointerInfo.mPressure == 0) {
        return false;  // hover
      }
      touchType = MultiTouchInput::MULTITOUCH_MOVE;
      break;
    case WM_POINTERUP:
      touchType = MultiTouchInput::MULTITOUCH_END;
      break;
    default:
      return false;
  }

  nsPointWin touchPoint;
  touchPoint.x = GET_X_LPARAM(aLParam);
  touchPoint.y = GET_Y_LPARAM(aLParam);
  touchPoint.ScreenToClient(mWnd);

  SingleTouchData touchData(static_cast<int32_t>(aPointerInfo.pointerId),
                            ScreenIntPoint::FromUnknownPoint(touchPoint),
                            ScreenSize(1, 1),  // pixel size radius for pen
                            0.0f,              // no radius rotation
                            aPointerInfo.mPressure);
  touchData.mTiltX = aPointerInfo.tiltX;
  touchData.mTiltY = aPointerInfo.tiltY;
  touchData.mTwist = aPointerInfo.twist;

  MultiTouchInput touchInput;
  touchInput.mType = touchType;
  touchInput.mTime = ::GetMessageTime();
  touchInput.mTimeStamp =
      GetMessageTimeStamp(static_cast<long>(touchInput.mTime));
  touchInput.mTouches.AppendElement(touchData);
  touchInput.mButton = aButton;
  touchInput.mButtons = aPointerInfo.mButtons;

  // POINTER_INFO.dwKeyStates can't be used as it only supports Shift and Ctrl
  ModifierKeyState modifierKeyState;
  touchInput.modifiers = modifierKeyState.GetModifiers();

  DispatchTouchInput(touchInput, MouseEvent_Binding::MOZ_SOURCE_PEN);
  return true;
}

static MouseButton PenFlagsToMouseButton(PEN_FLAGS aPenFlags) {
  // Theoretically flags can be set together but they do not
  if (aPenFlags & PEN_FLAG_BARREL) {
    return MouseButton::eSecondary;
  }
  if (aPenFlags & PEN_FLAG_ERASER) {
    return MouseButton::eEraser;
  }
  return MouseButton::ePrimary;
}

bool nsWindow::OnPointerEvents(UINT msg, WPARAM aWParam, LPARAM aLParam) {
  if (!mAPZC) {
    // APZ is not available on context menu. Follow the behavior of touch input
    // which fallbacks to WM_LBUTTON* and WM_GESTURE, to keep consistency.
    return false;
  }
  if (!mPointerEvents.ShouldHandleWinPointerMessages(msg, aWParam)) {
    return false;
  }
  if (!mPointerEvents.ShouldFirePointerEventByWinPointerMessages()) {
    // We have to handle WM_POINTER* to fetch and cache pen related information
    // and fire WidgetMouseEvent with the cached information the WM_*BUTTONDOWN
    // handler. This is because Windows doesn't support ::DoDragDrop in the
    // touch or pen message handlers.
    mPointerEvents.ConvertAndCachePointerInfo(msg, aWParam);
    // Don't consume the Windows WM_POINTER* messages
    return false;
  }

  uint32_t pointerId = mPointerEvents.GetPointerId(aWParam);
  POINTER_PEN_INFO penInfo{};
  if (!mPointerEvents.GetPointerPenInfo(pointerId, &penInfo)) {
    return false;
  }

  // When dispatching mouse events with pen, there may be some
  // WM_POINTERUPDATE messages between WM_POINTERDOWN and WM_POINTERUP with
  // small movements. Those events will reset sLastMousePoint and reset
  // sLastClickCount. To prevent that, we keep the last pen down position
  // and compare it with the subsequent WM_POINTERUPDATE. If the movement is
  // smaller than GetSystemMetrics(SM_CXDRAG), then we suppress firing
  // eMouseMove for WM_POINTERUPDATE.
  static POINT sLastPointerDownPoint = {0};

  // We don't support chorded buttons for pen. Keep the button at
  // WM_POINTERDOWN.
  static mozilla::MouseButton sLastPenDownButton = MouseButton::ePrimary;
  static bool sPointerDown = false;

  EventMessage message;
  mozilla::MouseButton button = MouseButton::ePrimary;
  switch (msg) {
    case WM_POINTERDOWN: {
      LayoutDeviceIntPoint eventPoint(GET_X_LPARAM(aLParam),
                                      GET_Y_LPARAM(aLParam));
      sLastPointerDownPoint.x = eventPoint.x;
      sLastPointerDownPoint.y = eventPoint.y;
      message = eMouseDown;
      button = PenFlagsToMouseButton(penInfo.penFlags);
      sLastPenDownButton = button;
      sPointerDown = true;
    } break;
    case WM_POINTERUP:
      message = eMouseUp;
      MOZ_ASSERT(sPointerDown, "receive WM_POINTERUP w/o WM_POINTERDOWN");
      button = sPointerDown ? sLastPenDownButton : MouseButton::ePrimary;
      sPointerDown = false;
      break;
    case WM_POINTERUPDATE:
      message = eMouseMove;
      if (sPointerDown) {
        LayoutDeviceIntPoint eventPoint(GET_X_LPARAM(aLParam),
                                        GET_Y_LPARAM(aLParam));
        int32_t movementX = sLastPointerDownPoint.x > eventPoint.x
                                ? sLastPointerDownPoint.x - eventPoint.x
                                : eventPoint.x - sLastPointerDownPoint.x;
        int32_t movementY = sLastPointerDownPoint.y > eventPoint.y
                                ? sLastPointerDownPoint.y - eventPoint.y
                                : eventPoint.y - sLastPointerDownPoint.y;
        bool insideMovementThreshold =
            movementX < (int32_t)::GetSystemMetrics(SM_CXDRAG) &&
            movementY < (int32_t)::GetSystemMetrics(SM_CYDRAG);

        if (insideMovementThreshold) {
          // Suppress firing eMouseMove for WM_POINTERUPDATE if the movement
          // from last WM_POINTERDOWN is smaller than SM_CXDRAG / SM_CYDRAG
          return false;
        }
        button = sLastPenDownButton;
      }
      break;
    case WM_POINTERLEAVE:
      message = eMouseExitFromWidget;
      break;
    default:
      return false;
  }

  // Windows defines the pen pressure is normalized to a range between 0 and
  // 1024. Convert it to float.
  float pressure = penInfo.pressure ? (float)penInfo.pressure / 1024 : 0;
  int16_t buttons = sPointerDown
                        ? nsContentUtils::GetButtonsFlagForButton(button)
                        : MouseButtonsFlag::eNoButtons;
  WinPointerInfo pointerInfo(pointerId, penInfo.tiltX, penInfo.tiltY, pressure,
                             buttons);
  pointerInfo.twist = penInfo.rotation;

  // Fire touch events but not when the barrel button is pressed.
  if (button != MouseButton::eSecondary &&
      StaticPrefs::dom_w3c_pointer_events_scroll_by_pen_enabled() &&
      DispatchTouchEventFromWMPointer(msg, aLParam, pointerInfo, button)) {
    return true;
  }

  // The aLParam of WM_POINTER* is the screen location. Convert it to client
  // location
  LPARAM newLParam = lParamToClient(aLParam);
  DispatchMouseEvent(message, aWParam, newLParam, false, button,
                     MouseEvent_Binding::MOZ_SOURCE_PEN, &pointerInfo);

  if (button == MouseButton::eSecondary && message == eMouseUp) {
    // Fire eContextMenu manually since consuming WM_POINTER* blocks
    // WM_CONTEXTMENU
    DispatchMouseEvent(eContextMenu, aWParam, newLParam, false, button,
                       MouseEvent_Binding::MOZ_SOURCE_PEN, &pointerInfo);
  }
  // Consume WM_POINTER* to stop Windows fires WM_*BUTTONDOWN / WM_*BUTTONUP
  // WM_MOUSEMOVE.
  return true;
}

void nsWindow::GetCompositorWidgetInitData(
    mozilla::widget::CompositorWidgetInitData* aInitData) {
  *aInitData = WinCompositorWidgetInitData(
      reinterpret_cast<uintptr_t>(mWnd),
      reinterpret_cast<uintptr_t>(static_cast<nsIWidget*>(this)),
      mTransparencyMode, mFrameState->GetSizeMode());
}

bool nsWindow::SynchronouslyRepaintOnResize() {
  return !gfxWindowsPlatform::GetPlatform()->DwmCompositionEnabled();
}

void nsWindow::MaybeDispatchInitialFocusEvent() {
  if (mIsShowingPreXULSkeletonUI && ::GetActiveWindow() == mWnd) {
    DispatchFocusToTopLevelWindow(true);
  }
}

already_AddRefed<nsIWidget> nsIWidget::CreateTopLevelWindow() {
  nsCOMPtr<nsIWidget> window = new nsWindow();
  return window.forget();
}

already_AddRefed<nsIWidget> nsIWidget::CreateChildWindow() {
  nsCOMPtr<nsIWidget> window = new nsWindow(true);
  return window.forget();
}

// static
bool nsWindow::InitTouchInjection() {
  if (!sTouchInjectInitialized) {
    // Initialize touch injection on the first call
    HMODULE hMod = LoadLibraryW(kUser32LibName);
    if (!hMod) {
      return false;
    }

    InitializeTouchInjectionPtr func =
        (InitializeTouchInjectionPtr)GetProcAddress(hMod,
                                                    "InitializeTouchInjection");
    if (!func) {
      WinUtils::Log("InitializeTouchInjection not available.");
      return false;
    }

    if (!func(TOUCH_INJECT_MAX_POINTS, TOUCH_FEEDBACK_DEFAULT)) {
      WinUtils::Log("InitializeTouchInjection failure. GetLastError=%d",
                    GetLastError());
      return false;
    }

    sInjectTouchFuncPtr =
        (InjectTouchInputPtr)GetProcAddress(hMod, "InjectTouchInput");
    if (!sInjectTouchFuncPtr) {
      WinUtils::Log("InjectTouchInput not available.");
      return false;
    }
    sTouchInjectInitialized = true;
  }
  return true;
}

bool nsWindow::InjectTouchPoint(uint32_t aId, LayoutDeviceIntPoint& aPoint,
                                POINTER_FLAGS aFlags, uint32_t aPressure,
                                uint32_t aOrientation) {
  if (aId > TOUCH_INJECT_MAX_POINTS) {
    WinUtils::Log("Pointer ID exceeds maximum. See TOUCH_INJECT_MAX_POINTS.");
    return false;
  }

  POINTER_TOUCH_INFO info{};

  info.touchFlags = TOUCH_FLAG_NONE;
  info.touchMask =
      TOUCH_MASK_CONTACTAREA | TOUCH_MASK_ORIENTATION | TOUCH_MASK_PRESSURE;
  info.pressure = aPressure;
  info.orientation = aOrientation;

  info.pointerInfo.pointerFlags = aFlags;
  info.pointerInfo.pointerType = PT_TOUCH;
  info.pointerInfo.pointerId = aId;
  info.pointerInfo.ptPixelLocation.x = aPoint.x;
  info.pointerInfo.ptPixelLocation.y = aPoint.y;

  info.rcContact.top = info.pointerInfo.ptPixelLocation.y - 2;
  info.rcContact.bottom = info.pointerInfo.ptPixelLocation.y + 2;
  info.rcContact.left = info.pointerInfo.ptPixelLocation.x - 2;
  info.rcContact.right = info.pointerInfo.ptPixelLocation.x + 2;

  for (int i = 0; i < 3; i++) {
    if (sInjectTouchFuncPtr(1, &info)) {
      break;
    }
    DWORD error = GetLastError();
    if (error == ERROR_NOT_READY && i < 2) {
      // We sent it too quickly after the previous injection (see bug 1535140
      // comment 10). On the first loop iteration we just yield (via Sleep(0))
      // and try again. If it happens again on the second loop iteration we
      // explicitly Sleep(1) and try again. If that doesn't work either we just
      // error out.
      ::Sleep(i);
      continue;
    }
    WinUtils::Log("InjectTouchInput failure. GetLastError=%d", error);
    return false;
  }
  return true;
}

void nsWindow::ChangedDPI() {
  if (mWidgetListener) {
    if (PresShell* presShell = mWidgetListener->GetPresShell()) {
      presShell->BackingScaleFactorChanged();
    }
  }
}

static Result<POINTER_FLAGS, nsresult> PointerStateToFlag(
    nsWindow::TouchPointerState aPointerState, bool isUpdate) {
  bool hover = aPointerState & nsWindow::TOUCH_HOVER;
  bool contact = aPointerState & nsWindow::TOUCH_CONTACT;
  bool remove = aPointerState & nsWindow::TOUCH_REMOVE;
  bool cancel = aPointerState & nsWindow::TOUCH_CANCEL;

  POINTER_FLAGS flags;
  if (isUpdate) {
    // We know about this pointer, send an update
    flags = POINTER_FLAG_UPDATE;
    if (hover) {
      flags |= POINTER_FLAG_INRANGE;
    } else if (contact) {
      flags |= POINTER_FLAG_INCONTACT | POINTER_FLAG_INRANGE;
    } else if (remove) {
      flags = POINTER_FLAG_UP;
    }

    if (cancel) {
      flags |= POINTER_FLAG_CANCELED;
    }
  } else {
    // Missing init state, error out
    if (remove || cancel) {
      return Err(NS_ERROR_INVALID_ARG);
    }

    // Create a new pointer
    flags = POINTER_FLAG_INRANGE;
    if (contact) {
      flags |= POINTER_FLAG_INCONTACT | POINTER_FLAG_DOWN;
    }
  }
  return flags;
}

nsresult nsWindow::SynthesizeNativeTouchPoint(
    uint32_t aPointerId, nsIWidget::TouchPointerState aPointerState,
    LayoutDeviceIntPoint aPoint, double aPointerPressure,
    uint32_t aPointerOrientation, nsIObserver* aObserver) {
  AutoObserverNotifier notifier(aObserver, "touchpoint");

  if (StaticPrefs::apz_test_fails_with_native_injection() ||
      !InitTouchInjection()) {
    // If we don't have touch injection from the OS, or if we are running a test
    // that cannot properly inject events to satisfy the OS requirements (see
    // bug 1313170)  we can just fake it and synthesize the events from here.
    MOZ_ASSERT(NS_IsMainThread());
    if (aPointerState == TOUCH_HOVER) {
      return NS_ERROR_UNEXPECTED;
    }

    if (!mSynthesizedTouchInput) {
      mSynthesizedTouchInput = MakeUnique<MultiTouchInput>();
    }

    WidgetEventTime time = CurrentMessageWidgetEventTime();
    LayoutDeviceIntPoint pointInWindow = aPoint - WidgetToScreenOffset();
    MultiTouchInput inputToDispatch = UpdateSynthesizedTouchState(
        mSynthesizedTouchInput.get(), time.mTime, time.mTimeStamp, aPointerId,
        aPointerState, pointInWindow, aPointerPressure, aPointerOrientation);
    DispatchTouchInput(inputToDispatch);
    return NS_OK;
  }

  // win api expects a value from 0 to 1024. aPointerPressure is a value
  // from 0.0 to 1.0.
  uint32_t pressure = (uint32_t)ceil(aPointerPressure * 1024);

  // If we already know about this pointer id get it's record
  return mActivePointers.WithEntryHandle(aPointerId, [&](auto&& entry) {
    POINTER_FLAGS flags;
    // Can't use MOZ_TRY_VAR because it confuses WithEntryHandle
    auto result = PointerStateToFlag(aPointerState, !!entry);
    if (result.isOk()) {
      flags = result.unwrap();
    } else {
      return result.unwrapErr();
    }

    if (!entry) {
      entry.Insert(MakeUnique<PointerInfo>(aPointerId, aPoint,
                                           PointerInfo::PointerType::TOUCH));
    } else {
      if (entry.Data()->mType != PointerInfo::PointerType::TOUCH) {
        return NS_ERROR_UNEXPECTED;
      }
      if (aPointerState & TOUCH_REMOVE) {
        // Remove the pointer from our tracking list. This is UniquePtr wrapped,
        // so shouldn't leak.
        entry.Remove();
      }
    }

    return !InjectTouchPoint(aPointerId, aPoint, flags, pressure,
                             aPointerOrientation)
               ? NS_ERROR_UNEXPECTED
               : NS_OK;
  });
}

nsresult nsWindow::ClearNativeTouchSequence(nsIObserver* aObserver) {
  AutoObserverNotifier notifier(aObserver, "cleartouch");
  if (!sTouchInjectInitialized) {
    return NS_OK;
  }

  // cancel all input points
  for (auto iter = mActivePointers.Iter(); !iter.Done(); iter.Next()) {
    auto* info = iter.UserData();
    if (info->mType != PointerInfo::PointerType::TOUCH) {
      continue;
    }
    InjectTouchPoint(info->mPointerId, info->mPosition, POINTER_FLAG_CANCELED);
    iter.Remove();
  }

  nsBaseWidget::ClearNativeTouchSequence(nullptr);

  return NS_OK;
}

#if !defined(NTDDI_WIN10_RS5) || (NTDDI_VERSION < NTDDI_WIN10_RS5)
static CreateSyntheticPointerDevicePtr CreateSyntheticPointerDevice;
static DestroySyntheticPointerDevicePtr DestroySyntheticPointerDevice;
static InjectSyntheticPointerInputPtr InjectSyntheticPointerInput;
#endif
static HSYNTHETICPOINTERDEVICE sSyntheticPenDevice;

static bool InitPenInjection() {
  if (sSyntheticPenDevice) {
    return true;
  }
#if !defined(NTDDI_WIN10_RS5) || (NTDDI_VERSION < NTDDI_WIN10_RS5)
  HMODULE hMod = LoadLibraryW(kUser32LibName);
  if (!hMod) {
    return false;
  }
  CreateSyntheticPointerDevice =
      (CreateSyntheticPointerDevicePtr)GetProcAddress(
          hMod, "CreateSyntheticPointerDevice");
  if (!CreateSyntheticPointerDevice) {
    WinUtils::Log("CreateSyntheticPointerDevice not available.");
    return false;
  }
  DestroySyntheticPointerDevice =
      (DestroySyntheticPointerDevicePtr)GetProcAddress(
          hMod, "DestroySyntheticPointerDevice");
  if (!DestroySyntheticPointerDevice) {
    WinUtils::Log("DestroySyntheticPointerDevice not available.");
    return false;
  }
  InjectSyntheticPointerInput = (InjectSyntheticPointerInputPtr)GetProcAddress(
      hMod, "InjectSyntheticPointerInput");
  if (!InjectSyntheticPointerInput) {
    WinUtils::Log("InjectSyntheticPointerInput not available.");
    return false;
  }
#endif
  sSyntheticPenDevice =
      CreateSyntheticPointerDevice(PT_PEN, 1, POINTER_FEEDBACK_DEFAULT);
  return !!sSyntheticPenDevice;
}

nsresult nsWindow::SynthesizeNativePenInput(
    uint32_t aPointerId, nsIWidget::TouchPointerState aPointerState,
    LayoutDeviceIntPoint aPoint, double aPressure, uint32_t aRotation,
    int32_t aTiltX, int32_t aTiltY, int32_t aButton, nsIObserver* aObserver) {
  AutoObserverNotifier notifier(aObserver, "peninput");
  if (!InitPenInjection()) {
    return NS_ERROR_UNEXPECTED;
  }

  // win api expects a value from 0 to 1024. aPointerPressure is a value
  // from 0.0 to 1.0.
  uint32_t pressure = (uint32_t)ceil(aPressure * 1024);

  // If we already know about this pointer id get it's record
  return mActivePointers.WithEntryHandle(aPointerId, [&](auto&& entry) {
    POINTER_FLAGS flags;
    // Can't use MOZ_TRY_VAR because it confuses WithEntryHandle
    auto result = PointerStateToFlag(aPointerState, !!entry);
    if (result.isOk()) {
      flags = result.unwrap();
    } else {
      return result.unwrapErr();
    }

    if (!entry) {
      entry.Insert(MakeUnique<PointerInfo>(aPointerId, aPoint,
                                           PointerInfo::PointerType::PEN));
    } else {
      if (entry.Data()->mType != PointerInfo::PointerType::PEN) {
        return NS_ERROR_UNEXPECTED;
      }
      if (aPointerState & TOUCH_REMOVE) {
        // Remove the pointer from our tracking list. This is UniquePtr wrapped,
        // so shouldn't leak.
        entry.Remove();
      }
    }

    POINTER_TYPE_INFO info{};

    info.type = PT_PEN;
    info.penInfo.pointerInfo.pointerType = PT_PEN;
    info.penInfo.pointerInfo.pointerFlags = flags;
    info.penInfo.pointerInfo.pointerId = aPointerId;
    info.penInfo.pointerInfo.ptPixelLocation.x = aPoint.x;
    info.penInfo.pointerInfo.ptPixelLocation.y = aPoint.y;

    info.penInfo.penFlags = PEN_FLAG_NONE;
    // PEN_FLAG_ERASER is not supported this way, unfortunately.
    if (aButton == 2) {
      info.penInfo.penFlags |= PEN_FLAG_BARREL;
    }
    info.penInfo.penMask = PEN_MASK_PRESSURE | PEN_MASK_ROTATION |
                           PEN_MASK_TILT_X | PEN_MASK_TILT_Y;
    info.penInfo.pressure = pressure;
    info.penInfo.rotation = aRotation;
    info.penInfo.tiltX = aTiltX;
    info.penInfo.tiltY = aTiltY;

    return InjectSyntheticPointerInput(sSyntheticPenDevice, &info, 1)
               ? NS_OK
               : NS_ERROR_UNEXPECTED;
  });
};

bool nsWindow::HandleAppCommandMsg(const MSG& aAppCommandMsg,
                                   LRESULT* aRetValue) {
  ModifierKeyState modKeyState;
  NativeKey nativeKey(this, aAppCommandMsg, modKeyState);
  bool consumed = nativeKey.HandleAppCommandMessage();
  *aRetValue = consumed ? 1 : 0;
  return consumed;
}

static nsSizeMode GetSizeModeForWindowFrame(HWND aWnd, bool aFullscreenMode) {
  WINDOWPLACEMENT pl;
  pl.length = sizeof(pl);
  ::GetWindowPlacement(aWnd, &pl);

  if (pl.showCmd == SW_SHOWMINIMIZED) {
    return nsSizeMode_Minimized;
  } else if (aFullscreenMode) {
    return nsSizeMode_Fullscreen;
  } else if (pl.showCmd == SW_SHOWMAXIMIZED) {
    return nsSizeMode_Maximized;
  } else {
    return nsSizeMode_Normal;
  }
}

static void ShowWindowWithMode(HWND aWnd, nsSizeMode aMode) {
  // This will likely cause a callback to
  // nsWindow::FrameState::{OnFrameChanging() and OnFrameChanged()}
  switch (aMode) {
    case nsSizeMode_Fullscreen:
      ::ShowWindow(aWnd, SW_SHOW);
      break;

    case nsSizeMode_Maximized:
      ::ShowWindow(aWnd, SW_MAXIMIZE);
      break;

    case nsSizeMode_Minimized:
      ::ShowWindow(aWnd, SW_MINIMIZE);
      break;

    default:
      // Don't call ::ShowWindow if we're trying to "restore" a window that is
      // already in a normal state.  Prevents a bug where snapping to one side
      // of the screen and then minimizing would cause Windows to forget our
      // window's correct restored position/size.
      if (GetCurrentShowCmd(aWnd) != SW_SHOWNORMAL) {
        ::ShowWindow(aWnd, SW_RESTORE);
      }
  }
}

nsWindow::FrameState::FrameState(nsWindow* aWindow) : mWindow(aWindow) {}

nsSizeMode nsWindow::FrameState::GetSizeMode() const { return mSizeMode; }

void nsWindow::FrameState::CheckInvariant() const {
  MOZ_ASSERT(mSizeMode >= 0 && mSizeMode < nsSizeMode_Invalid);
  MOZ_ASSERT(mLastSizeMode >= 0 && mLastSizeMode < nsSizeMode_Invalid);
  MOZ_ASSERT(mOldSizeMode >= 0 && mOldSizeMode < nsSizeMode_Invalid);
  MOZ_ASSERT(mWindow);

  // We should never observe fullscreen sizemode unless fullscreen is enabled
  MOZ_ASSERT_IF(mSizeMode == nsSizeMode_Fullscreen, mFullscreenMode);
  MOZ_ASSERT_IF(!mFullscreenMode, mSizeMode != nsSizeMode_Fullscreen);

  // Something went wrong if we somehow saved fullscreen mode when we are
  // changing into fullscreen mode
  MOZ_ASSERT(mOldSizeMode != nsSizeMode_Fullscreen);
}

void nsWindow::FrameState::ConsumePreXULSkeletonState(bool aWasMaximized) {
  mSizeMode = aWasMaximized ? nsSizeMode_Maximized : nsSizeMode_Normal;
}

void nsWindow::FrameState::EnsureSizeMode(nsSizeMode aMode) {
  if (mSizeMode == aMode) {
    return;
  }

  if (aMode == nsSizeMode_Fullscreen) {
    EnsureFullscreenMode(true);
  } else if ((mSizeMode == nsSizeMode_Fullscreen) &&
             (aMode == nsSizeMode_Normal)) {
    // If we are in fullscreen mode, minimize should work like normal and
    // return us to fullscreen mode when unminimized. Maximize isn't really
    // available and won't do anything. "Restore" should do the same thing as
    // requesting to end fullscreen.
    EnsureFullscreenMode(false);
  } else {
    SetSizeModeInternal(aMode);
  }
}

void nsWindow::FrameState::EnsureFullscreenMode(bool aFullScreen) {
  if (mFullscreenMode == aFullScreen) {
    return;
  }

  mWindow->OnFullscreenWillChange(aFullScreen);

  mFullscreenMode = aFullScreen;
  if (aFullScreen) {
    mOldSizeMode = mSizeMode;
    SetSizeModeInternal(nsSizeMode_Fullscreen);
  } else {
    SetSizeModeInternal(mOldSizeMode);
  }

  mWindow->OnFullscreenChanged(aFullScreen);
}

void nsWindow::FrameState::OnFrameChanging() {
  if (mSizeMode == nsSizeMode_Fullscreen) {
    return;
  }

  WINDOWPLACEMENT pl;
  pl.length = sizeof(pl);
  ::GetWindowPlacement(mWindow->mWnd, &pl);

  const nsSizeMode newSizeMode =
      GetSizeModeForWindowFrame(mWindow->mWnd, mFullscreenMode);

  mWindow->OnSizeModeChange(newSizeMode);

  mWindow->UpdateNonClientMargins(newSizeMode, false);
}

void nsWindow::FrameState::OnFrameChanged() {
  if (mSizeMode == nsSizeMode_Fullscreen) {
    return;
  }

  const nsSizeMode previousSizeMode = mSizeMode;

  // Windows has just changed the size mode of this window. The call to
  // SizeModeChanged will trigger a call into SetSizeMode where we will
  // set the min/max window state again or for nsSizeMode_Normal, call
  // SetWindow with a parameter of SW_RESTORE. There's no need however as
  // this window's mode has already changed. Updating SizeMode here
  // insures the SetSizeMode call is a no-op. Addresses a bug on Win7 related
  // to window docking. (bug 489258)
  mSizeMode = GetSizeModeForWindowFrame(mWindow->mWnd, mFullscreenMode);

  MaybeLogSizeMode(mSizeMode);

  if (mSizeMode != previousSizeMode) {
    mWindow->OnSizeModeChange(mSizeMode);
  }

  // If window was restored, window activation was bypassed during the
  // SetSizeMode call originating from OnWindowPosChanging to avoid saving
  // pre-restore attributes. Force activation now to get correct attributes.
  if (mLastSizeMode != mSizeMode) {
    if (mSizeMode == nsSizeMode_Normal) {
      mWindow->DispatchFocusToTopLevelWindow(true);
    }
    mLastSizeMode = mSizeMode;
  }
}

void nsWindow::FrameState::SetSizeModeInternal(nsSizeMode aMode) {
  if (mSizeMode == aMode) {
    return;
  }

  mLastSizeMode = mSizeMode;
  mSizeMode = aMode;

  if (mWindow->mIsVisible) {
    ShowWindowWithMode(mWindow->mWnd, aMode);
  }

  // we activate here to ensure that the right child window is focused
  if (mWindow->mIsVisible &&
      (aMode == nsSizeMode_Maximized || aMode == nsSizeMode_Fullscreen)) {
    mWindow->DispatchFocusToTopLevelWindow(true);
  }
}
