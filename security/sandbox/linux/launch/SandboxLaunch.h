/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SandboxLaunch_h
#define mozilla_SandboxLaunch_h

#include "base/process_util.h"
#include "mozilla/ipc/UtilityProcessSandboxing.h"
#include "nsXULAppAPI.h"
#include <vector>

namespace mozilla {

class SandboxLaunch final {
 public:
  SandboxLaunch();
  ~SandboxLaunch();

  SandboxLaunch(const SandboxLaunch&) = delete;
  SandboxLaunch& operator=(const SandboxLaunch&) = delete;

  using LaunchOptions = base::LaunchOptions;
  using SandboxingKind = ipc::SandboxingKind;

  // Decide what sandboxing features will be used for a process, and
  // modify `*aOptions` accordingly.  This does not allocate fds or
  // other OS resources (other than memory for strings).
  //
  // This is meant to be called in the parent process (even if the
  // fork server will be used), and if `aType` is Content then it must
  // be called on the main thread in order to access prefs.
  static void Configure(GeckoProcessType aType, SandboxingKind aKind,
                        LaunchOptions* aOptions);

  // Finish setting up for process launch, based on the information
  // from `Configure(...)`. Called in the process that will do the
  // launch (fork server if applicable, otherwise parent), and before
  // calling `FileDescriptorShuffle::Init`.
  //
  // This can allocate fds (owned by `*this`) and modify
  // `aOptions->fds_to_remap`, but does not access the
  // environment-related fields of `*aOptions`.
  bool Prepare(LaunchOptions* aOptions);

  // Launch the child process, similarly to `::fork()`; called after
  // `Configure` and `Prepare`.
  //
  // If launch-time sandboxing features are used, `pthread_atfork`
  // hooks are not currently supported in that case, and signal
  // handlers are reset in the child process.  If sandboxing is not
  // used, this is equivalent to `::fork()`.
  pid_t Fork();

 private:
  int mFlags;
  int mChrootServer;
  int mChrootClient;

  void StartChrootServer();
};

// This doesn't really belong in this header but it's used in both
// SandboxLaunch and SandboxBrokerPolicyFactory.
bool HasAtiDrivers();

}  // namespace mozilla

#endif  // mozilla_SandboxLaunch_h
