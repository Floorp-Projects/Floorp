/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_recordreplay_Monitor_h
#define mozilla_recordreplay_Monitor_h

#include "mozilla/PlatformConditionVariable.h"

namespace mozilla {
namespace recordreplay {

// Simple wrapper around mozglue mutexes and condvars. This is a lighter weight
// abstraction than mozilla::Monitor and has simpler interactions with the
// record/replay system.
class Monitor : public detail::MutexImpl
{
public:
  Monitor()
    : detail::MutexImpl(Behavior::DontPreserve)
  {}

  void Lock() { detail::MutexImpl::lock(); }
  void Unlock() { detail::MutexImpl::unlock(); }
  void Wait() { mCondVar.wait(*this); }
  void Notify() { mCondVar.notify_one(); }
  void NotifyAll() { mCondVar.notify_all(); }

  void WaitUntil(TimeStamp aTime) {
    AutoEnsurePassThroughThreadEvents pt;
    mCondVar.wait_for(*this, aTime - TimeStamp::Now());
  }

private:
  detail::ConditionVariableImpl mCondVar;
};

// RAII class to lock a monitor.
struct MOZ_RAII MonitorAutoLock
{
  explicit MonitorAutoLock(Monitor& aMonitor)
    : mMonitor(aMonitor)
  {
    mMonitor.Lock();
  }

  ~MonitorAutoLock()
  {
    mMonitor.Unlock();
  }

private:
  Monitor& mMonitor;
};

// RAII class to unlock a monitor.
struct MOZ_RAII MonitorAutoUnlock
{
  explicit MonitorAutoUnlock(Monitor& aMonitor)
    : mMonitor(aMonitor)
  {
    mMonitor.Unlock();
  }

  ~MonitorAutoUnlock()
  {
    mMonitor.Lock();
  }

private:
  Monitor& mMonitor;
};

} // namespace recordreplay
} // namespace mozilla

#endif // mozilla_recordreplay_Monitor_h
