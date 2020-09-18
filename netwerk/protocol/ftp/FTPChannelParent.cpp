/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/net/FTPChannelParent.h"
#include "nsStringStream.h"
#include "mozilla/net/ChannelEventQueue.h"
#include "mozilla/dom/BrowserParent.h"
#include "nsFTPChannel.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsQueryObject.h"
#include "nsFtpProtocolHandler.h"
#include "nsIAuthPrompt.h"
#include "nsIAuthPromptProvider.h"
#include "nsIHttpChannelInternal.h"
#include "nsISecureBrowserUI.h"
#include "nsIForcePendingChannel.h"
#include "mozilla/ipc/IPCStreamUtils.h"
#include "mozilla/ipc/URIUtils.h"
#include "mozilla/Unused.h"
#include "SerializedLoadContext.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/LoadInfo.h"
#include "mozilla/dom/ContentParent.h"

using namespace mozilla::dom;
using namespace mozilla::ipc;

#undef LOG
#define LOG(args) MOZ_LOG(gFTPLog, mozilla::LogLevel::Debug, args)

namespace mozilla {
namespace net {

FTPChannelParent::FTPChannelParent(dom::BrowserParent* aIframeEmbedding,
                                   nsILoadContext* aLoadContext,
                                   PBOverrideStatus aOverrideStatus)
    : mIPCClosed(false),
      mLoadContext(aLoadContext),
      mPBOverride(aOverrideStatus),
      mStatus(NS_OK),
      mBrowserParent(aIframeEmbedding),
      mUseUTF8(false) {
  nsIProtocolHandler* handler;
  CallGetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "ftp", &handler);
  MOZ_ASSERT(handler, "no ftp handler");

  mEventQ = new ChannelEventQueue(static_cast<nsIParentChannel*>(this));
}

FTPChannelParent::~FTPChannelParent() { gFtpHandler->Release(); }

void FTPChannelParent::ActorDestroy(ActorDestroyReason why) {
  // We may still have refcount>0 if the channel hasn't called OnStopRequest
  // yet, but we must not send any more msgs to child.
  mIPCClosed = true;
}

//-----------------------------------------------------------------------------
// FTPChannelParent::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS(FTPChannelParent, nsIStreamListener, nsIParentChannel,
                  nsIInterfaceRequestor, nsIRequestObserver,
                  nsIChannelEventSink, nsIFTPChannelParentInternal)

//-----------------------------------------------------------------------------
// FTPChannelParent::PFTPChannelParent
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// FTPChannelParent methods
//-----------------------------------------------------------------------------

bool FTPChannelParent::Init(const FTPChannelCreationArgs& aArgs) {
  switch (aArgs.type()) {
    case FTPChannelCreationArgs::TFTPChannelOpenArgs: {
      const FTPChannelOpenArgs& a = aArgs.get_FTPChannelOpenArgs();
      return DoAsyncOpen(a.uri(), a.startPos(), a.entityID(), a.uploadStream(),
                         a.loadInfo(), a.loadFlags());
    }
    case FTPChannelCreationArgs::TFTPChannelConnectArgs: {
      const FTPChannelConnectArgs& cArgs = aArgs.get_FTPChannelConnectArgs();
      return ConnectChannel(cArgs.channelId());
    }
    default:
      MOZ_ASSERT_UNREACHABLE("unknown open type");
      return false;
  }
}

bool FTPChannelParent::DoAsyncOpen(const URIParams& aURI,
                                   const uint64_t& aStartPos,
                                   const nsCString& aEntityID,
                                   const Maybe<IPCStream>& aUploadStream,
                                   const Maybe<LoadInfoArgs>& aLoadInfoArgs,
                                   const uint32_t& aLoadFlags) {
  nsresult rv;

  nsCOMPtr<nsIURI> uri = DeserializeURI(aURI);
  if (!uri) return false;

#ifdef DEBUG
  LOG(("FTPChannelParent DoAsyncOpen [this=%p uri=%s]\n", this,
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

  if (NS_FAILED(rv)) return SendFailedAsyncOpen(rv);

  mChannel = chan;

  // later on mChannel may become an HTTP channel (we'll be redirected to one
  // if we're using a proxy), but for now this is safe
  nsFtpChannel* ftpChan = static_cast<nsFtpChannel*>(mChannel.get());

  if (mPBOverride != kPBOverride_Unset) {
    ftpChan->SetPrivate(mPBOverride == kPBOverride_Private ? true : false);
  }
  rv = ftpChan->SetNotificationCallbacks(this);
  if (NS_FAILED(rv)) return SendFailedAsyncOpen(rv);

  nsCOMPtr<nsIInputStream> upload = DeserializeIPCStream(aUploadStream);
  if (upload) {
    // contentType and contentLength are ignored
    rv = ftpChan->SetUploadStream(upload, EmptyCString(), 0);
    if (NS_FAILED(rv)) return SendFailedAsyncOpen(rv);
  }

  rv = ftpChan->ResumeAt(aStartPos, aEntityID);
  if (NS_FAILED(rv)) return SendFailedAsyncOpen(rv);

  rv = ftpChan->AsyncOpen(this);

  if (NS_FAILED(rv)) return SendFailedAsyncOpen(rv);

  return true;
}

bool FTPChannelParent::ConnectChannel(const uint64_t& channelId) {
  nsresult rv;

  LOG(("Looking for a registered channel [this=%p, id=%" PRIx64 "]", this,
       channelId));

  nsCOMPtr<nsIChannel> channel;
  rv = NS_LinkRedirectChannels(channelId, this, getter_AddRefs(channel));
  if (NS_SUCCEEDED(rv)) mChannel = channel;

  LOG(("  found channel %p, rv=%08" PRIx32, mChannel.get(),
       static_cast<uint32_t>(rv)));

  return true;
}

mozilla::ipc::IPCResult FTPChannelParent::RecvCancel(const nsresult& status) {
  if (mChannel) mChannel->Cancel(status);

  return IPC_OK();
}

mozilla::ipc::IPCResult FTPChannelParent::RecvSuspend() {
  if (mChannel) {
    mChannel->Suspend();
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult FTPChannelParent::RecvResume() {
  if (mChannel) {
    mChannel->Resume();
  }
  return IPC_OK();
}

//-----------------------------------------------------------------------------
// FTPChannelParent::nsIRequestObserver
//-----------------------------------------------------------------------------

NS_IMETHODIMP
FTPChannelParent::OnStartRequest(nsIRequest* aRequest) {
  LOG(("FTPChannelParent::OnStartRequest [this=%p]\n", this));

  nsCOMPtr<nsIChannel> chan = do_QueryInterface(aRequest);
  MOZ_ASSERT(chan);
  NS_ENSURE_TRUE(chan, NS_ERROR_UNEXPECTED);

  // Send down any permissions which are relevant to this URL if we are
  // performing a document load.
  if (!mIPCClosed) {
    PContentParent* pcp = Manager()->Manager();
    MOZ_ASSERT(pcp, "We should have a manager if our IPC isn't closed");
    DebugOnly<nsresult> rv =
        static_cast<ContentParent*>(pcp)->AboutToLoadHttpFtpDocumentForChild(
            chan);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }

  int64_t contentLength;
  chan->GetContentLength(&contentLength);
  nsCString contentType;
  chan->GetContentType(contentType);
  nsresult channelStatus = NS_OK;
  chan->GetStatus(&channelStatus);

  nsCString entityID;
  nsCOMPtr<nsIResumableChannel> resChan = do_QueryInterface(aRequest);
  MOZ_ASSERT(
      resChan);  // both FTP and HTTP should implement nsIResumableChannel
  if (resChan) {
    resChan->GetEntityID(entityID);
  }

  PRTime lastModified = 0;
  nsCOMPtr<nsIFTPChannel> ftpChan = do_QueryInterface(aRequest);
  if (ftpChan) {
    ftpChan->GetLastModifiedTime(&lastModified);
  }
  nsCOMPtr<nsIHttpChannelInternal> httpChan = do_QueryInterface(aRequest);
  if (httpChan) {
    Unused << httpChan->GetLastModifiedTime(&lastModified);
  }

  URIParams uriparam;
  nsCOMPtr<nsIURI> uri;
  chan->GetURI(getter_AddRefs(uri));
  SerializeURI(uri, uriparam);

  if (mIPCClosed ||
      !SendOnStartRequest(channelStatus, contentLength, contentType,
                          lastModified, entityID, uriparam)) {
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

NS_IMETHODIMP
FTPChannelParent::OnStopRequest(nsIRequest* aRequest, nsresult aStatusCode) {
  LOG(("FTPChannelParent::OnStopRequest: [this=%p status=%" PRIu32 "]\n", this,
       static_cast<uint32_t>(aStatusCode)));

  if (mIPCClosed || !SendOnStopRequest(aStatusCode, mErrorMsg, mUseUTF8)) {
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

//-----------------------------------------------------------------------------
// FTPChannelParent::nsIStreamListener
//-----------------------------------------------------------------------------

NS_IMETHODIMP
FTPChannelParent::OnDataAvailable(nsIRequest* aRequest,
                                  nsIInputStream* aInputStream,
                                  uint64_t aOffset, uint32_t aCount) {
  LOG(("FTPChannelParent::OnDataAvailable [this=%p]\n", this));

  nsCString data;
  nsresult rv = NS_ReadInputStreamToString(aInputStream, data, aCount);
  if (NS_FAILED(rv)) return rv;

  nsresult channelStatus = NS_OK;
  mChannel->GetStatus(&channelStatus);

  if (mIPCClosed || !SendOnDataAvailable(channelStatus, data, aOffset, aCount))
    return NS_ERROR_UNEXPECTED;

  return NS_OK;
}

//-----------------------------------------------------------------------------
// FTPChannelParent::nsIParentChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
FTPChannelParent::SetParentListener(ParentChannelListener* aListener) {
  // Do not need ptr to ParentChannelListener.
  return NS_OK;
}

NS_IMETHODIMP
FTPChannelParent::NotifyClassificationFlags(uint32_t aClassificationFlags,
                                            bool aIsThirdParty) {
  // One day, this should probably be filled in.
  return NS_OK;
}

NS_IMETHODIMP
FTPChannelParent::NotifyFlashPluginStateChanged(
    nsIHttpChannel::FlashPluginState aState) {
  // One day, this should probably be filled in.
  return NS_OK;
}

NS_IMETHODIMP
FTPChannelParent::SetClassifierMatchedInfo(const nsACString& aList,
                                           const nsACString& aProvider,
                                           const nsACString& aFullHash) {
  // One day, this should probably be filled in.
  return NS_OK;
}

NS_IMETHODIMP
FTPChannelParent::SetClassifierMatchedTrackingInfo(
    const nsACString& aLists, const nsACString& aFullHashes) {
  // One day, this should probably be filled in.
  return NS_OK;
}

NS_IMETHODIMP
FTPChannelParent::Delete() {
  if (mIPCClosed || !SendDeleteSelf()) return NS_ERROR_UNEXPECTED;

  return NS_OK;
}

NS_IMETHODIMP
FTPChannelParent::GetRemoteType(nsACString& aRemoteType) {
  if (!CanSend()) {
    return NS_ERROR_UNEXPECTED;
  }

  dom::PContentParent* pcp = Manager()->Manager();
  aRemoteType = static_cast<dom::ContentParent*>(pcp)->GetRemoteType();
  return NS_OK;
}

//-----------------------------------------------------------------------------
// FTPChannelParent::nsIInterfaceRequestor
//-----------------------------------------------------------------------------

NS_IMETHODIMP
FTPChannelParent::GetInterface(const nsIID& uuid, void** result) {
  if (uuid.Equals(NS_GET_IID(nsIAuthPromptProvider)) ||
      uuid.Equals(NS_GET_IID(nsISecureBrowserUI))) {
    if (mBrowserParent) {
      return mBrowserParent->QueryInterface(uuid, result);
    }
  } else if (uuid.Equals(NS_GET_IID(nsIAuthPrompt)) ||
             uuid.Equals(NS_GET_IID(nsIAuthPrompt2))) {
    nsCOMPtr<nsIAuthPromptProvider> provider(do_QueryObject(mBrowserParent));
    if (provider) {
      nsresult rv = provider->GetAuthPrompt(
          nsIAuthPromptProvider::PROMPT_NORMAL, uuid, result);
      if (NS_FAILED(rv)) {
        return NS_ERROR_NO_INTERFACE;
      }
      return NS_OK;
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

//-----------------------------------------------------------------------------
// FTPChannelParent::nsIChannelEventSink
//-----------------------------------------------------------------------------

NS_IMETHODIMP
FTPChannelParent::AsyncOnChannelRedirect(
    nsIChannel* oldChannel, nsIChannel* newChannel, uint32_t redirectFlags,
    nsIAsyncVerifyRedirectCallback* callback) {
  nsCOMPtr<nsIFTPChannel> ftpChan = do_QueryInterface(newChannel);
  if (!ftpChan) {
    // when FTP is set to use HTTP proxying, we wind up getting redirected to an
    // HTTP channel.
    nsCOMPtr<nsIHttpChannel> httpChan = do_QueryInterface(newChannel);
    if (!httpChan) return NS_ERROR_UNEXPECTED;
  }
  mChannel = newChannel;
  callback->OnRedirectVerifyCallback(NS_OK);
  return NS_OK;
}

NS_IMETHODIMP
FTPChannelParent::SetErrorMsg(const char* aMsg, bool aUseUTF8) {
  mErrorMsg = aMsg;
  mUseUTF8 = aUseUTF8;
  return NS_OK;
}

//---------------------
}  // namespace net
}  // namespace mozilla
