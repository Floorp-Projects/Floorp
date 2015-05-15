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

  // nsBaseChannel::nsIChannel
  NS_IMETHOD GetContentType(nsACString & aContentType) override final;
  NS_IMETHOD AsyncOpen(nsIStreamListener *listener, nsISupports *aContext)
                       override final;
  NS_IMETHOD AsyncOpen2(nsIStreamListener *listener) override final;

  // nsBaseChannel::nsIStreamListener::nsIRequestObserver
  NS_IMETHOD OnStartRequest(nsIRequest *aRequest, nsISupports *aContext)
                            override final;
  NS_IMETHOD OnStopRequest(nsIRequest *aRequest,
                           nsISupports *aContext,
                           nsresult aStatusCode) override final;

  // nsBaseChannel::nsIStreamListener
  NS_IMETHOD OnDataAvailable(nsIRequest *aRequest,
                             nsISupports *aContext,
                             nsIInputStream *aInputStream,
                             uint64_t aOffset,
                             uint32_t aCount) override final;

  // nsBaseChannel::nsIChannel::nsIRequest
  NS_IMETHOD Cancel(nsresult status) override final;
  NS_IMETHOD Suspend() override final;
  NS_IMETHOD Resume() override final;

  // nsBaseChannel
  NS_IMETHOD OpenContentStream(bool aAsync,
                               nsIInputStream **aStream,
                               nsIChannel **aChannel) override final;

  // IPDL
  void AddIPDLReference();
  void ReleaseIPDLReference();

  // RtspChannelChild
  nsIStreamingProtocolController* GetController();
  void ReleaseController();

protected:
  ~RtspChannelChild();

private:
  bool mIPCOpen;
  bool mCanceled;
  nsCOMPtr<nsIStreamingProtocolController> mMediaStreamController;
};

} // namespace net
} // namespace mozilla

#endif // RtspChannelChild_h
