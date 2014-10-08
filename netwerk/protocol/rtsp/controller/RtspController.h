/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef RtspController_h
#define RtspController_h

#include "mozilla/Mutex.h"
#include "nsIStreamingProtocolController.h"
#include "nsIChannel.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsITimer.h"
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

  // These callbacks will be called when mPlayTimer/mPauseTimer fires.
  static void PlayTimerCallback(nsITimer *aTimer, void *aClosure);
  static void PauseTimerCallback(nsITimer *aTimer, void *aClosure);

private:
  enum State {
    INIT,
    CONNECTED,
    DISCONNECTED
  };

  // RTSP URL refer to a stream or an aggregate of streams.
  nsCOMPtr<nsIURI> mURI;
  // The nsIStreamingProtocolListener implementation.
  nsCOMPtr<nsIStreamingProtocolListener> mListener;
  // ASCII encoded URL spec.
  nsCString mSpec;
  // UserAgent string.
  nsCString mUserAgent;
  // Indicate the connection state between the
  // media streaming server and the Rtsp client.
  State mState;
  // Rtsp Streaming source.
  android::sp<android::RTSPSource> mRtspSource;
  // This lock protects mPlayTimer and mPauseTimer.
  Mutex mTimerLock;
  // Timers to delay the play and pause operations.
  // They are used for optimization and avoid sending unnecessary requests to
  // the server.
  nsCOMPtr<nsITimer> mPlayTimer;
  nsCOMPtr<nsITimer> mPauseTimer;
};

}
} // namespace mozilla::net
#endif
