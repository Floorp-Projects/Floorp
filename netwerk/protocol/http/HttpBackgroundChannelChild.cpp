/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "HttpBackgroundChannelChild.h"

#include "HttpChannelChild.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/net/BackgroundDataBridgeChild.h"
#include "mozilla/Unused.h"
#include "nsSocketTransportService2.h"

using mozilla::ipc::BackgroundChild;
using mozilla::ipc::IPCResult;

namespace mozilla {
namespace net {

// HttpBackgroundChannelChild
HttpBackgroundChannelChild::HttpBackgroundChannelChild() = default;

HttpBackgroundChannelChild::~HttpBackgroundChannelChild() = default;

nsresult HttpBackgroundChannelChild::Init(HttpChannelChild* aChannelChild) {
  LOG(
      ("HttpBackgroundChannelChild::Init [this=%p httpChannel=%p "
       "channelId=%" PRIu64 "]\n",
       this, aChannelChild, aChannelChild->ChannelId()));
  MOZ_ASSERT(OnSocketThread());
  NS_ENSURE_ARG(aChannelChild);

  mChannelChild = aChannelChild;

  if (NS_WARN_IF(!CreateBackgroundChannel())) {
    mChannelChild = nullptr;
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

void HttpBackgroundChannelChild::CreateDataBridge() {
  MOZ_ASSERT(OnSocketThread());
  PBackgroundChild* actorChild =
      BackgroundChild::GetOrCreateSocketActorForCurrentThread();
  if (NS_WARN_IF(!actorChild)) {
    return;
  }

  mDataBridgeChild = new BackgroundDataBridgeChild(this);
  if (!actorChild->SendPBackgroundDataBridgeConstructor(
          mDataBridgeChild, mChannelChild->ChannelId())) {
    mDataBridgeChild = nullptr;
  }
}

void HttpBackgroundChannelChild::OnChannelClosed() {
  LOG(("HttpBackgroundChannelChild::OnChannelClosed [this=%p]\n", this));
  MOZ_ASSERT(OnSocketThread());

  // HttpChannelChild is not going to handle any incoming message.
  mChannelChild = nullptr;

  // Remove pending IPC messages as well.
  mQueuedRunnables.Clear();

  if (mDataBridgeChild) {
    mDataBridgeChild->Destroy();
    mDataBridgeChild = nullptr;
  }
}

void HttpBackgroundChannelChild::OnStartRequestReceived() {
  LOG(("HttpBackgroundChannelChild::OnStartRequestReceived [this=%p]\n", this));
  MOZ_ASSERT(OnSocketThread());
  MOZ_ASSERT(mChannelChild);
  MOZ_ASSERT(!mStartReceived);  // Should only be called once.

  mStartReceived = true;

  nsTArray<nsCOMPtr<nsIRunnable>> runnables;
  runnables.SwapElements(mQueuedRunnables);

  for (const auto& event : runnables) {
    // Note: these runnables call Recv* methods on HttpBackgroundChannelChild
    // but not the Process* methods on HttpChannelChild.
    event->Run();
  }

  // Ensure no new message is enqueued.
  MOZ_ASSERT(mQueuedRunnables.IsEmpty());
}

bool HttpBackgroundChannelChild::CreateBackgroundChannel() {
  LOG(("HttpBackgroundChannelChild::CreateBackgroundChannel [this=%p]\n",
       this));
  MOZ_ASSERT(OnSocketThread());
  MOZ_ASSERT(mChannelChild);

  PBackgroundChild* actorChild = BackgroundChild::GetOrCreateForCurrentThread();
  if (NS_WARN_IF(!actorChild)) {
    return false;
  }

  const uint64_t channelId = mChannelChild->ChannelId();
  if (!actorChild->SendPHttpBackgroundChannelConstructor(this, channelId)) {
    return false;
  }

  mChannelChild->OnBackgroundChildReady(this);
  return true;
}

bool HttpBackgroundChannelChild::IsWaitingOnStartRequest(
    bool aDataFromSocketProcess) {
  MOZ_ASSERT(OnSocketThread());

  // When data is from socket process, it is possible that both mStartSent and
  // mStartReceived are false here. We need to wait until OnStartRequest sent
  // from parent process.
  // TODO: We can remove this code when diversion is removed in bug 1604448.
  if (aDataFromSocketProcess) {
    return !mStartReceived;
  }

  // Need to wait for OnStartRequest if it is sent by
  // parent process but not received by content process.
  return (mStartSent && !mStartReceived);
}

// PHttpBackgroundChannelChild
IPCResult HttpBackgroundChannelChild::RecvOnStartRequestSent() {
  LOG(("HttpBackgroundChannelChild::RecvOnStartRequestSent [this=%p]\n", this));
  MOZ_ASSERT(OnSocketThread());
  MOZ_ASSERT(!mStartSent);  // Should only receive this message once.

  mStartSent = true;
  return IPC_OK();
}

IPCResult HttpBackgroundChannelChild::RecvOnTransportAndData(
    const nsresult& aChannelStatus, const nsresult& aTransportStatus,
    const uint64_t& aOffset, const uint32_t& aCount, const nsCString& aData,
    const bool& aDataFromSocketProcess) {
  LOG(
      ("HttpBackgroundChannelChild::RecvOnTransportAndData [this=%p, "
       "aDataFromSocketProcess=%d]\n",
       this, aDataFromSocketProcess));
  MOZ_ASSERT(OnSocketThread());

  if (NS_WARN_IF(!mChannelChild)) {
    return IPC_OK();
  }

  if (IsWaitingOnStartRequest(aDataFromSocketProcess)) {
    LOG(("  > pending until OnStartRequest [offset=%" PRIu64 " count=%" PRIu32
         "]\n",
         aOffset, aCount));

    mQueuedRunnables.AppendElement(
        NewRunnableMethod<const nsresult, const nsresult, const uint64_t,
                          const uint32_t, const nsCString, bool>(
            "HttpBackgroundChannelChild::RecvOnTransportAndData", this,
            &HttpBackgroundChannelChild::RecvOnTransportAndData, aChannelStatus,
            aTransportStatus, aOffset, aCount, aData, aDataFromSocketProcess));

    return IPC_OK();
  }

  mChannelChild->ProcessOnTransportAndData(aChannelStatus, aTransportStatus,
                                           aOffset, aCount, aData);

  return IPC_OK();
}

IPCResult HttpBackgroundChannelChild::RecvOnStopRequest(
    const nsresult& aChannelStatus, const ResourceTimingStructArgs& aTiming,
    const TimeStamp& aLastActiveTabOptHit,
    const nsHttpHeaderArray& aResponseTrailers,
    const nsTArray<ConsoleReportCollected>& aConsoleReports) {
  LOG(("HttpBackgroundChannelChild::RecvOnStopRequest [this=%p]\n", this));
  MOZ_ASSERT(gSocketTransportService);
  MOZ_ASSERT(gSocketTransportService->IsOnCurrentThreadInfallible());

  // It's enough to set this from (just before) OnStopRequest notification,
  // since we don't need this value sooner than a channel was done loading -
  // everything this timestamp affects takes place only after a channel's
  // OnStopRequest.
  nsHttp::SetLastActiveTabLoadOptimizationHit(aLastActiveTabOptHit);

  if (NS_WARN_IF(!mChannelChild)) {
    return IPC_OK();
  }

  if (IsWaitingOnStartRequest()) {
    LOG(("  > pending until OnStartRequest [status=%" PRIx32 "]\n",
         static_cast<uint32_t>(aChannelStatus)));

    RefPtr<HttpBackgroundChannelChild> self = this;

    nsCOMPtr<nsIRunnable> task = NS_NewRunnableFunction(
        "HttpBackgroundChannelChild::RecvOnStopRequest",
        [self, aChannelStatus, aTiming, aLastActiveTabOptHit, aResponseTrailers,
         consoleReports = aConsoleReports.Clone()] {
          self->RecvOnStopRequest(aChannelStatus, aTiming, aLastActiveTabOptHit,
                                  aResponseTrailers, consoleReports);
        });

    mQueuedRunnables.AppendElement(task.forget());
    return IPC_OK();
  }

  mChannelChild->ProcessOnStopRequest(aChannelStatus, aTiming,
                                      aResponseTrailers, aConsoleReports);

  return IPC_OK();
}

IPCResult HttpBackgroundChannelChild::RecvFlushedForDiversion() {
  LOG(("HttpBackgroundChannelChild::RecvFlushedForDiversion [this=%p]\n",
       this));
  MOZ_ASSERT(OnSocketThread());

  if (NS_WARN_IF(!mChannelChild)) {
    return IPC_OK();
  }

  if (IsWaitingOnStartRequest()) {
    LOG(("  > pending until OnStartRequest\n"));

    mQueuedRunnables.AppendElement(NewRunnableMethod(
        "HttpBackgroundChannelChild::RecvFlushedForDiversion", this,
        &HttpBackgroundChannelChild::RecvFlushedForDiversion));

    return IPC_OK();
  }

  mChannelChild->ProcessFlushedForDiversion();

  return IPC_OK();
}

IPCResult HttpBackgroundChannelChild::RecvDivertMessages() {
  LOG(("HttpBackgroundChannelChild::RecvDivertMessages [this=%p]\n", this));
  MOZ_ASSERT(OnSocketThread());

  if (NS_WARN_IF(!mChannelChild)) {
    return IPC_OK();
  }

  if (IsWaitingOnStartRequest()) {
    LOG(("  > pending until OnStartRequest\n"));

    mQueuedRunnables.AppendElement(NewRunnableMethod(
        "HttpBackgroundChannelChild::RecvDivertMessages", this,
        &HttpBackgroundChannelChild::RecvDivertMessages));

    return IPC_OK();
  }

  mChannelChild->ProcessDivertMessages();

  return IPC_OK();
}

void HttpBackgroundChannelChild::ActorDestroy(ActorDestroyReason aWhy) {
  LOG(("HttpBackgroundChannelChild::ActorDestroy[this=%p]\n", this));
  // This function might be called during shutdown phase, so OnSocketThread()
  // might return false even on STS thread. Use IsOnCurrentThreadInfallible()
  // to get correct information.
  MOZ_ASSERT(gSocketTransportService);
  MOZ_ASSERT(gSocketTransportService->IsOnCurrentThreadInfallible());

  // Ensure all IPC messages received before ActorDestroy can be
  // handled correctly. If there is any pending IPC message, destroyed
  // mChannelChild until those messages are flushed.
  // If background channel is not closed by normal IPDL actor deletion,
  // remove the HttpChannelChild reference and notify background channel
  // destroyed immediately.
  if (aWhy == Deletion && !mQueuedRunnables.IsEmpty()) {
    LOG(("  > pending until queued messages are flushed\n"));
    RefPtr<HttpBackgroundChannelChild> self = this;
    mQueuedRunnables.AppendElement(NS_NewRunnableFunction(
        "HttpBackgroundChannelChild::ActorDestroy", [self]() {
          MOZ_ASSERT(OnSocketThread());
          RefPtr<HttpChannelChild> channelChild =
              std::move(self->mChannelChild);

          if (channelChild) {
            channelChild->OnBackgroundChildDestroyed(self);
          }
        }));
    return;
  }

  if (mChannelChild) {
    RefPtr<HttpChannelChild> channelChild = std::move(mChannelChild);

    channelChild->OnBackgroundChildDestroyed(this);
  }
}

}  // namespace net
}  // namespace mozilla
