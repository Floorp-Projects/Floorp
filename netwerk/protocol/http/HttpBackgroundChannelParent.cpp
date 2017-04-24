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
    : mActor(aActor)
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
  , mBackgroundThread(NS_GetCurrentThread())
{
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
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

  MOZ_DIAGNOSTIC_ASSERT(mIPCOpened);
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

  RefPtr<HttpBackgroundChannelParent> self = this;
  rv = mBackgroundThread->Dispatch(NS_NewRunnableFunction([self]() {
    LOG(("HttpBackgroundChannelParent::DeleteRunnable [this=%p]\n", self.get()));
    AssertIsOnBackgroundThread();

    if (!self->mIPCOpened.compareExchange(true, false)) {
      return;
    }

    Unused << self->Send__delete__(self);
  }), NS_DISPATCH_NORMAL);

  Unused << NS_WARN_IF(NS_FAILED(rv));
}

void
HttpBackgroundChannelParent::ActorDestroy(ActorDestroyReason aWhy)
{
  LOG(("HttpBackgroundChannelParent::ActorDestroy [this=%p]\n", this));
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  mIPCOpened = false;

  RefPtr<HttpBackgroundChannelParent> self = this;
  DebugOnly<nsresult> rv =
    NS_DispatchToMainThread(NS_NewRunnableFunction([self]() {
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
