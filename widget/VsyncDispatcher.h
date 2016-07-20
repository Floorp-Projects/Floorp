/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_VsyncDispatcher_h
#define mozilla_widget_VsyncDispatcher_h

#include "mozilla/Mutex.h"
#include "mozilla/TimeStamp.h"
#include "nsISupportsImpl.h"
#include "nsTArray.h"
#include "mozilla/RefPtr.h"

namespace mozilla {

class VsyncObserver
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VsyncObserver)

public:
  // The method called when a vsync occurs. Return true if some work was done.
  // In general, this vsync notification will occur on the hardware vsync
  // thread from VsyncSource. But it might also be called on PVsync ipc thread
  // if this notification is cross process. Thus all observer should check the
  // thread model before handling the real task.
  virtual bool NotifyVsync(TimeStamp aVsyncTimestamp) = 0;

protected:
  VsyncObserver() {}
  virtual ~VsyncObserver() {}
}; // VsyncObserver

// Used to dispatch vsync events in the parent process to compositors.
//
// When the compositor is in-process, CompositorWidgets own a
// CompositorVsyncDispatcher, and directly attach the compositor's observer
// to it.
//
// When the compositor is out-of-process, the CompositorWidgetDelegate owns
// the vsync dispatcher instead. The widget receives vsync observer/unobserve
// commands via IPDL, and uses this to attach a CompositorWidgetVsyncObserver.
// This observer forwards vsync notifications (on the vsync thread) to a
// dedicated vsync I/O thread, which then forwards the notification to the
// compositor thread in the compositor process.
class CompositorVsyncDispatcher final
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CompositorVsyncDispatcher)

public:
  CompositorVsyncDispatcher();

  // Called on the vsync thread when a hardware vsync occurs
  void NotifyVsync(TimeStamp aVsyncTimestamp);

  // Compositor vsync observers must be added/removed on the compositor thread
  void SetCompositorVsyncObserver(VsyncObserver* aVsyncObserver);
  void Shutdown();

private:
  virtual ~CompositorVsyncDispatcher();
  void ObserveVsync(bool aEnable);

  Mutex mCompositorObserverLock;
  RefPtr<VsyncObserver> mCompositorVsyncObserver;
  bool mDidShutdown;
};

// Dispatch vsync event to ipc actor parent and chrome RefreshTimer.
class RefreshTimerVsyncDispatcher final
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RefreshTimerVsyncDispatcher)

public:
  RefreshTimerVsyncDispatcher();

  // Please check CompositorVsyncDispatcher::NotifyVsync().
  void NotifyVsync(TimeStamp aVsyncTimestamp);

  // Set chrome process's RefreshTimer to this dispatcher.
  // This function can be called from any thread.
  void SetParentRefreshTimer(VsyncObserver* aVsyncObserver);

  // Add or remove the content process' RefreshTimer to this dispatcher. This
  // will be a no-op for AddChildRefreshTimer() if the observer is already
  // registered.
  // These functions can be called from any thread.
  void AddChildRefreshTimer(VsyncObserver* aVsyncObserver);
  void RemoveChildRefreshTimer(VsyncObserver* aVsyncObserver);

private:
  virtual ~RefreshTimerVsyncDispatcher();
  void UpdateVsyncStatus();
  bool NeedsVsync();

  Mutex mRefreshTimersLock;
  RefPtr<VsyncObserver> mParentRefreshTimer;
  nsTArray<RefPtr<VsyncObserver>> mChildRefreshTimers;
};

} // namespace mozilla

#endif // mozilla_widget_VsyncDispatcher_h
