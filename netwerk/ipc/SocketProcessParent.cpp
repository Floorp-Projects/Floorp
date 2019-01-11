/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SocketProcessParent.h"

#include "SocketProcessHost.h"
#include "mozilla/ipc/CrashReporterHost.h"

namespace mozilla {
namespace net {

SocketProcessParent::SocketProcessParent(SocketProcessHost* aHost)
    : mHost(aHost) {
  MOZ_ASSERT(mHost);

  MOZ_COUNT_CTOR(SocketProcessParent);
}

SocketProcessParent::~SocketProcessParent() {
  MOZ_COUNT_DTOR(SocketProcessParent);
}

mozilla::ipc::IPCResult SocketProcessParent::RecvInitCrashReporter(
    Shmem&& aShmem, const NativeThreadId& aThreadId) {
  mCrashReporter = MakeUnique<CrashReporterHost>(GeckoProcessType_Content,
                                                 aShmem, aThreadId);

  return IPC_OK();
}

void SocketProcessParent::ActorDestroy(ActorDestroyReason aWhy) {
  if (aWhy == AbnormalShutdown) {
    if (mCrashReporter) {
      mCrashReporter->GenerateCrashReport(OtherPid());
      mCrashReporter = nullptr;
    }
  }

  if (mHost) {
    mHost->OnChannelClosed();
  }
}

// To ensure that IPDL is finished before SocketParent gets deleted.
class DeferredDeleteSocketProcessParent : public Runnable {
 public:
  explicit DeferredDeleteSocketProcessParent(
      UniquePtr<SocketProcessParent>&& aParent)
      : Runnable("net::DeferredDeleteSocketProcessParent"),
        mParent(std::move(aParent)) {}

  NS_IMETHODIMP Run() override { return NS_OK; }

 private:
  UniquePtr<SocketProcessParent> mParent;
};

/* static */ void SocketProcessParent::Destroy(
    UniquePtr<SocketProcessParent>&& aParent) {
  NS_DispatchToMainThread(
      new DeferredDeleteSocketProcessParent(std::move(aParent)));
}

}  // namespace net
}  // namespace mozilla
