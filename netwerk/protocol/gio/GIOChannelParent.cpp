/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=4 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GIOChannelParent.h"
#include "nsGIOProtocolHandler.h"
#include "mozilla/Assertions.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/net/NeckoParent.h"
#include "nsNetUtil.h"
#include "nsIChannel.h"
#include "mozilla/net/NeckoChannelParams.h"
#include "nsIAuthPrompt.h"
#include "nsIAuthPromptProvider.h"
#include "nsISecureBrowserUI.h"
#include "nsQueryObject.h"
#include "mozilla/Logging.h"
#include "mozilla/net/ChannelEventQueue.h"
#include "mozilla/ipc/URIUtils.h"

using namespace mozilla::dom;
using namespace mozilla::ipc;

namespace mozilla {
#undef LOG
#define LOG(args) MOZ_LOG(gGIOLog, mozilla::LogLevel::Debug, args)
namespace net {

GIOChannelParent::GIOChannelParent(dom::BrowserParent* aIframeEmbedding,
                                   nsILoadContext* aLoadContext,
                                   PBOverrideStatus aOverrideStatus)
    : mLoadContext(aLoadContext),
      mPBOverride(aOverrideStatus),
      mBrowserParent(aIframeEmbedding) {
  mEventQ = new ChannelEventQueue(static_cast<nsIParentChannel*>(this));
}

void GIOChannelParent::ActorDestroy(ActorDestroyReason why) {
  // We may still have refcount>0 if the channel hasn't called OnStopRequest
  // yet, but we must not send any more msgs to child.
  mIPCClosed = true;
}

//-----------------------------------------------------------------------------
// GIOChannelParent::nsISupports
//-----------------------------------------------------------------------------
NS_IMPL_ISUPPORTS(GIOChannelParent, nsIStreamListener, nsIParentChannel,
                  nsIInterfaceRequestor, nsIRequestObserver)

//-----------------------------------------------------------------------------
// GIOChannelParent methods
//-----------------------------------------------------------------------------

bool GIOChannelParent::Init(const GIOChannelCreationArgs& aOpenArgs) {
  switch (aOpenArgs.type()) {
    case GIOChannelCreationArgs::TGIOChannelOpenArgs: {
      const GIOChannelOpenArgs& a = aOpenArgs.get_GIOChannelOpenArgs();
      return DoAsyncOpen(a.uri(), a.startPos(), a.entityID(), a.uploadStream(),
                         a.loadInfo(), a.loadFlags());
    }
    case GIOChannelCreationArgs::TGIOChannelConnectArgs: {
      const GIOChannelConnectArgs& cArgs =
          aOpenArgs.get_GIOChannelConnectArgs();
      return ConnectChannel(cArgs.channelId());
    }
    default:
      MOZ_ASSERT_UNREACHABLE("unknown open type");
      return false;
  }
}

bool GIOChannelParent::DoAsyncOpen(const URIParams& aURI,
                                   const uint64_t& aStartPos,
                                   const nsCString& aEntityID,
                                   const Maybe<IPCStream>& aUploadStream,
                                   const Maybe<LoadInfoArgs>& aLoadInfoArgs,
                                   const uint32_t& aLoadFlags) {
  nsresult rv;

  nsCOMPtr<nsIURI> uri = DeserializeURI(aURI);
  if (!uri) {
    return false;
  }

#ifdef DEBUG
  LOG(("GIOChannelParent DoAsyncOpen [this=%p uri=%s]\n", this,
       uri->GetSpecOrDefault().get()));
#endif

  nsCOMPtr<nsIIOService> ios(do_GetIOService(&rv));
  if (NS_FAILED(rv)) {
    return SendFailedAsyncOpen(rv);
  }

  nsCOMPtr<nsILoadInfo> loadInfo;
  rv = mozilla::ipc::LoadInfoArgsToLoadInfo(aLoadInfoArgs,
                                            getter_AddRefs(loadInfo));
  if (NS_FAILED(rv)) {
    return SendFailedAsyncOpen(rv);
  }

  OriginAttributes attrs;
  rv = loadInfo->GetOriginAttributes(&attrs);
  if (NS_FAILED(rv)) {
    return SendFailedAsyncOpen(rv);
  }

  nsCOMPtr<nsIChannel> chan;
  rv = NS_NewChannelInternal(getter_AddRefs(chan), uri, loadInfo, nullptr,
                             nullptr, nullptr, aLoadFlags, ios);

  if (NS_FAILED(rv)) {
    return SendFailedAsyncOpen(rv);
  }

  mChannel = chan;

  nsIChannel* gioChan = static_cast<nsIChannel*>(mChannel.get());

  rv = gioChan->AsyncOpen(this);
  if (NS_FAILED(rv)) {
    return SendFailedAsyncOpen(rv);
  }

  return true;
}

bool GIOChannelParent::ConnectChannel(const uint64_t& channelId) {
  nsresult rv;

  LOG(("Looking for a registered channel [this=%p, id=%" PRIx64 "]", this,
       channelId));

  nsCOMPtr<nsIChannel> channel;
  rv = NS_LinkRedirectChannels(channelId, this, getter_AddRefs(channel));
  if (NS_SUCCEEDED(rv)) {
    mChannel = channel;
  }

  LOG(("  found channel %p, rv=%08" PRIx32, mChannel.get(),
       static_cast<uint32_t>(rv)));

  return true;
}

mozilla::ipc::IPCResult GIOChannelParent::RecvCancel(const nsresult& status) {
  if (mChannel) {
    mChannel->Cancel(status);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult GIOChannelParent::RecvSuspend() {
  if (mChannel) {
    mChannel->Suspend();
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult GIOChannelParent::RecvResume() {
  if (mChannel) {
    mChannel->Resume();
  }
  return IPC_OK();
}

//-----------------------------------------------------------------------------
// GIOChannelParent::nsIRequestObserver
//-----------------------------------------------------------------------------

NS_IMETHODIMP
GIOChannelParent::OnStartRequest(nsIRequest* aRequest) {
  LOG(("GIOChannelParent::OnStartRequest [this=%p]\n", this));
  nsCOMPtr<nsIChannel> chan = do_QueryInterface(aRequest);
  MOZ_ASSERT(chan);
  NS_ENSURE_TRUE(chan, NS_ERROR_UNEXPECTED);

  int64_t contentLength;
  chan->GetContentLength(&contentLength);
  nsCString contentType;
  chan->GetContentType(contentType);
  nsresult channelStatus = NS_OK;
  chan->GetStatus(&channelStatus);

  nsCString entityID;
  URIParams uriparam;
  nsCOMPtr<nsIURI> uri;
  chan->GetURI(getter_AddRefs(uri));
  SerializeURI(uri, uriparam);

  if (mIPCClosed || !SendOnStartRequest(channelStatus, contentLength,
                                        contentType, entityID, uriparam)) {
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

NS_IMETHODIMP
GIOChannelParent::OnStopRequest(nsIRequest* aRequest, nsresult aStatusCode) {
  LOG(("GIOChannelParent::OnStopRequest: [this=%p status=%" PRIu32 "]\n", this,
       static_cast<uint32_t>(aStatusCode)));

  if (mIPCClosed || !SendOnStopRequest(aStatusCode)) {
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

//-----------------------------------------------------------------------------
// GIOChannelParent::nsIStreamListener
//-----------------------------------------------------------------------------

NS_IMETHODIMP
GIOChannelParent::OnDataAvailable(nsIRequest* aRequest,
                                  nsIInputStream* aInputStream,
                                  uint64_t aOffset, uint32_t aCount) {
  LOG(("GIOChannelParent::OnDataAvailable [this=%p]\n", this));

  nsCString data;
  nsresult rv = NS_ReadInputStreamToString(aInputStream, data, aCount);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsresult channelStatus = NS_OK;
  mChannel->GetStatus(&channelStatus);

  if (mIPCClosed ||
      !SendOnDataAvailable(channelStatus, data, aOffset, aCount)) {
    return NS_ERROR_UNEXPECTED;
  }
  return NS_OK;
}

//-----------------------------------------------------------------------------
// GIOChannelParent::nsIParentChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
GIOChannelParent::SetParentListener(ParentChannelListener* aListener) {
  // Do not need ptr to ParentChannelListener.
  return NS_OK;
}

NS_IMETHODIMP
GIOChannelParent::NotifyClassificationFlags(uint32_t aClassificationFlags,
                                            bool aIsThirdParty) {
  // Nothing to do.
  return NS_OK;
}

NS_IMETHODIMP
GIOChannelParent::SetClassifierMatchedInfo(const nsACString& aList,
                                           const nsACString& aProvider,
                                           const nsACString& aFullHash) {
  // nothing to do
  return NS_OK;
}

NS_IMETHODIMP
GIOChannelParent::SetClassifierMatchedTrackingInfo(
    const nsACString& aLists, const nsACString& aFullHashes) {
  // nothing to do
  return NS_OK;
}

NS_IMETHODIMP
GIOChannelParent::Delete() {
  if (mIPCClosed || !SendDeleteSelf()) {
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

NS_IMETHODIMP
GIOChannelParent::GetRemoteType(nsACString& aRemoteType) {
  if (!CanSend()) {
    return NS_ERROR_UNEXPECTED;
  }

  dom::PContentParent* pcp = Manager()->Manager();
  aRemoteType = static_cast<dom::ContentParent*>(pcp)->GetRemoteType();
  return NS_OK;
}

//-----------------------------------------------------------------------------
// GIOChannelParent::nsIInterfaceRequestor
//-----------------------------------------------------------------------------

NS_IMETHODIMP
GIOChannelParent::GetInterface(const nsIID& uuid, void** result) {
  if (uuid.Equals(NS_GET_IID(nsIAuthPromptProvider)) ||
      uuid.Equals(NS_GET_IID(nsISecureBrowserUI))) {
    if (mBrowserParent) {
      return mBrowserParent->QueryInterface(uuid, result);
    }
  }

  // Only support nsILoadContext if child channel's callbacks did too
  if (uuid.Equals(NS_GET_IID(nsILoadContext)) && mLoadContext) {
    nsCOMPtr<nsILoadContext> copy = mLoadContext;
    copy.forget(result);
    return NS_OK;
  }

  return QueryInterface(uuid, result);
}

//---------------------
}  // namespace net
}  // namespace mozilla
