/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/stats/rtc_stats.h"

#include <cstdio>

#include "rtc_base/strings/string_builder.h"

namespace webrtc {

RTCStats::RTCStats(const RTCStats& other)
    : RTCStats(other.id_, other.timestamp_) {}

RTCStats::~RTCStats() {}

bool RTCStats::operator==(const RTCStats& other) const {
  if (type() != other.type() || id() != other.id())
    return false;
  std::vector<const RTCStatsMemberInterface*> members = Members();
  std::vector<const RTCStatsMemberInterface*> other_members = other.Members();
  RTC_DCHECK_EQ(members.size(), other_members.size());
  for (size_t i = 0; i < members.size(); ++i) {
    const RTCStatsMemberInterface* member = members[i];
    const RTCStatsMemberInterface* other_member = other_members[i];
    RTC_DCHECK_EQ(member->type(), other_member->type());
    RTC_DCHECK_EQ(member->name(), other_member->name());
    if (*member != *other_member)
      return false;
  }
  return true;
}

bool RTCStats::operator!=(const RTCStats& other) const {
  return !(*this == other);
}

std::string RTCStats::ToJson() const {
  rtc::StringBuilder sb;
  sb << "{\"type\":\"" << type()
     << "\","
        "\"id\":\""
     << id_
     << "\","
        "\"timestamp\":"
     << timestamp_.us();
  for (const RTCStatsMemberInterface* member : Members()) {
    if (member->is_defined()) {
      sb << ",\"" << member->name() << "\":";
      if (member->is_string())
        sb << "\"" << member->ValueToJson() << "\"";
      else
        sb << member->ValueToJson();
    }
  }
  sb << "}";
  return sb.Release();
}

std::vector<const RTCStatsMemberInterface*> RTCStats::Members() const {
  if (cached_attributes_.empty()) {
    cached_attributes_ = Attributes();
  }
  std::vector<const RTCStatsMemberInterface*> members;
  members.reserve(cached_attributes_.size());
  for (const auto& attribute : cached_attributes_) {
    members.push_back(&attribute);
  }
  return members;
}

std::vector<Attribute> RTCStats::Attributes() const {
  return AttributesImpl(0);
}

std::vector<Attribute> RTCStats::AttributesImpl(
    size_t additional_capacity) const {
  std::vector<Attribute> attributes;
  attributes.reserve(additional_capacity);
  return attributes;
}

}  // namespace webrtc
