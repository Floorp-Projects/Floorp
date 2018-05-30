/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SimpleChannel.h"

#include "nsBaseChannel.h"
#include "nsIChannel.h"
#include "nsIChildChannel.h"
#include "nsIInputStream.h"
#include "nsIRequest.h"
#include "nsISupportsImpl.h"
#include "nsNetUtil.h"

#include "mozilla/Unused.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/net/NeckoChild.h"
#include "mozilla/net/PSimpleChannelChild.h"

namespace mozilla {
namespace net {

class SimpleChannel : public nsBaseChannel
{
public:
  explicit SimpleChannel(UniquePtr<SimpleChannelCallbacks>&& aCallbacks);

protected:
  virtual ~SimpleChannel() = default;

  virtual nsresult OpenContentStream(bool async, nsIInputStream **streamOut,
                                     nsIChannel** channel) override;

  virtual nsresult BeginAsyncRead(nsIStreamListener* listener,
                                  nsIRequest** request) override;

private:
  UniquePtr<SimpleChannelCallbacks> mCallbacks;
};

SimpleChannel::SimpleChannel(UniquePtr<SimpleChannelCallbacks>&& aCallbacks)
  : mCallbacks(std::move(aCallbacks))
{
  EnableSynthesizedProgressEvents(true);
}

nsresult
SimpleChannel::OpenContentStream(bool async, nsIInputStream **streamOut, nsIChannel** channel)
{
  NS_ENSURE_TRUE(mCallbacks, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIInputStream> stream;
  MOZ_TRY_VAR(stream, mCallbacks->OpenContentStream(async, this));
  MOZ_ASSERT(stream);

  mCallbacks = nullptr;

  *channel = nullptr;
  stream.forget(streamOut);
  return NS_OK;
}

nsresult
SimpleChannel::BeginAsyncRead(nsIStreamListener* listener, nsIRequest** request)
{
  NS_ENSURE_TRUE(mCallbacks, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIRequest> req;
  MOZ_TRY_VAR(req, mCallbacks->StartAsyncRead(listener, this));

  mCallbacks = nullptr;

  req.forget(request);
  return NS_OK;
}

class SimpleChannelChild final : public SimpleChannel
                               , public nsIChildChannel
                               , public PSimpleChannelChild
{
public:
  explicit SimpleChannelChild(UniquePtr<SimpleChannelCallbacks>&& aCallbacks);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSICHILDCHANNEL

protected:
  virtual void ActorDestroy(ActorDestroyReason why) override;

private:
  virtual ~SimpleChannelChild() = default;

  void AddIPDLReference();

  RefPtr<SimpleChannelChild> mIPDLRef;
};

NS_IMPL_ISUPPORTS_INHERITED(SimpleChannelChild, SimpleChannel, nsIChildChannel)

SimpleChannelChild::SimpleChannelChild(UniquePtr<SimpleChannelCallbacks>&& aCallbacks)
  : SimpleChannel(std::move(aCallbacks))
  , mIPDLRef(nullptr)
{
}

NS_IMETHODIMP
SimpleChannelChild::ConnectParent(uint32_t aId)
{
  MOZ_ASSERT(NS_IsMainThread());
  mozilla::dom::ContentChild* cc =
    static_cast<mozilla::dom::ContentChild*>(gNeckoChild->Manager());
  if (cc->IsShuttingDown()) {
    return NS_ERROR_FAILURE;
  }

  if (!gNeckoChild->SendPSimpleChannelConstructor(this, aId)) {
    return NS_ERROR_FAILURE;
  }

  // IPC now has a ref to us.
  mIPDLRef = this;
  return NS_OK;
}

NS_IMETHODIMP
SimpleChannelChild::CompleteRedirectSetup(nsIStreamListener* aListener,
                                          nsISupports* aContext)
{
  if (mIPDLRef) {
    MOZ_ASSERT(NS_IsMainThread());
  }

  nsresult rv;
  if (mLoadInfo && mLoadInfo->GetEnforceSecurity()) {
    MOZ_ASSERT(!aContext, "aContext should be null!");
    rv = AsyncOpen2(aListener);
  } else {
    rv = AsyncOpen(aListener, aContext);
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (mIPDLRef) {
    Unused << Send__delete__(this);
  }
  return NS_OK;
}

void
SimpleChannelChild::ActorDestroy(ActorDestroyReason why)
{
  MOZ_ASSERT(mIPDLRef);
  mIPDLRef = nullptr;
}


already_AddRefed<nsIChannel>
NS_NewSimpleChannelInternal(nsIURI* aURI, nsILoadInfo* aLoadInfo, UniquePtr<SimpleChannelCallbacks>&& aCallbacks)
{
  RefPtr<SimpleChannel> chan;
  if (IsNeckoChild()) {
    chan = new SimpleChannelChild(std::move(aCallbacks));
  } else {
    chan = new SimpleChannel(std::move(aCallbacks));
  }

  chan->SetURI(aURI);

  MOZ_ALWAYS_SUCCEEDS(chan->SetLoadInfo(aLoadInfo));

  return chan.forget();
}

} // namespace net
} // namespace mozilla
