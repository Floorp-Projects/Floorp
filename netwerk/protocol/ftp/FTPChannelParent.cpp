/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/net/FTPChannelParent.h"
#include "nsFTPChannel.h"
#include "nsNetUtil.h"
#include "nsISupportsPriority.h"
#include "nsIRedirectChannelRegistrar.h"
#include "nsFtpProtocolHandler.h"
#include "mozilla/LoadContext.h"

#undef LOG
#define LOG(args) PR_LOG(gFTPLog, PR_LOG_DEBUG, args)

namespace mozilla {
namespace net {

FTPChannelParent::FTPChannelParent()
  : mIPCClosed(false)
{
  nsIProtocolHandler* handler;
  CallGetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "ftp", &handler);
  NS_ASSERTION(handler, "no ftp handler");
}

FTPChannelParent::~FTPChannelParent()
{
  gFtpHandler->Release();
}

void
FTPChannelParent::ActorDestroy(ActorDestroyReason why)
{
  // We may still have refcount>0 if the channel hasn't called OnStopRequest
  // yet, but we must not send any more msgs to child.
  mIPCClosed = true;
}

//-----------------------------------------------------------------------------
// FTPChannelParent::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS4(FTPChannelParent,
                   nsIStreamListener,
                   nsIParentChannel,
                   nsIInterfaceRequestor,
                   nsIRequestObserver);

//-----------------------------------------------------------------------------
// FTPChannelParent::PFTPChannelParent
//-----------------------------------------------------------------------------

bool
FTPChannelParent::RecvAsyncOpen(const IPC::URI& aURI,
                                const PRUint64& aStartPos,
                                const nsCString& aEntityID,
                                const IPC::InputStream& aUploadStream,
                                const IPC::SerializedLoadContext& loadContext)
{
  nsCOMPtr<nsIURI> uri(aURI);

#ifdef DEBUG
  nsCString uriSpec;
  uri->GetSpec(uriSpec);
  LOG(("FTPChannelParent RecvAsyncOpen [this=%x uri=%s]\n",
       this, uriSpec.get()));
#endif

  nsresult rv;
  nsCOMPtr<nsIIOService> ios(do_GetIOService(&rv));
  if (NS_FAILED(rv))
    return SendFailedAsyncOpen(rv);

  nsCOMPtr<nsIChannel> chan;
  rv = NS_NewChannel(getter_AddRefs(chan), uri, ios);
  if (NS_FAILED(rv))
    return SendFailedAsyncOpen(rv);

  mChannel = static_cast<nsFtpChannel*>(chan.get());
  
  nsCOMPtr<nsIInputStream> upload(aUploadStream);
  if (upload) {
    // contentType and contentLength are ignored
    rv = mChannel->SetUploadStream(upload, EmptyCString(), 0);
    if (NS_FAILED(rv))
      return SendFailedAsyncOpen(rv);
  }

  rv = mChannel->ResumeAt(aStartPos, aEntityID);
  if (NS_FAILED(rv))
    return SendFailedAsyncOpen(rv);

  if (loadContext.IsNotNull())
    mLoadContext = new LoadContext(loadContext);

  rv = mChannel->AsyncOpen(this, nullptr);
  if (NS_FAILED(rv))
    return SendFailedAsyncOpen(rv);
  
  return true;
}

bool
FTPChannelParent::RecvConnectChannel(const PRUint32& channelId)
{
  nsresult rv;

  LOG(("Looking for a registered channel [this=%p, id=%d]", this, channelId));

  nsCOMPtr<nsIChannel> channel;
  rv = NS_LinkRedirectChannels(channelId, this, getter_AddRefs(channel));
  if (NS_SUCCEEDED(rv))
    mChannel = static_cast<nsFtpChannel*>(channel.get());

  LOG(("  found channel %p, rv=%08x", mChannel.get(), rv));

  return true;
}

bool
FTPChannelParent::RecvCancel(const nsresult& status)
{
  if (mChannel)
    mChannel->Cancel(status);
  return true;
}

bool
FTPChannelParent::RecvSuspend()
{
  if (mChannel)
    mChannel->Suspend();
  return true;
}

bool
FTPChannelParent::RecvResume()
{
  if (mChannel)
    mChannel->Resume();
  return true;
}

//-----------------------------------------------------------------------------
// FTPChannelParent::nsIRequestObserver
//-----------------------------------------------------------------------------

NS_IMETHODIMP
FTPChannelParent::OnStartRequest(nsIRequest* aRequest, nsISupports* aContext)
{
  LOG(("FTPChannelParent::OnStartRequest [this=%x]\n", this));

  nsFtpChannel* chan = static_cast<nsFtpChannel*>(aRequest);
  PRInt32 aContentLength;
  chan->GetContentLength(&aContentLength);
  nsCString contentType;
  chan->GetContentType(contentType);
  nsCString entityID;
  chan->GetEntityID(entityID);
  PRTime lastModified;
  chan->GetLastModifiedTime(&lastModified);

  if (mIPCClosed || !SendOnStartRequest(aContentLength, contentType,
                                       lastModified, entityID, chan->URI())) {
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

NS_IMETHODIMP
FTPChannelParent::OnStopRequest(nsIRequest* aRequest,
                                nsISupports* aContext,
                                nsresult aStatusCode)
{
  LOG(("FTPChannelParent::OnStopRequest: [this=%x status=%ul]\n",
       this, aStatusCode));

  if (mIPCClosed || !SendOnStopRequest(aStatusCode)) {
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

//-----------------------------------------------------------------------------
// FTPChannelParent::nsIStreamListener
//-----------------------------------------------------------------------------

NS_IMETHODIMP
FTPChannelParent::OnDataAvailable(nsIRequest* aRequest,
                                  nsISupports* aContext,
                                  nsIInputStream* aInputStream,
                                  PRUint32 aOffset,
                                  PRUint32 aCount)
{
  LOG(("FTPChannelParent::OnDataAvailable [this=%x]\n", this));
  
  nsCString data;
  nsresult rv = NS_ReadInputStreamToString(aInputStream, data, aCount);
  if (NS_FAILED(rv))
    return rv;

  if (mIPCClosed || !SendOnDataAvailable(data, aOffset, aCount))
    return NS_ERROR_UNEXPECTED;

  return NS_OK;
}

//-----------------------------------------------------------------------------
// FTPChannelParent::nsIParentChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
FTPChannelParent::Delete()
{
  if (mIPCClosed || !SendDeleteSelf())
    return NS_ERROR_UNEXPECTED;

  return NS_OK;
}

//-----------------------------------------------------------------------------
// FTPChannelParent::nsIInterfaceRequestor
//-----------------------------------------------------------------------------

NS_IMETHODIMP
FTPChannelParent::GetInterface(const nsIID& uuid, void** result)
{
  // Only support nsILoadContext if child channel's callbacks did too
  if (uuid.Equals(NS_GET_IID(nsILoadContext)) && mLoadContext) {
    NS_ADDREF(mLoadContext);
    *result = static_cast<nsILoadContext*>(mLoadContext);
    return NS_OK;
  }

  return QueryInterface(uuid, result);
}

//---------------------
} // namespace net
} // namespace mozilla

