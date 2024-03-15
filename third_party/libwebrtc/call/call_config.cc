/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/call_config.h"

#include "api/environment/environment.h"
#include "api/task_queue/task_queue_base.h"

namespace webrtc {

CallConfig::CallConfig(const Environment& env,
                       TaskQueueBase* network_task_queue)
    : env(env),
      network_task_queue_(network_task_queue) {}

CallConfig::CallConfig(const CallConfig& config) = default;

RtpTransportConfig CallConfig::ExtractTransportConfig() const {
  RtpTransportConfig transportConfig;
  transportConfig.bitrate_config = bitrate_config;
  transportConfig.event_log = &env.event_log();
  transportConfig.network_controller_factory = network_controller_factory;
  transportConfig.network_state_predictor_factory =
      network_state_predictor_factory;
  transportConfig.task_queue_factory = &env.task_queue_factory();
  transportConfig.trials = &env.field_trials();
  transportConfig.pacer_burst_interval = pacer_burst_interval;

  return transportConfig;
}

CallConfig::~CallConfig() = default;

}  // namespace webrtc
