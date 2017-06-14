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

#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "webrtc/modules/audio_processing/aec3/aec3_constants.h"
#include "webrtc/test/gtest.h"

namespace webrtc {
namespace {

// Verifies that the basic BlockProcessor functionality works and that the API
// methods are callable.
void RunBasicSetupAndApiCallTest(int sample_rate_hz) {
  std::unique_ptr<BlockProcessor> block_processor(
      BlockProcessor::Create(sample_rate_hz));
  std::vector<std::vector<float>> block(NumBandsForRate(sample_rate_hz),
                                        std::vector<float>(kBlockSize, 0.f));

  EXPECT_FALSE(block_processor->BufferRender(&block));
  block_processor->ProcessCapture(false, false, &block);
  block_processor->ReportEchoLeakage(false);
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
void RunRenderBlockSizeVerificationTest(int sample_rate_hz) {
  std::unique_ptr<BlockProcessor> block_processor(
      BlockProcessor::Create(sample_rate_hz));
  std::vector<std::vector<float>> block(
      NumBandsForRate(sample_rate_hz), std::vector<float>(kBlockSize - 1, 0.f));

  EXPECT_DEATH(block_processor->BufferRender(&block), "");
}

void RunCaptureBlockSizeVerificationTest(int sample_rate_hz) {
  std::unique_ptr<BlockProcessor> block_processor(
      BlockProcessor::Create(sample_rate_hz));
  std::vector<std::vector<float>> block(
      NumBandsForRate(sample_rate_hz), std::vector<float>(kBlockSize - 1, 0.f));

  EXPECT_DEATH(block_processor->ProcessCapture(false, false, &block), "");
}

void RunRenderNumBandsVerificationTest(int sample_rate_hz) {
  const size_t wrong_num_bands = NumBandsForRate(sample_rate_hz) < 3
                                     ? NumBandsForRate(sample_rate_hz) + 1
                                     : 1;
  std::unique_ptr<BlockProcessor> block_processor(
      BlockProcessor::Create(sample_rate_hz));
  std::vector<std::vector<float>> block(wrong_num_bands,
                                        std::vector<float>(kBlockSize, 0.f));

  EXPECT_DEATH(block_processor->BufferRender(&block), "");
}

void RunCaptureNumBandsVerificationTest(int sample_rate_hz) {
  const size_t wrong_num_bands = NumBandsForRate(sample_rate_hz) < 3
                                     ? NumBandsForRate(sample_rate_hz) + 1
                                     : 1;
  std::unique_ptr<BlockProcessor> block_processor(
      BlockProcessor::Create(sample_rate_hz));
  std::vector<std::vector<float>> block(wrong_num_bands,
                                        std::vector<float>(kBlockSize, 0.f));

  EXPECT_DEATH(block_processor->ProcessCapture(false, false, &block), "");
}
#endif

std::string ProduceDebugText(int sample_rate_hz) {
  std::ostringstream ss;
  ss << "Sample rate: " << sample_rate_hz;
  return ss.str();
}

}  // namespace

TEST(BlockProcessor, BasicSetupAndApiCalls) {
  for (auto rate : {8000, 16000, 32000, 48000}) {
    SCOPED_TRACE(ProduceDebugText(rate));
    RunBasicSetupAndApiCallTest(rate);
  }
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
// TODO(peah): Enable all DEATH tests below, or add suppressions, once clarity
// has been reached on why they fail on the trybots.
TEST(BlockProcessor, DISABLED_VerifyRenderBlockSizeCheck) {
  for (auto rate : {8000, 16000, 32000, 48000}) {
    SCOPED_TRACE(ProduceDebugText(rate));
    RunRenderBlockSizeVerificationTest(rate);
  }
}

TEST(BlockProcessor, DISABLED_VerifyCaptureBlockSizeCheck) {
  for (auto rate : {8000, 16000, 32000, 48000}) {
    SCOPED_TRACE(ProduceDebugText(rate));
    RunCaptureBlockSizeVerificationTest(rate);
  }
}

TEST(BlockProcessor, DISABLED_VerifyRenderNumBandsCheck) {
  for (auto rate : {8000, 16000, 32000, 48000}) {
    SCOPED_TRACE(ProduceDebugText(rate));
    RunRenderNumBandsVerificationTest(rate);
  }
}

TEST(BlockProcessor, DISABLED_VerifyCaptureNumBandsCheck) {
  for (auto rate : {8000, 16000, 32000, 48000}) {
    SCOPED_TRACE(ProduceDebugText(rate));
    RunCaptureNumBandsVerificationTest(rate);
  }
}

// Verifiers that the verification for null ProcessCapture input works.
TEST(BlockProcessor, DISABLED_NullProcessCaptureParameter) {
  EXPECT_DEATH(std::unique_ptr<BlockProcessor>(BlockProcessor::Create(8000))
                   ->ProcessCapture(false, false, nullptr),
               "");
}

// Verifiers that the verification for null BufferRender input works.
TEST(BlockProcessor, DISABLED_NullBufferRenderParameter) {
  EXPECT_DEATH(std::unique_ptr<BlockProcessor>(BlockProcessor::Create(8000))
                   ->BufferRender(nullptr),
               "");
}

#endif

}  // namespace webrtc
