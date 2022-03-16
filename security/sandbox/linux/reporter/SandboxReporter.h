/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SandboxReporter_h
#define mozilla_SandboxReporter_h

#include "SandboxReporterCommon.h"

#include "base/platform_thread.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Mutex.h"
#include "mozilla/Types.h"
#include "mozilla/UniquePtr.h"
#include "nsTArray.h"

namespace mozilla {

// This object collects the SandboxReport messages from all of the
// child processes, submits them to Telemetry, and maintains a ring
// buffer of the last kSandboxReporterBufferSize reports.
class SandboxReporter final : public PlatformThread::Delegate {
 public:
  // For normal use, don't construct this directly; use the
  // Singleton() method.
  //
  // For unit testing, use this constructor followed by the Init
  // method; the object isn't usable unless Init returns true.
  explicit SandboxReporter();
  ~SandboxReporter();

  // See above; this method is not thread-safe.
  bool Init();

  // Used in GeckoChildProcessHost to connect the child process's
  // client to this report collector.
  void GetClientFileDescriptorMapping(int* aSrcFd, int* aDstFd) const;

  // A snapshot of the report ring buffer; element 0 of `mReports` is
  // the `mOffset`th report to be received, and so on.
  struct Snapshot {
    // The buffer has to fit in memory, but the total number of
    // reports received in the session can increase without bound and
    // could potentially overflow a uint32_t, so this is 64-bit.
    // (It's exposed to JS as a 53-bit int, effectively, but that
    // should also be large enough.)
    uint64_t mOffset;
    nsTArray<SandboxReport> mReports;
  };

  // Read the ring buffer contents; this method is thread-safe.
  Snapshot GetSnapshot();

  // Gets or creates the singleton report collector.  Crashes if
  // initialization fails (if a socketpair and/or thread can't be
  // created, there was almost certainly about to be a crash anyway).
  // Thread-safe as long as the pointer isn't used during/after XPCOM
  // shutdown.
  static SandboxReporter* Singleton();

 private:
  // These are constant over the life of the object:
  int mClientFd;
  int mServerFd;
  PlatformThreadHandle mThread;

  Mutex mMutex MOZ_UNANNOTATED;
  // These are protected by mMutex:
  UniquePtr<SandboxReport[]> mBuffer;
  uint64_t mCount;

  static StaticAutoPtr<SandboxReporter> sSingleton;

  void ThreadMain(void) override;
  void AddOne(const SandboxReport& aReport);
};

// This is a constant so the % operations can be optimized.  This is
// exposed in the header so that unit tests can see it.
static const size_t kSandboxReporterBufferSize = 32;

}  // namespace mozilla

#endif  // mozilla_SandboxReporter_h
