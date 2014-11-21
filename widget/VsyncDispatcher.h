/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_VsyncDispatcher_h
#define mozilla_widget_VsyncDispatcher_h

#include "base/message_loop.h"
#include "mozilla/Mutex.h"
#include "nsISupportsImpl.h"
#include "nsTArray.h"
#include "ThreadSafeRefcountingWithMainThreadDestruction.h"

class MessageLoop;

namespace mozilla {
class TimeStamp;

namespace layers {
class CompositorVsyncObserver;
}

// Controls how and when to enable/disable vsync.
class VsyncSource
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VsyncSource)
  virtual void EnableVsync() = 0;
  virtual void DisableVsync() = 0;
  virtual bool IsVsyncEnabled() = 0;

protected:
  virtual ~VsyncSource() {}
}; // VsyncSource

class VsyncObserver
{
  // Must be destroyed on main thread since the compositor is as well
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_MAIN_THREAD_DESTRUCTION(VsyncObserver)

public:
  // The method called when a vsync occurs. Return true if some work was done.
  // Vsync notifications will occur on the hardware vsync thread
  virtual bool NotifyVsync(TimeStamp aVsyncTimestamp) = 0;

protected:
  VsyncObserver() {}
  virtual ~VsyncObserver() {}
}; // VsyncObserver

// VsyncDispatcher is used to dispatch vsync events to the registered observers.
class VsyncDispatcher
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VsyncDispatcher)

public:
  static VsyncDispatcher* GetInstance();
  // Called on the vsync thread when a hardware vsync occurs
  // The aVsyncTimestamp can mean different things depending on the platform:
  // b2g - The vsync timestamp of the previous frame that was just displayed
  // OSX - The vsync timestamp of the upcoming frame
  // TODO: Windows / Linux. DOCUMENT THIS WHEN IMPLEMENTING ON THOSE PLATFORMS
  // Android: TODO
  void NotifyVsync(TimeStamp aVsyncTimestamp);
  void SetVsyncSource(VsyncSource* aVsyncSource);

  // Compositor vsync observers must be added/removed on the compositor thread
  void AddCompositorVsyncObserver(VsyncObserver* aVsyncObserver);
  void RemoveCompositorVsyncObserver(VsyncObserver* aVsyncObserver);

private:
  VsyncDispatcher();
  virtual ~VsyncDispatcher();
  void DispatchTouchEvents(bool aNotifiedCompositors, TimeStamp aVsyncTime);

  // Called on the vsync thread. Returns true if observers were notified
  bool NotifyVsyncObservers(TimeStamp aVsyncTimestamp, nsTArray<nsRefPtr<VsyncObserver>>& aObservers);

  // Can have multiple compositors. On desktop, this is 1 compositor per window
  Mutex mCompositorObserverLock;
  nsTArray<nsRefPtr<VsyncObserver>> mCompositorObservers;
  nsRefPtr<VsyncSource> mVsyncSource;
}; // VsyncDispatcher

} // namespace mozilla

#endif // __mozilla_widget_VsyncDispatcher_h
