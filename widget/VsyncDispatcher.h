/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_VsyncDispatcher_h
#define mozilla_widget_VsyncDispatcher_h

#include "nsISupportsImpl.h"

namespace mozilla {
class TimeStamp;

class VsyncObserver
{
public:
  // The method called when a vsync occurs. Return true if some work was done.
  // Vsync notifications will occur on the hardware vsync thread
  virtual bool NotifyVsync(TimeStamp aVsyncTimestamp) = 0;

protected:
  virtual ~VsyncObserver() { }
};

// VsyncDispatcher is used to dispatch vsync events to the registered observers.
class VsyncDispatcher
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VsyncDispatcher)

public:
  static VsyncDispatcher* GetInstance();

  // Compositor vsync observers must be added/removed on the compositor thread
  void AddCompositorVsyncObserver(VsyncObserver* aVsyncObserver);
  void RemoveCompositorVsyncObserver(VsyncObserver* aVsyncObserver);

private:
  VsyncDispatcher();
  virtual ~VsyncDispatcher();
};

} // namespace mozilla

#endif // __mozilla_widget_VsyncDispatcher_h
