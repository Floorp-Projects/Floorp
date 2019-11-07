/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ThreadResponsiveness.h"

#include "platform.h"

#include "mozilla/Atomics.h"
#include "mozilla/SystemGroup.h"
#include "nsITimer.h"

using namespace mozilla;

class CheckResponsivenessTask : public CancelableRunnable,
                                public nsITimerCallback {
 public:
  explicit CheckResponsivenessTask(nsIEventTarget* aThread, bool aIsMainThread)
      : CancelableRunnable("CheckResponsivenessTask"),
        mStartToPrevTracer_us(uint64_t(profiler_time() * 1000.0)),
        mStop(false),
        mHasEverBeenSuccessfullyDispatched(false),
        mThread(aThread),
        mIsMainThread(aIsMainThread) {}

 protected:
  ~CheckResponsivenessTask() {}

 public:
  // Must be called from the same thread every time. Call that the update
  // thread, because it's the thread that ThreadResponsiveness::Update() is
  // called on. In reality it's the profiler's sampler thread.
  bool DoFirstDispatchIfNeeded() {
    if (mHasEverBeenSuccessfullyDispatched) {
      return true;
    }

    // The profiler for the main thread is set up before the thread manager is,
    // meaning we can't get the nsIThread when the CheckResponsivenessTask is
    // constructed. We _do_ know whether it is the main thread at that time,
    // however, so here's the workaround. We can still hit this code before the
    // thread manager is initted, in which case we won't try to record
    // responsiveness, which is fine because there's no event queue to check
    // responsiveness on anyway.
    if (mIsMainThread) {
      if (!mThread) {
        nsCOMPtr<nsIThread> temp;
        NS_GetMainThread(getter_AddRefs(temp));
        mThread = temp.forget();
      }

      if (mThread) {
        nsresult rv =
            SystemGroup::Dispatch(TaskCategory::Other, do_AddRef(this));
        if (NS_SUCCEEDED(rv)) {
          mHasEverBeenSuccessfullyDispatched = true;
        }
      }
    } else if (mThread) {
      nsresult rv = mThread->Dispatch(this, nsIThread::NS_DISPATCH_NORMAL);
      if (NS_SUCCEEDED(rv)) {
        mHasEverBeenSuccessfullyDispatched = true;
      }
    }

    return mHasEverBeenSuccessfullyDispatched;
  }

  nsresult Cancel() override {
    // No special work needed.
    return NS_OK;
  }

  // Only runs on the thread being profiled. Always called via a thread
  // dispatch, so inherently functions as a responsiveness statistic.
  NS_IMETHOD Run() override {
    // This approach means that the 16ms delay in the timer below, _plus_ any
    // additional delays in the TimerThread itself, become part of the
    // responsiveness statistic for this thread. What we should probably be
    // doing is recording responsiveness only when we have dispatched (but not
    // executed) a call to this function, either because of a call to
    // DoFirstDispatchIfNeeded, or a call to Notify.
    mStartToPrevTracer_us = uint64_t(profiler_time() * 1000.0);

    if (!mStop) {
      if (!mTimer) {
        if (mIsMainThread) {
          mTimer =
              NS_NewTimer(SystemGroup::EventTargetFor(TaskCategory::Other));
        } else {
          mTimer = NS_NewTimer();
        }
      }
      mTimer->InitWithCallback(this, 16, nsITimer::TYPE_ONE_SHOT);
    }

    return NS_OK;
  }

  // Should always fire on the thread being profiled
  NS_IMETHOD Notify(nsITimer* aTimer) final {
    Run();
    return NS_OK;
  }

  // Can be called on any thread.
  void Terminate() { mStop = true; }

  // Can be called on any thread.
  double GetStartToPrevTracer_ms() const {
    return mStartToPrevTracer_us / 1000.0;
  }

  NS_DECL_ISUPPORTS_INHERITED

 private:
  // The timer that's responsible for redispatching this event to the thread we
  // are profiling (ie; mThread). Only touched on mThread.
  nsCOMPtr<nsITimer> mTimer;

  // The time (in integer microseconds since process startup) at which this
  // event was last processed (Run() was last called).
  // This field is written on mThread and read on the update thread.
  // This is stored as integer microseconds instead of double milliseconds
  // because Atomic<double> is not available.
  Atomic<uint64_t> mStartToPrevTracer_us;

  // Whether we should stop redispatching this event once the timer fires the
  // next time. Set to true by any thread when the profiler is stopped; read on
  // mThread.
  Atomic<bool> mStop;

  // Only accessed on the update thread.
  bool mHasEverBeenSuccessfullyDispatched;

  // The thread that we're profiling. Use nsIEventTarget to allow for checking
  // responsiveness on non-nsIThreads someday.
  nsCOMPtr<nsIEventTarget> mThread;
  bool mIsMainThread;
};

NS_IMPL_ISUPPORTS_INHERITED(CheckResponsivenessTask, CancelableRunnable,
                            nsITimerCallback)

ThreadResponsiveness::ThreadResponsiveness(nsIEventTarget* aThread,
                                           bool aIsMainThread)
    : mActiveTracerEvent(new CheckResponsivenessTask(aThread, aIsMainThread)) {
  MOZ_COUNT_CTOR(ThreadResponsiveness);
}

ThreadResponsiveness::~ThreadResponsiveness() {
  MOZ_COUNT_DTOR(ThreadResponsiveness);
  mActiveTracerEvent->Terminate();
}

void ThreadResponsiveness::Update() {
  if (!mActiveTracerEvent->DoFirstDispatchIfNeeded()) {
    return;
  }
  mStartToPrevTracer_ms = Some(mActiveTracerEvent->GetStartToPrevTracer_ms());
}
