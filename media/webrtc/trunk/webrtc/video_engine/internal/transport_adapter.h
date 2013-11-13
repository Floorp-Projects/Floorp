/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_VIDEO_ENGINE_INTERNAL_TRANSPORT_ADAPTER_H_
#define WEBRTC_VIDEO_ENGINE_INTERNAL_TRANSPORT_ADAPTER_H_

#include "webrtc/common_types.h"
#include "webrtc/video_engine/new_include/transport.h"

namespace webrtc {
namespace internal {

class TransportAdapter : public webrtc::Transport {
 public:
  explicit TransportAdapter(newapi::Transport* transport);

  virtual int SendPacket(int /*channel*/, const void* packet, int length)
      OVERRIDE;
  virtual int SendRTCPPacket(int /*channel*/, const void* packet, int length)
      OVERRIDE;

 private:
  newapi::Transport *transport_;
};
}  // namespace internal
}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_INTERNAL_TRANSPORT_ADAPTER_H_
