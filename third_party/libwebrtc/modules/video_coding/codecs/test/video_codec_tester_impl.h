/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_CODECS_TEST_VIDEO_CODEC_TESTER_IMPL_H_
#define MODULES_VIDEO_CODING_CODECS_TEST_VIDEO_CODEC_TESTER_IMPL_H_

#include <memory>

#include "api/task_queue/task_queue_factory.h"
#include "api/test/video_codec_tester.h"

namespace webrtc {
namespace test {

// A stateless implementation of `VideoCodecTester`. This class is thread safe.
class VideoCodecTesterImpl : public VideoCodecTester {
 public:
  VideoCodecTesterImpl();
  explicit VideoCodecTesterImpl(TaskQueueFactory* task_queue_factory);

  std::unique_ptr<VideoCodecStats> RunDecodeTest(
      std::unique_ptr<CodedVideoSource> video_source,
      std::unique_ptr<Decoder> decoder,
      const DecoderSettings& decoder_settings) override;

  std::unique_ptr<VideoCodecStats> RunEncodeTest(
      std::unique_ptr<RawVideoSource> video_source,
      std::unique_ptr<Encoder> encoder,
      const EncoderSettings& encoder_settings) override;

  std::unique_ptr<VideoCodecStats> RunEncodeDecodeTest(
      std::unique_ptr<RawVideoSource> video_source,
      std::unique_ptr<Encoder> encoder,
      std::unique_ptr<Decoder> decoder,
      const EncoderSettings& encoder_settings,
      const DecoderSettings& decoder_settings) override;

 protected:
  std::unique_ptr<TaskQueueFactory> owned_task_queue_factory_;
  TaskQueueFactory* task_queue_factory_;
};

}  // namespace test
}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_CODECS_TEST_VIDEO_CODEC_TESTER_IMPL_H_
