/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ThreadResponsiveness.h"
#include "platform.h"
#include "nsComponentManagerUtils.h"
#include "nsThreadUtils.h"
#include "nsITimer.h"
#include "mozilla/Mutex.h"

using mozilla::Mutex;
using mozilla::MutexAutoLock;
using mozilla::TimeStamp;

class CheckResponsivenessTask : public mozilla::Runnable,
                                public nsITimerCallback {
public:
  CheckResponsivenessTask()
    : mLastTracerTime(TimeStamp::Now())
    , mMutex("CheckResponsivenessTask")
    , mTimer(nullptr)
    , mHasEverBeenSuccessfullyDispatched(false)
    , mStop(false)
  {
  }

protected:
  ~CheckResponsivenessTask()
  {
  }

public:

  // Must be called from the same thread every time. Call that the "update"
  // thread, because it's the thread that ThreadResponsiveness::Update() is
  // called on. In reality it's the profiler's sampler thread.
  void DoFirstDispatchIfNeeded()
  {
    if (mHasEverBeenSuccessfullyDispatched) {
      return;
    }

    // Dispatching can fail during early startup, particularly when
    // MOZ_PROFILER_STARTUP is used.
    nsresult rv = NS_DispatchToMainThread(this);
    if (NS_SUCCEEDED(rv)) {
      mHasEverBeenSuccessfullyDispatched = true;
    }
  }

  // Can only run on the main thread.
  NS_IMETHOD Run() override
  {
    MutexAutoLock mon(mMutex);
    if (mStop)
      return NS_OK;

    // This is raced on because we might pause the thread here
    // for profiling so if we tried to use a monitor to protect
    // mLastTracerTime we could deadlock. We're risking seeing
    // a partial write which will show up as an outlier in our
    // performance data.
    mLastTracerTime = TimeStamp::Now();
    if (!mTimer) {
      mTimer = do_CreateInstance("@mozilla.org/timer;1");
    }
    mTimer->InitWithCallback(this, 16, nsITimer::TYPE_ONE_SHOT);

    return NS_OK;
  }

  NS_IMETHOD Notify(nsITimer* aTimer) final
  {
    NS_DispatchToMainThread(this);
    return NS_OK;
  }

  void Terminate() {
    MutexAutoLock mon(mMutex);
    mStop = true;
  }

  const TimeStamp& GetLastTracerTime() const {
    return mLastTracerTime;
  }

  NS_DECL_ISUPPORTS_INHERITED

private:
  TimeStamp mLastTracerTime;
  Mutex mMutex;
  nsCOMPtr<nsITimer> mTimer;
  bool mHasEverBeenSuccessfullyDispatched; // only accessed on the "update" thread
  bool mStop;
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
  mLastTracerTime = mActiveTracerEvent->GetLastTracerTime();
}

