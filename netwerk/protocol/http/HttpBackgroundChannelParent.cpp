/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "HttpBackgroundChannelParent.h"

#include "HttpChannelParent.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/Unused.h"
#include "mozilla/net/BackgroundChannelRegistrar.h"
#include "mozilla/net/ChannelEventQueue.h"
#include "nsNetCID.h"
#include "nsQueryObject.h"
#include "nsThreadUtils.h"

using mozilla::dom::ContentParent;
using mozilla::ipc::AssertIsInMainProcess;
using mozilla::ipc::AssertIsOnBackgroundThread;
using mozilla::ipc::BackgroundParent;
using mozilla::ipc::IPCResult;
using mozilla::ipc::IsOnBackgroundThread;

namespace mozilla {
namespace net {

/*
 * Helper class for continuing the AsyncOpen procedure on main thread.
 */
class ContinueAsyncOpenRunnable final : public Runnable {
 public:
  ContinueAsyncOpenRunnable(HttpBackgroundChannelParent* aActor,
                            const uint64_t& aChannelId)
      : Runnable("net::ContinueAsyncOpenRunnable"),
        mActor(aActor),
        mChannelId(aChannelId) {
    AssertIsInMainProcess();
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(mActor);
  }

  NS_IMETHOD Run() override {
    LOG(
        ("HttpBackgroundChannelParent::ContinueAsyncOpen [this=%p "
         "channelId=%" PRIu64 "]\n",
         mActor.get(), mChannelId));
    AssertIsInMainProcess();
    MOZ_ASSERT(NS_IsMainThread());

    nsCOMPtr<nsIBackgroundChannelRegistrar> registrar =
        BackgroundChannelRegistrar::GetOrCreate();
    MOZ_ASSERT(registrar);

    registrar->LinkBackgroundChannel(mChannelId, mActor);
    return NS_OK;
  }

 private:
  RefPtr<HttpBackgroundChannelParent> mActor;
  const uint64_t mChannelId;
};

HttpBackgroundChannelParent::HttpBackgroundChannelParent()
    : mIPCOpened(true),
      mBgThreadMutex("HttpBackgroundChannelParent::BgThreadMutex") {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  {
    MutexAutoLock lock(mBgThreadMutex);
    mBackgroundThread = NS_GetCurrentThread();
  }
}

HttpBackgroundChannelParent::~HttpBackgroundChannelParent() {
  MOZ_ASSERT(NS_IsMainThread() || IsOnBackgroundThread());
  MOZ_ASSERT(!mIPCOpened);
}

nsresult HttpBackgroundChannelParent::Init(const uint64_t& aChannelId) {
  LOG(("HttpBackgroundChannelParent::Init [this=%p channelId=%" PRIu64 "]\n",
       this, aChannelId));
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  RefPtr<ContinueAsyncOpenRunnable> runnable =
      new ContinueAsyncOpenRunnable(this, aChannelId);

  return NS_DispatchToMainThread(runnable);
}

void HttpBackgroundChannelParent::LinkToChannel(
    HttpChannelParent* aChannelParent) {
  LOG(("HttpBackgroundChannelParent::LinkToChannel [this=%p channel=%p]\n",
       this, aChannelParent));
  AssertIsInMainProcess();
  MOZ_ASSERT(NS_IsMainThread());

  if (!mIPCOpened) {
    return;
  }

  mChannelParent = aChannelParent;
}

void HttpBackgroundChannelParent::OnChannelClosed() {
  LOG(("HttpBackgroundChannelParent::OnChannelClosed [this=%p]\n", this));
  AssertIsInMainProcess();
  MOZ_ASSERT(NS_IsMainThread());

  if (!mIPCOpened) {
    return;
  }

  nsresult rv;

  {
    MutexAutoLock lock(mBgThreadMutex);
    RefPtr<HttpBackgroundChannelParent> self = this;
    rv = mBackgroundThread->Dispatch(
        NS_NewRunnableFunction(
            "net::HttpBackgroundChannelParent::OnChannelClosed",
            [self]() {
              LOG(("HttpBackgroundChannelParent::DeleteRunnable [this=%p]\n",
                   self.get()));
              AssertIsOnBackgroundThread();

              if (!self->mIPCOpened.compareExchange(true, false)) {
                return;
              }

              Unused << self->Send__delete__(self);
            }),
        NS_DISPATCH_NORMAL);
  }

  Unused << NS_WARN_IF(NS_FAILED(rv));
}

bool HttpBackgroundChannelParent::OnStartRequest(
    const nsHttpResponseHead& aResponseHead, const bool& aUseResponseHead,
    const nsHttpHeaderArray& aRequestHeaders,
    const HttpChannelOnStartRequestArgs& aArgs,
    const nsCOMPtr<nsICacheEntry>& aAltDataSource) {
  LOG(("HttpBackgroundChannelParent::OnStartRequest [this=%p]\n", this));
  AssertIsInMainProcess();

  if (NS_WARN_IF(!mIPCOpened)) {
    return false;
  }

  if (!IsOnBackgroundThread()) {
    MutexAutoLock lock(mBgThreadMutex);
    nsresult rv = mBackgroundThread->Dispatch(
        NewRunnableMethod<
            const nsHttpResponseHead, const bool, const nsHttpHeaderArray,
            const HttpChannelOnStartRequestArgs, const nsCOMPtr<nsICacheEntry>>(
            "net::HttpBackgroundChannelParent::OnStartRequest", this,
            &HttpBackgroundChannelParent::OnStartRequest, aResponseHead,
            aUseResponseHead, aRequestHeaders, aArgs, aAltDataSource),
        NS_DISPATCH_NORMAL);

    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));

    return NS_SUCCEEDED(rv);
  }

  HttpChannelAltDataStream altData;
  ipc::AutoIPCStream altDataInputStream(true /* delay start */);
  if (aAltDataSource) {
    nsAutoCString altDataType;
    Unused << aAltDataSource->GetAltDataType(altDataType);

    if (!altDataType.IsEmpty()) {
      nsCOMPtr<nsIInputStream> inputStream;
      nsresult rv = aAltDataSource->OpenAlternativeInputStream(
          altDataType, getter_AddRefs(inputStream));
      if (NS_SUCCEEDED(rv)) {
        Unused << altDataInputStream.Serialize(inputStream, Manager());
      }
    }
  }
  altData.altDataInputStream() = altDataInputStream.TakeOptionalValue();

  return SendOnStartRequest(aResponseHead, aUseResponseHead, aRequestHeaders,
                            aArgs, altData);
}

bool HttpBackgroundChannelParent::OnTransportAndData(
    const nsresult& aChannelStatus, const nsresult& aTransportStatus,
    const uint64_t& aOffset, const uint32_t& aCount, const nsCString& aData) {
  LOG(("HttpBackgroundChannelParent::OnTransportAndData [this=%p]\n", this));
  AssertIsInMainProcess();

  if (NS_WARN_IF(!mIPCOpened)) {
    return false;
  }

  if (!IsOnBackgroundThread()) {
    MutexAutoLock lock(mBgThreadMutex);
    nsresult rv = mBackgroundThread->Dispatch(
        NewRunnableMethod<const nsresult, const nsresult, const uint64_t,
                          const uint32_t, const nsCString>(
            "net::HttpBackgroundChannelParent::OnTransportAndData", this,
            &HttpBackgroundChannelParent::OnTransportAndData, aChannelStatus,
            aTransportStatus, aOffset, aCount, aData),
        NS_DISPATCH_NORMAL);

    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));

    return NS_SUCCEEDED(rv);
  }

  nsHttp::SendFunc<nsDependentCSubstring> sendFunc =
      [self = UnsafePtr<HttpBackgroundChannelParent>(this), aChannelStatus,
       aTransportStatus](const nsDependentCSubstring& aData, uint64_t aOffset,
                         uint32_t aCount) {
        return self->SendOnTransportAndData(aChannelStatus, aTransportStatus,
                                            aOffset, aCount, aData, false);
      };

  return nsHttp::SendDataInChunks(aData, aOffset, aCount, sendFunc);
}

bool HttpBackgroundChannelParent::OnStopRequest(
    const nsresult& aChannelStatus, const ResourceTimingStructArgs& aTiming,
    const nsHttpHeaderArray& aResponseTrailers,
    const nsTArray<ConsoleReportCollected>& aConsoleReports) {
  LOG(
      ("HttpBackgroundChannelParent::OnStopRequest [this=%p "
       "status=%" PRIx32 "]\n",
       this, static_cast<uint32_t>(aChannelStatus)));
  AssertIsInMainProcess();

  if (NS_WARN_IF(!mIPCOpened)) {
    return false;
  }

  if (!IsOnBackgroundThread()) {
    MutexAutoLock lock(mBgThreadMutex);
    nsresult rv = mBackgroundThread->Dispatch(
        NewRunnableMethod<const nsresult, const ResourceTimingStructArgs,
                          const nsHttpHeaderArray,
                          const CopyableTArray<ConsoleReportCollected>>(
            "net::HttpBackgroundChannelParent::OnStopRequest", this,
            &HttpBackgroundChannelParent::OnStopRequest, aChannelStatus,
            aTiming, aResponseTrailers, aConsoleReports),
        NS_DISPATCH_NORMAL);

    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));

    return NS_SUCCEEDED(rv);
  }

  // See the child code for why we do this.
  TimeStamp lastActTabOpt = nsHttp::GetLastActiveTabLoadOptimizationHit();

  return SendOnStopRequest(aChannelStatus, aTiming, lastActTabOpt,
                           aResponseTrailers, aConsoleReports, false);
}

bool HttpBackgroundChannelParent::OnConsoleReport(
    const nsTArray<ConsoleReportCollected>& aConsoleReports) {
  LOG(("HttpBackgroundChannelParent::OnConsoleReport [this=%p]", this));
  AssertIsInMainProcess();

  if (NS_WARN_IF(!mIPCOpened)) {
    return false;
  }

  if (!IsOnBackgroundThread()) {
    MutexAutoLock lock(mBgThreadMutex);
    nsresult rv = mBackgroundThread->Dispatch(
        NewRunnableMethod<const CopyableTArray<ConsoleReportCollected>>(
            "net::HttpBackgroundChannelParent::OnConsoleReport", this,
            &HttpBackgroundChannelParent::OnConsoleReport, aConsoleReports),
        NS_DISPATCH_NORMAL);

    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));

    return NS_SUCCEEDED(rv);
  }

  return SendOnConsoleReport(aConsoleReports);
}

bool HttpBackgroundChannelParent::OnAfterLastPart(const nsresult aStatus) {
  LOG(("HttpBackgroundChannelParent::OnAfterLastPart [this=%p]\n", this));
  AssertIsInMainProcess();

  if (NS_WARN_IF(!mIPCOpened)) {
    return false;
  }

  if (!IsOnBackgroundThread()) {
    MutexAutoLock lock(mBgThreadMutex);
    nsresult rv = mBackgroundThread->Dispatch(
        NewRunnableMethod<const nsresult>(
            "net::HttpBackgroundChannelParent::OnAfterLastPart", this,
            &HttpBackgroundChannelParent::OnAfterLastPart, aStatus),
        NS_DISPATCH_NORMAL);

    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));

    return NS_SUCCEEDED(rv);
  }

  return SendOnAfterLastPart(aStatus);
}

bool HttpBackgroundChannelParent::OnProgress(const int64_t aProgress,
                                             const int64_t aProgressMax) {
  LOG(("HttpBackgroundChannelParent::OnProgress [this=%p]\n", this));
  AssertIsInMainProcess();

  if (NS_WARN_IF(!mIPCOpened)) {
    return false;
  }

  if (!IsOnBackgroundThread()) {
    MutexAutoLock lock(mBgThreadMutex);
    nsresult rv = mBackgroundThread->Dispatch(
        NewRunnableMethod<const int64_t, const int64_t>(
            "net::HttpBackgroundChannelParent::OnProgress", this,
            &HttpBackgroundChannelParent::OnProgress, aProgress, aProgressMax),
        NS_DISPATCH_NORMAL);

    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));

    return NS_SUCCEEDED(rv);
  }

  return SendOnProgress(aProgress, aProgressMax);
}

bool HttpBackgroundChannelParent::OnStatus(const nsresult aStatus) {
  LOG(("HttpBackgroundChannelParent::OnStatus [this=%p]\n", this));
  AssertIsInMainProcess();

  if (NS_WARN_IF(!mIPCOpened)) {
    return false;
  }

  if (!IsOnBackgroundThread()) {
    MutexAutoLock lock(mBgThreadMutex);
    nsresult rv = mBackgroundThread->Dispatch(
        NewRunnableMethod<const nsresult>(
            "net::HttpBackgroundChannelParent::OnStatus", this,
            &HttpBackgroundChannelParent::OnStatus, aStatus),
        NS_DISPATCH_NORMAL);

    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));

    return NS_SUCCEEDED(rv);
  }

  return SendOnStatus(aStatus);
}

bool HttpBackgroundChannelParent::OnNotifyClassificationFlags(
    uint32_t aClassificationFlags, bool aIsThirdParty) {
  LOG(
      ("HttpBackgroundChannelParent::OnNotifyClassificationFlags "
       "classificationFlags=%" PRIu32 ", thirdparty=%d [this=%p]\n",
       aClassificationFlags, static_cast<int>(aIsThirdParty), this));
  AssertIsInMainProcess();

  if (NS_WARN_IF(!mIPCOpened)) {
    return false;
  }

  if (!IsOnBackgroundThread()) {
    MutexAutoLock lock(mBgThreadMutex);
    nsresult rv = mBackgroundThread->Dispatch(
        NewRunnableMethod<uint32_t, bool>(
            "net::HttpBackgroundChannelParent::OnNotifyClassificationFlags",
            this, &HttpBackgroundChannelParent::OnNotifyClassificationFlags,
            aClassificationFlags, aIsThirdParty),
        NS_DISPATCH_NORMAL);

    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));

    return NS_SUCCEEDED(rv);
  }

  return SendNotifyClassificationFlags(aClassificationFlags, aIsThirdParty);
}

bool HttpBackgroundChannelParent::OnNotifyFlashPluginStateChanged(
    nsIHttpChannel::FlashPluginState aState) {
  LOG(
      ("HttpBackgroundChannelParent::OnNotifyFlashPluginStateChanged "
       "[this=%p]\n",
       this));
  AssertIsInMainProcess();

  if (NS_WARN_IF(!mIPCOpened)) {
    return false;
  }

  if (!IsOnBackgroundThread()) {
    MutexAutoLock lock(mBgThreadMutex);
    RefPtr<HttpBackgroundChannelParent> self = this;
    nsresult rv = mBackgroundThread->Dispatch(
        NS_NewRunnableFunction(
            "net::HttpBackgroundChannelParent::OnNotifyFlashPluginStateChanged",
            [self, aState]() {
              self->OnNotifyFlashPluginStateChanged(aState);
            }),
        NS_DISPATCH_NORMAL);

    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));

    return NS_SUCCEEDED(rv);
  }

  return SendNotifyFlashPluginStateChanged(aState);
}

bool HttpBackgroundChannelParent::OnSetClassifierMatchedInfo(
    const nsACString& aList, const nsACString& aProvider,
    const nsACString& aFullHash) {
  LOG(("HttpBackgroundChannelParent::OnSetClassifierMatchedInfo [this=%p]\n",
       this));
  AssertIsInMainProcess();

  if (NS_WARN_IF(!mIPCOpened)) {
    return false;
  }

  if (!IsOnBackgroundThread()) {
    MutexAutoLock lock(mBgThreadMutex);
    nsresult rv = mBackgroundThread->Dispatch(
        NewRunnableMethod<const nsCString, const nsCString, const nsCString>(
            "net::HttpBackgroundChannelParent::OnSetClassifierMatchedInfo",
            this, &HttpBackgroundChannelParent::OnSetClassifierMatchedInfo,
            aList, aProvider, aFullHash),
        NS_DISPATCH_NORMAL);

    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));

    return NS_SUCCEEDED(rv);
  }

  ClassifierInfo info;
  info.list() = aList;
  info.fullhash() = aFullHash;
  info.provider() = aProvider;

  return SendSetClassifierMatchedInfo(info);
}

bool HttpBackgroundChannelParent::OnSetClassifierMatchedTrackingInfo(
    const nsACString& aLists, const nsACString& aFullHashes) {
  LOG(
      ("HttpBackgroundChannelParent::OnSetClassifierMatchedTrackingInfo "
       "[this=%p]\n",
       this));
  AssertIsInMainProcess();

  if (NS_WARN_IF(!mIPCOpened)) {
    return false;
  }

  if (!IsOnBackgroundThread()) {
    MutexAutoLock lock(mBgThreadMutex);
    nsresult rv = mBackgroundThread->Dispatch(
        NewRunnableMethod<const nsCString, const nsCString>(
            "net::HttpBackgroundChannelParent::"
            "OnSetClassifierMatchedTrackingInfo",
            this,
            &HttpBackgroundChannelParent::OnSetClassifierMatchedTrackingInfo,
            aLists, aFullHashes),
        NS_DISPATCH_NORMAL);

    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));

    return NS_SUCCEEDED(rv);
  }

  ClassifierInfo info;
  info.list() = aLists;
  info.fullhash() = aFullHashes;

  return SendSetClassifierMatchedTrackingInfo(info);
}

nsISerialEventTarget* HttpBackgroundChannelParent::GetBackgroundTarget() {
  MOZ_ASSERT(mBackgroundThread);
  return mBackgroundThread.get();
}

auto HttpBackgroundChannelParent::AttachStreamFilter(
    Endpoint<extensions::PStreamFilterParent>&& aParentEndpoint,
    Endpoint<extensions::PStreamFilterChild>&& aChildEndpoint)
    -> RefPtr<ChildEndpointPromise> {
  LOG(("HttpBackgroundChannelParent::AttachStreamFilter [this=%p]\n", this));
  MOZ_ASSERT(IsOnBackgroundThread());

  if (NS_WARN_IF(!mIPCOpened) ||
      !SendAttachStreamFilter(std::move(aParentEndpoint))) {
    return ChildEndpointPromise::CreateAndReject(false, __func__);
  }

  return ChildEndpointPromise::CreateAndResolve(std::move(aChildEndpoint),
                                                __func__);
}

void HttpBackgroundChannelParent::ActorDestroy(ActorDestroyReason aWhy) {
  LOG(("HttpBackgroundChannelParent::ActorDestroy [this=%p]\n", this));
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  mIPCOpened = false;

  RefPtr<HttpBackgroundChannelParent> self = this;
  DebugOnly<nsresult> rv = NS_DispatchToMainThread(NS_NewRunnableFunction(
      "net::HttpBackgroundChannelParent::ActorDestroy", [self]() {
        MOZ_ASSERT(NS_IsMainThread());

        RefPtr<HttpChannelParent> channelParent =
            std::move(self->mChannelParent);

        if (channelParent) {
          channelParent->OnBackgroundParentDestroyed();
        }
      }));
  MOZ_ASSERT(NS_SUCCEEDED(rv));
}

}  // namespace net
}  // namespace mozilla
