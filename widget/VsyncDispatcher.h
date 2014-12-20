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

namespace mozilla {
class TimeStamp;

namespace layers {
class CompositorVsyncObserver;
}

class VsyncObserver
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VsyncObserver)

public:
  // The method called when a vsync occurs. Return true if some work was done.
  // Vsync notifications will occur on the hardware vsync thread
  virtual bool NotifyVsync(TimeStamp aVsyncTimestamp) = 0;

protected:
  VsyncObserver() {}
  virtual ~VsyncObserver() {}
}; // VsyncObserver

class CompositorVsyncDispatcher MOZ_FINAL
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CompositorVsyncDispatcher)
public:
  CompositorVsyncDispatcher();

  // Called on the vsync thread when a hardware vsync occurs
  // The aVsyncTimestamp can mean different things depending on the platform:
  // b2g - The vsync timestamp of the previous frame that was just displayed
  // OSX - The vsync timestamp of the upcoming frame
  // TODO: Windows / Linux. DOCUMENT THIS WHEN IMPLEMENTING ON THOSE PLATFORMS
  // Android: TODO
  void NotifyVsync(TimeStamp aVsyncTimestamp);

  // Compositor vsync observers must be added/removed on the compositor thread
  void SetCompositorVsyncObserver(VsyncObserver* aVsyncObserver);
  void Shutdown();

private:
  virtual ~CompositorVsyncDispatcher();
  Mutex mCompositorObserverLock;
  nsRefPtr<VsyncObserver> mCompositorVsyncObserver;
};

} // namespace mozilla

#endif // __mozilla_widget_VsyncDispatcher_h
