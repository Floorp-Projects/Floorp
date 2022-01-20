/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <queue>
#include <windows.h>
#include <winuser.h>
#include <wtsapi32.h>

#include "WinWindowOcclusionTracker.h"

#include "base/thread.h"
#include "base/message_loop.h"
#include "base/platform_thread.h"
#include "gfxConfig.h"
#include "nsThreadUtils.h"
#include "mozilla/DataMutex.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/layers/SynchronousTask.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Logging.h"
#include "mozilla/StaticPrefs_widget.h"
#include "mozilla/StaticPtr.h"
#include "nsBaseWidget.h"
#include "transport/runnable_utils.h"
#include "WinUtils.h"

#if (_WIN32_WINNT < 0x0602)
#  define EVENT_OBJECT_CLOAKED 0x8017
#  define EVENT_OBJECT_UNCLOAKED 0x8018
#endif  // (_WIN32_WINNT < 0x0602)

namespace mozilla::widget {

// Can be called on Main thread
LazyLogModule gWinOcclusionTrackerLog("WinOcclusionTracker");
#define LOG(type, ...) MOZ_LOG(gWinOcclusionTrackerLog, type, (__VA_ARGS__))

// Can be called on OcclusionCalculator thread
LazyLogModule gWinOcclusionCalculatorLog("WinOcclusionCalculator");
#define CALC_LOG(type, ...) \
  MOZ_LOG(gWinOcclusionCalculatorLog, type, (__VA_ARGS__))

// ~16 ms = time between frames when frame rate is 60 FPS.
const int kOcclusionUpdateRunnableDelayMs = 16;

class OcclusionUpdateRunnable : public CancelableRunnable {
 public:
  explicit OcclusionUpdateRunnable(
      WinWindowOcclusionTracker::WindowOcclusionCalculator*
          aOcclusionCalculator)
      : CancelableRunnable("OcclusionUpdateRunnable"),
        mOcclusionCalculator(aOcclusionCalculator) {
    mTimeStamp = TimeStamp::Now();
  }

  NS_IMETHOD Run() override {
    if (mIsCanceled) {
      return NS_OK;
    }
    MOZ_ASSERT(WinWindowOcclusionTracker::IsInWinWindowOcclusionThread());

    uint32_t latencyMs =
        round((TimeStamp::Now() - mTimeStamp).ToMilliseconds());
    CALC_LOG(LogLevel::Debug,
             "ComputeNativeWindowOcclusionStatus() latencyMs %u", latencyMs);

    mOcclusionCalculator->ComputeNativeWindowOcclusionStatus();
    return NS_OK;
  }

  nsresult Cancel() override {
    mIsCanceled = true;
    mOcclusionCalculator = nullptr;
    return NS_OK;
  }

 private:
  bool mIsCanceled = false;
  RefPtr<WinWindowOcclusionTracker::WindowOcclusionCalculator>
      mOcclusionCalculator;
  TimeStamp mTimeStamp;
};

class UpdateOcclusionStateRunnable : public Runnable {
 public:
  UpdateOcclusionStateRunnable(std::unordered_map<HWND, OcclusionState>* aMap,
                               bool aShowAllWindows)
      : Runnable("UpdateOcclusionStateRunnable"),
        mMap(aMap),
        mShowAllWindows(aShowAllWindows) {}

  NS_IMETHOD Run() override {
    MOZ_ASSERT(NS_IsMainThread());

    auto* tracker = WinWindowOcclusionTracker::Get();
    if (tracker) {
      tracker->UpdateOcclusionState(mMap, mShowAllWindows);
    }
    return NS_OK;
  }

 private:
  std::unordered_map<HWND, OcclusionState>* const mMap;
  const bool mShowAllWindows;
};

class SerializedRunnable : public Runnable {
 public:
  SerializedRunnable(already_AddRefed<nsIRunnable> aTask,
                     nsISerialEventTarget* aEventTarget)
      : Runnable("SerializedRunnable"),
        mEventTarget(aEventTarget),
        mTask(aTask) {}

  NS_IMETHOD Run() override {
    mTask->Run();
    return NS_OK;
  }

  RefPtr<nsISerialEventTarget> mEventTarget;

 private:
  RefPtr<nsIRunnable> mTask;
};

// Used to serialize tasks related to mRootWindowHwndsOcclusionState.
class SerializedTaskDispatcher {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SerializedTaskDispatcher)

 public:
  SerializedTaskDispatcher();

  void Destroy();
  void PostTaskToMain(already_AddRefed<nsIRunnable> aTask);
  void PostTaskToCalculator(already_AddRefed<nsIRunnable> aTask);
  void PostDelayedTaskToCalculator(already_AddRefed<Runnable> aTask,
                                   int aDelayMs);
  bool IsOnCurrentThread();

 private:
  ~SerializedTaskDispatcher();

  struct Data {
    std::queue<RefPtr<SerializedRunnable>> mTasks;
    bool mDestroyed = false;
    RefPtr<Runnable> mCurrentRunnable;
  };

  void PostTasksIfNecessary(nsISerialEventTarget* aEventTarget,
                            const DataMutex<Data>::AutoLock& aProofOfLock);
  void HandleDelayedTask(already_AddRefed<nsIRunnable> aTask);
  void HandleTasks();

  // Hold current EventTarget during calling nsIRunnable::Run().
  nsISerialEventTarget* mCurrentEventTarget = nullptr;

  DataMutex<Data> mData;
};

SerializedTaskDispatcher::SerializedTaskDispatcher()
    : mData("SerializedTaskDispatcher::mData") {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  LOG(LogLevel::Info,
      "SerializedTaskDispatcher::SerializedTaskDispatcher() this %p", this);
}

SerializedTaskDispatcher::~SerializedTaskDispatcher() {
#ifdef DEBUG
  auto data = mData.Lock();
  MOZ_ASSERT(data->mDestroyed);
  MOZ_ASSERT(data->mTasks.empty());
#endif
}

void SerializedTaskDispatcher::Destroy() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  LOG(LogLevel::Info, "SerializedTaskDispatcher::Destroy() this %p", this);

  auto data = mData.Lock();
  if (data->mDestroyed) {
    return;
  }

  data->mDestroyed = true;
  std::queue<RefPtr<SerializedRunnable>> empty;
  std::swap(data->mTasks, empty);
}

void SerializedTaskDispatcher::PostTaskToMain(
    already_AddRefed<nsIRunnable> aTask) {
  auto data = mData.Lock();
  if (data->mDestroyed) {
    return;
  }

  nsISerialEventTarget* eventTarget = GetMainThreadSerialEventTarget();
  data->mTasks.push(new SerializedRunnable(std::move(aTask), eventTarget));

  MOZ_ASSERT_IF(!data->mCurrentRunnable, data->mTasks.size() == 1);
  PostTasksIfNecessary(eventTarget, data);
}

void SerializedTaskDispatcher::PostTaskToCalculator(
    already_AddRefed<nsIRunnable> aTask) {
  auto data = mData.Lock();
  if (data->mDestroyed) {
    return;
  }

  nsISerialEventTarget* eventTarget =
      WinWindowOcclusionTracker::OcclusionCalculatorLoop()->SerialEventTarget();
  data->mTasks.push(new SerializedRunnable(std::move(aTask), eventTarget));

  MOZ_ASSERT_IF(!data->mCurrentRunnable, data->mTasks.size() == 1);
  PostTasksIfNecessary(eventTarget, data);
}

void SerializedTaskDispatcher::PostDelayedTaskToCalculator(
    already_AddRefed<Runnable> aTask, int aDelayMs) {
  CALC_LOG(LogLevel::Debug,
           "SerializedTaskDispatcher::PostDelayedTaskToCalculator()");

  RefPtr<Runnable> runnable = WrapRunnable(
      RefPtr<SerializedTaskDispatcher>(this),
      &SerializedTaskDispatcher::HandleDelayedTask, std::move(aTask));
  MessageLoop* targetLoop =
      WinWindowOcclusionTracker::OcclusionCalculatorLoop();
  targetLoop->PostDelayedTask(runnable.forget(), aDelayMs);
}

bool SerializedTaskDispatcher::IsOnCurrentThread() {
  return !!mCurrentEventTarget;
}

void SerializedTaskDispatcher::PostTasksIfNecessary(
    nsISerialEventTarget* aEventTarget,
    const DataMutex<Data>::AutoLock& aProofOfLock) {
  MOZ_ASSERT(!aProofOfLock->mTasks.empty());

  if (aProofOfLock->mCurrentRunnable) {
    return;
  }

  RefPtr<Runnable> runnable =
      WrapRunnable(RefPtr<SerializedTaskDispatcher>(this),
                   &SerializedTaskDispatcher::HandleTasks);
  aProofOfLock->mCurrentRunnable = runnable;
  aEventTarget->Dispatch(runnable.forget());
}

void SerializedTaskDispatcher::HandleDelayedTask(
    already_AddRefed<nsIRunnable> aTask) {
  MOZ_ASSERT(WinWindowOcclusionTracker::IsInWinWindowOcclusionThread());
  CALC_LOG(LogLevel::Debug, "SerializedTaskDispatcher::HandleDelayedTask()");

  auto data = mData.Lock();
  if (data->mDestroyed) {
    return;
  }

  nsISerialEventTarget* eventTarget =
      WinWindowOcclusionTracker::OcclusionCalculatorLoop()->SerialEventTarget();
  data->mTasks.push(new SerializedRunnable(std::move(aTask), eventTarget));

  MOZ_ASSERT_IF(!data->mCurrentRunnable, data->mTasks.size() == 1);
  PostTasksIfNecessary(eventTarget, data);
}

void SerializedTaskDispatcher::HandleTasks() {
  RefPtr<SerializedRunnable> frontTask;

  // Get front task
  {
    auto data = mData.Lock();
    if (data->mDestroyed) {
      return;
    }
    MOZ_RELEASE_ASSERT(data->mCurrentRunnable);
    MOZ_RELEASE_ASSERT(!data->mTasks.empty());

    frontTask = data->mTasks.front();

    MOZ_RELEASE_ASSERT(!mCurrentEventTarget);
    mCurrentEventTarget = frontTask->mEventTarget;
  }

  while (frontTask) {
    if (NS_IsMainThread()) {
      LOG(LogLevel::Debug, "SerializedTaskDispatcher::HandleTasks()");
    } else {
      CALC_LOG(LogLevel::Debug, "SerializedTaskDispatcher::HandleTasks()");
    }

    MOZ_ASSERT_IF(NS_IsMainThread(),
                  frontTask->mEventTarget == GetMainThreadSerialEventTarget());
    MOZ_ASSERT_IF(
        !NS_IsMainThread(),
        frontTask->mEventTarget == MessageLoop::current()->SerialEventTarget());

    frontTask->Run();

    // Get next task
    {
      auto data = mData.Lock();

      frontTask = nullptr;
      data->mTasks.pop();
      // Check if next task could be handled on current thread
      if (!data->mTasks.empty() &&
          data->mTasks.front()->mEventTarget == mCurrentEventTarget) {
        frontTask = data->mTasks.front();
      }
    }
  }

  MOZ_ASSERT(!frontTask);

  // Post tasks to different thread if pending tasks exist.
  {
    auto data = mData.Lock();
    data->mCurrentRunnable = nullptr;
    mCurrentEventTarget = nullptr;

    if (data->mDestroyed || data->mTasks.empty()) {
      return;
    }

    auto& frontTask = data->mTasks.front();
    PostTasksIfNecessary(frontTask->mEventTarget, data);
  }
}

// static
StaticRefPtr<WinWindowOcclusionTracker> WinWindowOcclusionTracker::sTracker;

/* static */
WinWindowOcclusionTracker* WinWindowOcclusionTracker::Get() {
  MOZ_ASSERT(NS_IsMainThread());
  return sTracker;
}

/* static */
void WinWindowOcclusionTracker::Ensure() {
  MOZ_ASSERT(NS_IsMainThread());
  LOG(LogLevel::Info, "WinWindowOcclusionTracker::Ensure()");

  if (sTracker) {
    return;
  }

  base::Thread* thread = new base::Thread("WinWindowOcclusionCalc");

  base::Thread::Options options;
  options.message_loop_type = MessageLoop::TYPE_UI;

  if (!thread->StartWithOptions(options)) {
    delete thread;
    return;
  }

  sTracker = new WinWindowOcclusionTracker(thread);
  WindowOcclusionCalculator::CreateInstance();
}

/* static */
void WinWindowOcclusionTracker::ShutDown() {
  if (!sTracker) {
    return;
  }

  MOZ_ASSERT(NS_IsMainThread());
  LOG(LogLevel::Info, "WinWindowOcclusionTracker::ShutDown()");

  sTracker->Destroy();

  layers::SynchronousTask task("WinWindowOcclusionTracker");
  RefPtr<Runnable> runnable =
      WrapRunnable(RefPtr<WindowOcclusionCalculator>(
                       WindowOcclusionCalculator::GetInstance()),
                   &WindowOcclusionCalculator::Shutdown, &task);
  OcclusionCalculatorLoop()->PostTask(runnable.forget());
  task.Wait();

  WindowOcclusionCalculator::ClearInstance();
  sTracker = nullptr;
}

void WinWindowOcclusionTracker::Destroy() {
  if (mDisplayStatusObserver) {
    mDisplayStatusObserver->Destroy();
    mDisplayStatusObserver = nullptr;
  }
  if (mSessionChangeObserver) {
    mSessionChangeObserver->Destroy();
    mSessionChangeObserver = nullptr;
  }
  if (mSerializedTaskDispatcher) {
    mSerializedTaskDispatcher->Destroy();
  }
}

/* static */
MessageLoop* WinWindowOcclusionTracker::OcclusionCalculatorLoop() {
  return sTracker ? sTracker->mThread->message_loop() : nullptr;
}

/* static */
bool WinWindowOcclusionTracker::IsInWinWindowOcclusionThread() {
  return sTracker &&
         sTracker->mThread->thread_id() == PlatformThread::CurrentId();
}

void WinWindowOcclusionTracker::Enable(nsBaseWidget* aWindow, HWND aHwnd) {
  MOZ_ASSERT(NS_IsMainThread());
  LOG(LogLevel::Info, "WinWindowOcclusionTracker::Enable() aWindow %p aHwnd %p",
      aWindow, aHwnd);

  auto it = mHwndRootWindowMap.find(aHwnd);
  if (it != mHwndRootWindowMap.end()) {
    return;
  }

  mHwndRootWindowMap.emplace(aHwnd, aWindow);

  RefPtr<Runnable> runnable = WrapRunnable(
      RefPtr<WindowOcclusionCalculator>(
          WindowOcclusionCalculator::GetInstance()),
      &WindowOcclusionCalculator::EnableOcclusionTrackingForWindow, aHwnd);
  mSerializedTaskDispatcher->PostTaskToCalculator(runnable.forget());
}

void WinWindowOcclusionTracker::Disable(nsBaseWidget* aWindow, HWND aHwnd) {
  MOZ_ASSERT(NS_IsMainThread());
  LOG(LogLevel::Info,
      "WinWindowOcclusionTracker::Disable() aWindow %p aHwnd %p", aWindow,
      aHwnd);

  auto it = mHwndRootWindowMap.find(aHwnd);
  if (it == mHwndRootWindowMap.end()) {
    return;
  }

  mHwndRootWindowMap.erase(it);

  RefPtr<Runnable> runnable = WrapRunnable(
      RefPtr<WindowOcclusionCalculator>(
          WindowOcclusionCalculator::GetInstance()),
      &WindowOcclusionCalculator::DisableOcclusionTrackingForWindow, aHwnd);
  mSerializedTaskDispatcher->PostTaskToCalculator(runnable.forget());
}

void WinWindowOcclusionTracker::OnWindowVisibilityChanged(nsBaseWidget* aWindow,
                                                          bool aVisible) {
  MOZ_ASSERT(NS_IsMainThread());
  LOG(LogLevel::Info,
      "WinWindowOcclusionTracker::OnWindowVisibilityChanged() aWindow %p "
      "aVisible %d",
      aWindow, aVisible);

  RefPtr<Runnable> runnable = WrapRunnable(
      RefPtr<WindowOcclusionCalculator>(
          WindowOcclusionCalculator::GetInstance()),
      &WindowOcclusionCalculator::HandleVisibilityChanged, aVisible);
  mSerializedTaskDispatcher->PostTaskToCalculator(runnable.forget());
}

WinWindowOcclusionTracker::WinWindowOcclusionTracker(base::Thread* aThread)
    : mThread(aThread) {
  MOZ_ASSERT(NS_IsMainThread());
  LOG(LogLevel::Info, "WinWindowOcclusionTracker::WinWindowOcclusionTracker()");

  mDisplayStatusObserver = DisplayStatusObserver::Create(this);
  mSessionChangeObserver = SessionChangeObserver::Create(this);
  mSerializedTaskDispatcher = new SerializedTaskDispatcher();
}

WinWindowOcclusionTracker::~WinWindowOcclusionTracker() {
  MOZ_ASSERT(NS_IsMainThread());
  LOG(LogLevel::Info,
      "WinWindowOcclusionTracker::~WinWindowOcclusionTracker()");
  delete mThread;
}

// static
bool WinWindowOcclusionTracker::IsWindowVisibleAndFullyOpaque(
    HWND aHwnd, LayoutDeviceIntRect* aWindowRect) {
  // Filter out windows that are not "visible", IsWindowVisible().
  if (!::IsWindow(aHwnd) || !::IsWindowVisible(aHwnd)) {
    return false;
  }

  // Filter out minimized windows.
  if (::IsIconic(aHwnd)) {
    return false;
  }

  LONG exStyles = ::GetWindowLong(aHwnd, GWL_EXSTYLE);
  // Filter out "transparent" windows, windows where the mouse clicks fall
  // through them.
  if (exStyles & WS_EX_TRANSPARENT) {
    return false;
  }

  // Filter out "tool windows", which are floating windows that do not appear on
  // the taskbar or ALT-TAB. Floating windows can have larger window rectangles
  // than what is visible to the user, so by filtering them out we will avoid
  // incorrectly marking native windows as occluded. We do not filter out the
  // Windows Taskbar.
  if (exStyles & WS_EX_TOOLWINDOW) {
    nsAutoString className;
    if (WinUtils::GetClassName(aHwnd, className)) {
      if (!className.Equals(L"Shell_TrayWnd")) {
        return false;
      }
    }
  }

  // Filter out layered windows that are not opaque or that set a transparency
  // colorkey.
  if (exStyles & WS_EX_LAYERED) {
    BYTE alpha;
    DWORD flags;

    // GetLayeredWindowAttributes only works if the application has
    // previously called SetLayeredWindowAttributes on the window.
    // The function will fail if the layered window was setup with
    // UpdateLayeredWindow. Treat this failure as the window being transparent.
    // See Remarks section of
    // https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getlayeredwindowattributes
    if (!::GetLayeredWindowAttributes(aHwnd, nullptr, &alpha, &flags)) {
      return false;
    }

    if (flags & LWA_ALPHA && alpha < 255) {
      return false;
    }
    if (flags & LWA_COLORKEY) {
      return false;
    }
  }

  // Filter out windows that do not have a simple rectangular region.
  HRGN region = ::CreateRectRgn(0, 0, 0, 0);
  int result = GetWindowRgn(aHwnd, region);
  ::DeleteObject(region);
  if (result == COMPLEXREGION) {
    return false;
  }

  // Windows 10 has cloaked windows, windows with WS_VISIBLE attribute but
  // not displayed. explorer.exe, in particular has one that's the
  // size of the desktop. It's usually behind Chrome windows in the z-order,
  // but using a remote desktop can move it up in the z-order. So, ignore them.
  DWORD reason;
  if (SUCCEEDED(::DwmGetWindowAttribute(aHwnd, DWMWA_CLOAKED, &reason,
                                        sizeof(reason))) &&
      reason != 0) {
    return false;
  }

  RECT winRect;
  // Filter out windows that take up zero area. The call to GetWindowRect is one
  // of the most expensive parts of this function, so it is last.
  if (!::GetWindowRect(aHwnd, &winRect)) {
    return false;
  }
  if (::IsRectEmpty(&winRect)) {
    return false;
  }

  // Ignore popup windows since they're transient unless it is the Windows
  // Taskbar
  // XXX Chrome Widget popup handling is removed for now.
  if (::GetWindowLong(aHwnd, GWL_STYLE) & WS_POPUP) {
    nsAutoString className;
    if (WinUtils::GetClassName(aHwnd, className)) {
      if (!className.Equals(L"Shell_TrayWnd")) {
        return false;
      }
    }
  }

  *aWindowRect = LayoutDeviceIntRect(winRect.left, winRect.top,
                                     winRect.right - winRect.left,
                                     winRect.bottom - winRect.top);

  WINDOWPLACEMENT windowPlacement = {0};
  windowPlacement.length = sizeof(WINDOWPLACEMENT);
  ::GetWindowPlacement(aHwnd, &windowPlacement);
  if (windowPlacement.showCmd == SW_MAXIMIZE) {
    // If the window is maximized the window border extends beyond the visible
    // region of the screen.  Adjust the maximized window rect to fit the
    // screen dimensions to ensure that fullscreen windows, which do not extend
    // beyond the screen boundaries since they typically have no borders, will
    // occlude maximized windows underneath them.
    HMONITOR hmon = ::MonitorFromWindow(aHwnd, MONITOR_DEFAULTTONEAREST);
    if (hmon) {
      MONITORINFO mi;
      mi.cbSize = sizeof(mi);
      if (GetMonitorInfo(hmon, &mi)) {
        LayoutDeviceIntRect workArea(mi.rcWork.left, mi.rcWork.top,
                                     mi.rcWork.right - mi.rcWork.left,
                                     mi.rcWork.bottom - mi.rcWork.top);
        // Adjust aWindowRect to fit to monitor.
        aWindowRect->width = std::min(workArea.width, aWindowRect->width);
        if (aWindowRect->x < workArea.x) {
          aWindowRect->x = workArea.x;
        } else {
          aWindowRect->x = std::min(workArea.x + workArea.width,
                                    aWindowRect->x + aWindowRect->width) -
                           aWindowRect->width;
        }
        aWindowRect->height = std::min(workArea.height, aWindowRect->height);
        if (aWindowRect->y < workArea.y) {
          aWindowRect->y = workArea.y;
        } else {
          aWindowRect->y = std::min(workArea.y + workArea.height,
                                    aWindowRect->y + aWindowRect->height) -
                           aWindowRect->height;
        }
      }
    }
  }

  return true;
}

void WinWindowOcclusionTracker::UpdateOcclusionState(
    std::unordered_map<HWND, OcclusionState>* aMap, bool aShowAllWindows) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mSerializedTaskDispatcher->IsOnCurrentThread());
  LOG(LogLevel::Debug,
      "WinWindowOcclusionTracker::UpdateOcclusionState() aShowAllWindows %d",
      aShowAllWindows);

  mNumVisibleRootWindows = 0;
  for (auto& [hwnd, state] : *aMap) {
    auto it = mHwndRootWindowMap.find(hwnd);
    // The window was destroyed while processing occlusion.
    if (it == mHwndRootWindowMap.end()) {
      continue;
    }
    auto occlState = state;

    // If the screen is locked or off, ignore occlusion state results and
    // mark the window as occluded.
    if (mScreenLocked || !mDisplayOn) {
      occlState = OcclusionState::OCCLUDED;
    } else if (aShowAllWindows) {
      occlState = OcclusionState::VISIBLE;
    }
    it->second->NotifyOcclusionState(occlState);

    if (it->second->SizeMode() != nsSizeMode_Minimized) {
      mNumVisibleRootWindows++;
    }
  }
}

void WinWindowOcclusionTracker::OnSessionChange(WPARAM aStatusCode,
                                                Maybe<bool> aIsCurrentSession) {
  MOZ_ASSERT(NS_IsMainThread());

  if (aIsCurrentSession.isNothing() || !*aIsCurrentSession) {
    return;
  }

  if (aStatusCode == WTS_SESSION_UNLOCK) {
    LOG(LogLevel::Info,
        "WinWindowOcclusionTracker::OnSessionChange() WTS_SESSION_UNLOCK");

    // UNLOCK will cause a foreground window change, which will
    // trigger an occlusion calculation on its own.
    mScreenLocked = false;
  } else if (aStatusCode == WTS_SESSION_LOCK) {
    LOG(LogLevel::Info,
        "WinWindowOcclusionTracker::OnSessionChange() WTS_SESSION_LOCK");

    mScreenLocked = true;
    MarkNonIconicWindowsOccluded();
  }
}

void WinWindowOcclusionTracker::OnDisplayStateChanged(bool aDisplayOn) {
  MOZ_ASSERT(NS_IsMainThread());
  LOG(LogLevel::Info,
      "WinWindowOcclusionTracker::OnDisplayStateChanged() aDisplayOn %d",
      aDisplayOn);

  if (mDisplayOn == aDisplayOn) {
    return;
  }

  mDisplayOn = aDisplayOn;
  if (aDisplayOn) {
    // Notify the window occlusion calculator of the display turning on
    // which will schedule an occlusion calculation. This must be run
    // on the WindowOcclusionCalculator thread.
    RefPtr<Runnable> runnable =
        WrapRunnable(RefPtr<WindowOcclusionCalculator>(
                         WindowOcclusionCalculator::GetInstance()),
                     &WindowOcclusionCalculator::HandleVisibilityChanged,
                     /* aVisible */ true);
    mSerializedTaskDispatcher->PostTaskToCalculator(runnable.forget());
  } else {
    MarkNonIconicWindowsOccluded();
  }
}

void WinWindowOcclusionTracker::MarkNonIconicWindowsOccluded() {
  MOZ_ASSERT(NS_IsMainThread());
  LOG(LogLevel::Info,
      "WinWindowOcclusionTracker::MarkNonIconicWindowsOccluded()");

  // Set all visible root windows as occluded. If not visible,
  // set them as hidden.
  for (auto& [hwnd, window] : mHwndRootWindowMap) {
    auto state = (window->SizeMode() == nsSizeMode_Minimized)
                     ? OcclusionState::HIDDEN
                     : OcclusionState::OCCLUDED;
    window->NotifyOcclusionState(state);
  }
}

// static
BOOL WinWindowOcclusionTracker::DumpOccludingWindowsCallback(HWND aHWnd,
                                                             LPARAM aLParam) {
  HWND hwnd = reinterpret_cast<HWND>(aLParam);

  LayoutDeviceIntRect windowRect;
  bool windowIsOccluding = IsWindowVisibleAndFullyOpaque(aHWnd, &windowRect);
  if (windowIsOccluding) {
    nsAutoString className;
    if (WinUtils::GetClassName(aHWnd, className)) {
      const auto name = NS_ConvertUTF16toUTF8(className);
      printf_stderr(
          "DumpOccludingWindowsCallback() aHWnd %p className %s windowRect(%d, "
          "%d, %d, %d)\n",
          aHWnd, name.get(), windowRect.x, windowRect.y, windowRect.width,
          windowRect.height);
    }
  }

  if (aHWnd == hwnd) {
    return false;
  }
  return true;
}

void WinWindowOcclusionTracker::DumpOccludingWindows(HWND aHWnd) {
  printf_stderr("DumpOccludingWindows() until aHWnd %p visible %d iconic %d\n",
                aHWnd, ::IsWindowVisible(aHWnd), ::IsIconic(aHWnd));
  ::EnumWindows(&DumpOccludingWindowsCallback, reinterpret_cast<LPARAM>(aHWnd));
}

// static
StaticRefPtr<WinWindowOcclusionTracker::WindowOcclusionCalculator>
    WinWindowOcclusionTracker::WindowOcclusionCalculator::sCalculator;

WinWindowOcclusionTracker::WindowOcclusionCalculator::
    WindowOcclusionCalculator() {
  MOZ_ASSERT(NS_IsMainThread());
  LOG(LogLevel::Info, "WindowOcclusionCalculator()");

  mSerializedTaskDispatcher =
      WinWindowOcclusionTracker::Get()->GetSerializedTaskDispatcher();
}

WinWindowOcclusionTracker::WindowOcclusionCalculator::
    ~WindowOcclusionCalculator() {}

// static
void WinWindowOcclusionTracker::WindowOcclusionCalculator::CreateInstance() {
  MOZ_ASSERT(NS_IsMainThread());
  sCalculator = new WindowOcclusionCalculator();
}

// static
void WinWindowOcclusionTracker::WindowOcclusionCalculator::ClearInstance() {
  MOZ_ASSERT(NS_IsMainThread());
  sCalculator = nullptr;
}

void WinWindowOcclusionTracker::WindowOcclusionCalculator::Shutdown(
    layers::SynchronousTask* aTask) {
  MOZ_ASSERT(IsInWinWindowOcclusionThread());
  CALC_LOG(LogLevel::Info, "Shutdown()");

  layers::AutoCompleteTask complete(aTask);

  UnregisterEventHooks();
  if (mOcclusionUpdateRunnable) {
    mOcclusionUpdateRunnable->Cancel();
    mOcclusionUpdateRunnable = nullptr;
  }
}

void WinWindowOcclusionTracker::WindowOcclusionCalculator::
    EnableOcclusionTrackingForWindow(HWND aHwnd) {
  MOZ_ASSERT(IsInWinWindowOcclusionThread());
  MOZ_ASSERT(mSerializedTaskDispatcher->IsOnCurrentThread());
  CALC_LOG(LogLevel::Info, "EnableOcclusionTrackingForWindow() aHwnd %p",
           aHwnd);

  MOZ_RELEASE_ASSERT(mRootWindowHwndsOcclusionState.find(aHwnd) ==
                     mRootWindowHwndsOcclusionState.end());
  mRootWindowHwndsOcclusionState[aHwnd] = OcclusionState::UNKNOWN;

  if (mGlobalEventHooks.empty()) {
    RegisterEventHooks();
  }

  // Schedule an occlusion calculation so that the newly tracked window does
  // not have a stale occlusion status.
  ScheduleOcclusionCalculationIfNeeded();
}

void WinWindowOcclusionTracker::WindowOcclusionCalculator::
    DisableOcclusionTrackingForWindow(HWND aHwnd) {
  MOZ_ASSERT(IsInWinWindowOcclusionThread());
  MOZ_ASSERT(mSerializedTaskDispatcher->IsOnCurrentThread());
  CALC_LOG(LogLevel::Info, "DisableOcclusionTrackingForWindow() aHwnd %p",
           aHwnd);

  MOZ_RELEASE_ASSERT(mRootWindowHwndsOcclusionState.find(aHwnd) !=
                     mRootWindowHwndsOcclusionState.end());
  mRootWindowHwndsOcclusionState.erase(aHwnd);

  if (mMovingWindow == aHwnd) {
    mMovingWindow = 0;
  }

  if (mRootWindowHwndsOcclusionState.empty()) {
    UnregisterEventHooks();
    if (mOcclusionUpdateRunnable) {
      mOcclusionUpdateRunnable->Cancel();
      mOcclusionUpdateRunnable = nullptr;
    }
  }
}

void WinWindowOcclusionTracker::WindowOcclusionCalculator::
    HandleVisibilityChanged(bool aVisible) {
  MOZ_ASSERT(IsInWinWindowOcclusionThread());
  CALC_LOG(LogLevel::Info, "HandleVisibilityChange() aVisible %d", aVisible);

  // May have gone from having no visible windows to having one, in
  // which case we need to register event hooks, and make sure that an
  // occlusion calculation is scheduled.
  if (aVisible) {
    MaybeRegisterEventHooks();
    ScheduleOcclusionCalculationIfNeeded();
  }
}

void WinWindowOcclusionTracker::WindowOcclusionCalculator::
    MaybeRegisterEventHooks() {
  if (mGlobalEventHooks.empty()) {
    RegisterEventHooks();
  }
}

// static
void CALLBACK
WinWindowOcclusionTracker::WindowOcclusionCalculator::EventHookCallback(
    HWINEVENTHOOK aWinEventHook, DWORD aEvent, HWND aHwnd, LONG aIdObject,
    LONG aIdChild, DWORD aEventThread, DWORD aMsEventTime) {
  if (sCalculator) {
    sCalculator->ProcessEventHookCallback(aWinEventHook, aEvent, aHwnd,
                                          aIdObject, aIdChild);
  }
}

// static
BOOL CALLBACK WinWindowOcclusionTracker::WindowOcclusionCalculator::
    ComputeNativeWindowOcclusionStatusCallback(HWND aHwnd, LPARAM aLParam) {
  if (sCalculator) {
    return sCalculator->ProcessComputeNativeWindowOcclusionStatusCallback(
        aHwnd, reinterpret_cast<std::unordered_set<DWORD>*>(aLParam));
  }
  return FALSE;
}

// static
BOOL CALLBACK WinWindowOcclusionTracker::WindowOcclusionCalculator::
    UpdateVisibleWindowProcessIdsCallback(HWND aHwnd, LPARAM aLParam) {
  if (sCalculator) {
    sCalculator->ProcessUpdateVisibleWindowProcessIdsCallback(aHwnd);
    return TRUE;
  }
  return FALSE;
}

void WinWindowOcclusionTracker::WindowOcclusionCalculator::
    UpdateVisibleWindowProcessIds() {
  mPidsForLocationChangeHook.clear();
  ::EnumWindows(&UpdateVisibleWindowProcessIdsCallback, 0);
}

void WinWindowOcclusionTracker::WindowOcclusionCalculator::
    ComputeNativeWindowOcclusionStatus() {
  MOZ_ASSERT(IsInWinWindowOcclusionThread());
  MOZ_ASSERT(mSerializedTaskDispatcher->IsOnCurrentThread());

  if (mOcclusionUpdateRunnable) {
    mOcclusionUpdateRunnable = nullptr;
  }

  if (mRootWindowHwndsOcclusionState.empty()) {
    return;
  }

  // Set up initial conditions for occlusion calculation.
  bool shouldUnregisterEventHooks = true;

  // Compute the LayoutDeviceIntRegion for the screen.
  int screenLeft = ::GetSystemMetrics(SM_XVIRTUALSCREEN);
  int screenTop = ::GetSystemMetrics(SM_YVIRTUALSCREEN);
  int screenWidth = ::GetSystemMetrics(SM_CXVIRTUALSCREEN);
  int screenHeight = ::GetSystemMetrics(SM_CYVIRTUALSCREEN);
  LayoutDeviceIntRegion screenRegion =
      LayoutDeviceIntRect(screenLeft, screenTop, screenWidth, screenHeight);
  mNumRootWindowsWithUnknownOcclusionState = 0;

  CALC_LOG(LogLevel::Debug,
           "ComputeNativeWindowOcclusionStatus() screen(%d, %d, %d, %d)",
           screenLeft, screenTop, screenWidth, screenHeight);

  for (auto& [hwnd, state] : mRootWindowHwndsOcclusionState) {
    // IsIconic() checks for a minimized window. Immediately set the state of
    // minimized windows to HIDDEN.
    if (::IsIconic(hwnd)) {
      state = OcclusionState::HIDDEN;
    } else if (IsWindowOnCurrentVirtualDesktop(hwnd) == Some(false)) {
      // If window is not on the current virtual desktop, immediately
      // set the state of the window to OCCLUDED.
      state = OcclusionState::OCCLUDED;
      // Don't unregister event hooks when not on current desktop. There's no
      // notification when that changes, so we can't reregister event hooks.
      shouldUnregisterEventHooks = false;
    } else {
      state = OcclusionState::UNKNOWN;
      shouldUnregisterEventHooks = false;
      mNumRootWindowsWithUnknownOcclusionState++;
    }
  }

  // Unregister event hooks if all native windows are minimized.
  if (shouldUnregisterEventHooks) {
    UnregisterEventHooks();
  } else {
    std::unordered_set<DWORD> currentPidsWithVisibleWindows;
    mUnoccludedDesktopRegion = screenRegion;
    // Calculate unoccluded region if there is a non-minimized native window.
    // Also compute |current_pids_with_visible_windows| as we enumerate
    // the windows.
    EnumWindows(&ComputeNativeWindowOcclusionStatusCallback,
                reinterpret_cast<LPARAM>(&currentPidsWithVisibleWindows));
    // Check if mPidsForLocationChangeHook has any pids of processes
    // currently without visible windows. If so, unhook the win event,
    // remove the pid from mPidsForLocationChangeHook and remove
    // the corresponding event hook from mProcessEventHooks.
    std::unordered_set<DWORD> pidsToRemove;
    for (auto locChangePid : mPidsForLocationChangeHook) {
      if (currentPidsWithVisibleWindows.find(locChangePid) ==
          currentPidsWithVisibleWindows.end()) {
        // Remove the event hook from our map, and unregister the event hook.
        // It's possible the eventhook will no longer be valid, but if we don't
        // unregister the event hook, a process that toggles between having
        // visible windows and not having visible windows could cause duplicate
        // event hooks to get registered for the process.
        UnhookWinEvent(mProcessEventHooks[locChangePid]);
        mProcessEventHooks.erase(locChangePid);
        pidsToRemove.insert(locChangePid);
      }
    }
    if (!pidsToRemove.empty()) {
      // XXX simplify
      for (auto it = mPidsForLocationChangeHook.begin();
           it != mPidsForLocationChangeHook.end();) {
        if (pidsToRemove.find(*it) != pidsToRemove.end()) {
          it = mPidsForLocationChangeHook.erase(it);
        } else {
          ++it;
        }
      }
    }
  }

  RefPtr<Runnable> runnable = new UpdateOcclusionStateRunnable(
      &mRootWindowHwndsOcclusionState, mShowingThumbnails);
  mSerializedTaskDispatcher->PostTaskToMain(runnable.forget());
}

void WinWindowOcclusionTracker::WindowOcclusionCalculator::
    ScheduleOcclusionCalculationIfNeeded() {
  MOZ_ASSERT(IsInWinWindowOcclusionThread());

  // OcclusionUpdateRunnable is already queued.
  if (mOcclusionUpdateRunnable) {
    return;
  }

  CALC_LOG(LogLevel::Debug, "ScheduleOcclusionCalculationIfNeeded()");

  RefPtr<CancelableRunnable> task = new OcclusionUpdateRunnable(this);
  mOcclusionUpdateRunnable = task;
  mSerializedTaskDispatcher->PostDelayedTaskToCalculator(
      task.forget(), kOcclusionUpdateRunnableDelayMs);
}

void WinWindowOcclusionTracker::WindowOcclusionCalculator::
    RegisterGlobalEventHook(DWORD aEventMin, DWORD aEventMax) {
  HWINEVENTHOOK eventHook =
      ::SetWinEventHook(aEventMin, aEventMax, nullptr, &EventHookCallback, 0, 0,
                        WINEVENT_OUTOFCONTEXT);
  mGlobalEventHooks.push_back(eventHook);
}

void WinWindowOcclusionTracker::WindowOcclusionCalculator::
    RegisterEventHookForProcess(DWORD aPid) {
  mPidsForLocationChangeHook.insert(aPid);
  mProcessEventHooks[aPid] = SetWinEventHook(
      EVENT_OBJECT_LOCATIONCHANGE, EVENT_OBJECT_LOCATIONCHANGE, nullptr,
      &EventHookCallback, aPid, 0, WINEVENT_OUTOFCONTEXT);
}

void WinWindowOcclusionTracker::WindowOcclusionCalculator::
    RegisterEventHooks() {
  MOZ_ASSERT(IsInWinWindowOcclusionThread());
  MOZ_RELEASE_ASSERT(mGlobalEventHooks.empty());
  CALC_LOG(LogLevel::Info, "RegisterEventHooks()");

  // Detects native window move (drag) and resizing events.
  RegisterGlobalEventHook(EVENT_SYSTEM_MOVESIZESTART, EVENT_SYSTEM_MOVESIZEEND);

  // Detects native window minimize and restore from taskbar events.
  RegisterGlobalEventHook(EVENT_SYSTEM_MINIMIZESTART, EVENT_SYSTEM_MINIMIZEEND);

  // Detects foreground window changing.
  RegisterGlobalEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND);

  // Detects objects getting shown and hidden. Used to know when the task bar
  // and alt tab are showing preview windows so we can unocclude windows.
  RegisterGlobalEventHook(EVENT_OBJECT_SHOW, EVENT_OBJECT_HIDE);

  // Detects object state changes, e.g., enable/disable state, native window
  // maximize and native window restore events.
  RegisterGlobalEventHook(EVENT_OBJECT_STATECHANGE, EVENT_OBJECT_STATECHANGE);

  // Cloaking and uncloaking of windows should trigger an occlusion calculation.
  // In particular, switching virtual desktops seems to generate these events.
  RegisterGlobalEventHook(EVENT_OBJECT_CLOAKED, EVENT_OBJECT_UNCLOAKED);

  // Determine which subset of processes to set EVENT_OBJECT_LOCATIONCHANGE on
  // because otherwise event throughput is very high, as it generates events
  // for location changes of all objects, including the mouse moving on top of a
  // window.
  UpdateVisibleWindowProcessIds();
  for (DWORD pid : mPidsForLocationChangeHook) {
    RegisterEventHookForProcess(pid);
  }
}

void WinWindowOcclusionTracker::WindowOcclusionCalculator::
    UnregisterEventHooks() {
  MOZ_ASSERT(IsInWinWindowOcclusionThread());
  CALC_LOG(LogLevel::Info, "UnregisterEventHooks()");

  for (const auto eventHook : mGlobalEventHooks) {
    ::UnhookWinEvent(eventHook);
  }
  mGlobalEventHooks.clear();

  for (const auto& [pid, eventHook] : mProcessEventHooks) {
    ::UnhookWinEvent(eventHook);
  }
  mProcessEventHooks.clear();

  mPidsForLocationChangeHook.clear();
}

bool WinWindowOcclusionTracker::WindowOcclusionCalculator::
    ProcessComputeNativeWindowOcclusionStatusCallback(
        HWND aHwnd, std::unordered_set<DWORD>* aCurrentPidsWithVisibleWindows) {
  LayoutDeviceIntRegion currUnoccludedDestkop = mUnoccludedDesktopRegion;
  LayoutDeviceIntRect windowRect;
  bool windowIsOccluding =
      WindowCanOccludeOtherWindowsOnCurrentVirtualDesktop(aHwnd, &windowRect);
  if (windowIsOccluding) {
    // Hook this window's process with EVENT_OBJECT_LOCATION_CHANGE, if we are
    // not already doing so.
    DWORD pid;
    ::GetWindowThreadProcessId(aHwnd, &pid);
    aCurrentPidsWithVisibleWindows->insert(pid);
    auto it = mProcessEventHooks.find(pid);
    if (it == mProcessEventHooks.end()) {
      RegisterEventHookForProcess(pid);
    }

    // If no more root windows to consider, return true so we can continue
    // looking for windows we haven't hooked.
    if (mNumRootWindowsWithUnknownOcclusionState == 0) {
      return true;
    }

    mUnoccludedDesktopRegion.SubOut(windowRect);
  } else if (mNumRootWindowsWithUnknownOcclusionState == 0) {
    // This window can't occlude other windows, but we've determined the
    // occlusion state of all root windows, so we can return.
    return true;
  }

  // Ignore moving windows when deciding if windows under it are occluded.
  if (aHwnd == mMovingWindow) {
    return true;
  }

  // Check if |hwnd| is a root window; if so, we're done figuring out
  // if it's occluded because we've seen all the windows "over" it.
  auto it = mRootWindowHwndsOcclusionState.find(aHwnd);
  if (it == mRootWindowHwndsOcclusionState.end() ||
      it->second != OcclusionState::UNKNOWN) {
    return true;
  }

  CALC_LOG(LogLevel::Debug,
           "ProcessComputeNativeWindowOcclusionStatusCallback() windowRect(%d, "
           "%d, %d, %d) IsOccluding %d",
           windowRect.x, windowRect.y, windowRect.width, windowRect.height,
           windowIsOccluding);

  // On Win7, default theme makes root windows have complex regions by
  // default. But we can still check if their bounding rect is occluded.
  if (!windowIsOccluding) {
    RECT rect;
    if (::GetWindowRect(aHwnd, &rect) != 0) {
      LayoutDeviceIntRect windowRect(
          rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
      currUnoccludedDestkop.SubOut(windowRect);
    }
  }

  it->second = (mUnoccludedDesktopRegion == currUnoccludedDestkop)
                   ? OcclusionState::OCCLUDED
                   : OcclusionState::VISIBLE;
  mNumRootWindowsWithUnknownOcclusionState--;

  return true;
}

void WinWindowOcclusionTracker::WindowOcclusionCalculator::
    ProcessEventHookCallback(HWINEVENTHOOK aWinEventHook, DWORD aEvent,
                             HWND aHwnd, LONG aIdObject, LONG aIdChild) {
  MOZ_ASSERT(IsInWinWindowOcclusionThread());

  // No need to calculate occlusion if a zero HWND generated the event. This
  // happens if there is no window associated with the event, e.g., mouse move
  // events.
  if (!aHwnd) {
    return;
  }

  // We only care about events for window objects. In particular, we don't care
  // about OBJID_CARET, which is spammy.
  if (aIdObject != OBJID_WINDOW) {
    return;
  }

  CALC_LOG(LogLevel::Debug,
           "WindowOcclusionCalculator::ProcessEventHookCallback() aEvent 0x%x",
           aEvent);

  // We generally ignore events for popup windows, except for when the taskbar
  // is hidden or Windows Taskbar, in which case we recalculate occlusion.
  // XXX Chrome Widget popup handling is removed for now.
  bool calculateOcclusion = true;
  if (::GetWindowLong(aHwnd, GWL_STYLE) & WS_POPUP) {
    nsAutoString className;
    if (WinUtils::GetClassName(aHwnd, className)) {
      calculateOcclusion = className.Equals(L"Shell_TrayWnd");
    }
  }

  // Detect if either the alt tab view or the task list thumbnail is being
  // shown. If so, mark all non-hidden windows as occluded, and remember that
  // we're in the showing_thumbnails state. This lasts until we get told that
  // either the alt tab view or task list thumbnail are hidden.
  if (aEvent == EVENT_OBJECT_SHOW) {
    // Avoid getting the aHwnd's class name, and recomputing occlusion, if not
    // needed.
    if (mShowingThumbnails) {
      return;
    }
    nsAutoString className;
    if (WinUtils::GetClassName(aHwnd, className)) {
      const auto name = NS_ConvertUTF16toUTF8(className);
      CALC_LOG(LogLevel::Debug,
               "ProcessEventHookCallback() EVENT_OBJECT_SHOW %s", name.get());

      if (name.Equals("MultitaskingViewFrame") ||
          name.Equals("TaskListThumbnailWnd")) {
        CALC_LOG(LogLevel::Info,
                 "ProcessEventHookCallback() mShowingThumbnails = true");
        mShowingThumbnails = true;

        RefPtr<Runnable> runnable = new UpdateOcclusionStateRunnable(
            &mRootWindowHwndsOcclusionState, mShowingThumbnails);
        mSerializedTaskDispatcher->PostTaskToMain(runnable.forget());
      }
    }
    return;
  } else if (aEvent == EVENT_OBJECT_HIDE) {
    // Avoid getting the aHwnd's class name, and recomputing occlusion, if not
    // needed.
    if (!mShowingThumbnails) {
      return;
    }
    nsAutoString className;
    WinUtils::GetClassName(aHwnd, className);
    const auto name = NS_ConvertUTF16toUTF8(className);
    CALC_LOG(LogLevel::Debug, "ProcessEventHookCallback() EVENT_OBJECT_HIDE %s",
             name.get());
    if (name.Equals("MultitaskingViewFrame") ||
        name.Equals("TaskListThumbnailWnd")) {
      CALC_LOG(LogLevel::Info,
               "ProcessEventHookCallback() mShowingThumbnails = false");
      mShowingThumbnails = false;
      // Let occlusion calculation fix occlusion state, even though hwnd might
      // be a popup window.
      calculateOcclusion = true;
    } else {
      return;
    }
  }
  // Don't continually calculate occlusion while a window is moving (unless it's
  // a root window), but instead once at the beginning and once at the end.
  // Remember the window being moved so if it's a root window, we can ignore
  // it when deciding if windows under it are occluded.
  else if (aEvent == EVENT_SYSTEM_MOVESIZESTART) {
    mMovingWindow = aHwnd;
  } else if (aEvent == EVENT_SYSTEM_MOVESIZEEND) {
    mMovingWindow = 0;
  } else if (mMovingWindow != 0) {
    if (aEvent == EVENT_OBJECT_LOCATIONCHANGE ||
        aEvent == EVENT_OBJECT_STATECHANGE) {
      // Ignore move events if it's not a root window that's being moved. If it
      // is a root window, we want to calculate occlusion to support tab
      // dragging to windows that were occluded when the drag was started but
      // are no longer occluded.
      if (mRootWindowHwndsOcclusionState.find(aHwnd) ==
          mRootWindowHwndsOcclusionState.end()) {
        return;
      }
    } else {
      // If we get an event that isn't a location/state change, then we probably
      // missed the movesizeend notification, or got events out of order. In
      // that case, we want to go back to normal occlusion calculation.
      mMovingWindow = 0;
    }
  }

  if (!calculateOcclusion) {
    return;
  }

  ScheduleOcclusionCalculationIfNeeded();
}

void WinWindowOcclusionTracker::WindowOcclusionCalculator::
    ProcessUpdateVisibleWindowProcessIdsCallback(HWND aHwnd) {
  MOZ_ASSERT(IsInWinWindowOcclusionThread());

  LayoutDeviceIntRect windowRect;
  if (WindowCanOccludeOtherWindowsOnCurrentVirtualDesktop(aHwnd, &windowRect)) {
    DWORD pid;
    ::GetWindowThreadProcessId(aHwnd, &pid);
    mPidsForLocationChangeHook.insert(pid);
  }
}

bool WinWindowOcclusionTracker::WindowOcclusionCalculator::
    WindowCanOccludeOtherWindowsOnCurrentVirtualDesktop(
        HWND aHwnd, LayoutDeviceIntRect* aWindowRect) {
  return IsWindowVisibleAndFullyOpaque(aHwnd, aWindowRect) &&
         (IsWindowOnCurrentVirtualDesktop(aHwnd) == Some(true));
}

Maybe<bool> WinWindowOcclusionTracker::WindowOcclusionCalculator::
    IsWindowOnCurrentVirtualDesktop(HWND aHwnd) {
  if (!mVirtualDesktopManager) {
    return Some(true);
  }

  // XXX VirtualDesktopManager is going to be handled by Bug 1732737.

  return Nothing();
}

#undef LOG
#undef CALC_LOG

}  // namespace mozilla::widget
