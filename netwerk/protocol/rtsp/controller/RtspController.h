/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef RtspController_h
#define RtspController_h

#include "nsIStreamingProtocolController.h"
#include "nsIChannel.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "RTSPSource.h"

namespace mozilla {
namespace net {

class RtspController : public nsIStreamingProtocolController
                     , public nsIStreamingProtocolListener
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSISTREAMINGPROTOCOLCONTROLLER
  NS_DECL_NSISTREAMINGPROTOCOLLISTENER

  RtspController(nsIChannel *channel);
  ~RtspController();

private:
  enum State {
    INIT,
    CONNECTED,
    DISCONNECTED
  };

  // RTSP URL refer to a stream or an aggregate of streams.
  nsCOMPtr<nsIURI> mURI;
  // The nsIStreamingProtocolListener implementation.
  nsMainThreadPtrHandle<nsIStreamingProtocolListener> mListener;
  // ASCII encoded URL spec.
  nsCString mSpec;
  // Indicate the connection state between the
  // media streaming server and the Rtsp client.
  State mState;
  // Rtsp Streaming source.
  android::sp<android::RTSPSource> mRtspSource;
};

}
} // namespace mozilla::net
#endif
