/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MockCall.h"

namespace test {

const webrtc::AudioSendStream::Config& MockAudioSendStream::GetConfig() const {
  return *mCallWrapper->GetMockCall()->mAudioSendConfig;
}

void MockAudioSendStream::Reconfigure(const Config& config) {
  mCallWrapper->GetMockCall()->mAudioSendConfig = mozilla::Some(config);
}

void MockAudioReceiveStream::Reconfigure(const Config& config) {
  mCallWrapper->GetMockCall()->mAudioReceiveConfig = mozilla::Some(config);
}

void MockVideoSendStream::ReconfigureVideoEncoder(
    webrtc::VideoEncoderConfig config) {
  mCallWrapper->GetMockCall()->mVideoSendEncoderConfig =
      mozilla::Some(config.Copy());
}

}  // namespace test
