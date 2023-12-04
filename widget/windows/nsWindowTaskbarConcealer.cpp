/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWindowTaskbarConcealer.h"

#include "nsIWinTaskbar.h"
#define NS_TASKBAR_CONTRACTID "@mozilla.org/windows-taskbar;1"

#include "mozilla/Logging.h"
#include "mozilla/StaticPrefs_widget.h"
#include "WinUtils.h"

using namespace mozilla;

/**
 * TaskbarConcealerImpl
 *
 * Implement Windows-fullscreen marking.
 *
 * nsWindow::TaskbarConcealer implements logic determining _whether_ to tell
 * Windows that a given window is fullscreen. TaskbarConcealerImpl performs the
 * platform-specific work of actually communicating that fact to Windows.
 *
 * (This object is not persistent; it's constructed on the stack when needed.)
 */
struct TaskbarConcealerImpl {
  void MarkAsHidingTaskbar(HWND aWnd, bool aMark);

 private:
  nsCOMPtr<nsIWinTaskbar> mTaskbarInfo;
};

/**
 * nsWindow::TaskbarConcealer
 *
 * Issue taskbar-hide requests to the OS as needed.
 */

/*
  Per MSDN [0], one should mark and unmark fullscreen windows via the
  ITaskbarList2::MarkFullscreenWindow method. Unfortunately, Windows pays less
  attention to this than one might prefer -- in particular, it typically fails
  to show the taskbar when switching focus from a window marked as fullscreen to
  one not thus marked. [1]

  Experimentation has (so far) suggested that its behavior is reasonable when
  switching between multiple monitors, or between a set of windows which are all
  from different processes [2]. This leaves us to handle the same-monitor, same-
  process case.

  Rather than do anything subtle here, we take the blanket approach of simply
  listening for every potentially-relevant state change, and then explicitly
  marking or unmarking every potentially-visible toplevel window.

  ----

  [0] Relevant link:
      https://docs.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-itaskbarlist2-markfullscreenwindow

      The "NonRudeHWND" property described therein doesn't help with anything
      in this comment, unfortunately. (See its use in MarkAsHidingTaskbar for
      more details.)

  [1] This is an oversimplification; Windows' actual behavior here is...
      complicated. See bug 1732517 comment 6 for some examples.

  [2] A comment in Chromium asserts that this is actually different threads. For
      us, of course, that makes no difference.
      https://github.com/chromium/chromium/blob/2b822268bd3/ui/views/win/hwnd_message_handler.cc#L1342
*/

/**************************************************************
 *
 * SECTION: TaskbarConcealer utilities
 *
 **************************************************************/

static mozilla::LazyLogModule sTaskbarConcealerLog("TaskbarConcealer");

// Map of all relevant Gecko windows, along with the monitor on which each
// window was last known to be located.
/* static */
nsTHashMap<HWND, HMONITOR> nsWindow::TaskbarConcealer::sKnownWindows;

// Returns Nothing if the window in question is irrelevant (for any reason),
// or Some(the window's current state) otherwise.
/* static */
Maybe<nsWindow::TaskbarConcealer::WindowState>
nsWindow::TaskbarConcealer::GetWindowState(HWND aWnd) {
  // Classical Win32 visibility conditions.
  if (!::IsWindowVisible(aWnd)) {
    return Nothing();
  }
  if (::IsIconic(aWnd)) {
    return Nothing();
  }

  // Non-nsWindow windows associated with this thread may include file dialogs
  // and IME input popups.
  nsWindow* pWin = widget::WinUtils::GetNSWindowPtr(aWnd);
  if (!pWin) {
    return Nothing();
  }

  // nsWindows of other window-classes include tooltips and drop-shadow-bearing
  // menus.
  if (pWin->mWindowType != WindowType::TopLevel) {
    return Nothing();
  }

  // Cloaked windows are (presumably) on a different virtual desktop.
  // https://devblogs.microsoft.com/oldnewthing/20200302-00/?p=103507
  if (pWin->mIsCloaked) {
    return Nothing();
  }

  return Some(
      WindowState{::MonitorFromWindow(aWnd, MONITOR_DEFAULTTONULL),
                  pWin->mFrameState->GetSizeMode() == nsSizeMode_Fullscreen});
}

/**************************************************************
 *
 * SECTION: TaskbarConcealer::UpdateAllState
 *
 **************************************************************/

// Update all Windows-fullscreen-marking state and internal caches to represent
// the current state of the system.
/* static */
void nsWindow::TaskbarConcealer::UpdateAllState(
    HWND destroyedHwnd /* = nullptr */
) {
  // sKnownWindows is otherwise-unprotected shared state
  MOZ_ASSERT(NS_IsMainThread(),
             "TaskbarConcealer can only be used from the main thread!");

  if (MOZ_LOG_TEST(sTaskbarConcealerLog, LogLevel::Info)) {
    static size_t sLogCounter = 0;
    MOZ_LOG(sTaskbarConcealerLog, LogLevel::Info,
            ("Calling UpdateAllState() for the %zuth time", sLogCounter++));

    MOZ_LOG(sTaskbarConcealerLog, LogLevel::Info, ("Last known state:"));
    if (sKnownWindows.IsEmpty()) {
      MOZ_LOG(sTaskbarConcealerLog, LogLevel::Info,
              ("  none (no windows known)"));
    } else {
      for (const auto& entry : sKnownWindows) {
        MOZ_LOG(
            sTaskbarConcealerLog, LogLevel::Info,
            ("  window %p was on monitor %p", entry.GetKey(), entry.GetData()));
      }
    }
  }

  // Array of all our potentially-relevant HWNDs, in Z-order (topmost first),
  // along with their associated relevant state.
  struct Item {
    HWND hwnd;
    HMONITOR monitor;
    bool isGkFullscreen;
  };
  const nsTArray<Item> windows = [&] {
    nsTArray<Item> windows;

    // USE OF UNDOCUMENTED BEHAVIOR: The EnumWindows family of functions
    // enumerates windows in Z-order, topmost first. (This has been true since
    // at least Windows 2000, and possibly since Windows 3.0.)
    //
    // It's necessarily unreliable if windows are reordered while being
    // enumerated; but in that case we'll get a message informing us of that
    // fact, and can redo our state-calculations then.
    //
    // There exists no documented interface to acquire this information (other
    // than ::GetWindow(), which is racy).
    mozilla::EnumerateThreadWindows([&](HWND hwnd) {
      // Depending on details of window-destruction that probably shouldn't be
      // relied on, this HWND may or may not still be in the window list.
      // Pretend it's not.
      if (hwnd == destroyedHwnd) {
        return;
      }

      const auto maybeState = GetWindowState(hwnd);
      if (!maybeState) {
        return;
      }
      const WindowState& state = *maybeState;

      windows.AppendElement(Item{.hwnd = hwnd,
                                 .monitor = state.monitor,
                                 .isGkFullscreen = state.isGkFullscreen});
    });

    return windows;
  }();

  // Relevant monitors are exactly those with relevant windows.
  const nsTHashSet<HMONITOR> relevantMonitors = [&]() {
    nsTHashSet<HMONITOR> relevantMonitors;
    for (const Item& item : windows) {
      relevantMonitors.Insert(item.monitor);
    }
    return relevantMonitors;
  }();

  // Update the cached mapping from windows to monitors. (This is only used as
  // an optimization in TaskbarConcealer::OnWindowPosChanged().)
  sKnownWindows.Clear();
  for (const Item& item : windows) {
    MOZ_LOG(
        sTaskbarConcealerLog, LogLevel::Debug,
        ("Found relevant window %p on monitor %p", item.hwnd, item.monitor));
    sKnownWindows.InsertOrUpdate(item.hwnd, item.monitor);
  }

  // Auxiliary function. Does what it says on the tin.
  const auto FindUppermostWindowOn = [&windows](HMONITOR aMonitor) -> HWND {
    for (const Item& item : windows) {
      if (item.monitor == aMonitor) {
        MOZ_LOG(sTaskbarConcealerLog, LogLevel::Info,
                ("on monitor %p, uppermost relevant HWND is %p", aMonitor,
                 item.hwnd));
        return item.hwnd;
      }
    }

    // This should never happen, since we're drawing our monitor-set from the
    // set of relevant windows.
    MOZ_LOG(sTaskbarConcealerLog, LogLevel::Warning,
            ("on monitor %p, no relevant windows were found", aMonitor));
    return nullptr;
  };

  TaskbarConcealerImpl impl;

  // Mark all relevant windows as not hiding the taskbar, unless they're both
  // Gecko-fullscreen and the uppermost relevant window on their monitor.
  for (HMONITOR monitor : relevantMonitors) {
    const HWND topmost = FindUppermostWindowOn(monitor);

    for (const Item& item : windows) {
      if (item.monitor != monitor) continue;
      impl.MarkAsHidingTaskbar(item.hwnd,
                               item.isGkFullscreen && item.hwnd == topmost);
    }
  }
}  // nsWindow::TaskbarConcealer::UpdateAllState()

// Mark this window as requesting to occlude the taskbar. (The caller is
// responsible for keeping any local state up-to-date.)
void TaskbarConcealerImpl::MarkAsHidingTaskbar(HWND aWnd, bool aMark) {
  const char* const sMark = aMark ? "true" : "false";

  if (!mTaskbarInfo) {
    mTaskbarInfo = do_GetService(NS_TASKBAR_CONTRACTID);

    if (!mTaskbarInfo) {
      MOZ_LOG(
          sTaskbarConcealerLog, LogLevel::Warning,
          ("could not acquire IWinTaskbar (aWnd %p, aMark %s)", aWnd, sMark));
      return;
    }
  }

  MOZ_LOG(sTaskbarConcealerLog, LogLevel::Info,
          ("Calling PrepareFullScreen(%p, %s)", aWnd, sMark));

  const nsresult hr = mTaskbarInfo->PrepareFullScreen(aWnd, aMark);

  if (FAILED(hr)) {
    MOZ_LOG(sTaskbarConcealerLog, LogLevel::Error,
            ("Call to PrepareFullScreen(%p, %s) failed with nsresult %x", aWnd,
             sMark, uint32_t(hr)));
  }
};

/**************************************************************
 *
 * SECTION: TaskbarConcealer event callbacks
 *
 **************************************************************/

void nsWindow::TaskbarConcealer::OnWindowDestroyed(HWND aWnd) {
  MOZ_LOG(sTaskbarConcealerLog, LogLevel::Info,
          ("==> OnWindowDestroyed() for HWND %p", aWnd));

  UpdateAllState(aWnd);
}

void nsWindow::TaskbarConcealer::OnFocusAcquired(nsWindow* aWin) {
  // Update state unconditionally.
  //
  // This is partially because focus-acquisition only updates the z-order, which
  // we don't cache and therefore can't notice changes to -- but also because
  // it's probably a good idea to give the user a natural way to refresh the
  // current fullscreen-marking state if it's somehow gone bad.

  MOZ_LOG(sTaskbarConcealerLog, LogLevel::Info,
          ("==> OnFocusAcquired() for HWND %p on HMONITOR %p", aWin->mWnd,
           ::MonitorFromWindow(aWin->mWnd, MONITOR_DEFAULTTONULL)));

  UpdateAllState();
}

void nsWindow::TaskbarConcealer::OnFullscreenChanged(nsWindow* aWin,
                                                     bool enteredFullscreen) {
  MOZ_LOG(sTaskbarConcealerLog, LogLevel::Info,
          ("==> OnFullscreenChanged() for HWND %p on HMONITOR %p", aWin->mWnd,
           ::MonitorFromWindow(aWin->mWnd, MONITOR_DEFAULTTONULL)));

  UpdateAllState();
}

void nsWindow::TaskbarConcealer::OnWindowPosChanged(nsWindow* aWin) {
  // Optimization: don't bother updating the state if the window hasn't moved
  // (including appearances and disappearances).
  const HWND myHwnd = aWin->mWnd;
  const HMONITOR oldMonitor = sKnownWindows.Get(myHwnd);  // or nullptr
  const HMONITOR newMonitor = GetWindowState(myHwnd)
                                  .map([](auto state) { return state.monitor; })
                                  .valueOr(nullptr);

  if (oldMonitor == newMonitor) {
    return;
  }

  MOZ_LOG(sTaskbarConcealerLog, LogLevel::Info,
          ("==> OnWindowPosChanged() for HWND %p (HMONITOR %p -> %p)", myHwnd,
           oldMonitor, newMonitor));

  UpdateAllState();
}

void nsWindow::TaskbarConcealer::OnAsyncStateUpdateRequest(HWND hwnd) {
  MOZ_LOG(sTaskbarConcealerLog, LogLevel::Info,
          ("==> OnAsyncStateUpdateRequest()"));

  // Work around a race condition in explorer.exe.
  //
  // When a window is unminimized (and on several other events), the taskbar
  // receives a notification that it needs to recalculate the current
  // is-a-fullscreen-window-active-here-state ("rudeness") of each monitor.
  // Unfortunately, this notification is sent concurrently with the
  // WM_WINDOWPOSCHANGING message that performs the unminimization.
  //
  // Until that message is resolved, the window's position is still "minimized".
  // If the taskbar processes its notification faster than the window handles
  // its WM_WINDOWPOSCHANGING message, then the window will appear to the
  // taskbar to still be minimized, and won't be taken into account for
  // computing rudeness. This usually presents as a just-unminimized Firefox
  // fullscreen-window occasionally having the taskbar stuck above it.
  //
  // Unfortunately, it's a bit difficult to improve Firefox's speed-of-response
  // to WM_WINDOWPOSCHANGING messages (we can, and do, execute JavaScript during
  // these), and even if we could that wouldn't always fix it. We instead adopt
  // a variant of a strategy by Etienne Duchamps, who has investigated and
  // documented this issue extensively[0]: we simply send another signal to the
  // shell to notify it to recalculate the current rudeness state of all
  // monitors.
  //
  // [0]
  // https://github.com/dechamps/RudeWindowFixer#a-race-condition-activating-a-minimized-window
  //
  static UINT const shellHookMsg = ::RegisterWindowMessageW(L"SHELLHOOK");
  if (shellHookMsg != 0) {
    // Identifying the particular thread of the particular instance of the
    // shell associated with our current desktop is probably possible, but
    // also probably not worth the effort. Just broadcast the message
    // globally.
    DWORD info = BSM_APPLICATIONS;
    ::BroadcastSystemMessage(BSF_POSTMESSAGE | BSF_IGNORECURRENTTASK, &info,
                             shellHookMsg, HSHELL_WINDOWACTIVATED,
                             (LPARAM)hwnd);
  }
}

void nsWindow::TaskbarConcealer::OnCloakChanged() {
  MOZ_LOG(sTaskbarConcealerLog, LogLevel::Info, ("==> OnCloakChanged()"));

  UpdateAllState();
}
