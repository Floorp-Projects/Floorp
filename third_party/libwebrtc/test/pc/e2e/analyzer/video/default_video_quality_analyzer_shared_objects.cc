/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/pc/e2e/analyzer/video/default_video_quality_analyzer_shared_objects.h"

#include "api/units/timestamp.h"
#include "rtc_base/checks.h"
#include "rtc_base/strings/string_builder.h"

namespace webrtc {
namespace webrtc_pc_e2e {
namespace {

constexpr int kMicrosPerSecond = 1000000;

}  // namespace

void RateCounter::AddEvent(Timestamp event_time) {
  if (event_first_time_.IsMinusInfinity()) {
    event_first_time_ = event_time;
  }
  event_last_time_ = event_time;
  event_count_++;
}

double RateCounter::GetEventsPerSecond() const {
  RTC_DCHECK(!IsEmpty());
  // Divide on us and multiply on kMicrosPerSecond to correctly process cases
  // where there were too small amount of events, so difference is less then 1
  // sec. We can use us here, because Timestamp has us resolution.
  return static_cast<double>(event_count_) /
         (event_last_time_ - event_first_time_).us() * kMicrosPerSecond;
}

std::string StatsKey::ToString() const {
  rtc::StringBuilder out;
  out << stream_label << "_" << sender << "_" << receiver;
  return out.str();
}

bool operator<(const StatsKey& a, const StatsKey& b) {
  if (a.stream_label != b.stream_label) {
    return a.stream_label < b.stream_label;
  }
  if (a.sender != b.sender) {
    return a.sender < b.sender;
  }
  return a.receiver < b.receiver;
}

bool operator==(const StatsKey& a, const StatsKey& b) {
  return a.stream_label == b.stream_label && a.sender == b.sender &&
         a.receiver == b.receiver;
}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
