/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/test/create_time_controller.h"

#include <memory>
#include <utility>

#include "absl/base/nullability.h"
#include "api/enable_media_with_defaults.h"
#include "api/peer_connection_interface.h"
#include "call/call.h"
#include "call/rtp_transport_config.h"
#include "call/rtp_transport_controller_send_factory_interface.h"
#include "pc/media_factory.h"
#include "rtc_base/checks.h"
#include "system_wrappers/include/clock.h"
#include "test/time_controller/external_time_controller.h"
#include "test/time_controller/simulated_time_controller.h"

namespace webrtc {

std::unique_ptr<TimeController> CreateTimeController(
    ControlledAlarmClock* alarm) {
  return std::make_unique<ExternalTimeController>(alarm);
}

std::unique_ptr<TimeController> CreateSimulatedTimeController() {
  return std::make_unique<GlobalSimulatedTimeController>(
      Timestamp::Seconds(10000));
}

std::unique_ptr<CallFactoryInterface> CreateTimeControllerBasedCallFactory(
    TimeController* time_controller) {
  class TimeControllerBasedCallFactory : public CallFactoryInterface {
   public:
    explicit TimeControllerBasedCallFactory(TimeController* time_controller)
        : time_controller_(time_controller) {}
    std::unique_ptr<Call> CreateCall(const CallConfig& config) override {
      RtpTransportConfig transportConfig = config.ExtractTransportConfig();

      return Call::Create(config, time_controller_->GetClock(),
                          config.rtp_transport_controller_send_factory->Create(
                              transportConfig, time_controller_->GetClock()));
    }

   private:
    TimeController* time_controller_;
  };
  return std::make_unique<TimeControllerBasedCallFactory>(time_controller);
}

void EnableMediaWithDefaultsAndTimeController(
    TimeController& time_controller,
    PeerConnectionFactoryDependencies& deps) {
  class TimeControllerBasedFactory : public MediaFactory {
   public:
    TimeControllerBasedFactory(
        absl::Nonnull<Clock*> clock,
        absl::Nonnull<std::unique_ptr<MediaFactory>> media_factory)
        : clock_(clock), media_factory_(std::move(media_factory)) {}

    std::unique_ptr<Call> CreateCall(const CallConfig& config) override {
      return Call::Create(config, clock_,
                          config.rtp_transport_controller_send_factory->Create(
                              config.ExtractTransportConfig(), clock_));
    }

    std::unique_ptr<cricket::MediaEngineInterface> CreateMediaEngine(
        PeerConnectionFactoryDependencies& dependencies) override {
      return media_factory_->CreateMediaEngine(dependencies);
    }

   private:
    absl::Nonnull<Clock*> clock_;
    absl::Nonnull<std::unique_ptr<MediaFactory>> media_factory_;
  };

  EnableMediaWithDefaults(deps);
  RTC_CHECK(deps.media_factory);
  deps.media_factory = std::make_unique<TimeControllerBasedFactory>(
      time_controller.GetClock(), std::move(deps.media_factory));
}

}  // namespace webrtc
