/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "HttpBackgroundChannelChild.h"

#include "HttpChannelChild.h"
#include "MainThreadUtils.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/Unused.h"
#include "nsIIPCBackgroundChildCreateCallback.h"

using mozilla::ipc::BackgroundChild;
using mozilla::ipc::IPCResult;

namespace mozilla {
namespace net {

// Callbacks for PBackgroundChild creation
class BackgroundChannelCreateCallback final
  : public nsIIPCBackgroundChildCreateCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIIPCBACKGROUNDCHILDCREATECALLBACK

  explicit BackgroundChannelCreateCallback(HttpBackgroundChannelChild* aBgChild)
    : mBgChild(aBgChild)
  {
    MOZ_ASSERT(aBgChild);
  }

private:
  virtual ~BackgroundChannelCreateCallback() { }

  RefPtr<HttpBackgroundChannelChild> mBgChild;
};

NS_IMPL_ISUPPORTS(BackgroundChannelCreateCallback,
                  nsIIPCBackgroundChildCreateCallback)

void
BackgroundChannelCreateCallback::ActorCreated(PBackgroundChild* aActor)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(mBgChild);

  if (!mBgChild->mChannelChild) {
    // HttpChannelChild is closed during PBackground creation,
    // abort the rest of steps.
    return;
  }

  const uint64_t channelId = mBgChild->mChannelChild->ChannelId();
  if (!aActor->SendPHttpBackgroundChannelConstructor(mBgChild,
                                                     channelId)) {
    ActorFailed();
    return;
  }

  // hold extra reference for IPDL
  RefPtr<HttpBackgroundChannelChild> child = mBgChild;
  Unused << child.forget().take();
}

void
BackgroundChannelCreateCallback::ActorFailed()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mBgChild);

  mBgChild->OnBackgroundChannelCreationFailed();
}

// HttpBackgroundChannelChild
HttpBackgroundChannelChild::HttpBackgroundChannelChild()
{
}

HttpBackgroundChannelChild::~HttpBackgroundChannelChild()
{
}

nsresult
HttpBackgroundChannelChild::Init(HttpChannelChild* aChannelChild)
{
  LOG(("HttpBackgroundChannelChild::Init [this=%p httpChannel=%p channelId=%"
       PRIu64 "]\n", this, aChannelChild, aChannelChild->ChannelId()));
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG(aChannelChild);

  mChannelChild = aChannelChild;

  if (NS_WARN_IF(!CreateBackgroundChannel())) {
    mChannelChild = nullptr;
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

void
HttpBackgroundChannelChild::OnChannelClosed()
{
  LOG(("HttpBackgroundChannelChild::OnChannelClosed [this=%p]\n", this));
  MOZ_ASSERT(NS_IsMainThread());

  // HttpChannelChild is not going to handle any incoming message.
  mChannelChild = nullptr;
}

void
HttpBackgroundChannelChild::OnBackgroundChannelCreationFailed()
{
  LOG(("HttpBackgroundChannelChild::OnBackgroundChannelCreationFailed"
       " [this=%p]\n", this));
  MOZ_ASSERT(NS_IsMainThread());

  if (mChannelChild) {
    RefPtr<HttpChannelChild> channelChild = mChannelChild.forget();
    channelChild->FailedAsyncOpen(NS_ERROR_UNEXPECTED);
  }
}

bool
HttpBackgroundChannelChild::CreateBackgroundChannel()
{
  LOG(("HttpBackgroundChannelChild::CreateBackgroundChannel [this=%p]\n", this));
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<BackgroundChannelCreateCallback> callback =
    new BackgroundChannelCreateCallback(this);

  return BackgroundChild::GetOrCreateForCurrentThread(callback);
}

// PHttpBackgroundChannelChild
IPCResult
HttpBackgroundChannelChild::RecvOnStartRequestSent()
{
  return IPC_OK();
}

IPCResult
HttpBackgroundChannelChild::RecvOnTransportAndData(
                                               const nsresult& aChannelStatus,
                                               const nsresult& aTransportStatus,
                                               const uint64_t& aOffset,
                                               const uint32_t& aCount,
                                               const nsCString& aData)
{
  return IPC_OK();
}

IPCResult
HttpBackgroundChannelChild::RecvOnStopRequest(
                                            const nsresult& aChannelStatus,
                                            const ResourceTimingStruct& aTiming)
{
  return IPC_OK();
}

IPCResult
HttpBackgroundChannelChild::RecvOnProgress(const int64_t& aProgress,
                                           const int64_t& aProgressMax)
{
  return IPC_OK();
}

IPCResult
HttpBackgroundChannelChild::RecvOnStatus(const nsresult& aStatus)
{
  return IPC_OK();
}

IPCResult
HttpBackgroundChannelChild::RecvFlushedForDiversion()
{
  return IPC_OK();
}

IPCResult
HttpBackgroundChannelChild::RecvDivertMessages()
{
  return IPC_OK();
}

void
HttpBackgroundChannelChild::ActorDestroy(ActorDestroyReason aWhy)
{
  LOG(("HttpBackgroundChannelChild::ActorDestroy[this=%p]\n", this));
  MOZ_ASSERT(NS_IsMainThread());

  if (mChannelChild) {
    RefPtr<HttpChannelChild> channelChild = mChannelChild.forget();
    channelChild->OnBackgroundChildDestroyed();
  }
}

} // namespace net
} // namespace mozilla
