/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_TEST_ENCODER_SETTINGS_H_
#define WEBRTC_TEST_ENCODER_SETTINGS_H_

#include "webrtc/video_receive_stream.h"
#include "webrtc/video_send_stream.h"

namespace webrtc {
namespace test {
std::vector<VideoStream> CreateVideoStreams(size_t num_streams);

VideoReceiveStream::Decoder CreateMatchingDecoder(
    const VideoSendStream::Config::EncoderSettings& encoder_settings);
}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_TEST_ENCODER_SETTINGS_H_
