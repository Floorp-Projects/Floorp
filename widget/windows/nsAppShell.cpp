/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ipc/MessageChannel.h"
#include "mozilla/ipc/WindowsMessageLoop.h"
#include "nsAppShell.h"
#include "nsToolkit.h"
#include "nsThreadUtils.h"
#include "WinUtils.h"
#include "WinTaskbar.h"
#include "WinMouseScrollHandler.h"
#include "nsWindowDefs.h"
#include "nsString.h"
#include "WinIMEHandler.h"
#include "mozilla/widget/AudioSession.h"
#include "mozilla/HangMonitor.h"
#include "nsIDOMWakeLockListener.h"
#include "nsIPowerManagerService.h"
#include "mozilla/StaticPtr.h"
#include "nsTHashtable.h"
#include "nsHashKeys.h"
#include "GeckoProfiler.h"
#include "nsComponentManagerUtils.h"
#include "nsITimer.h"
#include "ScreenHelperWin.h"
#include "mozilla/widget/ScreenManager.h"

// These are two messages that the code in winspool.drv on Windows 7 explicitly
// waits for while it is pumping other Windows messages, during display of the
// Printer Properties dialog.
#define MOZ_WM_PRINTER_PROPERTIES_COMPLETION 0x5b7a
#define MOZ_WM_PRINTER_PROPERTIES_FAILURE 0x5b7f

using namespace mozilla;
using namespace mozilla::widget;

#define WAKE_LOCK_LOG(...) MOZ_LOG(gWinWakeLockLog, mozilla::LogLevel::Debug, (__VA_ARGS__))
static mozilla::LazyLogModule gWinWakeLockLog("WinWakeLock");

// A wake lock listener that disables screen saver when requested by
// Gecko. For example when we're playing video in a foreground tab we
// don't want the screen saver to turn on.
class WinWakeLockListener final : public nsIDOMMozWakeLockListener
                                , public nsITimerCallback {
public:
  NS_DECL_ISUPPORTS;

  NS_IMETHOD Notify(nsITimer *timer) override {
    WAKE_LOCK_LOG("WinWakeLock: periodic timer fired");
    ResetScreenSaverTimeout();
    return NS_OK;
  }
private:
  ~WinWakeLockListener() {}

  NS_IMETHOD Callback(const nsAString& aTopic, const nsAString& aState) {
    if (!aTopic.EqualsASCII("screen")) {
      return NS_OK;
    }
    // Note the wake lock code ensures that we're not sent duplicate
    // "locked-foreground" notifications when multipe wake locks are held.
    if (aState.EqualsASCII("locked-foreground")) {
      WAKE_LOCK_LOG("WinWakeLock: Blocking screen saver");
      // We block the screen saver by periodically resetting the screen
      // saver timeout.
      StartTimer();
      // Prevent the display turning off. On Win7 and later this also
      // blocks the screen saver, but we need the timer started above
      // to block on Win XP and Vista.
      SetThreadExecutionState(ES_DISPLAY_REQUIRED|ES_CONTINUOUS);
    } else {
      WAKE_LOCK_LOG("WinWakeLock: Unblocking screen saver");
      // Re-enable screen saver.
      StopTimer();
      // Unblock display turning off.
      SetThreadExecutionState(ES_CONTINUOUS);
    }
    return NS_OK;
  }

  void StartTimer() {
    ResetScreenSaverTimeout();
    MOZ_ASSERT(!mTimer);
    if (mTimer) {
      return;
    }

    nsresult rv;
    nsCOMPtr<nsITimer> timer = do_CreateInstance(NS_TIMER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to create screen saver timeout reset timer");
      return;
    }
    // The minimum screensaver timeout that can be specified with Windows' UI
    // is 60 seconds. We set a timer to re-jig the screen saver 10 seconds
    // before we expect the timer to run out, but always at least in 1 second
    // intervals. We reset the timer at a max of 50 seconds, so that if the
    // user changes the timeout using the UI, we won't be caught out.
    int32_t timeout = std::max(std::min(50, (int32_t)mScreenSaverTimeout - 10), 1);
    uint32_t timeoutMs = (uint32_t)timeout * 1000;
    WAKE_LOCK_LOG("WinWakeLock: Setting periodic timer for %d ms", timeoutMs);
    rv = timer->InitWithCallback(this,
                                 timeoutMs,
                                 nsITimer::TYPE_REPEATING_SLACK);
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to initialize screen saver timeout reset timer");
      return;
    }

    mTimer = timer.forget();
  }

  void StopTimer() {
    WAKE_LOCK_LOG("WinWakeLock: StopTimer()");
    if (!mTimer) {
      return;
    }
    mTimer->Cancel();
    mTimer = nullptr;
  }

  // Resets the operating system's timeout for when to disable the screen.
  // Called periodically to keep the screensaver off.
  void ResetScreenSaverTimeout() {
    if (SystemParametersInfo(SPI_GETSCREENSAVETIMEOUT, 0, &mScreenSaverTimeout, 0)) {
      SystemParametersInfo(SPI_SETSCREENSAVETIMEOUT, mScreenSaverTimeout, NULL, 0);
    }
    WAKE_LOCK_LOG("WinWakeLock: ResetScreenSaverTimeout() mScreenSaverTimeout=%d", mScreenSaverTimeout);
  }

  UINT mScreenSaverTimeout = 60;
  nsCOMPtr<nsITimer> mTimer;
};

NS_IMPL_ISUPPORTS(WinWakeLockListener, nsIDOMMozWakeLockListener, nsITimerCallback)
StaticRefPtr<WinWakeLockListener> sWakeLockListener;

static void
AddScreenWakeLockListener()
{
  nsCOMPtr<nsIPowerManagerService> sPowerManagerService = do_GetService(POWERMANAGERSERVICE_CONTRACTID);
  if (sPowerManagerService) {
    sWakeLockListener = new WinWakeLockListener();
    sPowerManagerService->AddWakeLockListener(sWakeLockListener);
  } else {
    NS_WARNING("Failed to retrieve PowerManagerService, wakelocks will be broken!");
  }
}

static void
RemoveScreenWakeLockListener()
{
  nsCOMPtr<nsIPowerManagerService> sPowerManagerService = do_GetService(POWERMANAGERSERVICE_CONTRACTID);
  if (sPowerManagerService) {
    sPowerManagerService->RemoveWakeLockListener(sWakeLockListener);
    sPowerManagerService = nullptr;
    sWakeLockListener = nullptr;
  }
}

namespace mozilla {
namespace widget {
// Native event callback message.
UINT sAppShellGeckoMsgId = RegisterWindowMessageW(L"nsAppShell:EventID");
} }

const wchar_t* kTaskbarButtonEventId = L"TaskbarButtonCreated";
UINT sTaskbarButtonCreatedMsg;

/* static */
UINT nsAppShell::GetTaskbarButtonCreatedMessage() {
	return sTaskbarButtonCreatedMsg;
}

namespace mozilla {
namespace crashreporter {
void LSPAnnotate();
} // namespace crashreporter
} // namespace mozilla

using mozilla::crashreporter::LSPAnnotate;

//-------------------------------------------------------------------------

/*static*/ LRESULT CALLBACK
nsAppShell::EventWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  if (uMsg == sAppShellGeckoMsgId) {
    nsAppShell *as = reinterpret_cast<nsAppShell *>(lParam);
    as->NativeEventCallback();
    NS_RELEASE(as);
    return TRUE;
  }
  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

nsAppShell::~nsAppShell()
{
  if (mEventWnd) {
    // DestroyWindow doesn't do anything when called from a non UI thread.
    // Since mEventWnd was created on the UI thread, it must be destroyed on
    // the UI thread.
    SendMessage(mEventWnd, WM_CLOSE, 0, 0);
  }
}

nsresult
nsAppShell::Init()
{
#ifdef MOZ_CRASHREPORTER
  LSPAnnotate();
#endif

  mLastNativeEventScheduled = TimeStamp::NowLoRes();

  mozilla::ipc::windows::InitUIThread();

  sTaskbarButtonCreatedMsg = ::RegisterWindowMessageW(kTaskbarButtonEventId);
  NS_ASSERTION(sTaskbarButtonCreatedMsg, "Could not register taskbar button creation message");

  WNDCLASSW wc;
  HINSTANCE module = GetModuleHandle(nullptr);

  const wchar_t *const kWindowClass = L"nsAppShell:EventWindowClass";
  if (!GetClassInfoW(module, kWindowClass, &wc)) {
    wc.style         = 0;
    wc.lpfnWndProc   = EventWindowProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = module;
    wc.hIcon         = nullptr;
    wc.hCursor       = nullptr;
    wc.hbrBackground = (HBRUSH) nullptr;
    wc.lpszMenuName  = (LPCWSTR) nullptr;
    wc.lpszClassName = kWindowClass;
    RegisterClassW(&wc);
  }

  mEventWnd = CreateWindowW(kWindowClass, L"nsAppShell:EventWindow",
                            0, 0, 0, 10, 10, HWND_MESSAGE, nullptr, module,
                            nullptr);
  NS_ENSURE_STATE(mEventWnd);

  if (XRE_IsParentProcess()) {
    ScreenManager& screenManager = ScreenManager::GetSingleton();
    screenManager.SetHelper(mozilla::MakeUnique<ScreenHelperWin>());
    ScreenHelperWin::RefreshScreens();
  }

  return nsBaseAppShell::Init();
}

NS_IMETHODIMP
nsAppShell::Run(void)
{
  // Content processes initialize audio later through PContent using audio
  // tray id information pulled from the browser process AudioSession. This
  // way the two share a single volume control.
  // Note StopAudioSession() is called from nsAppRunner.cpp after xpcom is torn
  // down to insure the browser shuts down after child processes.
  if (XRE_IsParentProcess()) {
    mozilla::widget::StartAudioSession();
  }

  // Add an observer that disables the screen saver when requested by Gecko.
  // For example when we're playing video in the foreground tab.
  AddScreenWakeLockListener();

  nsresult rv = nsBaseAppShell::Run();

  RemoveScreenWakeLockListener();

  return rv;
}

NS_IMETHODIMP
nsAppShell::Exit(void)
{
  return nsBaseAppShell::Exit();
}

void
nsAppShell::DoProcessMoreGeckoEvents()
{
  // Called by nsBaseAppShell's NativeEventCallback() after it has finished
  // processing pending gecko events and there are still gecko events pending
  // for the thread. (This can happen if NS_ProcessPendingEvents reached it's
  // starvation timeout limit.) The default behavior in nsBaseAppShell is to
  // call ScheduleNativeEventCallback to post a follow up native event callback
  // message. This triggers an additional call to NativeEventCallback for more
  // gecko event processing.

  // There's a deadlock risk here with certain internal Windows modal loops. In
  // our dispatch code, we prioritize messages so that input is handled first.
  // However Windows modal dispatch loops often prioritize posted messages. If
  // we find ourselves in a tight gecko timer loop where NS_ProcessPendingEvents
  // takes longer than the timer duration, NS_HasPendingEvents(thread) will
  // always be true. ScheduleNativeEventCallback will be called on every
  // NativeEventCallback callback, and in a Windows modal dispatch loop, the
  // callback message will be processed first -> input gets starved, dead lock.

  // To avoid, don't post native callback messages from NativeEventCallback
  // when we're in a modal loop. This gets us back into the Windows modal
  // dispatch loop dispatching input messages. Once we drop out of the modal
  // loop, we use mNativeCallbackPending to fire off a final NativeEventCallback
  // if we need it, which insures NS_ProcessPendingEvents gets called and all
  // gecko events get processed.
  if (mEventloopNestingLevel < 2) {
    OnDispatchedEvent(nullptr);
    mNativeCallbackPending = false;
  } else {
    mNativeCallbackPending = true;
  }
}

void
nsAppShell::ScheduleNativeEventCallback()
{
  // Post a message to the hidden message window
  NS_ADDREF_THIS(); // will be released when the event is processed
  {
    MutexAutoLock lock(mLastNativeEventScheduledMutex);
    // Time stamp this event so we can detect cases where the event gets
    // dropping in sub classes / modal loops we do not control.
    mLastNativeEventScheduled = TimeStamp::NowLoRes();
  }
  ::PostMessage(mEventWnd, sAppShellGeckoMsgId, 0, reinterpret_cast<LPARAM>(this));
}

bool
nsAppShell::ProcessNextNativeEvent(bool mayWait)
{
  // Notify ipc we are spinning a (possibly nested) gecko event loop.
  mozilla::ipc::MessageChannel::NotifyGeckoEventDispatch();

  bool gotMessage = false;

  do {
    MSG msg;
    bool uiMessage = false;

    // For avoiding deadlock between our process and plugin process by
    // mouse wheel messages, we're handling actually when we receive one of
    // following internal messages which is posted by native mouse wheel
    // message handler. Any other events, especially native modifier key
    // events, should not be handled between native message and posted
    // internal message because it may make different modifier key state or
    // mouse cursor position between them.
    if (mozilla::widget::MouseScrollHandler::IsWaitingInternalMessage()) {
      gotMessage = WinUtils::PeekMessage(&msg, nullptr, MOZ_WM_MOUSEWHEEL_FIRST,
                                         MOZ_WM_MOUSEWHEEL_LAST, PM_REMOVE);
      NS_ASSERTION(gotMessage,
                   "waiting internal wheel message, but it has not come");
      uiMessage = gotMessage;
    }

    if (!gotMessage) {
      gotMessage = WinUtils::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE);
      uiMessage =
        (msg.message >= WM_KEYFIRST && msg.message <= WM_IME_KEYLAST) ||
        (msg.message >= NS_WM_IMEFIRST && msg.message <= NS_WM_IMELAST) ||
        (msg.message >= WM_MOUSEFIRST && msg.message <= WM_MOUSELAST);
    }

    if (gotMessage) {
      if (msg.message == WM_QUIT) {
        ::PostQuitMessage(msg.wParam);
        Exit();
      } else {
        // If we had UI activity we would be processing it now so we know we
        // have either kUIActivity or kActivityNoUIAVail.
        mozilla::HangMonitor::NotifyActivity(
          uiMessage ? mozilla::HangMonitor::kUIActivity :
                      mozilla::HangMonitor::kActivityNoUIAVail);

        if (msg.message >= WM_KEYFIRST && msg.message <= WM_KEYLAST &&
            IMEHandler::ProcessRawKeyMessage(msg)) {
          continue;  // the message is consumed.
        }

        // Store Printer Properties messages for reposting, because they are not
        // processed by a window procedure, but are explicitly waited for in the
        // winspool.drv code that will be further up the stack.
        if (msg.message == MOZ_WM_PRINTER_PROPERTIES_COMPLETION ||
            msg.message == MOZ_WM_PRINTER_PROPERTIES_FAILURE) {
          mMsgsToRepost.push_back(msg);
          continue;
        }

        ::TranslateMessage(&msg);
        ::DispatchMessageW(&msg);
      }
    } else if (mayWait) {
      // Block and wait for any posted application message
      mozilla::HangMonitor::Suspend();
      {
        AutoProfilerThreadSleep sleep;
        WinUtils::WaitForMessage();
      }
    }
  } while (!gotMessage && mayWait);

  // See DoProcessNextNativeEvent, mEventloopNestingLevel will be
  // one when a modal loop unwinds.
  if (mNativeCallbackPending && mEventloopNestingLevel == 1)
    DoProcessMoreGeckoEvents();

  // Check for starved native callbacks. If we haven't processed one
  // of these events in NATIVE_EVENT_STARVATION_LIMIT, fire one off.
  static const mozilla::TimeDuration nativeEventStarvationLimit =
    mozilla::TimeDuration::FromSeconds(NATIVE_EVENT_STARVATION_LIMIT);

  TimeDuration timeSinceLastNativeEventScheduled;
  {
    MutexAutoLock lock(mLastNativeEventScheduledMutex);
    timeSinceLastNativeEventScheduled =
        TimeStamp::NowLoRes() - mLastNativeEventScheduled;
  }
  if (timeSinceLastNativeEventScheduled > nativeEventStarvationLimit) {
    ScheduleNativeEventCallback();
  }

  return gotMessage;
}

nsresult
nsAppShell::AfterProcessNextEvent(nsIThreadInternal* /* unused */,
                                  bool /* unused */)
{
  if (!mMsgsToRepost.empty()) {
    for (MSG msg : mMsgsToRepost) {
      ::PostMessageW(msg.hwnd, msg.message, msg.wParam, msg.lParam);
    }
    mMsgsToRepost.clear();
  }
  return NS_OK;
}
