/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef RtspChannelChild_h
#define RtspChannelChild_h

#include "mozilla/net/PRtspChannelChild.h"
#include "mozilla/net/NeckoChild.h"
#include "nsBaseChannel.h"
#include "nsIChildChannel.h"
#include "nsIStreamingProtocolController.h"
#include "nsIStreamingProtocolService.h"

namespace mozilla {
namespace net {

//-----------------------------------------------------------------------------
// RtspChannelChild is a dummy channel used to aid MediaResource creation in
// HTMLMediaElement. Network control and data flows are managed by an
// RtspController object, which is created by us and manipulated by
// RtspMediaResource. This object is also responsible for inter-process
// communication with the parent process.
// When RtspChannelChild::AsyncOpen is called, it should create an
// RtspController object, dispatch an OnStartRequest and immediately return.
// We expect an RtspMediaResource object will be created in the calling context
// and it will use the RtpController we create.

class RtspChannelChild : public PRtspChannelChild
                       , public nsBaseChannel
                       , public nsIChildChannel
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICHILDCHANNEL

  RtspChannelChild(nsIURI *aUri);
  ~RtspChannelChild();

  // nsBaseChannel::nsIChannel
  NS_IMETHOD GetContentType(nsACString & aContentType) MOZ_OVERRIDE MOZ_FINAL;
  NS_IMETHOD AsyncOpen(nsIStreamListener *listener, nsISupports *aContext)
                       MOZ_OVERRIDE MOZ_FINAL;

  // nsBaseChannel::nsIStreamListener::nsIRequestObserver
  NS_IMETHOD OnStartRequest(nsIRequest *aRequest, nsISupports *aContext)
                            MOZ_OVERRIDE MOZ_FINAL;
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

  // IPDL
  void AddIPDLReference();
  void ReleaseIPDLReference();

  // RtspChannelChild
  nsIStreamingProtocolController* GetController();
  void ReleaseController();

private:
  bool mIPCOpen;
  bool mCanceled;
  nsCOMPtr<nsIStreamingProtocolController> mMediaStreamController;
};

} // namespace net
} // namespace mozilla

#endif // RtspChannelChild_h
