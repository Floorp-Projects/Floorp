/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RtspChannelParent.h"

using namespace mozilla::ipc;

namespace mozilla {
namespace net {

//-----------------------------------------------------------------------------
// RtspChannelParent
//-----------------------------------------------------------------------------
RtspChannelParent::RtspChannelParent(nsIURI *aUri)
  : mIPCClosed(false)
{
  nsBaseChannel::SetURI(aUri);
}

RtspChannelParent::~RtspChannelParent()
{
}

void
RtspChannelParent::ActorDestroy(ActorDestroyReason why)
{
  mIPCClosed = true;
}

//-----------------------------------------------------------------------------
// nsISupports
//-----------------------------------------------------------------------------
NS_IMPL_ISUPPORTS_INHERITED1(RtspChannelParent,
                             nsBaseChannel,
                             nsIParentChannel)

//-----------------------------------------------------------------------------
// RtspChannelParent methods
//-----------------------------------------------------------------------------
bool
RtspChannelParent::Init(const RtspChannelConnectArgs& aArgs)
{
  return ConnectChannel(aArgs.channelId());
}

bool
RtspChannelParent::ConnectChannel(const uint32_t& channelId)
{
  nsresult rv;
  nsCOMPtr<nsIChannel> channel;
  rv = NS_LinkRedirectChannels(channelId, this, getter_AddRefs(channel));

  return true;
}

//-----------------------------------------------------------------------------
// nsBaseChannel::nsIChannel
//-----------------------------------------------------------------------------
NS_IMETHODIMP
RtspChannelParent::GetContentType(nsACString& aContentType)
{
  aContentType.AssignLiteral("RTSP");
  return NS_OK;
}

NS_IMETHODIMP
RtspChannelParent::AsyncOpen(nsIStreamListener *aListener, nsISupports *aContext)
{
  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsBaseChannel::nsIStreamListener::nsIRequestObserver
//-----------------------------------------------------------------------------
NS_IMETHODIMP
RtspChannelParent::OnStartRequest(nsIRequest *aRequest,
                            nsISupports *aContext)
{
  MOZ_CRASH("Should never be called");
}

NS_IMETHODIMP
RtspChannelParent::OnStopRequest(nsIRequest *aRequest,
                           nsISupports *aContext,
                           nsresult aStatusCode)
{
  MOZ_CRASH("Should never be called");
}

//-----------------------------------------------------------------------------
// nsBaseChannel::nsIStreamListener
//-----------------------------------------------------------------------------
NS_IMETHODIMP
RtspChannelParent::OnDataAvailable(nsIRequest *aRequest,
                             nsISupports *aContext,
                             nsIInputStream *aInputStream,
                             uint64_t aOffset,
                             uint32_t aCount)
{
  MOZ_CRASH("Should never be called");
}

//-----------------------------------------------------------------------------
// nsBaseChannel::nsIChannel::nsIRequeset
//-----------------------------------------------------------------------------
NS_IMETHODIMP
RtspChannelParent::Cancel(nsresult status)
{
  // FIXME: This method will be called by
  // nsXMLHttpRequest::CloseRequestWithError while closing the browser app.
  // However, the root cause is RtspChannelParent will be created by
  // nsXMLHttpRequest::Open when we navigate away from an RTSP web page.
  // We should find out why it happens and decide how to fix it.
  return NS_OK;
}

NS_IMETHODIMP
RtspChannelParent::Suspend()
{
  MOZ_CRASH("Should never be called");
}

NS_IMETHODIMP
RtspChannelParent::Resume()
{
  MOZ_CRASH("Should never be called");
}

//-----------------------------------------------------------------------------
// nsBaseChannel
//-----------------------------------------------------------------------------
NS_IMETHODIMP
RtspChannelParent::OpenContentStream(bool aAsync,
                               nsIInputStream **aStream,
                               nsIChannel **aChannel)
{
  MOZ_CRASH("Should never be called");
}

//-----------------------------------------------------------------------------
// nsIParentChannel
//-----------------------------------------------------------------------------
NS_IMETHODIMP
RtspChannelParent::SetParentListener(HttpChannelParentListener *aListener)
{
  return NS_OK;
}

NS_IMETHODIMP
RtspChannelParent::Delete()
{
  return NS_OK;
}

} // namespace net
} // namespace mozilla
