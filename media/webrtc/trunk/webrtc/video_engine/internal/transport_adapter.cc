/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video_engine/internal/transport_adapter.h"

namespace webrtc {
namespace internal {

TransportAdapter::TransportAdapter(newapi::Transport* transport)
    : transport_(transport) {}

int TransportAdapter::SendPacket(int /*channel*/,
                                 const void* packet,
                                 int length) {
  bool success = transport_->SendRTP(static_cast<const uint8_t*>(packet),
                                     static_cast<size_t>(length));
  return success ? length : -1;
}

int TransportAdapter::SendRTCPPacket(int /*channel*/,
                                     const void* packet,
                                     int length) {
  bool success = transport_->SendRTCP(static_cast<const uint8_t*>(packet),
                                      static_cast<size_t>(length));
  return success ? length : -1;
}

}  // namespace internal
}  // namespace webrtc
