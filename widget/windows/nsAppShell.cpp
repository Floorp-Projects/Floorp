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
#include "nsWindow.h"
#include "nsString.h"
#include "WinIMEHandler.h"
#include "mozilla/widget/AudioSession.h"
#include "mozilla/BackgroundHangMonitor.h"
#include "mozilla/Hal.h"
#include "nsIDOMWakeLockListener.h"
#include "nsIPowerManagerService.h"
#include "mozilla/ProfilerLabels.h"
#include "mozilla/StaticPtr.h"
#include "nsTHashtable.h"
#include "nsHashKeys.h"
#include "nsComponentManagerUtils.h"
#include "ScreenHelperWin.h"
#include "HeadlessScreenHelper.h"
#include "mozilla/widget/ScreenManager.h"
#include "mozilla/Atomics.h"
#include "mozilla/WindowsProcessMitigations.h"

#if defined(ACCESSIBILITY)
#  include "mozilla/a11y/Compatibility.h"
#  include "mozilla/a11y/Platform.h"
#endif  // defined(ACCESSIBILITY)

using namespace mozilla;
using namespace mozilla::widget;

#define WAKE_LOCK_LOG(...) \
  MOZ_LOG(gWinWakeLockLog, mozilla::LogLevel::Debug, (__VA_ARGS__))
static mozilla::LazyLogModule gWinWakeLockLog("WinWakeLock");

// This wakelock listener is used for Window7 and above.
class WinWakeLockListener final : public nsIDOMMozWakeLockListener {
 public:
  NS_DECL_ISUPPORTS
  WinWakeLockListener() { MOZ_ASSERT(XRE_IsParentProcess()); }

 private:
  ~WinWakeLockListener() {
    ReleaseWakelockIfNeeded(PowerRequestDisplayRequired);
    ReleaseWakelockIfNeeded(PowerRequestExecutionRequired);
  }

  void SetHandle(HANDLE aHandle, POWER_REQUEST_TYPE aType) {
    switch (aType) {
      case PowerRequestDisplayRequired: {
        if (!aHandle && mDisplayHandle) {
          CloseHandle(mDisplayHandle);
        }
        mDisplayHandle = aHandle;
        return;
      }
      case PowerRequestExecutionRequired: {
        if (!aHandle && mNonDisplayHandle) {
          CloseHandle(mNonDisplayHandle);
        }
        mNonDisplayHandle = aHandle;
        return;
      }
      default:
        MOZ_ASSERT_UNREACHABLE("Invalid request type");
        return;
    }
  }

  HANDLE GetHandle(POWER_REQUEST_TYPE aType) const {
    switch (aType) {
      case PowerRequestDisplayRequired:
        return mDisplayHandle;
      case PowerRequestExecutionRequired:
        return mNonDisplayHandle;
      default:
        MOZ_ASSERT_UNREACHABLE("Invalid request type");
        return nullptr;
    }
  }

  HANDLE CreateHandle(POWER_REQUEST_TYPE aType) {
    MOZ_ASSERT(!GetHandle(aType));
    REASON_CONTEXT context = {0};
    context.Version = POWER_REQUEST_CONTEXT_VERSION;
    context.Flags = POWER_REQUEST_CONTEXT_SIMPLE_STRING;
    context.Reason.SimpleReasonString = RequestTypeLPWSTR(aType);
    HANDLE handle = PowerCreateRequest(&context);
    if (!handle) {
      WAKE_LOCK_LOG("Failed to create handle for %s, error=%d",
                    RequestTypeStr(aType), GetLastError());
      return nullptr;
    }
    SetHandle(handle, aType);
    return handle;
  }

  LPWSTR RequestTypeLPWSTR(POWER_REQUEST_TYPE aType) const {
    switch (aType) {
      case PowerRequestDisplayRequired:
        return const_cast<LPWSTR>(L"display request");  // -Wwritable-strings
      case PowerRequestExecutionRequired:
        return const_cast<LPWSTR>(
            L"non-display request");  // -Wwritable-strings
      default:
        MOZ_ASSERT_UNREACHABLE("Invalid request type");
        return const_cast<LPWSTR>(L"unknown");  // -Wwritable-strings
    }
  }

  const char* RequestTypeStr(POWER_REQUEST_TYPE aType) const {
    switch (aType) {
      case PowerRequestDisplayRequired:
        return "display request";
      case PowerRequestExecutionRequired:
        return "non-display request";
      default:
        MOZ_ASSERT_UNREACHABLE("Invalid request type");
        return "unknown";
    }
  }

  void RequestWakelockIfNeeded(POWER_REQUEST_TYPE aType) {
    if (GetHandle(aType)) {
      WAKE_LOCK_LOG("Already requested lock for %s", RequestTypeStr(aType));
      return;
    }

    WAKE_LOCK_LOG("Prepare a wakelock for %s", RequestTypeStr(aType));
    HANDLE handle = CreateHandle(aType);
    if (!handle) {
      WAKE_LOCK_LOG("Failed due to no handle for %s", RequestTypeStr(aType));
      return;
    }

    if (PowerSetRequest(handle, aType)) {
      WAKE_LOCK_LOG("Requested %s lock", RequestTypeStr(aType));
    } else {
      WAKE_LOCK_LOG("Failed to request %s lock, error=%d",
                    RequestTypeStr(aType), GetLastError());
      SetHandle(nullptr, aType);
    }
  }

  void ReleaseWakelockIfNeeded(POWER_REQUEST_TYPE aType) {
    if (!GetHandle(aType)) {
      WAKE_LOCK_LOG("Already released lock for %s", RequestTypeStr(aType));
      return;
    }

    WAKE_LOCK_LOG("Prepare to release wakelock for %s", RequestTypeStr(aType));
    if (!PowerClearRequest(GetHandle(aType), aType)) {
      WAKE_LOCK_LOG("Failed to release %s lock, error=%d",
                    RequestTypeStr(aType), GetLastError());
      return;
    }
    SetHandle(nullptr, aType);
    WAKE_LOCK_LOG("Released wakelock for %s", RequestTypeStr(aType));
  }

  NS_IMETHOD Callback(const nsAString& aTopic,
                      const nsAString& aState) override {
    WAKE_LOCK_LOG("topic=%s, state=%s", NS_ConvertUTF16toUTF8(aTopic).get(),
                  NS_ConvertUTF16toUTF8(aState).get());
    if (!aTopic.EqualsASCII("screen") && !aTopic.EqualsASCII("audio-playing") &&
        !aTopic.EqualsASCII("video-playing")) {
      return NS_OK;
    }

    const bool isNonDisplayLock = aTopic.EqualsASCII("audio-playing");
    bool requestLock = false;
    if (isNonDisplayLock) {
      requestLock = aState.EqualsASCII("locked-foreground") ||
                    aState.EqualsASCII("locked-background");
    } else {
      requestLock = aState.EqualsASCII("locked-foreground");
    }

    if (isNonDisplayLock) {
      if (requestLock) {
        RequestWakelockIfNeeded(PowerRequestExecutionRequired);
      } else {
        ReleaseWakelockIfNeeded(PowerRequestExecutionRequired);
      }
    } else {
      if (requestLock) {
        RequestWakelockIfNeeded(PowerRequestDisplayRequired);
      } else {
        ReleaseWakelockIfNeeded(PowerRequestDisplayRequired);
      }
    }
    return NS_OK;
  }

  // Handle would only exist when we request wakelock successfully.
  HANDLE mDisplayHandle = nullptr;
  HANDLE mNonDisplayHandle = nullptr;
};
NS_IMPL_ISUPPORTS(WinWakeLockListener, nsIDOMMozWakeLockListener)

// This wakelock is used for the version older than Windows7.
class LegacyWinWakeLockListener final : public nsIDOMMozWakeLockListener {
 public:
  NS_DECL_ISUPPORTS
  LegacyWinWakeLockListener() { MOZ_ASSERT(XRE_IsParentProcess()); }

 private:
  ~LegacyWinWakeLockListener() {}

  NS_IMETHOD Callback(const nsAString& aTopic,
                      const nsAString& aState) override {
    WAKE_LOCK_LOG("WinWakeLock: topic=%s, state=%s",
                  NS_ConvertUTF16toUTF8(aTopic).get(),
                  NS_ConvertUTF16toUTF8(aState).get());
    if (!aTopic.EqualsASCII("screen") && !aTopic.EqualsASCII("audio-playing") &&
        !aTopic.EqualsASCII("video-playing")) {
      return NS_OK;
    }

    // Check what kind of lock we will require, if both display lock and non
    // display lock are needed, we would require display lock because it has
    // higher priority.
    if (aTopic.EqualsASCII("audio-playing")) {
      mRequireForNonDisplayLock = aState.EqualsASCII("locked-foreground") ||
                                  aState.EqualsASCII("locked-background");
    } else if (aTopic.EqualsASCII("screen") ||
               aTopic.EqualsASCII("video-playing")) {
      mRequireForDisplayLock = aState.EqualsASCII("locked-foreground");
    }

    if (mRequireForDisplayLock) {
      WAKE_LOCK_LOG("WinWakeLock: Request display lock");
      SetThreadExecutionState(ES_DISPLAY_REQUIRED | ES_CONTINUOUS);
    } else if (mRequireForNonDisplayLock) {
      WAKE_LOCK_LOG("WinWakeLock: Request non-display lock");
      SetThreadExecutionState(ES_SYSTEM_REQUIRED | ES_CONTINUOUS);
    } else {
      WAKE_LOCK_LOG("WinWakeLock: reset lock");
      SetThreadExecutionState(ES_CONTINUOUS);
    }
    return NS_OK;
  }

  bool mRequireForDisplayLock = false;
  bool mRequireForNonDisplayLock = false;
};

NS_IMPL_ISUPPORTS(LegacyWinWakeLockListener, nsIDOMMozWakeLockListener)
StaticRefPtr<nsIDOMMozWakeLockListener> sWakeLockListener;

static void AddScreenWakeLockListener() {
  nsCOMPtr<nsIPowerManagerService> sPowerManagerService =
      do_GetService(POWERMANAGERSERVICE_CONTRACTID);
  if (sPowerManagerService) {
    if (IsWin7SP1OrLater()) {
      sWakeLockListener = new WinWakeLockListener();
    } else {
      sWakeLockListener = new LegacyWinWakeLockListener();
    }
    sPowerManagerService->AddWakeLockListener(sWakeLockListener);
  } else {
    NS_WARNING(
        "Failed to retrieve PowerManagerService, wakelocks will be broken!");
  }
}

static void RemoveScreenWakeLockListener() {
  nsCOMPtr<nsIPowerManagerService> sPowerManagerService =
      do_GetService(POWERMANAGERSERVICE_CONTRACTID);
  if (sPowerManagerService) {
    sPowerManagerService->RemoveWakeLockListener(sWakeLockListener);
    sPowerManagerService = nullptr;
    sWakeLockListener = nullptr;
  }
}

class SingleNativeEventPump final : public nsIThreadObserver {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSITHREADOBSERVER

  SingleNativeEventPump() {
    MOZ_ASSERT(!XRE_UseNativeEventProcessing(),
               "Should only be used when not properly processing events.");
  }

 private:
  ~SingleNativeEventPump() {}
};

NS_IMPL_ISUPPORTS(SingleNativeEventPump, nsIThreadObserver)

NS_IMETHODIMP
SingleNativeEventPump::OnDispatchedEvent() { return NS_OK; }

NS_IMETHODIMP
SingleNativeEventPump::OnProcessNextEvent(nsIThreadInternal* aThread,
                                          bool aMayWait) {
  MSG msg;
  bool gotMessage = WinUtils::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE);
  if (gotMessage) {
    ::TranslateMessage(&msg);
    ::DispatchMessageW(&msg);
  }
  return NS_OK;
}

NS_IMETHODIMP
SingleNativeEventPump::AfterProcessNextEvent(nsIThreadInternal* aThread,
                                             bool aMayWait) {
  return NS_OK;
}

// RegisterWindowMessage values
// Native event callback message
const wchar_t* kAppShellGeckoEventId = L"nsAppShell:EventID";
UINT sAppShellGeckoMsgId;
// Taskbar button creation message
const wchar_t* kTaskbarButtonEventId = L"TaskbarButtonCreated";
UINT sTaskbarButtonCreatedMsg;

/* static */
UINT nsAppShell::GetTaskbarButtonCreatedMessage() {
  return sTaskbarButtonCreatedMsg;
}

namespace mozilla {
namespace crashreporter {
void LSPAnnotate();
}  // namespace crashreporter
}  // namespace mozilla

using mozilla::crashreporter::LSPAnnotate;

//-------------------------------------------------------------------------

// Note that since we're on x86-ish processors here, ReleaseAcquire is the
// semantics that normal loads and stores would use anyway.
static Atomic<size_t, ReleaseAcquire> sOutstandingNativeEventCallbacks;

/*static*/ LRESULT CALLBACK nsAppShell::EventWindowProc(HWND hwnd, UINT uMsg,
                                                        WPARAM wParam,
                                                        LPARAM lParam) {
  if (uMsg == sAppShellGeckoMsgId) {
    // The app shell might have been destroyed between this message being
    // posted and being executed, so be extra careful.
    if (!sOutstandingNativeEventCallbacks) {
      return TRUE;
    }

    nsAppShell* as = reinterpret_cast<nsAppShell*>(lParam);
    as->NativeEventCallback();
    --sOutstandingNativeEventCallbacks;
    return TRUE;
  }
  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

nsAppShell::~nsAppShell() {
  hal::Shutdown();

  if (mEventWnd) {
    // DestroyWindow doesn't do anything when called from a non UI thread.
    // Since mEventWnd was created on the UI thread, it must be destroyed on
    // the UI thread.
    SendMessage(mEventWnd, WM_CLOSE, 0, 0);
  }

  // Cancel any outstanding native event callbacks.
  sOutstandingNativeEventCallbacks = 0;
}

#if defined(ACCESSIBILITY)

static ULONG gUiaMsg;
static HHOOK gUiaHook;
static uint32_t gUiaAttempts;
static const uint32_t kMaxUiaAttempts = 5;

static void InitUIADetection();

static LRESULT CALLBACK UiaHookProc(int aCode, WPARAM aWParam, LPARAM aLParam) {
  if (aCode < 0) {
    return ::CallNextHookEx(nullptr, aCode, aWParam, aLParam);
  }

  auto cwp = reinterpret_cast<CWPSTRUCT*>(aLParam);
  if (gUiaMsg && cwp->message == gUiaMsg) {
    if (gUiaAttempts < kMaxUiaAttempts) {
      ++gUiaAttempts;

      Maybe<bool> shouldCallNextHook =
          a11y::Compatibility::OnUIAMessage(cwp->wParam, cwp->lParam);
      if (shouldCallNextHook.isSome()) {
        // We've got an instantiator.
        if (!shouldCallNextHook.value()) {
          // We're blocking this instantiation. We need to keep this hook set
          // so that we can catch any future instantiation attempts.
          return 0;
        }

        // We're allowing the instantiator to proceed, so this hook is no longer
        // needed.
        if (::UnhookWindowsHookEx(gUiaHook)) {
          gUiaHook = nullptr;
        }
      } else {
        // Our hook might be firing after UIA; let's try reinstalling ourselves.
        InitUIADetection();
      }
    } else {
      // We've maxed out our attempts. Let's unhook.
      if (::UnhookWindowsHookEx(gUiaHook)) {
        gUiaHook = nullptr;
      }
    }
  }

  return ::CallNextHookEx(nullptr, aCode, aWParam, aLParam);
}

static void InitUIADetection() {
  if (gUiaHook) {
    // In this case we want to re-hook so that the hook is always called ahead
    // of UIA's hook.
    if (::UnhookWindowsHookEx(gUiaHook)) {
      gUiaHook = nullptr;
    }
  }

  if (!gUiaMsg) {
    // This is the message that UIA sends to trigger a command. UIA's
    // CallWndProc looks for this message and then handles the request.
    // Our hook gets in front of UIA's hook and examines the message first.
    gUiaMsg = ::RegisterWindowMessageW(L"HOOKUTIL_MSG");
  }

  if (!gUiaHook) {
    gUiaHook = ::SetWindowsHookEx(WH_CALLWNDPROC, &UiaHookProc, nullptr,
                                  ::GetCurrentThreadId());
  }
}

#endif  // defined(ACCESSIBILITY)

NS_IMETHODIMP
nsAppShell::Observe(nsISupports* aSubject, const char* aTopic,
                    const char16_t* aData) {
  if (XRE_IsParentProcess()) {
    nsCOMPtr<nsIObserverService> obsServ(
        mozilla::services::GetObserverService());

#if defined(ACCESSIBILITY)
    if (!strcmp(aTopic, "dll-loaded-main-thread")) {
      if (a11y::PlatformDisabledState() != a11y::ePlatformIsDisabled &&
          !gUiaHook) {
        nsDependentString dllName(aData);

        if (StringEndsWith(dllName, u"uiautomationcore.dll"_ns,
                           nsCaseInsensitiveStringComparator)) {
          InitUIADetection();

          // Now that we've handled the observer notification, we can remove it
          obsServ->RemoveObserver(this, "dll-loaded-main-thread");
        }
      }

      return NS_OK;
    }
#endif  // defined(ACCESSIBILITY)

    if (!strcmp(aTopic, "sessionstore-restoring-on-startup")) {
      nsWindow::SetIsRestoringSession(true);
      // Now that we've handled the observer notification, we can remove it
      obsServ->RemoveObserver(this, "sessionstore-restoring-on-startup");
      return NS_OK;
    }

    if (!strcmp(aTopic, "sessionstore-windows-restored")) {
      nsWindow::SetIsRestoringSession(false);
      // Now that we've handled the observer notification, we can remove it
      obsServ->RemoveObserver(this, "sessionstore-windows-restored");
      return NS_OK;
    }
  }

  return nsBaseAppShell::Observe(aSubject, aTopic, aData);
}

nsresult nsAppShell::Init() {
  LSPAnnotate();

  hal::Init();

  if (XRE_IsParentProcess()) {
    sTaskbarButtonCreatedMsg = ::RegisterWindowMessageW(kTaskbarButtonEventId);
    NS_ASSERTION(sTaskbarButtonCreatedMsg,
                 "Could not register taskbar button creation message");
  }

  // The hidden message window is used for interrupting the processing of native
  // events, so that we can process gecko events. Therefore, we only need it if
  // we are processing native events. Disabling this is required for win32k
  // syscall lockdown.
  if (XRE_UseNativeEventProcessing()) {
    sAppShellGeckoMsgId = ::RegisterWindowMessageW(kAppShellGeckoEventId);
    NS_ASSERTION(sAppShellGeckoMsgId,
                 "Could not register hidden window event message!");

    mLastNativeEventScheduled = TimeStamp::NowLoRes();

    WNDCLASSW wc;
    HINSTANCE module = GetModuleHandle(nullptr);

    const wchar_t* const kWindowClass = L"nsAppShell:EventWindowClass";
    if (!GetClassInfoW(module, kWindowClass, &wc)) {
      wc.style = 0;
      wc.lpfnWndProc = EventWindowProc;
      wc.cbClsExtra = 0;
      wc.cbWndExtra = 0;
      wc.hInstance = module;
      wc.hIcon = nullptr;
      wc.hCursor = nullptr;
      wc.hbrBackground = (HBRUSH) nullptr;
      wc.lpszMenuName = (LPCWSTR) nullptr;
      wc.lpszClassName = kWindowClass;
      RegisterClassW(&wc);
    }

    mEventWnd = CreateWindowW(kWindowClass, L"nsAppShell:EventWindow", 0, 0, 0,
                              10, 10, HWND_MESSAGE, nullptr, module, nullptr);
    NS_ENSURE_STATE(mEventWnd);
  } else if (XRE_IsContentProcess() && !IsWin32kLockedDown()) {
    // We're not generally processing native events, but still using GDI and we
    // still have some internal windows, e.g. from calling CoInitializeEx.
    // So we use a class that will do a single event pump where previously we
    // might have processed multiple events to make sure any occasional messages
    // to these windows are processed. This also allows any internal Windows
    // messages to be processed to ensure the GDI data remains fresh.
    nsCOMPtr<nsIThreadInternal> threadInt =
        do_QueryInterface(NS_GetCurrentThread());
    if (threadInt) {
      threadInt->SetObserver(new SingleNativeEventPump());
    }
  }

  if (XRE_IsParentProcess()) {
    ScreenManager& screenManager = ScreenManager::GetSingleton();
    if (gfxPlatform::IsHeadless()) {
      screenManager.SetHelper(mozilla::MakeUnique<HeadlessScreenHelper>());
    } else {
      screenManager.SetHelper(mozilla::MakeUnique<ScreenHelperWin>());
      ScreenHelperWin::RefreshScreens();
    }

    nsCOMPtr<nsIObserverService> obsServ(
        mozilla::services::GetObserverService());

    obsServ->AddObserver(this, "sessionstore-restoring-on-startup", false);
    obsServ->AddObserver(this, "sessionstore-windows-restored", false);

#if defined(ACCESSIBILITY)
    if (::GetModuleHandleW(L"uiautomationcore.dll")) {
      InitUIADetection();
    } else {
      obsServ->AddObserver(this, "dll-loaded-main-thread", false);
    }
#endif  // defined(ACCESSIBILITY)
  }

  return nsBaseAppShell::Init();
}

NS_IMETHODIMP
nsAppShell::Run(void) {
  // Content processes initialize audio later through PContent using audio
  // tray id information pulled from the browser process AudioSession. This
  // way the two share a single volume control.
  // Note StopAudioSession() is called from nsAppRunner.cpp after xpcom is torn
  // down to insure the browser shuts down after child processes.
  if (XRE_IsParentProcess()) {
    mozilla::widget::StartAudioSession();
  }

  // Add an observer that disables the screen saver when requested by Gecko.
  // For example when we're playing video in the foreground tab. Whole firefox
  // only needs one wakelock instance, so we would only create one listener in
  // chrome process to prevent requesting unnecessary wakelock.
  if (XRE_IsParentProcess()) {
    AddScreenWakeLockListener();
  }

  nsresult rv = nsBaseAppShell::Run();

  if (XRE_IsParentProcess()) {
    RemoveScreenWakeLockListener();
  }

  return rv;
}

NS_IMETHODIMP
nsAppShell::Exit(void) {
#if defined(ACCESSIBILITY)
  if (XRE_IsParentProcess()) {
    nsCOMPtr<nsIObserverService> obsServ(
        mozilla::services::GetObserverService());
    obsServ->RemoveObserver(this, "dll-loaded-main-thread");

    if (gUiaHook && ::UnhookWindowsHookEx(gUiaHook)) {
      gUiaHook = nullptr;
    }
  }
#endif  // defined(ACCESSIBILITY)

  return nsBaseAppShell::Exit();
}

void nsAppShell::DoProcessMoreGeckoEvents() {
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
    OnDispatchedEvent();
    mNativeCallbackPending = false;
  } else {
    mNativeCallbackPending = true;
  }
}

void nsAppShell::ScheduleNativeEventCallback() {
  MOZ_ASSERT(mEventWnd,
             "We should have created mEventWnd in Init, if this is called.");

  // Post a message to the hidden message window
  ++sOutstandingNativeEventCallbacks;
  {
    MutexAutoLock lock(mLastNativeEventScheduledMutex);
    // Time stamp this event so we can detect cases where the event gets
    // dropping in sub classes / modal loops we do not control.
    mLastNativeEventScheduled = TimeStamp::NowLoRes();
  }
  ::PostMessage(mEventWnd, sAppShellGeckoMsgId, 0,
                reinterpret_cast<LPARAM>(this));
}

bool nsAppShell::ProcessNextNativeEvent(bool mayWait) {
  // Notify ipc we are spinning a (possibly nested) gecko event loop.
  mozilla::ipc::MessageChannel::NotifyGeckoEventDispatch();

  bool gotMessage = false;

  do {
    MSG msg;

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
    }

    if (!gotMessage) {
      gotMessage = WinUtils::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE);
    }

    if (gotMessage) {
      if (msg.message == WM_QUIT) {
        ::PostQuitMessage(msg.wParam);
        Exit();
      } else {
        // If we had UI activity we would be processing it now so we know we
        // have either kUIActivity or kActivityNoUIAVail.
        mozilla::BackgroundHangMonitor().NotifyActivity();

        if (msg.message >= WM_KEYFIRST && msg.message <= WM_KEYLAST &&
            IMEHandler::ProcessRawKeyMessage(msg)) {
          continue;  // the message is consumed.
        }

#if defined(_X86_)
        // Store Printer dialog messages for reposting on x86, because on x86
        // Windows 7 they are not processed by a window procedure, but are
        // explicitly waited for in the winspool.drv code that will be further
        // up the stack (winspool!WaitForCompletionMessage). These are
        // undocumented Windows Message identifiers found in winspool.drv.
        if (msg.message == 0x5b7a || msg.message == 0x5b7f ||
            msg.message == 0x5b80 || msg.message == 0x5b81) {
          mMsgsToRepost.push_back(msg);
          continue;
        }
#endif

        ::TranslateMessage(&msg);
        ::DispatchMessageW(&msg);
      }
    } else if (mayWait) {
      // Block and wait for any posted application message
      mozilla::BackgroundHangMonitor().NotifyWait();
      {
        AUTO_PROFILER_LABEL("nsAppShell::ProcessNextNativeEvent::Wait", IDLE);
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

nsresult nsAppShell::AfterProcessNextEvent(nsIThreadInternal* /* unused */,
                                           bool /* unused */) {
  if (!mMsgsToRepost.empty()) {
    for (MSG msg : mMsgsToRepost) {
      ::PostMessageW(msg.hwnd, msg.message, msg.wParam, msg.lParam);
    }
    mMsgsToRepost.clear();
  }
  return NS_OK;
}
