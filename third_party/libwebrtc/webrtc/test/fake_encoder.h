/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_FAKE_ENCODER_H_
#define TEST_FAKE_ENCODER_H_

#include <vector>
#include <memory>

#include "api/video_codecs/video_encoder.h"
#include "common_types.h"  // NOLINT(build/include)
#include "rtc_base/criticalsection.h"
#include "rtc_base/sequenced_task_checker.h"
#include "rtc_base/task_queue.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {
namespace test {

class FakeEncoder : public VideoEncoder {
 public:
  explicit FakeEncoder(Clock* clock);
  virtual ~FakeEncoder() = default;

  // Sets max bitrate. Not thread-safe, call before registering the encoder.
  void SetMaxBitrate(int max_kbps);

  int32_t InitEncode(const VideoCodec* config,
                     int32_t number_of_cores,
                     size_t max_payload_size) override;
  int32_t Encode(const VideoFrame& input_image,
                 const CodecSpecificInfo* codec_specific_info,
                 const std::vector<FrameType>* frame_types) override;
  int32_t RegisterEncodeCompleteCallback(
      EncodedImageCallback* callback) override;
  int32_t Release() override;
  int32_t SetChannelParameters(uint32_t packet_loss, int64_t rtt) override;
  int32_t SetRateAllocation(const BitrateAllocation& rate_allocation,
                            uint32_t framerate) override;
  const char* ImplementationName() const override;
  int GetConfiguredInputFramerate() const;

  static const char* kImplementationName;

 protected:
  Clock* const clock_;
  VideoCodec config_ RTC_GUARDED_BY(crit_sect_);
  EncodedImageCallback* callback_ RTC_GUARDED_BY(crit_sect_);
  BitrateAllocation target_bitrate_ RTC_GUARDED_BY(crit_sect_);
  int configured_input_framerate_ RTC_GUARDED_BY(crit_sect_);
  int max_target_bitrate_kbps_ RTC_GUARDED_BY(crit_sect_);
  bool pending_keyframe_ RTC_GUARDED_BY(crit_sect_);
  rtc::CriticalSection crit_sect_;

  uint8_t encoded_buffer_[100000];

  // Current byte debt to be payed over a number of frames.
  // The debt is acquired by keyframes overshooting the bitrate target.
  size_t debt_bytes_;
};

class FakeH264Encoder : public FakeEncoder, public EncodedImageCallback {
 public:
  explicit FakeH264Encoder(Clock* clock);
  virtual ~FakeH264Encoder() = default;

  int32_t RegisterEncodeCompleteCallback(
      EncodedImageCallback* callback) override;

  Result OnEncodedImage(const EncodedImage& encodedImage,
                        const CodecSpecificInfo* codecSpecificInfo,
                        const RTPFragmentationHeader* fragments) override;

 private:
  EncodedImageCallback* callback_ RTC_GUARDED_BY(local_crit_sect_);
  int idr_counter_ RTC_GUARDED_BY(local_crit_sect_);
  rtc::CriticalSection local_crit_sect_;
};

class DelayedEncoder : public test::FakeEncoder {
 public:
  DelayedEncoder(Clock* clock, int delay_ms);
  virtual ~DelayedEncoder() = default;

  void SetDelay(int delay_ms);
  int32_t Encode(const VideoFrame& input_image,
                 const CodecSpecificInfo* codec_specific_info,
                 const std::vector<FrameType>* frame_types) override;

 private:
  int delay_ms_ RTC_ACCESS_ON(sequence_checker_);
  rtc::SequencedTaskChecker sequence_checker_;
};

// This class implements a multi-threaded fake encoder by posting
// FakeH264Encoder::Encode(.) tasks to |queue1_| and |queue2_|, in an
// alternating fashion. The class itself does not need to be thread safe,
// as it is called from the task queue in VideoStreamEncoder.
class MultithreadedFakeH264Encoder : public test::FakeH264Encoder {
 public:
  explicit MultithreadedFakeH264Encoder(Clock* clock);
  virtual ~MultithreadedFakeH264Encoder() = default;

  int32_t InitEncode(const VideoCodec* config,
                     int32_t number_of_cores,
                     size_t max_payload_size) override;

  int32_t Encode(const VideoFrame& input_image,
                 const CodecSpecificInfo* codec_specific_info,
                 const std::vector<FrameType>* frame_types) override;

  int32_t EncodeCallback(const VideoFrame& input_image,
                         const CodecSpecificInfo* codec_specific_info,
                         const std::vector<FrameType>* frame_types);

  int32_t Release() override;

 protected:
  class EncodeTask;

  int current_queue_ RTC_ACCESS_ON(sequence_checker_);
  std::unique_ptr<rtc::TaskQueue> queue1_ RTC_ACCESS_ON(sequence_checker_);
  std::unique_ptr<rtc::TaskQueue> queue2_ RTC_ACCESS_ON(sequence_checker_);
  rtc::SequencedTaskChecker sequence_checker_;
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_FAKE_ENCODER_H_
