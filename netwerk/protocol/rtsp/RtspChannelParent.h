/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef RtspChannelParent_h
#define RtspChannelParent_h

#include "mozilla/net/PRtspChannelParent.h"
#include "mozilla/net/NeckoParent.h"
#include "nsBaseChannel.h"
#include "nsIParentChannel.h"

namespace mozilla {
namespace net {

//-----------------------------------------------------------------------------
// Note: RtspChannel doesn't transport streams as normal channel does.
// (See RtspChannelChild.h for detail).
// The reason for the existence of RtspChannelParent is to support HTTP->RTSP
// redirection.
// When redirection happens, two instances of RtspChannelParent will be created:
// - One will be created when HTTP creates the new channel for redirects, and
//   will be registered as an nsIChannel.
// - The other will be created via IPDL by RtspChannelChild, and will be
//   registered as an nsIParentChannel.
class RtspChannelParent : public PRtspChannelParent
                        , public nsBaseChannel
                        , public nsIParentChannel
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPARENTCHANNEL

  RtspChannelParent(nsIURI *aUri);
  ~RtspChannelParent();

  // nsBaseChannel::nsIChannel
  NS_IMETHOD GetContentType(nsACString & aContentType) MOZ_OVERRIDE MOZ_FINAL;
  NS_IMETHOD AsyncOpen(nsIStreamListener *listener,
                       nsISupports *aContext) MOZ_OVERRIDE MOZ_FINAL;

  // nsBaseChannel::nsIStreamListener::nsIRequestObserver
  NS_IMETHOD OnStartRequest(nsIRequest *aRequest,
                            nsISupports *aContext) MOZ_OVERRIDE MOZ_FINAL;
  NS_IMETHOD OnStopRequest(nsIRequest *aRequest,
                           nsISupports *aContext,
                           nsresult aStatusCode) MOZ_OVERRIDE MOZ_FINAL;

  // nsBaseChannel::nsIStreamListener
  NS_IMETHOD OnDataAvailable(nsIRequest *aRequest,
                             nsISupports *aContext,
                             nsIInputStream *aInputStream,
                             uint64_t aOffset,
                             uint32_t aCount) MOZ_OVERRIDE MOZ_FINAL;

  // nsBaseChannel::nsIChannel::nsIRequest
  NS_IMETHOD Cancel(nsresult status) MOZ_OVERRIDE MOZ_FINAL;
  NS_IMETHOD Suspend() MOZ_OVERRIDE MOZ_FINAL;
  NS_IMETHOD Resume() MOZ_OVERRIDE MOZ_FINAL;

  // nsBaseChannel
  NS_IMETHOD OpenContentStream(bool aAsync,
                               nsIInputStream **aStream,
                               nsIChannel **aChannel) MOZ_OVERRIDE MOZ_FINAL;

  // RtspChannelParent
  bool Init(const RtspChannelConnectArgs& aArgs);

protected:
  // Used to connect redirected-to channel in parent with just created
  // ChildChannel. Used during HTTP->RTSP redirection.
  bool ConnectChannel(const uint32_t& channelId);

private:
  bool mIPCClosed;
  virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE;
};

} // namespace net
} // namespace mozilla

#endif // RtspChannelParent_h
