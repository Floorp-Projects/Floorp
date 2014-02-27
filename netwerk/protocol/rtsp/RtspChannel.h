/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef RtspChannel_h
#define RtspChannel_h

#include "nsBaseChannel.h"

namespace mozilla {
namespace net {

//-----------------------------------------------------------------------------
// RtspChannel is a dummy channel used to aid MediaResource creation in
// HTMLMediaElement. Actual network control and data flows are managed by an
// RtspController object created and owned by RtspMediaResource.
// Therefore, when RtspChannel::AsyncOpen is called, mListener->OnStartRequest
// will be called immediately. It is expected that an RtspMediaResource object
// will be created in that calling context or after; the RtspController object
// will be created internally by RtspMediaResource."

class RtspChannel : public nsBaseChannel
{
public:
  NS_DECL_ISUPPORTS

  RtspChannel() { }

  ~RtspChannel() { }

  // Overrides nsBaseChannel::AsyncOpen and call listener's OnStartRequest immediately.
  NS_IMETHOD AsyncOpen(nsIStreamListener *listener,
                       nsISupports *aContext) MOZ_OVERRIDE MOZ_FINAL;
  // Set Rtsp URL.
  NS_IMETHOD Init(nsIURI* uri);
  // Overrides nsBaseChannel::GetContentType, return streaming protocol type "RTSP".
  NS_IMETHOD GetContentType(nsACString & aContentType) MOZ_OVERRIDE MOZ_FINAL;

  NS_IMETHOD OpenContentStream(bool aAsync,
                               nsIInputStream **aStream,
                               nsIChannel **aChannel) MOZ_OVERRIDE MOZ_FINAL
  {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // nsIRequestObserver
  NS_IMETHOD OnStartRequest(nsIRequest *aRequest,
                            nsISupports *aContext) MOZ_OVERRIDE MOZ_FINAL
  {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  NS_IMETHOD OnStopRequest(nsIRequest *aRequest,
                           nsISupports *aContext,
                           nsresult aStatusCode) MOZ_OVERRIDE MOZ_FINAL
  {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // nsIStreamListener
  NS_IMETHOD OnDataAvailable(nsIRequest *aRequest,
                             nsISupports *aContext,
                             nsIInputStream *aInputStream,
                             uint64_t aOffset,
                             uint32_t aCount) MOZ_OVERRIDE MOZ_FINAL
  {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
};

}
} // namespace mozilla::net
#endif // RtspChannel_h
