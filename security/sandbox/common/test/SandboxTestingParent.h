/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SandboxTestingParent_h
#define mozilla_SandboxTestingParent_h

#include "mozilla/PSandboxTestingParent.h"
#include "mozilla/Monitor.h"
#include "mozilla/UniquePtr.h"

#if !defined(MOZ_SANDBOX) || !defined(MOZ_DEBUG) || !defined(ENABLE_TESTS)
#  error "This file should not be used outside of debug with tests"
#endif

namespace mozilla {

class SandboxTestingThread;

class SandboxTestingParent : public PSandboxTestingParent {
 public:
  static SandboxTestingParent* Create(
      Endpoint<PSandboxTestingParent>&& aParentEnd);
  static void Destroy(SandboxTestingParent* aInstance);

  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvReportTestResults(const nsCString& testName,
                                                bool shouldSucceed,
                                                bool didSucceed,
                                                const nsCString& resultMessage);
  mozilla::ipc::IPCResult RecvTestCompleted();

 private:
  explicit SandboxTestingParent(SandboxTestingThread* aThread,
                                Endpoint<PSandboxTestingParent>&& aParentEnd);
  virtual ~SandboxTestingParent() = default;
  void ShutdownSandboxTestThread();
  void Bind(Endpoint<PSandboxTestingParent>&& aEnd);

  UniquePtr<SandboxTestingThread> mThread;
  Monitor mMonitor;
  bool mShutdownDone;
};

}  // namespace mozilla

#endif  // mozilla_SandboxTestingParent_h
