/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_TEST_COMMON_FAKE_ENCODER_H_
#define WEBRTC_VIDEO_ENGINE_TEST_COMMON_FAKE_ENCODER_H_

#include <vector>

#include "webrtc/common_types.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/video_encoder.h"

namespace webrtc {
namespace test {

class FakeEncoder : public VideoEncoder {
 public:
  explicit FakeEncoder(Clock* clock);
  virtual ~FakeEncoder();

  // Sets max bitrate. Not thread-safe, call before registering the encoder.
  void SetMaxBitrate(int max_kbps);

  virtual int32_t InitEncode(const VideoCodec* config,
                             int32_t number_of_cores,
                             uint32_t max_payload_size) OVERRIDE;
  virtual int32_t Encode(
     const I420VideoFrame& input_image,
     const CodecSpecificInfo* codec_specific_info,
     const std::vector<VideoFrameType>* frame_types) OVERRIDE;
  virtual int32_t RegisterEncodeCompleteCallback(
      EncodedImageCallback* callback) OVERRIDE;
  virtual int32_t Release() OVERRIDE;
  virtual int32_t SetChannelParameters(uint32_t packet_loss, int rtt) OVERRIDE;
  virtual int32_t SetRates(uint32_t new_target_bitrate,
                           uint32_t framerate) OVERRIDE;

 private:
  Clock* const clock_;
  VideoCodec config_;
  EncodedImageCallback* callback_;
  int target_bitrate_kbps_;
  int max_target_bitrate_kbps_;
  int64_t last_encode_time_ms_;
  uint8_t encoded_buffer_[100000];
};

class FakeH264Encoder : public FakeEncoder, public EncodedImageCallback {
 public:
  explicit FakeH264Encoder(Clock* clock);
  virtual ~FakeH264Encoder() {}

  virtual int32_t RegisterEncodeCompleteCallback(
      EncodedImageCallback* callback) OVERRIDE;

  virtual int32_t Encoded(
      EncodedImage& encodedImage,
      const CodecSpecificInfo* codecSpecificInfo = NULL,
      const RTPFragmentationHeader* fragments = NULL) OVERRIDE;

 private:
  EncodedImageCallback* callback_;
  int idr_counter_;
};
}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_TEST_COMMON_FAKE_ENCODER_H_
