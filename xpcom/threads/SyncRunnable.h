/* -*- Mode: C++; tab-width: 12; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SyncRunnable_h

#include "nsThreadUtils.h"
#include "mozilla/Monitor.h"

namespace mozilla {

/**
 * This class will wrap a nsIRunnable and dispatch it to the main thread
 * synchronously. This is different from nsIEventTarget.DISPATCH_SYNC:
 * this class does not spin the event loop waiting for the event to be
 * dispatched. This means that you don't risk reentrance from pending
 * messages, but you must be sure that the target thread does not ever block
 * on this thread, or else you will deadlock.
 *
 * Typical usage:
 * nsRefPtr<SyncRunnable> sr = new SyncRunnable(new myrunnable...());
 * sr->DispatchToThread(t);
 *
 * We also provide a convenience wrapper:
 * SyncRunnable::DispatchToThread(new myrunnable...());
 *
 */
class SyncRunnable : public nsRunnable
{
public:
  SyncRunnable(nsIRunnable* r)
    : mRunnable(r)
    , mMonitor("SyncRunnable")
  { }

  void DispatchToThread(nsIEventTarget* thread)
  {
    nsresult rv;
    bool on;

    rv = thread->IsOnCurrentThread(&on);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    if (NS_SUCCEEDED(rv) && on) {
      mRunnable->Run();
      return;
    }

    mozilla::MonitorAutoLock lock(mMonitor);
    rv = thread->Dispatch(this, NS_DISPATCH_NORMAL);
    if (NS_SUCCEEDED(rv)) {
      lock.Wait();
    }
  }

  static void DispatchToThread(nsIEventTarget* thread,
                               nsIRunnable* r)
  {
    nsRefPtr<SyncRunnable> s(new SyncRunnable(r));
    s->DispatchToThread(thread);
  }

protected:
  NS_IMETHODIMP Run()
  {
    mRunnable->Run();
    mozilla::MonitorAutoLock(mMonitor).Notify();
    return NS_OK;
  }

private:
  nsCOMPtr<nsIRunnable> mRunnable;
  mozilla::Monitor mMonitor;
};

} // namespace mozilla

#endif // mozilla_SyncRunnable_h
