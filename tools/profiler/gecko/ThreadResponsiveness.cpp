/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ThreadResponsiveness.h"

#include "mozilla/Atomics.h"
#include "mozilla/SystemGroup.h"

#include "nsITimer.h"
#include "platform.h"

using namespace mozilla;

class CheckResponsivenessTask : public Runnable,
                                public nsITimerCallback {
public:
  CheckResponsivenessTask()
    : Runnable("CheckResponsivenessTask")
    , mStartToPrevTracer_us(uint64_t(profiler_time() * 1000.0))
    , mStop(false)
    , mHasEverBeenSuccessfullyDispatched(false)
  {
  }

protected:
  ~CheckResponsivenessTask()
  {
  }

public:

  // Must be called from the same thread every time. Call that the update
  // thread, because it's the thread that ThreadResponsiveness::Update() is
  // called on. In reality it's the profiler's sampler thread.
  void DoFirstDispatchIfNeeded()
  {
    if (mHasEverBeenSuccessfullyDispatched) {
      return;
    }

    // We can hit this code very early during startup, at a time where the
    // thread manager hasn't been initialized with the main thread yet.
    // In that case, calling SystemGroup::Dispatch would assert, so we make
    // sure that NS_GetMainThread succeeds before attempting to dispatch this
    // runnable.
    nsCOMPtr<nsIThread> mainThread;
    nsresult rv = NS_GetMainThread(getter_AddRefs(mainThread));
    if (NS_SUCCEEDED(rv) && mainThread) {
      rv = SystemGroup::Dispatch(TaskCategory::Other, do_AddRef(this));
      if (NS_SUCCEEDED(rv)) {
        mHasEverBeenSuccessfullyDispatched = true;
      }
    }
  }

  // Can only run on the main thread.
  NS_IMETHOD Run() override
  {
    mStartToPrevTracer_us = uint64_t(profiler_time() * 1000.0);

    if (!mStop) {
      if (!mTimer) {
        mTimer = NS_NewTimer(
          SystemGroup::EventTargetFor(TaskCategory::Other));
      }
      mTimer->InitWithCallback(this, 16, nsITimer::TYPE_ONE_SHOT);
    }

    return NS_OK;
  }

  // Main thread only
  NS_IMETHOD Notify(nsITimer* aTimer) final override
  {
    SystemGroup::Dispatch(TaskCategory::Other,
                          do_AddRef(this));
    return NS_OK;
  }

  // Can be called on any thread.
  void Terminate() {
    mStop = true;
  }

  // Can be called on any thread.
  double GetStartToPrevTracer_ms() const {
    return mStartToPrevTracer_us / 1000.0;
  }

  NS_DECL_ISUPPORTS_INHERITED

private:
  // The timer that's responsible for redispatching this event to the main
  // thread. This field is only accessed on the main thread.
  nsCOMPtr<nsITimer> mTimer;

  // The time (in integer microseconds since process startup) at which this
  // event was last processed (Run() was last called).
  // This field is written on the main thread and read on the update thread.
  // This is stored as integer microseconds instead of double milliseconds
  // because Atomic<double> is not available.
  Atomic<uint64_t> mStartToPrevTracer_us;

  // Whether we should stop redispatching this event once the timer fires the
  // next time. Set to true by any thread when the profiler is stopped; read on
  // the main thread.
  Atomic<bool> mStop;

  // Only accessed on the update thread.
  bool mHasEverBeenSuccessfullyDispatched;
};

NS_IMPL_ISUPPORTS_INHERITED(CheckResponsivenessTask, mozilla::Runnable,
                            nsITimerCallback)

ThreadResponsiveness::ThreadResponsiveness()
  : mActiveTracerEvent(new CheckResponsivenessTask())
{
  MOZ_COUNT_CTOR(ThreadResponsiveness);
}

ThreadResponsiveness::~ThreadResponsiveness()
{
  MOZ_COUNT_DTOR(ThreadResponsiveness);
  mActiveTracerEvent->Terminate();
}

void
ThreadResponsiveness::Update()
{
  mActiveTracerEvent->DoFirstDispatchIfNeeded();
  mStartToPrevTracer_ms = Some(mActiveTracerEvent->GetStartToPrevTracer_ms());
}

