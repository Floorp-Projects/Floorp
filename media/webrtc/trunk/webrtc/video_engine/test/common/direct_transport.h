/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_VIDEO_ENGINE_TEST_COMMON_DIRECT_TRANSPORT_H_
#define WEBRTC_VIDEO_ENGINE_TEST_COMMON_DIRECT_TRANSPORT_H_

#include "webrtc/video_engine/new_include/transport.h"

namespace webrtc {
namespace test {

class DirectTransport : public newapi::Transport {
 public:
  explicit DirectTransport(newapi::PacketReceiver* receiver)
      : receiver_(receiver) {}

  void SetReceiver(newapi::PacketReceiver* receiver) { receiver_ = receiver; }

  bool SendRTP(const void* data, size_t length) OVERRIDE {
    return receiver_->DeliverPacket(data, length);
  }

  bool SendRTCP(const void* data, size_t length) OVERRIDE {
    return SendRTP(data, length);
  }

 private:
  newapi::PacketReceiver* receiver_;
};
}  // test
}  // webrtc

#endif  // WEBRTC_VIDEO_ENGINE_TEST_COMMON_DIRECT_TRANSPORT_H_
