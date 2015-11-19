/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video/transport_adapter.h"

namespace webrtc {
namespace internal {

TransportAdapter::TransportAdapter(newapi::Transport* transport)
    : transport_(transport), enabled_(0) {}

int TransportAdapter::SendPacket(int /*channel*/,
                                 const void* packet,
                                 size_t length) {
  if (enabled_.Value() == 0)
    return false;

  bool success = transport_->SendRtp(static_cast<const uint8_t*>(packet),
                                     length);
  return success ? static_cast<int>(length) : -1;
}

int TransportAdapter::SendRTCPPacket(int /*channel*/,
                                     const void* packet,
                                     size_t length) {
  if (enabled_.Value() == 0)
    return false;

  bool success = transport_->SendRtcp(static_cast<const uint8_t*>(packet),
                                      length);
  return success ? static_cast<int>(length) : -1;
}

void TransportAdapter::Enable() {
  // If this exchange fails it means enabled_ was already true, no need to
  // check result and iterate.
  enabled_.CompareExchange(1, 0);
}

void TransportAdapter::Disable() { enabled_.CompareExchange(0, 1); }

}  // namespace internal
}  // namespace webrtc
