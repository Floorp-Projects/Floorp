/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_AUDIO_NETWORK_ADAPTATION_H_
#define LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_AUDIO_NETWORK_ADAPTATION_H_

#include <memory>

#include "logging/rtc_event_log/events/rtc_event.h"

namespace webrtc {

struct AudioEncoderRuntimeConfig;

class RtcEventAudioNetworkAdaptation final : public RtcEvent {
 public:
  explicit RtcEventAudioNetworkAdaptation(
      std::unique_ptr<AudioEncoderRuntimeConfig> config);
  ~RtcEventAudioNetworkAdaptation() override;

  Type GetType() const override;

  bool IsConfigEvent() const override;

  const std::unique_ptr<const AudioEncoderRuntimeConfig> config_;
};

}  // namespace webrtc

#endif  // LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_AUDIO_NETWORK_ADAPTATION_H_
