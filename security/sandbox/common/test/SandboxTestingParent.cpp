/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "SandboxTestingParent.h"
#include "SandboxTestingThread.h"

namespace mozilla {

/* static */
SandboxTestingParent* SandboxTestingParent::Create(
    Endpoint<PSandboxTestingParent>&& aParentEnd) {
  SandboxTestingThread* thread = SandboxTestingThread::Create();
  if (!thread) {
    return nullptr;
  }
  return new SandboxTestingParent(thread, std::move(aParentEnd));
}

SandboxTestingParent::SandboxTestingParent(
    SandboxTestingThread* aThread, Endpoint<PSandboxTestingParent>&& aParentEnd)
    : mThread(aThread),
      mMonitor("SandboxTestingParent Lock"),
      mShutdownDone(false) {
  MOZ_ASSERT(mThread);
  mThread->Dispatch(
      NewNonOwningRunnableMethod<Endpoint<PSandboxTestingParent>&&>(
          "SandboxTestingParent::Bind", this, &SandboxTestingParent::Bind,
          std::move(aParentEnd)));
}

void SandboxTestingParent::Bind(Endpoint<PSandboxTestingParent>&& aEnd) {
  MOZ_RELEASE_ASSERT(mThread->IsOnThread());
  DebugOnly<bool> ok = aEnd.Bind(this);
  MOZ_ASSERT(ok);
}

void SandboxTestingParent::ShutdownSandboxTestThread() {
  MOZ_ASSERT(mThread->IsOnThread());
  Close();
  // Notify waiting thread that we are done.
  MonitorAutoLock lock(mMonitor);
  mShutdownDone = true;
  mMonitor.Notify();
}

void SandboxTestingParent::Destroy(SandboxTestingParent* aInstance) {
  MOZ_ASSERT(NS_IsMainThread());
  if (!aInstance) {
    return;
  }

  {
    // Hold the lock while we destroy the actor on the test thread.
    MonitorAutoLock lock(aInstance->mMonitor);
    aInstance->mThread->Dispatch(NewNonOwningRunnableMethod(
        "SandboxTestingParent::ShutdownSandboxTestThread", aInstance,
        &SandboxTestingParent::ShutdownSandboxTestThread));

    // Wait for test thread to complete destruction.
    while (!aInstance->mShutdownDone) {
      aInstance->mMonitor.Wait();
    }
  }

  delete aInstance;
}

void SandboxTestingParent::ActorDestroy(ActorDestroyReason aWhy) {
  MOZ_RELEASE_ASSERT(mThread->IsOnThread());
}

mozilla::ipc::IPCResult SandboxTestingParent::RecvReportTestResults(
    const nsCString& testName, bool shouldSucceed, bool didSucceed,
    const nsCString& resultMessage) {
  NS_DispatchToMainThread(
      NS_NewRunnableFunction("SandboxReportTestResults", [=]() {
        nsCOMPtr<nsIObserverService> observerService =
            mozilla::services::GetObserverService();
        MOZ_RELEASE_ASSERT(observerService);
        const char* kFmt =
            "{ \"testid\" : \"%s\", \"shouldPermit\" : %s, "
            "\"wasPermitted\" : %s, \"message\" : \"%s\" }";
        nsString json;
        json.AppendPrintf(
            kFmt, testName.BeginReading(), shouldSucceed ? "true" : "false",
            didSucceed ? "true" : "false", resultMessage.BeginReading());
        observerService->NotifyObservers(nullptr, "sandbox-test-result",
                                         json.BeginReading());
      }));
  return IPC_OK();
}

mozilla::ipc::IPCResult SandboxTestingParent::RecvTestCompleted() {
  Unused << SendShutDown();
  NS_DispatchToMainThread(
      NS_NewRunnableFunction("SandboxReportTestResults", []() {
        nsCOMPtr<nsIObserverService> observerService =
            mozilla::services::GetObserverService();
        MOZ_RELEASE_ASSERT(observerService);
        observerService->NotifyObservers(nullptr, "sandbox-test-done", 0);
      }));
  return IPC_OK();
}

}  // namespace mozilla
