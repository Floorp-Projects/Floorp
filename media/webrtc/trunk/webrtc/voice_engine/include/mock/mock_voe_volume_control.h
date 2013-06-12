/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_INCLUDE_MOCK_MOCK_VOE_VOLUME_CONTROL_H_
#define WEBRTC_VOICE_ENGINE_INCLUDE_MOCK_MOCK_VOE_VOLUME_CONTROL_H_

#include "testing/gmock/include/gmock/gmock.h"
#include "webrtc/voice_engine/include/voe_volume_control.h"

namespace webrtc {

class VoiceEngine;

class MockVoEVolumeControl : public VoEVolumeControl {
 public:
  MOCK_METHOD0(Release, int());
  MOCK_METHOD1(SetSpeakerVolume, int(unsigned int volume));
  MOCK_METHOD1(GetSpeakerVolume, int(unsigned int& volume));
  MOCK_METHOD1(SetSystemOutputMute, int(bool enable));
  MOCK_METHOD1(GetSystemOutputMute, int(bool &enabled));
  MOCK_METHOD1(SetMicVolume, int(unsigned int volume));
  MOCK_METHOD1(GetMicVolume, int(unsigned int& volume));
  MOCK_METHOD2(SetInputMute, int(int channel, bool enable));
  MOCK_METHOD2(GetInputMute, int(int channel, bool& enabled));
  MOCK_METHOD1(SetSystemInputMute, int(bool enable));
  MOCK_METHOD1(GetSystemInputMute, int(bool& enabled));
  MOCK_METHOD1(GetSpeechInputLevel, int(unsigned int& level));
  MOCK_METHOD2(GetSpeechOutputLevel, int(int channel, unsigned int& level));
  MOCK_METHOD1(GetSpeechInputLevelFullRange, int(unsigned int& level));
  MOCK_METHOD2(GetSpeechOutputLevelFullRange,
               int(int channel, unsigned int& level));
  MOCK_METHOD2(SetChannelOutputVolumeScaling, int(int channel, float scaling));
  MOCK_METHOD2(GetChannelOutputVolumeScaling, int(int channel, float& scaling));
  MOCK_METHOD3(SetOutputVolumePan, int(int channel, float left, float right));
  MOCK_METHOD3(GetOutputVolumePan, int(int channel, float& left, float& right));
};

}  // namespace webrtc

#endif  // WEBRTC_VOICE_ENGINE_INCLUDE_MOCK_MOCK_VOE_VOLUME_CONTROL_H_
