/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/field_trial.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>

#include "rtc_base/checks.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {
namespace test {

ScopedFieldTrials::ScopedFieldTrials(const std::string& config)
    : previous_field_trials_(webrtc::field_trial::GetFieldTrialString()) {
  RTC_CHECK(webrtc::field_trial::FieldTrialsStringIsValid(config.c_str()))
      << "Invalid field trials string: " << config;
  current_field_trials_ = config;
  webrtc::field_trial::InitFieldTrialsFromString(current_field_trials_.c_str());
}

ScopedFieldTrials::~ScopedFieldTrials() {
  RTC_CHECK(
      webrtc::field_trial::FieldTrialsStringIsValid(previous_field_trials_))
      << "Invalid field trials string: " << previous_field_trials_;
  webrtc::field_trial::InitFieldTrialsFromString(previous_field_trials_);
}

}  // namespace test
}  // namespace webrtc
