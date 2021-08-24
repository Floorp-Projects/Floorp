/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef RegisteredThread_h
#define RegisteredThread_h

#include "platform.h"

#include "mozilla/NotNull.h"
#include "mozilla/ProfilerThreadRegistration.h"
#include "mozilla/RefPtr.h"
#include "nsIEventTarget.h"
#include "nsIThread.h"

class ProfilingStack;

// This class contains the state for a single thread that is accessible without
// protection from gPSMutex in platform.cpp. Because there is no external
// protection against data races, it must provide internal protection. Hence
// the "Racy" prefix.
//
class RacyRegisteredThread final {
 public:
  explicit RacyRegisteredThread(
      mozilla::profiler::ThreadRegistration& aThreadRegistration);

  MOZ_COUNTED_DTOR(RacyRegisteredThread)

  void SetIsBeingProfiled(bool aIsBeingProfiled) {
    mThreadRegistration.mData.mIsBeingProfiled = aIsBeingProfiled;
  }

  // This is called on every profiler restart. Put things that should happen at
  // that time here.
  void ReinitializeOnResume() {
    mThreadRegistration.mData.ReinitializeOnResume();
  }

  class ProfilingStack& ProfilingStack() {
    return mThreadRegistration.mData.ProfilingStackRef();
  }
  const class ProfilingStack& ProfilingStack() const {
    return mThreadRegistration.mData.ProfilingStackCRef();
  }

  mozilla::profiler::ThreadRegistration& mThreadRegistration;
};

// This class contains information that's relevant to a single thread only
// while that thread is running and registered with the profiler, but
// regardless of whether the profiler is running. All accesses to it are
// protected by the profiler state lock.
class RegisteredThread final {
 public:
  explicit RegisteredThread(
      mozilla::profiler::ThreadRegistration& aThreadRegistration);
  ~RegisteredThread();

  class RacyRegisteredThread& RacyRegisteredThread() {
    return mRacyRegisteredThread;
  }
  const class RacyRegisteredThread& RacyRegisteredThread() const {
    return mRacyRegisteredThread;
  }

  PlatformData* GetPlatformData() const { return mPlatformData.get(); }

  // aDelay is the time the event that is currently running on the thread
  // was queued before starting to run (if a PrioritizedEventQueue
  // (i.e. MainThread), this will be 0 for any event at a lower priority
  // than Input).
  // aRunning is the time the event has been running.  If no event is
  // running these will both be TimeDuration() (i.e. 0).  Both are out
  // params, and are always set.  Their initial value is discarded.
  void GetRunningEventDelay(const mozilla::TimeStamp& aNow,
                            mozilla::TimeDuration& aDelay,
                            mozilla::TimeDuration& aRunning);

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  // Set the JSContext of the thread to be sampled. Sampling cannot begin until
  // this has been set.
  void SetJSContext(JSContext* aContext);

  void ClearJSContext() {
    // This function runs on-thread.
    mRacyRegisteredThread.mThreadRegistration.mData.mJSContext = nullptr;
  }

  JSContext* GetJSContext() const {
    return mRacyRegisteredThread.mThreadRegistration.mData.mJSContext;
  }

  const mozilla::profiler::ThreadRegistrationInfo& Info() const {
    return mRacyRegisteredThread.mThreadRegistration.mData.mInfo;
  }
  nsCOMPtr<nsIEventTarget> GetEventTarget() const {
    return mRacyRegisteredThread.mThreadRegistration.mData.mThread;
  }
  void ResetMainThread(nsIThread* aThread) {
    mRacyRegisteredThread.mThreadRegistration.mData.mThread = aThread;
  }

  // Request that this thread start JS sampling. JS sampling won't actually
  // start until a subsequent PollJSSampling() call occurs *and* mContext has
  // been set.
  void StartJSSampling(uint32_t aJSFlags) {
    mRacyRegisteredThread.mThreadRegistration.mData.StartJSSampling(aJSFlags);
  }

  // Request that this thread stop JS sampling. JS sampling won't actually stop
  // until a subsequent PollJSSampling() call occurs.
  void StopJSSampling() {
    mRacyRegisteredThread.mThreadRegistration.mData.StopJSSampling();
  }

  // Poll to see if JS sampling should be started/stopped.
  void PollJSSampling();

 private:
  class RacyRegisteredThread mRacyRegisteredThread;

  const UniquePlatformData mPlatformData;
};

#endif  // RegisteredThread_h
