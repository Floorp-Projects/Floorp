/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#ifndef MEDIA_ENGINE_ENCODER_SIMULCAST_PROXY_H_
#define MEDIA_ENGINE_ENCODER_SIMULCAST_PROXY_H_

#include <stddef.h>
#include <stdint.h>
#include <memory>
#include <vector>

#include "api/video/video_frame.h"
#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_encoder.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "common_types.h"  // NOLINT(build/include)
#include "modules/video_coding/include/video_codec_interface.h"

namespace webrtc {

// This class provides fallback to SimulcastEncoderAdapter if default VP8Encoder
// doesn't support simulcast for provided settings.
class EncoderSimulcastProxy : public VideoEncoder {
 public:
  EncoderSimulcastProxy(VideoEncoderFactory* factory,
                        const SdpVideoFormat& format);
  // Deprecated. Remove once all clients use constructor with both factory and
  // SdpVideoFormat;
  explicit EncoderSimulcastProxy(VideoEncoderFactory* factory);

  ~EncoderSimulcastProxy() override;

  // Implements VideoEncoder.
  int Release() override;
  int InitEncode(const VideoCodec* inst, int number_of_cores,
                 size_t max_payload_size) override;
  int Encode(const VideoFrame& input_image,
             const CodecSpecificInfo* codec_specific_info,
             const std::vector<FrameType>* frame_types) override;
  int RegisterEncodeCompleteCallback(EncodedImageCallback* callback) override;
  int SetRateAllocation(const BitrateAllocation& bitrate,
                        uint32_t new_framerate) override;
  int SetChannelParameters(uint32_t packet_loss, int64_t rtt) override;

 private:
  VideoEncoderFactory* const factory_;
  SdpVideoFormat video_format_;
  std::unique_ptr<VideoEncoder> encoder_;
  EncodedImageCallback* callback_;
};

}  // namespace webrtc

#endif  // MEDIA_ENGINE_ENCODER_SIMULCAST_PROXY_H_
