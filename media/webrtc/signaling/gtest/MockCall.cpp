/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MockCall.h"

namespace test {

const webrtc::AudioSendStream::Config& MockAudioSendStream::GetConfig() const {
  return *mCallWrapper->GetMockCall()->mAudioSendConfig;
}

void MockAudioSendStream::Reconfigure(const Config& config,
                                      webrtc::SetParametersCallback callback) {
  mCallWrapper->GetMockCall()->mAudioSendConfig = mozilla::Some(config);
}

void MockAudioReceiveStream::SetDecoderMap(
    std::map<int, webrtc::SdpAudioFormat> decoder_map) {
  MOZ_ASSERT(mCallWrapper->GetMockCall()->mAudioReceiveConfig.isSome());
  mCallWrapper->GetMockCall()->mAudioReceiveConfig->decoder_map =
      std::move(decoder_map);
}

void MockVideoSendStream::ReconfigureVideoEncoder(
    webrtc::VideoEncoderConfig config) {
  mCallWrapper->GetMockCall()->mVideoSendEncoderConfig =
      mozilla::Some(config.Copy());
}

void MockVideoSendStream::ReconfigureVideoEncoder(
    webrtc::VideoEncoderConfig config, webrtc::SetParametersCallback callback) {
  ReconfigureVideoEncoder(std::move(config));
}

}  // namespace test
