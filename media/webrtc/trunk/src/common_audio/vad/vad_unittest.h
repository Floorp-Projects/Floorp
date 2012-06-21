/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_COMMONT_AUDIO_VAD_VAD_UNIT_TESTS_H
#define WEBRTC_COMMONT_AUDIO_VAD_VAD_UNIT_TESTS_H

#include <stddef.h>  // size_t

#include "gtest/gtest.h"
#include "typedefs.h"

namespace {

// Modes we support
const int kModes[] = { 0, 1, 2, 3 };
const size_t kModesSize = sizeof(kModes) / sizeof(*kModes);

// Rates we support.
const int16_t kRates[] = { 8000, 12000, 16000, 24000, 32000 };
const size_t kRatesSize = sizeof(kRates) / sizeof(*kRates);

// Frame lengths we support.
const int16_t kMaxFrameLength = 960;
const int16_t kFrameLengths[] = { 80, 120, 160, 240, 320, 480, 640,
    kMaxFrameLength };
const size_t kFrameLengthsSize = sizeof(kFrameLengths) / sizeof(*kFrameLengths);

}  // namespace

class VadTest : public ::testing::Test {
 protected:
  VadTest();
  virtual void SetUp();
  virtual void TearDown();

  // Returns true if the rate and frame length combination is valid.
  bool ValidRatesAndFrameLengths(int16_t rate, int16_t frame_length);
};

#endif  // WEBRTC_COMMONT_AUDIO_VAD_VAD_UNIT_TESTS_H
