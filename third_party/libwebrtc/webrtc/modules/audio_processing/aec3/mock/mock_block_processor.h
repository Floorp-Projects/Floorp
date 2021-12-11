/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_MOCK_MOCK_BLOCK_PROCESSOR_H_
#define MODULES_AUDIO_PROCESSING_AEC3_MOCK_MOCK_BLOCK_PROCESSOR_H_

#include <vector>

#include "modules/audio_processing/aec3/block_processor.h"
#include "test/gmock.h"

namespace webrtc {
namespace test {

class MockBlockProcessor : public BlockProcessor {
 public:
  virtual ~MockBlockProcessor() {}

  MOCK_METHOD3(ProcessCapture,
               void(bool level_change,
                    bool saturated_microphone_signal,
                    std::vector<std::vector<float>>* capture_block));
  MOCK_METHOD1(BufferRender,
               void(const std::vector<std::vector<float>>& block));
  MOCK_METHOD1(UpdateEchoLeakageStatus, void(bool leakage_detected));
  MOCK_CONST_METHOD1(GetMetrics, void(EchoControl::Metrics* metrics));
};

}  // namespace test
}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_MOCK_MOCK_BLOCK_PROCESSOR_H_
