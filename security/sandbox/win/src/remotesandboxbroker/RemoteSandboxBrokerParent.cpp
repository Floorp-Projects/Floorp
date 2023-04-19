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
    uint32_t aLaunchArch, const nsTArray<uint64_t>& aHandlesToShare,
    nsISerialEventTarget* aThread) {
  MOZ_ASSERT(!mProcess);
  if (mProcess) {
    // Don't re-init.
    return GenericPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  mProcess = new RemoteSandboxBrokerProcessParent();
#ifdef ALLOW_GECKO_CHILD_PROCESS_ARCH
  mProcess->SetLaunchArchitecture(aLaunchArch);
#endif
  for (uint64_t handle : aHandlesToShare) {
    mProcess->AddHandleToShare(HANDLE(handle));
  }

  auto resolve = [self = RefPtr{this}](base::ProcessHandle handle) {
    self->mOpened = self->mProcess->TakeInitialEndpoint().Bind(self);
    if (!self->mOpened) {
      self->mProcess->Destroy();
      self->mProcess = nullptr;
      return GenericPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
    }
    return GenericPromise::CreateAndResolve(true, __func__);
  };

  auto reject = [self = RefPtr{this}]() {
    NS_ERROR("failed to launch child in the parent");
    if (self->mProcess) {
      self->mProcess->Destroy();
      self->mProcess = nullptr;
    }
    return GenericPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  };

  return mProcess->AsyncLaunch()->Then(aThread, __func__, std::move(resolve),
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
                          nsDependentCString(XRE_GeckoProcessTypeToString(
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
