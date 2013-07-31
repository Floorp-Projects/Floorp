/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ipc/RPCChannel.h"
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

using namespace mozilla::widget;

const PRUnichar* kAppShellEventId = L"nsAppShell:EventID";
const PRUnichar* kTaskbarButtonEventId = L"TaskbarButtonCreated";

static UINT sMsgId;

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
  if (uMsg == sMsgId) {
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

  if (!sMsgId)
    sMsgId = RegisterWindowMessageW(kAppShellEventId);

  sTaskbarButtonCreatedMsg = ::RegisterWindowMessageW(kTaskbarButtonEventId);
  NS_ASSERTION(sTaskbarButtonCreatedMsg, "Could not register taskbar button creation message");

  WNDCLASSW wc;
  HINSTANCE module = GetModuleHandle(NULL);

  const PRUnichar *const kWindowClass = L"nsAppShell:EventWindowClass";
  if (!GetClassInfoW(module, kWindowClass, &wc)) {
    wc.style         = 0;
    wc.lpfnWndProc   = EventWindowProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = module;
    wc.hIcon         = NULL;
    wc.hCursor       = NULL;
    wc.hbrBackground = (HBRUSH) NULL;
    wc.lpszMenuName  = (LPCWSTR) NULL;
    wc.lpszClassName = kWindowClass;
    RegisterClassW(&wc);
  }

  mEventWnd = CreateWindowW(kWindowClass, L"nsAppShell:EventWindow",
                           0, 0, 0, 10, 10, NULL, NULL, module, NULL);
  NS_ENSURE_STATE(mEventWnd);

  return nsBaseAppShell::Init();
}


NS_IMETHODIMP
nsAppShell::Run(void)
{
  // Ignore failure; failing to start the application is not exactly an
  // appropriate response to failing to start an audio session.
  mozilla::widget::StartAudioSession();

  nsresult rv = nsBaseAppShell::Run();

  mozilla::widget::StopAudioSession();

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
  // Time stamp this event so we can detect cases where the event gets
  // dropping in sub classes / modal loops we do not control. 
  mLastNativeEventScheduled = TimeStamp::NowLoRes();
  ::PostMessage(mEventWnd, sMsgId, 0, reinterpret_cast<LPARAM>(this));
}

bool
nsAppShell::ProcessNextNativeEvent(bool mayWait)
{
  // Notify ipc we are spinning a (possibly nested) gecko event loop.
  mozilla::ipc::RPCChannel::NotifyGeckoEventDispatch();

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
      gotMessage = WinUtils::PeekMessage(&msg, NULL, MOZ_WM_MOUSEWHEEL_FIRST,
                                         MOZ_WM_MOUSEWHEEL_LAST, PM_REMOVE);
      NS_ASSERTION(gotMessage,
                   "waiting internal wheel message, but it has not come");
      uiMessage = gotMessage;
    }

    if (!gotMessage) {
      gotMessage = WinUtils::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE);
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

        ::TranslateMessage(&msg);
        ::DispatchMessageW(&msg);
      }
    } else if (mayWait) {
      // Block and wait for any posted application message
      mozilla::HangMonitor::Suspend();
      WinUtils::WaitForMessage();
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

  if ((TimeStamp::NowLoRes() - mLastNativeEventScheduled) >
      nativeEventStarvationLimit) {
    ScheduleNativeEventCallback();
  }
  
  return gotMessage;
}
