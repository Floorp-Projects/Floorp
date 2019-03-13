/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef RemoteSandboxBrokerParent_h_
#define RemoteSandboxBrokerParent_h_

#include "mozilla/PRemoteSandboxBrokerParent.h"
#include "RemoteSandboxBrokerProcessParent.h"
#include "mozilla/ipc/CrashReporterHost.h"

namespace mozilla {

class RemoteSandboxBrokerParent : public PRemoteSandboxBrokerParent {
  friend class PRemoteSandboxBrokerParent;

 public:
  bool DuplicateFromLauncher(HANDLE aLauncherHandle, LPHANDLE aOurHandle);

  void Shutdown();

  // Asynchronously launches the launcher process.
  // Note: we rely on the caller to keep this instance alive
  // until this promise resolves.
  RefPtr<GenericPromise> Launch(const nsTArray<uint64_t>& aHandlesToShare);

 private:
  mozilla::ipc::IPCResult RecvInitCrashReporter(
      Shmem&& aShmem, const NativeThreadId& aThreadId);

  void ActorDestroy(ActorDestroyReason aWhy) override;

  RemoteSandboxBrokerProcessParent* mProcess = nullptr;

  bool mOpened = false;
  UniquePtr<ipc::CrashReporterHost> mCrashReporter;
};

}  // namespace mozilla

#endif  // RemoteSandboxBrokerParent_h_
