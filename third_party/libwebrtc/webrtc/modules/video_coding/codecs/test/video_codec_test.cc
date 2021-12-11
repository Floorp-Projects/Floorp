/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/test/video_codec_test.h"

#include "api/video/i420_buffer.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "test/frame_utils.h"
#include "test/testsupport/fileutils.h"

static const int kEncodeTimeoutMs = 100;
static const int kDecodeTimeoutMs = 25;
// Set bitrate to get higher quality.
static const int kStartBitrate = 300;
static const int kTargetBitrate = 2000;
static const int kMaxBitrate = 4000;
static const int kWidth = 172;        // Width of the input image.
static const int kHeight = 144;       // Height of the input image.
static const int kMaxFramerate = 30;  // Arbitrary value.

namespace webrtc {

EncodedImageCallback::Result
VideoCodecTest::FakeEncodeCompleteCallback::OnEncodedImage(
    const EncodedImage& frame,
    const CodecSpecificInfo* codec_specific_info,
    const RTPFragmentationHeader* fragmentation) {
  rtc::CritScope lock(&test_->encoded_frame_section_);
  test_->encoded_frame_.emplace(frame);
  RTC_DCHECK(codec_specific_info);
  test_->codec_specific_info_.codecType = codec_specific_info->codecType;
  // Skip |codec_name|, to avoid allocating.
  test_->codec_specific_info_.codecSpecific =
      codec_specific_info->codecSpecific;
  test_->encoded_frame_event_.Set();
  return Result(Result::OK);
}

void VideoCodecTest::FakeDecodeCompleteCallback::Decoded(
    VideoFrame& frame,
    rtc::Optional<int32_t> decode_time_ms,
    rtc::Optional<uint8_t> qp) {
  rtc::CritScope lock(&test_->decoded_frame_section_);
  test_->decoded_frame_.emplace(frame);
  test_->decoded_qp_ = qp;
  test_->decoded_frame_event_.Set();
}

void VideoCodecTest::SetUp() {
  // Using a QCIF image. Processing only one frame.
  FILE* source_file_ =
      fopen(test::ResourcePath("paris_qcif", "yuv").c_str(), "rb");
  ASSERT_TRUE(source_file_ != NULL);
  rtc::scoped_refptr<VideoFrameBuffer> video_frame_buffer(
      test::ReadI420Buffer(kWidth, kHeight, source_file_));
  input_frame_.reset(new VideoFrame(video_frame_buffer, kVideoRotation_0, 0));
  fclose(source_file_);

  encoder_ = CreateEncoder();
  decoder_ = CreateDecoder();
  encoder_->RegisterEncodeCompleteCallback(&encode_complete_callback_);
  decoder_->RegisterDecodeCompleteCallback(&decode_complete_callback_);

  InitCodecs();
}

bool VideoCodecTest::WaitForEncodedFrame(
    EncodedImage* frame,
    CodecSpecificInfo* codec_specific_info) {
  bool ret = encoded_frame_event_.Wait(kEncodeTimeoutMs);
  EXPECT_TRUE(ret) << "Timed out while waiting for an encoded frame.";
  // This becomes unsafe if there are multiple threads waiting for frames.
  rtc::CritScope lock(&encoded_frame_section_);
  EXPECT_TRUE(encoded_frame_);
  if (encoded_frame_) {
    *frame = std::move(*encoded_frame_);
    encoded_frame_.reset();
    RTC_DCHECK(codec_specific_info);
    codec_specific_info->codecType = codec_specific_info_.codecType;
    codec_specific_info->codecSpecific = codec_specific_info_.codecSpecific;
    return true;
  } else {
    return false;
  }
}

bool VideoCodecTest::WaitForDecodedFrame(std::unique_ptr<VideoFrame>* frame,
                                         rtc::Optional<uint8_t>* qp) {
  bool ret = decoded_frame_event_.Wait(kDecodeTimeoutMs);
  EXPECT_TRUE(ret) << "Timed out while waiting for a decoded frame.";
  // This becomes unsafe if there are multiple threads waiting for frames.
  rtc::CritScope lock(&decoded_frame_section_);
  EXPECT_TRUE(decoded_frame_);
  if (decoded_frame_) {
    frame->reset(new VideoFrame(std::move(*decoded_frame_)));
    *qp = decoded_qp_;
    decoded_frame_.reset();
    return true;
  } else {
    return false;
  }
}

void VideoCodecTest::InitCodecs() {
  codec_settings_ = codec_settings();
  codec_settings_.startBitrate = kStartBitrate;
  codec_settings_.targetBitrate = kTargetBitrate;
  codec_settings_.maxBitrate = kMaxBitrate;
  codec_settings_.maxFramerate = kMaxFramerate;
  codec_settings_.width = kWidth;
  codec_settings_.height = kHeight;
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            encoder_->InitEncode(&codec_settings_, 1 /* number of cores */,
                                 0 /* max payload size (unused) */));
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK,
            decoder_->InitDecode(&codec_settings_, 1 /* number of cores */));
}

}  // namespace webrtc
