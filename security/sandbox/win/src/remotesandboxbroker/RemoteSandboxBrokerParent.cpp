/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "RemoteSandboxBrokerParent.h"
#include "RemoteSandboxBrokerProcessParent.h"
#include "mozilla/Telemetry.h"
#include <windows.h>

namespace mozilla {

RefPtr<GenericPromise> RemoteSandboxBrokerParent::Launch(
    const nsTArray<uint64_t>& aHandlesToShare) {
  MOZ_ASSERT(!mProcess);
  if (mProcess) {
    // Don't re-init.
    return GenericPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  mProcess = new RemoteSandboxBrokerProcessParent();
  for (uint64_t handle : aHandlesToShare) {
    mProcess->AddHandleToShare(HANDLE(handle));
  }

  // Note: we rely on the caller to keep this instance alive while we launch
  // the process, so that these closures point to valid memory.
  auto resolve = [this](base::ProcessHandle handle) {
    mOpened = Open(mProcess->GetChannel(), base::GetProcId(handle));
    if (!mOpened) {
      mProcess->Destroy();
      mProcess = nullptr;
      return GenericPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
    }
    return GenericPromise::CreateAndResolve(true, __func__);
  };

  auto reject = [this]() {
    NS_ERROR("failed to launch child in the parent");
    if (mProcess) {
      mProcess->Destroy();
      mProcess = nullptr;
    }
    return GenericPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  };

  return mProcess->AsyncLaunch()->Then(GetCurrentThreadSerialEventTarget(),
                                       __func__, std::move(resolve),
                                       std::move(reject));
}

bool RemoteSandboxBrokerParent::DuplicateFromLauncher(HANDLE aLauncherHandle,
                                                      LPHANDLE aOurHandle) {
  return ::DuplicateHandle(mProcess->GetChildProcessHandle(), aLauncherHandle,
                           ::GetCurrentProcess(), aOurHandle, 0, false,
                           DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE);
}

void RemoteSandboxBrokerParent::ActorDestroy(ActorDestroyReason aWhy) {
  if (AbnormalShutdown == aWhy) {
    Telemetry::Accumulate(Telemetry::SUBPROCESS_ABNORMAL_ABORT,
                          nsDependentCString(XRE_ChildProcessTypeToString(
                              GeckoProcessType_RemoteSandboxBroker)),
                          1);
    GenerateCrashReport(OtherPid());
  }
  Shutdown();
}

void RemoteSandboxBrokerParent::Shutdown() {
  if (mOpened) {
    mOpened = false;
    Close();
  }
  if (mProcess) {
    mProcess->Destroy();
    mProcess = nullptr;
  }
}

}  // namespace mozilla
