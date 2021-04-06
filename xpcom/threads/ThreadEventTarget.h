/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ThreadEventTarget_h
#define mozilla_ThreadEventTarget_h

#include "mozilla/MemoryReporting.h"
#include "mozilla/Mutex.h"
#include "mozilla/SynchronizedEventQueue.h"  // for ThreadTargetSink
#include "nsIDelayedRunnableObserver.h"
#include "nsISerialEventTarget.h"

namespace mozilla {
class DelayedRunnable;

// ThreadEventTarget handles the details of posting an event to a thread. It can
// be used with any ThreadTargetSink implementation.
class ThreadEventTarget final : public nsISerialEventTarget,
                                public nsIDelayedRunnableObserver {
 public:
  ThreadEventTarget(ThreadTargetSink* aSink, bool aIsMainThread);

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIEVENTTARGET_FULL

  // nsIDelayedRunnableObserver
  void OnDelayedRunnableCreated(DelayedRunnable* aRunnable) override;
  void OnDelayedRunnableScheduled(DelayedRunnable* aRunnable) override;
  void OnDelayedRunnableRan(DelayedRunnable* aRunnable) override;

  // Notification from, and on, the owner thread that it is shutting down.
  // Cancels any scheduled DelayedRunnables.
  void NotifyShutdown();

  // Disconnects the target so that it can no longer post events.
  void Disconnect(const MutexAutoLock& aProofOfLock) {
    mSink->Disconnect(aProofOfLock);
  }

  // Sets the thread for which IsOnCurrentThread returns true to the current
  // thread.
  void SetCurrentThread(PRThread* aThread);
  // Call ClearCurrentThread() before the PRThread is deleted on thread join.
  void ClearCurrentThread();

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const {
    size_t n = 0;
    if (mSink) {
      n += mSink->SizeOfIncludingThis(aMallocSizeOf);
    }
    return aMallocSizeOf(this) + n;
  }

 private:
  ~ThreadEventTarget();

  RefPtr<ThreadTargetSink> mSink;
  bool mIsMainThread;

  // DelayedRunnables (from DelayedDispatch) that are managed by their
  // respective timers, but have not yet run. Accessed only on this nsThread.
  nsTArray<RefPtr<mozilla::DelayedRunnable>> mScheduledDelayedRunnables;
};

}  // namespace mozilla

#endif  // mozilla_ThreadEventTarget_h
