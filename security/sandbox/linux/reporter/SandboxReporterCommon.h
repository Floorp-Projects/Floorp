/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SandboxReporterCommon_h
#define mozilla_SandboxReporterCommon_h

#include "mozilla/IntegerTypeTraits.h"
#include "mozilla/Types.h"

#include <sys/types.h>

// Note: this is also used in libmozsandbox, so dependencies on
// symbols from libxul probably won't work.

namespace mozilla {
static const size_t kSandboxSyscallArguments = 6;
// fds 0-2: stdio; fd 3: IPC; fd 4: crash reporter.  (The IPC child
// process launching code will check that we don't try to use the same
// fd twice.)
static const int kSandboxReporterFileDesc = 5;

// This struct represents a system call that was rejected by a
// seccomp-bpf policy.
struct SandboxReport {
  // In the future this may include finer distinctions than
  // GeckoProcessType -- e.g., whether a content process can load
  // file:/// URLs, or if it's reserved for content with certain
  // user-granted permissions.
  enum class ProcType : uint8_t {
    CONTENT,
    FILE,
    MEDIA_PLUGIN,
    RDD,
    SOCKET_PROCESS,
    UTILITY,
  };

  // The syscall number and arguments are usually `unsigned long`, but
  // that causes ambiguous overload errors with nsACString::AppendInt.
  using ULong = UnsignedStdintTypeForSize<sizeof(unsigned long)>::Type;

  // This time uses CLOCK_MONOTONIC_COARSE.  Displaying or reporting
  // it should usually be done relative to the current value of that
  // clock (or the time at some other event of interest, like a
  // subsequent crash).
  struct timespec mTime;

  // The pid/tid values, like every other field in this struct, aren't
  // authenticated and a compromised process could send anything, so
  // use the values with caution.
  pid_t mPid;
  pid_t mTid;
  ProcType mProcType;
  ULong mSyscall;
  ULong mArgs[kSandboxSyscallArguments];

  SandboxReport() : mPid(0) {}
  bool IsValid() const { return mPid > 0; }
};

}  // namespace mozilla

#endif  // mozilla_SandboxReporterCommon_h
