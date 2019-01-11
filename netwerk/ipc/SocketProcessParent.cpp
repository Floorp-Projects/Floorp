/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SocketProcessParent.h"

#include "SocketProcessHost.h"
#include "mozilla/ipc/CrashReporterHost.h"

namespace mozilla {
namespace net {

static SocketProcessParent* sSocketProcessParent;

SocketProcessParent::SocketProcessParent(SocketProcessHost* aHost)
    : mHost(aHost) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mHost);

  MOZ_COUNT_CTOR(SocketProcessParent);
  sSocketProcessParent = this;
}

SocketProcessParent::~SocketProcessParent() {
  MOZ_ASSERT(NS_IsMainThread());

  MOZ_COUNT_DTOR(SocketProcessParent);
  sSocketProcessParent = nullptr;
}

/* static */ SocketProcessParent* SocketProcessParent::GetSingleton() {
  MOZ_ASSERT(NS_IsMainThread());

  return sSocketProcessParent;
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

bool SocketProcessParent::SendRequestMemoryReport(
    const uint32_t& aGeneration, const bool& aAnonymize,
    const bool& aMinimizeMemoryUsage, const MaybeFileDesc& aDMDFile) {
  mMemoryReportRequest = MakeUnique<dom::MemoryReportRequestHost>(aGeneration);
  Unused << PSocketProcessParent::SendRequestMemoryReport(
      aGeneration, aAnonymize, aMinimizeMemoryUsage, aDMDFile);
  return true;
}

mozilla::ipc::IPCResult SocketProcessParent::RecvAddMemoryReport(
    const MemoryReport& aReport) {
  if (mMemoryReportRequest) {
    mMemoryReportRequest->RecvReport(aReport);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult SocketProcessParent::RecvFinishMemoryReport(
    const uint32_t& aGeneration) {
  if (mMemoryReportRequest) {
    mMemoryReportRequest->Finish(aGeneration);
    mMemoryReportRequest = nullptr;
  }
  return IPC_OK();
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
