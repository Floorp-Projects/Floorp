/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "SandboxTestingParent.h"
#include "SandboxTestingThread.h"
#include "nsIObserverService.h"
#include "mozilla/ipc/Endpoint.h"
#include "mozilla/Services.h"
#include "mozilla/SyncRunnable.h"
#include "nsDirectoryServiceUtils.h"

namespace mozilla {

/* static */
already_AddRefed<SandboxTestingParent> SandboxTestingParent::Create(
    Endpoint<PSandboxTestingParent>&& aParentEnd) {
  SandboxTestingThread* thread = SandboxTestingThread::Create();
  if (!thread) {
    return nullptr;
  }
  RefPtr<SandboxTestingParent> instance = new SandboxTestingParent(thread);
  thread->Dispatch(NewRunnableMethod<Endpoint<PSandboxTestingParent>&&>(
      "SandboxTestingParent::Bind", instance, &SandboxTestingParent::Bind,
      std::move(aParentEnd)));
  return instance.forget();
}

SandboxTestingParent::SandboxTestingParent(SandboxTestingThread* aThread)
    : mThread(aThread),
      mMonitor("SandboxTestingParent Lock"),
      mShutdownDone(false) {}

SandboxTestingParent::~SandboxTestingParent() = default;

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

void SandboxTestingParent::Destroy(
    already_AddRefed<SandboxTestingParent> aInstance) {
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<SandboxTestingParent> instance(aInstance);
  if (!instance) {
    return;
  }

  {
    // Hold the lock while we destroy the actor on the test thread.
    MonitorAutoLock lock(instance->mMonitor);
    instance->mThread->Dispatch(NewRunnableMethod(
        "SandboxTestingParent::ShutdownSandboxTestThread", instance,
        &SandboxTestingParent::ShutdownSandboxTestThread));

    // Wait for test thread to complete destruction.
    while (!instance->mShutdownDone) {
      instance->mMonitor.Wait();
    }
  }
}

void SandboxTestingParent::ActorDestroy(ActorDestroyReason aWhy) {
  MOZ_RELEASE_ASSERT(mThread->IsOnThread());
}

mozilla::ipc::IPCResult SandboxTestingParent::RecvReportTestResults(
    const nsCString& testName, bool passed, const nsCString& resultMessage) {
  NS_DispatchToMainThread(
      NS_NewRunnableFunction("SandboxReportTestResults", [=]() {
        nsCOMPtr<nsIObserverService> observerService =
            mozilla::services::GetObserverService();
        MOZ_RELEASE_ASSERT(observerService);
        nsCString passedStr(passed ? "true"_ns : "false"_ns);
        nsString json;
        json += u"{ \"testid\" : \""_ns + NS_ConvertUTF8toUTF16(testName) +
                u"\", \"passed\" : "_ns + NS_ConvertUTF8toUTF16(passedStr) +
                u", \"message\" : \""_ns +
                NS_ConvertUTF8toUTF16(resultMessage) + u"\" }"_ns;
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

mozilla::ipc::IPCResult SandboxTestingParent::RecvGetSpecialDirectory(
    const nsCString& aSpecialDirName, nsString* aDirPath) {
  RefPtr<Runnable> runnable = NS_NewRunnableFunction(
      "SandboxTestingParent::RecvGetSpecialDirectory", [&]() {
        nsCOMPtr<nsIFile> dir;
        NS_GetSpecialDirectory(aSpecialDirName.get(), getter_AddRefs(dir));
        if (dir) {
          dir->GetPath(*aDirPath);
        }
      });
  SyncRunnable::DispatchToThread(GetMainThreadSerialEventTarget(), runnable,
                                 /*aForceDispatch*/ true);
  return IPC_OK();
}

}  // namespace mozilla
