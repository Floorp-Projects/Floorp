/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/test/fake_decoder.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace webrtc {
namespace test {

FakeDecoder::FakeDecoder() : callback_(NULL) {}

int32_t FakeDecoder::InitDecode(const VideoCodec* config,
                                int32_t number_of_cores) {
  config_ = *config;
  size_t width = config->width;
  size_t height = config->height;
  frame_.CreateEmptyFrame(static_cast<int>(width),
                          static_cast<int>(height),
                          static_cast<int>(width),
                          static_cast<int>((width + 1) / 2),
                          static_cast<int>((width + 1) / 2));
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t FakeDecoder::Decode(const EncodedImage& input,
                            bool missing_frames,
                            const RTPFragmentationHeader* fragmentation,
                            const CodecSpecificInfo* codec_specific_info,
                            int64_t render_time_ms) {
  frame_.set_timestamp(input._timeStamp);
  frame_.set_ntp_time_ms(input.ntp_time_ms_);
  frame_.set_render_time_ms(render_time_ms);

  callback_->Decoded(frame_);

  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t FakeDecoder::RegisterDecodeCompleteCallback(
    DecodedImageCallback* callback) {
  callback_ = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t FakeDecoder::Release() {
  return WEBRTC_VIDEO_CODEC_OK;
}
int32_t FakeDecoder::Reset() {
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t FakeH264Decoder::Decode(const EncodedImage& input,
                                bool missing_frames,
                                const RTPFragmentationHeader* fragmentation,
                                const CodecSpecificInfo* codec_specific_info,
                                int64_t render_time_ms) {
  uint8_t value = 0;
  for (size_t i = 0; i < input._length; ++i) {
    uint8_t kStartCode[] = {0, 0, 0, 1};
    if (i < input._length - sizeof(kStartCode) &&
        !memcmp(&input._buffer[i], kStartCode, sizeof(kStartCode))) {
      i += sizeof(kStartCode) + 1;  // Skip start code and NAL header.
    }
    if (input._buffer[i] != value) {
      EXPECT_EQ(value, input._buffer[i])
          << "Bitstream mismatch between sender and receiver.";
      return -1;
    }
    ++value;
  }
  return FakeDecoder::Decode(input,
                             missing_frames,
                             fragmentation,
                             codec_specific_info,
                             render_time_ms);
}

}  // namespace test
}  // namespace webrtc
