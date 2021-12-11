/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SyncRunnable_h
#define mozilla_SyncRunnable_h

#include <utility>

#include "mozilla/AbstractThread.h"
#include "mozilla/Monitor.h"
#include "mozilla/dom/JSExecutionManager.h"
#include "nsThreadUtils.h"

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
 * RefPtr<SyncRunnable> sr = new SyncRunnable(new myrunnable...());
 * sr->DispatchToThread(t);
 *
 * We also provide a convenience wrapper:
 * SyncRunnable::DispatchToThread(new myrunnable...());
 *
 */
class SyncRunnable : public Runnable {
 public:
  explicit SyncRunnable(nsIRunnable* aRunnable)
      : Runnable("SyncRunnable"),
        mRunnable(aRunnable),
        mMonitor("SyncRunnable"),
        mDone(false) {}

  explicit SyncRunnable(already_AddRefed<nsIRunnable> aRunnable)
      : Runnable("SyncRunnable"),
        mRunnable(std::move(aRunnable)),
        mMonitor("SyncRunnable"),
        mDone(false) {}

  nsresult DispatchToThread(nsIEventTarget* aThread,
                            bool aForceDispatch = false) {
    nsresult rv;
    bool on;

    if (!aForceDispatch) {
      rv = aThread->IsOnCurrentThread(&on);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
      if (NS_SUCCEEDED(rv) && on) {
        mRunnable->Run();
        return NS_OK;
      }
    }

    rv = aThread->Dispatch(this, NS_DISPATCH_NORMAL);
    if (NS_SUCCEEDED(rv)) {
      mozilla::MonitorAutoLock lock(mMonitor);
      // This could be synchronously dispatching to a thread currently waiting
      // for JS execution clearance. Yield JS execution.
      dom::AutoYieldJSThreadExecution yield;

      while (!mDone) {
        lock.Wait();
      }
    }
    return rv;
  }

  nsresult DispatchToThread(AbstractThread* aThread,
                            bool aForceDispatch = false) {
    if (!aForceDispatch && aThread->IsCurrentThreadIn()) {
      mRunnable->Run();
      return NS_OK;
    }

    // Check we don't have tail dispatching here. Otherwise we will deadlock
    // ourself when spinning the loop below.
    MOZ_ASSERT(!aThread->RequiresTailDispatchFromCurrentThread());

    nsresult rv = aThread->Dispatch(RefPtr<nsIRunnable>(this).forget());
    if (NS_SUCCEEDED(rv)) {
      mozilla::MonitorAutoLock lock(mMonitor);
      while (!mDone) {
        lock.Wait();
      }
    }
    return rv;
  }

  static nsresult DispatchToThread(nsIEventTarget* aThread,
                                   nsIRunnable* aRunnable,
                                   bool aForceDispatch = false) {
    RefPtr<SyncRunnable> s(new SyncRunnable(aRunnable));
    return s->DispatchToThread(aThread, aForceDispatch);
  }

  static nsresult DispatchToThread(AbstractThread* aThread,
                                   nsIRunnable* aRunnable,
                                   bool aForceDispatch = false) {
    RefPtr<SyncRunnable> s(new SyncRunnable(aRunnable));
    return s->DispatchToThread(aThread, aForceDispatch);
  }

 protected:
  NS_IMETHOD Run() override {
    mRunnable->Run();

    mozilla::MonitorAutoLock lock(mMonitor);
    MOZ_ASSERT(!mDone);

    mDone = true;
    mMonitor.Notify();

    return NS_OK;
  }

 private:
  nsCOMPtr<nsIRunnable> mRunnable;
  mozilla::Monitor mMonitor;
  bool mDone;
};

}  // namespace mozilla

#endif  // mozilla_SyncRunnable_h
