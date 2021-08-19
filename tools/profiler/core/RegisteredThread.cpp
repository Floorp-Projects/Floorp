/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RegisteredThread.h"

#include "mozilla/ProfilerMarkers.h"
#include "js/AllocationRecording.h"
#include "js/ProfilingStack.h"
#include "js/TraceLoggerAPI.h"
#include "mozilla/ProfilerThreadRegistrationData.h"

RacyRegisteredThread::RacyRegisteredThread(
    mozilla::profiler::ThreadRegistration& aThreadRegistration)
    : mThreadRegistration(aThreadRegistration) {
  MOZ_COUNT_CTOR(RacyRegisteredThread);
}

RegisteredThread::RegisteredThread(
    mozilla::profiler::ThreadRegistration& aThreadRegistration)
    : mRacyRegisteredThread(aThreadRegistration) {
  MOZ_COUNT_CTOR(RegisteredThread);

  // NOTE: aThread can be null for the first thread, before the ThreadManager
  // is initialized.
}

RegisteredThread::~RegisteredThread() { MOZ_COUNT_DTOR(RegisteredThread); }

size_t RegisteredThread::SizeOfIncludingThis(
    mozilla::MallocSizeOf aMallocSizeOf) const {
  size_t n = aMallocSizeOf(this);

  // The following members are not measured:
  // - mThreadInfo: because it is non-owning

  return n;
}

void RegisteredThread::SetJSContext(JSContext* aContext) {
  mRacyRegisteredThread.mThreadRegistration.mData.SetJSContext(aContext);
}

void RegisteredThread::PollJSSampling() {
  mRacyRegisteredThread.mThreadRegistration.mData.PollJSSampling();
}

const RacyRegisteredThread& mozilla::profiler::
    ThreadRegistrationUnlockedConstReader::RacyRegisteredThreadCRef() const {
  MOZ_ASSERT(mRegisteredThread);
  return mRegisteredThread->RacyRegisteredThread();
}

RacyRegisteredThread&
mozilla::profiler::ThreadRegistrationUnlockedConstReaderAndAtomicRW::
    RacyRegisteredThreadRef() {
  MOZ_ASSERT(mRegisteredThread);
  return mRegisteredThread->RacyRegisteredThread();
}

RegisteredThread& mozilla::profiler::ThreadRegistrationLockedRWFromAnyThread::
    RegisteredThreadRef() {
  MOZ_ASSERT(mRegisteredThread);
  return *mRegisteredThread;
}

void mozilla::profiler::ThreadRegistrationLockedRWOnThread::SetRegisteredThread(
    RegisteredThread* aRegisteredThread) {
  mRegisteredThread = aRegisteredThread;
}
