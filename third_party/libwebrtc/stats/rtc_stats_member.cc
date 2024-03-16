/*
 *  Copyright 2023 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/stats/rtc_stats_member.h"

#include "rtc_base/arraysize.h"
#include "rtc_base/strings/string_builder.h"

namespace webrtc {

namespace {

// TODO(https://crbug.com/webrtc/15164): Delete all stringified functions in
// favor of Attribute::ToString().

// Produces "[a,b,c]". Works for non-vector `RTCStatsMemberInterface::Type`
// types.
template <typename T>
std::string VectorToString(const std::vector<T>& vector) {
  rtc::StringBuilder sb;
  sb << "[";
  const char* separator = "";
  for (const T& element : vector) {
    sb << separator << rtc::ToString(element);
    separator = ",";
  }
  sb << "]";
  return sb.Release();
}

// This overload is required because std::vector<bool> range loops don't
// return references but objects, causing -Wrange-loop-analysis diagnostics.
std::string VectorToString(const std::vector<bool>& vector) {
  rtc::StringBuilder sb;
  sb << "[";
  const char* separator = "";
  for (bool element : vector) {
    sb << separator << rtc::ToString(element);
    separator = ",";
  }
  sb << "]";
  return sb.Release();
}

// Produces "[\"a\",\"b\",\"c\"]". Works for vectors of both const char* and
// std::string element types.
template <typename T>
std::string VectorOfStringsToString(const std::vector<T>& strings) {
  rtc::StringBuilder sb;
  sb << "[";
  const char* separator = "";
  for (const T& element : strings) {
    sb << separator << "\"" << rtc::ToString(element) << "\"";
    separator = ",";
  }
  sb << "]";
  return sb.Release();
}

template <typename T>
std::string MapToString(const std::map<std::string, T>& map) {
  rtc::StringBuilder sb;
  sb << "{";
  const char* separator = "";
  for (const auto& element : map) {
    sb << separator << rtc::ToString(element.first) << ":"
       << rtc::ToString(element.second);
    separator = ",";
  }
  sb << "}";
  return sb.Release();
}

template <typename T>
std::string ToStringAsDouble(const T value) {
  // JSON represents numbers as floating point numbers with about 15 decimal
  // digits of precision.
  char buf[32];
  const int len = std::snprintf(&buf[0], arraysize(buf), "%.16g",
                                static_cast<double>(value));
  RTC_DCHECK_LE(len, arraysize(buf));
  return std::string(&buf[0], len);
}

template <typename T>
std::string VectorToStringAsDouble(const std::vector<T>& vector) {
  rtc::StringBuilder sb;
  sb << "[";
  const char* separator = "";
  for (const T& element : vector) {
    sb << separator << ToStringAsDouble<T>(element);
    separator = ",";
  }
  sb << "]";
  return sb.Release();
}

template <typename T>
std::string MapToStringAsDouble(const std::map<std::string, T>& map) {
  rtc::StringBuilder sb;
  sb << "{";
  const char* separator = "";
  for (const auto& element : map) {
    sb << separator << "\"" << rtc::ToString(element.first)
       << "\":" << ToStringAsDouble(element.second);
    separator = ",";
  }
  sb << "}";
  return sb.Release();
}

}  // namespace

#define WEBRTC_DEFINE_RTCSTATSMEMBER(T, type, is_seq, is_str, to_str, to_json) \
  template <>                                                                  \
  RTCStatsMemberInterface::Type RTCStatsMember<T>::StaticType() {              \
    return type;                                                               \
  }                                                                            \
  template <>                                                                  \
  bool RTCStatsMember<T>::is_sequence() const {                                \
    return is_seq;                                                             \
  }                                                                            \
  template <>                                                                  \
  bool RTCStatsMember<T>::is_string() const {                                  \
    return is_str;                                                             \
  }                                                                            \
  template <>                                                                  \
  std::string RTCStatsMember<T>::ValueToString() const {                       \
    RTC_DCHECK(value_.has_value());                                            \
    return to_str;                                                             \
  }                                                                            \
  template <>                                                                  \
  std::string RTCStatsMember<T>::ValueToJson() const {                         \
    RTC_DCHECK(value_.has_value());                                            \
    return to_json;                                                            \
  }                                                                            \
  template class RTC_EXPORT_TEMPLATE_DEFINE(RTC_EXPORT) RTCStatsMember<T>

WEBRTC_DEFINE_RTCSTATSMEMBER(bool,
                             kBool,
                             false,
                             false,
                             rtc::ToString(*value_),
                             rtc::ToString(*value_));
WEBRTC_DEFINE_RTCSTATSMEMBER(int32_t,
                             kInt32,
                             false,
                             false,
                             rtc::ToString(*value_),
                             rtc::ToString(*value_));
WEBRTC_DEFINE_RTCSTATSMEMBER(uint32_t,
                             kUint32,
                             false,
                             false,
                             rtc::ToString(*value_),
                             rtc::ToString(*value_));
WEBRTC_DEFINE_RTCSTATSMEMBER(int64_t,
                             kInt64,
                             false,
                             false,
                             rtc::ToString(*value_),
                             ToStringAsDouble(*value_));
WEBRTC_DEFINE_RTCSTATSMEMBER(uint64_t,
                             kUint64,
                             false,
                             false,
                             rtc::ToString(*value_),
                             ToStringAsDouble(*value_));
WEBRTC_DEFINE_RTCSTATSMEMBER(double,
                             kDouble,
                             false,
                             false,
                             rtc::ToString(*value_),
                             ToStringAsDouble(*value_));
WEBRTC_DEFINE_RTCSTATSMEMBER(std::string,
                             kString,
                             false,
                             true,
                             *value_,
                             *value_);
WEBRTC_DEFINE_RTCSTATSMEMBER(std::vector<bool>,
                             kSequenceBool,
                             true,
                             false,
                             VectorToString(*value_),
                             VectorToString(*value_));
WEBRTC_DEFINE_RTCSTATSMEMBER(std::vector<int32_t>,
                             kSequenceInt32,
                             true,
                             false,
                             VectorToString(*value_),
                             VectorToString(*value_));
WEBRTC_DEFINE_RTCSTATSMEMBER(std::vector<uint32_t>,
                             kSequenceUint32,
                             true,
                             false,
                             VectorToString(*value_),
                             VectorToString(*value_));
WEBRTC_DEFINE_RTCSTATSMEMBER(std::vector<int64_t>,
                             kSequenceInt64,
                             true,
                             false,
                             VectorToString(*value_),
                             VectorToStringAsDouble(*value_));
WEBRTC_DEFINE_RTCSTATSMEMBER(std::vector<uint64_t>,
                             kSequenceUint64,
                             true,
                             false,
                             VectorToString(*value_),
                             VectorToStringAsDouble(*value_));
WEBRTC_DEFINE_RTCSTATSMEMBER(std::vector<double>,
                             kSequenceDouble,
                             true,
                             false,
                             VectorToString(*value_),
                             VectorToStringAsDouble(*value_));
WEBRTC_DEFINE_RTCSTATSMEMBER(std::vector<std::string>,
                             kSequenceString,
                             true,
                             false,
                             VectorOfStringsToString(*value_),
                             VectorOfStringsToString(*value_));
WEBRTC_DEFINE_RTCSTATSMEMBER(rtc_stats_internal::MapStringUint64,
                             kMapStringUint64,
                             false,
                             false,
                             MapToString(*value_),
                             MapToStringAsDouble(*value_));
WEBRTC_DEFINE_RTCSTATSMEMBER(rtc_stats_internal::MapStringDouble,
                             kMapStringDouble,
                             false,
                             false,
                             MapToString(*value_),
                             MapToStringAsDouble(*value_));

}  // namespace webrtc
