/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
#include "SandboxTestingChild.h"
#include "SandboxTestingThread.h"

namespace mozilla {

SandboxTestingChild* SandboxTestingChild::sInstance = nullptr;

bool SandboxTestingChild::IsTestThread() { return mThread->IsOnThread(); }

void SandboxTestingChild::PostToTestThread(
    already_AddRefed<nsIRunnable>&& runnable) {
  mThread->Dispatch(std::move(runnable));
}

/* static */
bool SandboxTestingChild::Initialize(
    Endpoint<PSandboxTestingChild>&& aSandboxTestingEndpoint) {
  MOZ_ASSERT(!sInstance);
  SandboxTestingThread* thread = SandboxTestingThread::Create();
  if (!thread) {
    return false;
  }
  sInstance =
      new SandboxTestingChild(thread, std::move(aSandboxTestingEndpoint));
  return true;
}

/* static */
SandboxTestingChild* SandboxTestingChild::GetInstance() {
  MOZ_ASSERT(sInstance, "Must initialize SandboxTestingChild before using it");
  return sInstance;
}

SandboxTestingChild::SandboxTestingChild(
    SandboxTestingThread* aThread, Endpoint<PSandboxTestingChild>&& aEndpoint)
    : mThread(aThread) {
  MOZ_ASSERT(aThread);
  PostToTestThread(NewNonOwningRunnableMethod<Endpoint<PSandboxTestingChild>&&>(
      "SandboxTestingChild::Bind", this, &SandboxTestingChild::Bind,
      std::move(aEndpoint)));
}

void SandboxTestingChild::Bind(Endpoint<PSandboxTestingChild>&& aEndpoint) {
  MOZ_RELEASE_ASSERT(mThread->IsOnThread());
  DebugOnly<bool> ok = aEndpoint.Bind(this);
  MOZ_ASSERT(ok);

  // Placeholder usage of the APIs needed to report test results.
  // This will be fleshed out with tests that are OS and process type dependent.
  SendReportTestResults(nsCString("testId1"), true /* shouldSucceed */,
                        true /* didSucceed*/,
                        nsCString("These are some test results!"));
  // Tell SandboxTest that this process is done with all tests.
  SendTestCompleted();
}

void SandboxTestingChild::ActorDestroy(ActorDestroyReason aWhy) {
  MOZ_ASSERT(mThread->IsOnThread());
  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "SandboxChildDestroyer", []() { SandboxTestingChild::Destroy(); }));
}

void SandboxTestingChild::Destroy() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(sInstance);
  delete sInstance;
  sInstance = nullptr;
}

bool SandboxTestingChild::RecvShutDown() {
  Close();
  return true;
}

}  // namespace mozilla
