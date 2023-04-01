/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef widget_windows_WinWindowOcclusionTracker_h
#define widget_windows_WinWindowOcclusionTracker_h

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "nsIWeakReferenceUtils.h"
#include "mozilla/Monitor.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/WindowsVersion.h"
#include "mozilla/ThreadSafeWeakPtr.h"
#include "mozilla/widget/WindowOcclusionState.h"
#include "mozilla/widget/WinEventObserver.h"
#include "Units.h"
#include "nsThreadUtils.h"

class nsBaseWidget;
struct IVirtualDesktopManager;
class WinWindowOcclusionTrackerTest;
class WinWindowOcclusionTrackerInteractiveTest;

namespace base {
class Thread;
}  // namespace base

namespace mozilla {

namespace widget {

class OcclusionUpdateRunnable;
class SerializedTaskDispatcher;
class UpdateOcclusionStateRunnable;

// This class handles window occlusion tracking by using HWND.
// Implementation is borrowed from chromium's NativeWindowOcclusionTrackerWin.
class WinWindowOcclusionTracker final : public DisplayStatusListener,
                                        public SessionChangeListener {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WinWindowOcclusionTracker)

  /// Can only be called from the main thread.
  static WinWindowOcclusionTracker* Get();

  /// Can only be called from the main thread.
  static void Ensure();

  /// Can only be called from the main thread.
  static void ShutDown();

  /// Can be called from any thread.
  static MessageLoop* OcclusionCalculatorLoop();

  /// Can be called from any thread.
  static bool IsInWinWindowOcclusionThread();

  /// Can only be called from the main thread.
  void EnsureDisplayStatusObserver();

  /// Can only be called from the main thread.
  void EnsureSessionChangeObserver();

  // Enables notifying to widget via NotifyOcclusionState() when the occlusion
  // state has been computed.
  void Enable(nsBaseWidget* aWindow, HWND aHwnd);

  // Disables notifying to widget via NotifyOcclusionState() when the occlusion
  // state has been computed.
  void Disable(nsBaseWidget* aWindow, HWND aHwnd);

  // Called when widget's visibility is changed
  void OnWindowVisibilityChanged(nsBaseWidget* aWindow, bool aVisible);

  SerializedTaskDispatcher* GetSerializedTaskDispatcher() {
    return mSerializedTaskDispatcher;
  }

  void TriggerCalculation();

  void DumpOccludingWindows(HWND aHWnd);

 private:
  friend class ::WinWindowOcclusionTrackerTest;
  friend class ::WinWindowOcclusionTrackerInteractiveTest;

  explicit WinWindowOcclusionTracker(UniquePtr<base::Thread> aThread);
  virtual ~WinWindowOcclusionTracker();

  // This class computes the occlusion state of the tracked windows.
  // It runs on a separate thread, and notifies the main thread of
  // the occlusion state of the tracked windows.
  class WindowOcclusionCalculator {
    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WindowOcclusionCalculator)
   public:
    // Creates WindowOcclusionCalculator instance.
    static void CreateInstance();

    // Clear WindowOcclusionCalculator instance.
    static void ClearInstance();

    // Returns existing WindowOcclusionCalculator instance.
    static WindowOcclusionCalculator* GetInstance() { return sCalculator; }

    void Initialize();
    void Shutdown();

    void EnableOcclusionTrackingForWindow(HWND hwnd);
    void DisableOcclusionTrackingForWindow(HWND hwnd);

    // If a window becomes visible, makes sure event hooks are registered.
    void HandleVisibilityChanged(bool aVisible);

    void HandleTriggerCalculation();

   private:
    WindowOcclusionCalculator();
    ~WindowOcclusionCalculator();

    // Registers event hooks, if not registered.
    void MaybeRegisterEventHooks();

    // This is the callback registered to get notified of various Windows
    // events, like window moving/resizing.
    static void CALLBACK EventHookCallback(HWINEVENTHOOK aWinEventHook,
                                           DWORD aEvent, HWND aHwnd,
                                           LONG aIdObject, LONG aIdChild,
                                           DWORD aEventThread,
                                           DWORD aMsEventTime);

    // EnumWindows callback used to iterate over all hwnds to determine
    // occlusion status of all tracked root windows.  Also builds up
    // |current_pids_with_visible_windows_| and registers event hooks for newly
    // discovered processes with visible hwnds.
    static BOOL CALLBACK
    ComputeNativeWindowOcclusionStatusCallback(HWND hwnd, LPARAM lParam);

    // EnumWindows callback used to update the list of process ids with
    // visible hwnds, |pids_for_location_change_hook_|.
    static BOOL CALLBACK UpdateVisibleWindowProcessIdsCallback(HWND aHwnd,
                                                               LPARAM aLParam);

    // Determines which processes owning visible application windows to set the
    // EVENT_OBJECT_LOCATIONCHANGE event hook for and stores the pids in
    // |pids_for_location_change_hook_|.
    void UpdateVisibleWindowProcessIds();

    // Computes the native window occlusion status for all tracked root gecko
    // windows in |root_window_hwnds_occlusion_state_| and notifies them if
    // their occlusion status has changed.
    void ComputeNativeWindowOcclusionStatus();

    // Schedules an occlusion calculation , if one isn't already scheduled.
    void ScheduleOcclusionCalculationIfNeeded();

    // Registers a global event hook (not per process) for the events in the
    // range from |event_min| to |event_max|, inclusive.
    void RegisterGlobalEventHook(DWORD aEventMin, DWORD aEventMax);

    // Registers the EVENT_OBJECT_LOCATIONCHANGE event hook for the process with
    // passed id. The process has one or more visible, opaque windows.
    void RegisterEventHookForProcess(DWORD aPid);

    // Registers/Unregisters the event hooks necessary for occlusion tracking
    // via calls to RegisterEventHook. These event hooks are disabled when all
    // tracked windows are minimized.
    void RegisterEventHooks();
    void UnregisterEventHooks();

    // EnumWindows callback for occlusion calculation. Returns true to
    // continue enumeration, false otherwise. Currently, always returns
    // true because this function also updates currentPidsWithVisibleWindows,
    // and needs to see all HWNDs.
    bool ProcessComputeNativeWindowOcclusionStatusCallback(
        HWND aHwnd, std::unordered_set<DWORD>* aCurrentPidsWithVisibleWindows);

    // Processes events sent to OcclusionEventHookCallback.
    // It generally triggers scheduling of the occlusion calculation, but
    // ignores certain events in order to not calculate occlusion more than
    // necessary.
    void ProcessEventHookCallback(HWINEVENTHOOK aWinEventHook, DWORD aEvent,
                                  HWND aHwnd, LONG aIdObject, LONG aIdChild);

    // EnumWindows callback for determining which processes to set the
    // EVENT_OBJECT_LOCATIONCHANGE event hook for. We set that event hook for
    // processes hosting fully visible, opaque windows.
    void ProcessUpdateVisibleWindowProcessIdsCallback(HWND aHwnd);

    // Returns true if the window is visible, fully opaque, and on the current
    // virtual desktop, false otherwise.
    bool WindowCanOccludeOtherWindowsOnCurrentVirtualDesktop(
        HWND aHwnd, LayoutDeviceIntRect* aWindowRect);

    // Returns true if aHwnd is definitely on the current virtual desktop,
    // false if it's definitely not on the current virtual desktop, and Nothing
    // if we we can't tell for sure.
    Maybe<bool> IsWindowOnCurrentVirtualDesktop(HWND aHwnd);

    static StaticRefPtr<WindowOcclusionCalculator> sCalculator;

    // Map of root app window hwnds and their occlusion state. This contains
    // both visible and hidden windows.
    // It is accessed from WinWindowOcclusionTracker::UpdateOcclusionState()
    // without using mutex. The access is safe by using
    // SerializedTaskDispatcher.
    std::unordered_map<HWND, OcclusionState> mRootWindowHwndsOcclusionState;

    // Values returned by SetWinEventHook are stored so that hooks can be
    // unregistered when necessary.
    std::vector<HWINEVENTHOOK> mGlobalEventHooks;

    // Map from process id to EVENT_OBJECT_LOCATIONCHANGE event hook.
    std::unordered_map<DWORD, HWINEVENTHOOK> mProcessEventHooks;

    // Pids of processes for which the EVENT_OBJECT_LOCATIONCHANGE event hook is
    // set.
    std::unordered_set<DWORD> mPidsForLocationChangeHook;

    // Used as a timer to delay occlusion update.
    RefPtr<CancelableRunnable> mOcclusionUpdateRunnable;

    // Used to determine if a window is occluded. As we iterate through the
    // hwnds in z-order, we subtract each opaque window's rect from
    // mUnoccludedDesktopRegion. When we get to a root window, we subtract
    // it from mUnoccludedDesktopRegion, and if mUnoccludedDesktopRegion
    // doesn't change, the root window was already occluded.
    LayoutDeviceIntRegion mUnoccludedDesktopRegion;

    // Keeps track of how many root windows we need to compute the occlusion
    // state of in a call to ComputeNativeWindowOcclusionStatus. Once we've
    // determined the state of all root windows, we can stop subtracting
    // windows from mUnoccludedDesktopRegion;.
    int mNumRootWindowsWithUnknownOcclusionState;

    // This is true if the task bar thumbnails or the alt tab thumbnails are
    // showing.
    bool mShowingThumbnails = false;

    // Used to keep track of the window that's currently moving. That window
    // is ignored for calculation occlusion so that tab dragging won't
    // ignore windows occluded by the dragged window.
    HWND mMovingWindow = 0;

    // Only used on Win10+.
    RefPtr<IVirtualDesktopManager> mVirtualDesktopManager;

    // Used to serialize tasks related to mRootWindowHwndsOcclusionState.
    RefPtr<SerializedTaskDispatcher> mSerializedTaskDispatcher;

    // This is an alias to the singleton WinWindowOcclusionTracker mMonitor,
    // and is used in ShutDown().
    Monitor& mMonitor;

    friend class OcclusionUpdateRunnable;
  };

  static BOOL CALLBACK DumpOccludingWindowsCallback(HWND aHWnd, LPARAM aLParam);

  // Returns true if we are interested in |hwnd| for purposes of occlusion
  // calculation. We are interested in |hwnd| if it is a window that is
  // visible, opaque, bounded, and not a popup or floating window. If we are
  // interested in |hwnd|, stores the window rectangle in |window_rect|.
  static bool IsWindowVisibleAndFullyOpaque(HWND aHwnd,
                                            LayoutDeviceIntRect* aWindowRect);

  void Destroy();

  static void CallUpdateOcclusionState(
      std::unordered_map<HWND, OcclusionState>* aMap, bool aShowAllWindows);

  // Updates root windows occclusion state. If aShowAllWindows is true,
  // all non-hidden windows will be marked visible.  This is used to force
  // rendering of thumbnails.
  void UpdateOcclusionState(std::unordered_map<HWND, OcclusionState>* aMap,
                            bool aShowAllWindows);

  // This is called with session changed notifications. If the screen is locked
  // by the current session, it marks app windows as occluded.
  void OnSessionChange(WPARAM aStatusCode,
                       Maybe<bool> aIsCurrentSession) override;

  // This is called when the display is put to sleep. If the display is sleeping
  // it marks app windows as occluded.
  void OnDisplayStateChanged(bool aDisplayOn) override;

  // Marks all root windows as either occluded, or if hwnd IsIconic, hidden.
  void MarkNonIconicWindowsOccluded();

  static StaticRefPtr<WinWindowOcclusionTracker> sTracker;

  // "WinWindowOcclusionCalc" thread.
  UniquePtr<base::Thread> mThread;
  Monitor mMonitor;

  // Has ShutDown been called on us? We might have survived if our thread join
  // timed out.
  bool mHasAttemptedShutdown = false;

  // Map of HWND to widget. Maintained on main thread, and used to send
  // occlusion state notifications to Windows from
  // mRootWindowHwndsOcclusionState.
  std::unordered_map<HWND, nsWeakPtr> mHwndRootWindowMap;

  // This is set by UpdateOcclusionState(). It is currently only used by tests.
  int mNumVisibleRootWindows = 0;

  // If the screen is locked, windows are considered occluded.
  bool mScreenLocked = false;

  // If the display is off, windows are considered occluded.
  bool mDisplayOn = true;

  RefPtr<DisplayStatusObserver> mDisplayStatusObserver;

  RefPtr<SessionChangeObserver> mSessionChangeObserver;

  // Used to serialize tasks related to mRootWindowHwndsOcclusionState.
  RefPtr<SerializedTaskDispatcher> mSerializedTaskDispatcher;

  friend class OcclusionUpdateRunnable;
  friend class UpdateOcclusionStateRunnable;
};

}  // namespace widget
}  // namespace mozilla

#endif  // widget_windows_WinWindowOcclusionTracker_h
