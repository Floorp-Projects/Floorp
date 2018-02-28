/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ProfiledThreadData_h
#define ProfiledThreadData_h

#include "mozilla/NotNull.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtrExtensions.h"

#include "js/ProfilingStack.h"
#include "platform.h"
#include "ProfileBuffer.h"
#include "ThreadInfo.h"

// Contains data for partial profiles that get saved when
// ThreadInfo::FlushSamplesAndMarkers gets called.
struct PartialThreadProfile final
{
  PartialThreadProfile(mozilla::UniquePtr<char[]>&& aSamplesJSON,
                       mozilla::UniquePtr<char[]>&& aMarkersJSON,
                       mozilla::UniquePtr<UniqueStacks>&& aUniqueStacks)
    : mSamplesJSON(mozilla::Move(aSamplesJSON))
    , mMarkersJSON(mozilla::Move(aMarkersJSON))
    , mUniqueStacks(mozilla::Move(aUniqueStacks))
  {}

  mozilla::UniquePtr<char[]> mSamplesJSON;
  mozilla::UniquePtr<char[]> mMarkersJSON;
  mozilla::UniquePtr<UniqueStacks> mUniqueStacks;
};

// This class contains information about a thread that is only relevant while
// the profiler is running, for any threads (both alive and dead) whose thread
// name matches the "thread filter" in the current profiler run.
// ProfiledThreadData objects may be kept alive even after the thread is
// unregistered, as long as there is still data for that thread in the profiler
// buffer.
//
// Accesses to this class are protected by the profiler state lock.
//
// Created as soon as the following are true for the thread:
//  - The profiler is running, and
//  - the thread matches the profiler's thread filter, and
//  - the thread is registered with the profiler.
// So it gets created in response to either (1) the profiler being started (for
// an existing registered thread) or (2) the thread being registered (if the
// profiler is already running).
//
// The thread may be unregistered during the lifetime of ProfiledThreadData.
// If that happens, NotifyUnregistered() is called.
//
// This class is the right place to store buffer positions. Profiler buffer
// positions become invalid if the profiler buffer is destroyed, which happens
// when the profiler is stopped.
class ProfiledThreadData final
{
public:
  ProfiledThreadData(ThreadInfo* aThreadInfo, nsIEventTarget* aEventTarget);
  ~ProfiledThreadData();

  void NotifyUnregistered(uint64_t aBufferPosition)
  {
    mResponsiveness.reset();
    mLastSample = mozilla::Nothing();
    mUnregisterTime = TimeStamp::Now();
    mBufferPositionWhenUnregistered = mozilla::Some(aBufferPosition);
  }
  mozilla::Maybe<uint64_t> BufferPositionWhenUnregistered() { return mBufferPositionWhenUnregistered; }

  mozilla::Maybe<uint64_t>& LastSample() { return mLastSample; }

  void StreamJSON(const ProfileBuffer& aBuffer, JSContext* aCx,
                  SpliceableJSONWriter& aWriter,
                  const mozilla::TimeStamp& aProcessStartTime,
                  double aSinceTime);

  // Call this method when the JS entries inside the buffer are about to
  // become invalid, i.e., just before JS shutdown.
  void FlushSamplesAndMarkers(JSContext* aCx,
                              const mozilla::TimeStamp& aProcessStartTime,
                              ProfileBuffer& aBuffer);

  // Returns nullptr if this is not the main thread or if this thread is not
  // being profiled.
  ThreadResponsiveness* GetThreadResponsiveness()
  {
    ThreadResponsiveness* responsiveness = mResponsiveness.ptrOr(nullptr);
    return responsiveness;
  }

  const RefPtr<ThreadInfo> Info() const { return mThreadInfo; }

private:
  // Group A:
  // The following fields are interesting for the entire lifetime of a
  // ProfiledThreadData object.

  // This thread's thread info.
  const RefPtr<ThreadInfo> mThreadInfo;

  // JS frames in the buffer may require a live JSRuntime to stream (e.g.,
  // stringifying JIT frames). In the case of JSRuntime destruction,
  // FlushSamplesAndMarkers should be called to save them. These are spliced
  // into the final stream.
  UniquePtr<PartialThreadProfile> mPartialProfile;

  // Group B:
  // The following fields are only used while this thread is alive and
  // registered. They become Nothing() once the thread is unregistered.

  // A helper object that instruments nsIThreads to obtain responsiveness
  // information about their event loop.
  mozilla::Maybe<ThreadResponsiveness> mResponsiveness;

  // When sampling, this holds the position in ActivePS::mBuffer of the most
  // recent sample for this thread, or Nothing() if there is no sample for this
  // thread in the buffer.
  mozilla::Maybe<uint64_t> mLastSample;

  // Group C:
  // The following fields are only used once this thread has been unregistered.

  mozilla::Maybe<uint64_t> mBufferPositionWhenUnregistered;
  mozilla::TimeStamp mUnregisterTime;
};

void
StreamSamplesAndMarkers(const char* aName, int aThreadId,
                        const ProfileBuffer& aBuffer,
                        SpliceableJSONWriter& aWriter,
                        const mozilla::TimeStamp& aProcessStartTime,
                        const TimeStamp& aRegisterTime,
                        const TimeStamp& aUnregisterTime,
                        double aSinceTime,
                        JSContext* aContext,
                        UniquePtr<char[]>&& aPartialSamplesJSON,
                        UniquePtr<char[]>&& aPartialMarkersJSON,
                        UniqueStacks& aUniqueStacks);

#endif  // ProfiledThreadData_h
