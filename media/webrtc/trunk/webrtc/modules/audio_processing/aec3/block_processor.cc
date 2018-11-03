/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "webrtc/modules/audio_processing/aec3/block_processor.h"

#include "webrtc/base/atomicops.h"
#include "webrtc/base/optional.h"
#include "webrtc/modules/audio_processing/aec3/aec3_constants.h"

namespace webrtc {
namespace {

class BlockProcessorImpl final : public BlockProcessor {
 public:
  explicit BlockProcessorImpl(int sample_rate_hz);
  ~BlockProcessorImpl() override;

  void ProcessCapture(bool known_echo_path_change,
                      bool saturated_microphone_signal,
                      std::vector<std::vector<float>>* capture_block) override;

  bool BufferRender(std::vector<std::vector<float>>* block) override;

  void ReportEchoLeakage(bool leakage_detected) override;

 private:
  const size_t sample_rate_hz_;
  static int instance_count_;
  std::unique_ptr<ApmDataDumper> data_dumper_;
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(BlockProcessorImpl);
};

int BlockProcessorImpl::instance_count_ = 0;

BlockProcessorImpl::BlockProcessorImpl(int sample_rate_hz)
    : sample_rate_hz_(sample_rate_hz),
      data_dumper_(
          new ApmDataDumper(rtc::AtomicOps::Increment(&instance_count_))) {}

BlockProcessorImpl::~BlockProcessorImpl() = default;

void BlockProcessorImpl::ProcessCapture(
    bool known_echo_path_change,
    bool saturated_microphone_signal,
    std::vector<std::vector<float>>* capture_block) {
  RTC_DCHECK(capture_block);
  RTC_DCHECK_EQ(NumBandsForRate(sample_rate_hz_), capture_block->size());
  RTC_DCHECK_EQ(kBlockSize, (*capture_block)[0].size());
}

bool BlockProcessorImpl::BufferRender(
    std::vector<std::vector<float>>* render_block) {
  RTC_DCHECK(render_block);
  RTC_DCHECK_EQ(NumBandsForRate(sample_rate_hz_), render_block->size());
  RTC_DCHECK_EQ(kBlockSize, (*render_block)[0].size());
  return false;
}

void BlockProcessorImpl::ReportEchoLeakage(bool leakage_detected) {}

}  // namespace

BlockProcessor* BlockProcessor::Create(int sample_rate_hz) {
  return new BlockProcessorImpl(sample_rate_hz);
}

}  // namespace webrtc
