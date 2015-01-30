/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_TEST_COMMON_FAKE_DECODER_H_
#define WEBRTC_VIDEO_ENGINE_TEST_COMMON_FAKE_DECODER_H_

#include <vector>

#include "webrtc/modules/video_coding/codecs/interface/video_codec_interface.h"
#include "webrtc/system_wrappers/interface/clock.h"

namespace webrtc {
namespace test {

class FakeDecoder : public VideoDecoder {
 public:
  FakeDecoder();
  virtual ~FakeDecoder() {}

  virtual int32_t InitDecode(const VideoCodec* config,
                             int32_t number_of_cores) OVERRIDE;

  virtual int32_t Decode(const EncodedImage& input,
                         bool missing_frames,
                         const RTPFragmentationHeader* fragmentation,
                         const CodecSpecificInfo* codec_specific_info,
                         int64_t render_time_ms) OVERRIDE;

  virtual int32_t RegisterDecodeCompleteCallback(
      DecodedImageCallback* callback) OVERRIDE;

  virtual int32_t Release() OVERRIDE;
  virtual int32_t Reset() OVERRIDE;

 private:
  VideoCodec config_;
  I420VideoFrame frame_;
  DecodedImageCallback* callback_;
};

class FakeH264Decoder : public FakeDecoder {
 public:
  virtual ~FakeH264Decoder() {}

  virtual int32_t Decode(const EncodedImage& input,
                         bool missing_frames,
                         const RTPFragmentationHeader* fragmentation,
                         const CodecSpecificInfo* codec_specific_info,
                         int64_t render_time_ms) OVERRIDE;
};
}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_TEST_COMMON_FAKE_DECODER_H_
