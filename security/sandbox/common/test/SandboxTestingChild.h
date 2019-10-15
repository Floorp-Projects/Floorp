/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SandboxTestingChild_h
#define mozilla_SandboxTestingChild_h

#include "mozilla/PSandboxTestingChild.h"
#include "mozilla/Monitor.h"

#if !defined(MOZ_SANDBOX) || !defined(MOZ_DEBUG) || !defined(ENABLE_TESTS)
#  error "This file should not be used outside of debug with tests"
#endif

namespace mozilla {

class SandboxTestingThread;

/**
 * Runs tests that check sandbox in child process, depending on process type.
 */
class SandboxTestingChild : public PSandboxTestingChild {
 public:
  static bool Initialize(
      Endpoint<PSandboxTestingChild>&& aSandboxTestingEndpoint);
  static SandboxTestingChild* GetInstance();
  static void Destroy();

  bool IsTestThread();
  void PostToTestThread(already_AddRefed<nsIRunnable>&& runnable);

  void ActorDestroy(ActorDestroyReason aWhy) override;

  virtual bool RecvShutDown();

 private:
  explicit SandboxTestingChild(SandboxTestingThread* aThread,
                               Endpoint<PSandboxTestingChild>&& aEndpoint);
  void Bind(Endpoint<PSandboxTestingChild>&& aEndpoint);

  nsAutoPtr<SandboxTestingThread> mThread;

  static SandboxTestingChild* sInstance;
};

}  // namespace mozilla

#endif  // mozilla_SandboxTestingChild_h
