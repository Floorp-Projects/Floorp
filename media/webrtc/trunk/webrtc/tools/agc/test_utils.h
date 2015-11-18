/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_TOOLS_AGC_TEST_UTILS_H_
#define WEBRTC_TOOLS_AGC_TEST_UTILS_H_
namespace webrtc {

class AudioFrame;

float MicLevel2Gain(int gain_range_db, int level);
float Db2Linear(float db);
void ApplyGainLinear(float gain, float last_gain, AudioFrame* frame);
void ApplyGain(float gain_db, float last_gain_db, AudioFrame* frame);
void SimulateMic(int gain_range_db, int mic_level, int last_mic_level,
                 AudioFrame* frame);
void SimulateMic(int gain_map[255], int mic_level, int last_mic_level,
                 AudioFrame* frame);

}  // namespace webrtc

#endif  // WEBRTC_TOOLS_AGC_TEST_UTILS_H_
