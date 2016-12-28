/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_CODECS_VP8_VP8_FACTORY_H_
#define WEBRTC_MODULES_VIDEO_CODING_CODECS_VP8_VP8_FACTORY_H_

namespace webrtc {

// VP8EncoderFactoryConfig is the interface to control the VP8Encoder::Create
// to create VP8EncoderImpl or SimulcastEncoderAdapter+VP8EncoderImpl.
// TODO(ronghuawu): Remove when SimulcastEncoderAdapter+VP8EncoderImpl is ready
// to replace VP8EncoderImpl.
class VP8EncoderFactoryConfig {
 public:
  static void set_use_simulcast_adapter(bool use_simulcast_adapter) {
    use_simulcast_adapter_ = use_simulcast_adapter;
  }
  static bool use_simulcast_adapter() { return use_simulcast_adapter_; }

 private:
  static bool use_simulcast_adapter_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_CODECS_VP8_VP8_FACTORY_H_
