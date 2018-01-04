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
#include "nsIBackgroundChannelRegistrar.h"
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
class ContinueAsyncOpenRunnable final : public Runnable
{
public:
  ContinueAsyncOpenRunnable(HttpBackgroundChannelParent* aActor,
                            const uint64_t& aChannelId)
    : Runnable("net::ContinueAsyncOpenRunnable")
    , mActor(aActor)
    , mChannelId(aChannelId)
  {
    AssertIsInMainProcess();
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(mActor);
  }

  NS_IMETHOD Run() override
  {
    LOG(("HttpBackgroundChannelParent::ContinueAsyncOpen [this=%p channelId=%"
         PRIu64 "]\n", mActor.get(), mChannelId));
    AssertIsInMainProcess();
    MOZ_ASSERT(NS_IsMainThread());

    nsCOMPtr<nsIBackgroundChannelRegistrar> registrar =
      do_GetService(NS_BACKGROUNDCHANNELREGISTRAR_CONTRACTID);
    MOZ_ASSERT(registrar);

    registrar->LinkBackgroundChannel(mChannelId, mActor);
    return NS_OK;
  }

private:
  RefPtr<HttpBackgroundChannelParent> mActor;
  const uint64_t mChannelId;
};

HttpBackgroundChannelParent::HttpBackgroundChannelParent()
  : mIPCOpened(true)
  , mBgThreadMutex("HttpBackgroundChannelParent::BgThreadMutex")
{
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  {
    MutexAutoLock lock(mBgThreadMutex);
    mBackgroundThread = NS_GetCurrentThread();
  }
}

HttpBackgroundChannelParent::~HttpBackgroundChannelParent()
{
  MOZ_ASSERT(NS_IsMainThread() || IsOnBackgroundThread());
  MOZ_ASSERT(!mIPCOpened);
}

nsresult
HttpBackgroundChannelParent::Init(const uint64_t& aChannelId)
{
  LOG(("HttpBackgroundChannelParent::Init [this=%p channelId=%" PRIu64 "]\n",
       this, aChannelId));
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  RefPtr<ContinueAsyncOpenRunnable> runnable =
    new ContinueAsyncOpenRunnable(this, aChannelId);

  return NS_DispatchToMainThread(runnable);
}

void
HttpBackgroundChannelParent::LinkToChannel(HttpChannelParent* aChannelParent)
{
  LOG(("HttpBackgroundChannelParent::LinkToChannel [this=%p channel=%p]\n",
       this, aChannelParent));
  AssertIsInMainProcess();
  MOZ_ASSERT(NS_IsMainThread());

  if (!mIPCOpened) {
    return;
  }

  mChannelParent = aChannelParent;
}

void
HttpBackgroundChannelParent::OnChannelClosed()
{
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

bool
HttpBackgroundChannelParent::OnStartRequestSent()
{
  LOG(("HttpBackgroundChannelParent::OnStartRequestSent [this=%p]\n", this));
  AssertIsInMainProcess();

  if (NS_WARN_IF(!mIPCOpened)) {
    return false;
  }

  if (!IsOnBackgroundThread()) {
    MutexAutoLock lock(mBgThreadMutex);
    nsresult rv = mBackgroundThread->Dispatch(
      NewRunnableMethod("net::HttpBackgroundChannelParent::OnStartRequestSent",
                        this,
                        &HttpBackgroundChannelParent::OnStartRequestSent),
      NS_DISPATCH_NORMAL);

    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));

    return NS_SUCCEEDED(rv);
  }

  return SendOnStartRequestSent();
}

bool
HttpBackgroundChannelParent::OnTransportAndData(
                                               const nsresult& aChannelStatus,
                                               const nsresult& aTransportStatus,
                                               const uint64_t& aOffset,
                                               const uint32_t& aCount,
                                               const nsCString& aData)
{
  LOG(("HttpBackgroundChannelParent::OnTransportAndData [this=%p]\n", this));
  AssertIsInMainProcess();

  if (NS_WARN_IF(!mIPCOpened)) {
    return false;
  }

  if (!IsOnBackgroundThread()) {
    MutexAutoLock lock(mBgThreadMutex);
    nsresult rv = mBackgroundThread->Dispatch(
      NewRunnableMethod<const nsresult,
                        const nsresult,
                        const uint64_t,
                        const uint32_t,
                        const nsCString>(
        "net::HttpBackgroundChannelParent::OnTransportAndData",
        this,
        &HttpBackgroundChannelParent::OnTransportAndData,
        aChannelStatus,
        aTransportStatus,
        aOffset,
        aCount,
        aData),
      NS_DISPATCH_NORMAL);

    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));

    return NS_SUCCEEDED(rv);
  }

  return SendOnTransportAndData(aChannelStatus, aTransportStatus,
                                aOffset, aCount, aData);
}

bool
HttpBackgroundChannelParent::OnStopRequest(const nsresult& aChannelStatus,
                                           const ResourceTimingStruct& aTiming,
                                           const nsHttpHeaderArray& aResponseTrailers)
{
  LOG(("HttpBackgroundChannelParent::OnStopRequest [this=%p "
        "status=%" PRIx32 "]\n", this, static_cast<uint32_t>(aChannelStatus)));
  AssertIsInMainProcess();

  if (NS_WARN_IF(!mIPCOpened)) {
    return false;
  }

  if (!IsOnBackgroundThread()) {
    MutexAutoLock lock(mBgThreadMutex);
    nsresult rv = mBackgroundThread->Dispatch(
      NewRunnableMethod<const nsresult,
                        const ResourceTimingStruct,
                        const nsHttpHeaderArray>(
        "net::HttpBackgroundChannelParent::OnStopRequest",
        this,
        &HttpBackgroundChannelParent::OnStopRequest,
        aChannelStatus,
        aTiming,
        aResponseTrailers),
      NS_DISPATCH_NORMAL);

    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));

    return NS_SUCCEEDED(rv);
  }

  // See the child code for why we do this.
  TimeStamp lastActTabOpt = nsHttp::GetLastActiveTabLoadOptimizationHit();

  return SendOnStopRequest(aChannelStatus, aTiming, lastActTabOpt, aResponseTrailers);
}

bool
HttpBackgroundChannelParent::OnProgress(const int64_t& aProgress,
                                        const int64_t& aProgressMax)
{
  LOG(("HttpBackgroundChannelParent::OnProgress [this=%p progress=%" PRId64
       " max=%" PRId64 "]\n", this, aProgress, aProgressMax));
  AssertIsInMainProcess();

  if (NS_WARN_IF(!mIPCOpened)) {
    return false;
  }

  if (!IsOnBackgroundThread()) {
    MutexAutoLock lock(mBgThreadMutex);
    nsresult rv = mBackgroundThread->Dispatch(
      NewRunnableMethod<const int64_t, const int64_t>(
        "net::HttpBackgroundChannelParent::OnProgress",
        this,
        &HttpBackgroundChannelParent::OnProgress,
        aProgress,
        aProgressMax),
      NS_DISPATCH_NORMAL);

    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));

    return NS_SUCCEEDED(rv);
  }

  return SendOnProgress(aProgress, aProgressMax);
}

bool
HttpBackgroundChannelParent::OnStatus(const nsresult& aStatus)
{
  LOG(("HttpBackgroundChannelParent::OnStatus [this=%p stauts=%" PRIx32
       "]\n", this, static_cast<uint32_t>(aStatus)));
  AssertIsInMainProcess();

  if (NS_WARN_IF(!mIPCOpened)) {
    return false;
  }

  if (!IsOnBackgroundThread()) {
    MutexAutoLock lock(mBgThreadMutex);
    nsresult rv = mBackgroundThread->Dispatch(
      NewRunnableMethod<const nsresult>(
        "net::HttpBackgroundChannelParent::OnStatus",
        this,
        &HttpBackgroundChannelParent::OnStatus,
        aStatus),
      NS_DISPATCH_NORMAL);

    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));

    return NS_SUCCEEDED(rv);
  }

  return SendOnStatus(aStatus);
}

bool
HttpBackgroundChannelParent::OnDiversion()
{
  LOG(("HttpBackgroundChannelParent::OnDiversion [this=%p]\n", this));
  AssertIsInMainProcess();

  if (NS_WARN_IF(!mIPCOpened)) {
    return false;
  }

  if (!IsOnBackgroundThread()) {
    MutexAutoLock lock(mBgThreadMutex);
    nsresult rv = mBackgroundThread->Dispatch(
      NewRunnableMethod("net::HttpBackgroundChannelParent::OnDiversion",
                        this,
                        &HttpBackgroundChannelParent::OnDiversion),
      NS_DISPATCH_NORMAL);

    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));

    return NS_SUCCEEDED(rv);
  }

  if (!SendFlushedForDiversion()) {
    return false;
  }

  // The listener chain should now be setup; tell HttpChannelChild to divert
  // the OnDataAvailables and OnStopRequest to associated HttpChannelParent.
  if (!SendDivertMessages()) {
    return false;
  }

  return true;
}

bool
HttpBackgroundChannelParent::OnNotifyTrackingProtectionDisabled()
{
  LOG(("HttpBackgroundChannelParent::OnNotifyTrackingProtectionDisabled [this=%p]\n", this));
  AssertIsInMainProcess();

  if (NS_WARN_IF(!mIPCOpened)) {
    return false;
  }

  if (!IsOnBackgroundThread()) {
    MutexAutoLock lock(mBgThreadMutex);
    nsresult rv = mBackgroundThread->Dispatch(
      NewRunnableMethod(
        "net::HttpBackgroundChannelParent::OnNotifyTrackingProtectionDisabled",
        this,
        &HttpBackgroundChannelParent::OnNotifyTrackingProtectionDisabled),
      NS_DISPATCH_NORMAL);

    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));

    return NS_SUCCEEDED(rv);
  }

  return SendNotifyTrackingProtectionDisabled();
}

bool
HttpBackgroundChannelParent::OnNotifyTrackingResource()
{
  LOG(("HttpBackgroundChannelParent::OnNotifyTrackingResource [this=%p]\n", this));
  AssertIsInMainProcess();

  if (NS_WARN_IF(!mIPCOpened)) {
    return false;
  }

  if (!IsOnBackgroundThread()) {
    MutexAutoLock lock(mBgThreadMutex);
    nsresult rv = mBackgroundThread->Dispatch(
      NewRunnableMethod(
        "net::HttpBackgroundChannelParent::OnNotifyTrackingResource",
        this,
        &HttpBackgroundChannelParent::OnNotifyTrackingResource),
      NS_DISPATCH_NORMAL);

    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));

    return NS_SUCCEEDED(rv);
  }

  return SendNotifyTrackingResource();
}

bool
HttpBackgroundChannelParent::OnSetClassifierMatchedInfo(const nsACString& aList,
                                                        const nsACString& aProvider,
                                                        const nsACString& aFullHash)
{
  LOG(("HttpBackgroundChannelParent::OnSetClassifierMatchedInfo [this=%p]\n", this));
  AssertIsInMainProcess();

  if (NS_WARN_IF(!mIPCOpened)) {
    return false;
  }

  if (!IsOnBackgroundThread()) {
    MutexAutoLock lock(mBgThreadMutex);
    nsresult rv = mBackgroundThread->Dispatch(
      NewRunnableMethod<const nsCString, const nsCString, const nsCString>(
        "net::HttpBackgroundChannelParent::OnSetClassifierMatchedInfo",
        this,
        &HttpBackgroundChannelParent::OnSetClassifierMatchedInfo,
        aList,
        aProvider,
        aFullHash),
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

void
HttpBackgroundChannelParent::ActorDestroy(ActorDestroyReason aWhy)
{
  LOG(("HttpBackgroundChannelParent::ActorDestroy [this=%p]\n", this));
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  mIPCOpened = false;

  RefPtr<HttpBackgroundChannelParent> self = this;
  DebugOnly<nsresult> rv = NS_DispatchToMainThread(NS_NewRunnableFunction(
    "net::HttpBackgroundChannelParent::ActorDestroy", [self]() {
      MOZ_ASSERT(NS_IsMainThread());

      RefPtr<HttpChannelParent> channelParent =
        self->mChannelParent.forget();

      if (channelParent) {
        channelParent->OnBackgroundParentDestroyed();
      }
    }));
  MOZ_ASSERT(NS_SUCCEEDED(rv));
}

} // namespace net
} // namespace mozilla
