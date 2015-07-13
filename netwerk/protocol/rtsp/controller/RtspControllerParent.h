/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef RtspControllerParent_h
#define RtspControllerParent_h

#include "mozilla/net/PRtspControllerParent.h"
#include "mozilla/net/NeckoParent.h"
#include "nsIStreamingProtocolController.h"
#include "nsILoadContext.h"
#include "nsIURI.h"
#include "nsCOMPtr.h"
#include "nsString.h"

namespace mozilla {
namespace net {

class RtspControllerParent : public PRtspControllerParent
                           , public nsIStreamingProtocolListener
{
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSISTREAMINGPROTOCOLLISTENER

  RtspControllerParent();

  bool RecvAsyncOpen(const URIParams& aURI);
  bool RecvPlay();
  bool RecvPause();
  bool RecvResume();
  bool RecvSuspend();
  bool RecvSeek(const uint64_t& offset);
  bool RecvStop();
  bool RecvPlaybackEnded();

 protected:
  ~RtspControllerParent();

 private:
  bool mIPCOpen;
  void ActorDestroy(ActorDestroyReason why);
  // RTSP URL refer to a stream or an aggregate of streams.
  nsCOMPtr<nsIURI> mURI;
  // The nsIStreamingProtocolController implementation.
  nsCOMPtr<nsIStreamingProtocolController> mController;
  uint32_t mTotalTracks;
  // Ensure we are destroyed on the main thread.
  void Destroy();
};

} // namespace net
} // namespace mozilla

#endif // RtspControllerParent_h
