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

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

 private:
  class RacyRegisteredThread mRacyRegisteredThread;
};

#endif  // RegisteredThread_h
