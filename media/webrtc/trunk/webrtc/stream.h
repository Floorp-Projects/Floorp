/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_STREAM_H_
#define WEBRTC_STREAM_H_

#include "webrtc/common_types.h"

namespace webrtc {

enum NetworkState {
  kNetworkUp,
  kNetworkDown,
};

// Common base class for streams.
class Stream {
 public:
  // Starts stream activity.
  // When a stream is active, it can receive, process and deliver packets.
  virtual void Start() = 0;
  // Stops stream activity.
  // When a stream is stopped, it can't receive, process or deliver packets.
  virtual void Stop() = 0;
  // Called to notify that network state has changed, so that the stream can
  // respond, e.g. by pausing or resuming activity.
  virtual void SignalNetworkState(NetworkState state) = 0;
  // Called when a RTCP packet is received.
  virtual bool DeliverRtcp(const uint8_t* packet, size_t length) = 0;

 protected:
  virtual ~Stream() {}
};

// Common base class for receive streams.
class ReceiveStream : public Stream {
 public:
  // Called when a RTP packet is received.
  virtual bool DeliverRtp(const uint8_t* packet,
                          size_t length,
                          const PacketTime& packet_time) = 0;
};

// Common base class for send streams.
// A tag class that denotes send stream type.
class SendStream : public Stream {};

}  // namespace webrtc

#endif  // WEBRTC_STREAM_H_
