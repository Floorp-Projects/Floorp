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

namespace webrtc {

#define WEBRTC_DEFINE_RTCSTATSMEMBER(T, type, is_seq, is_str)     \
  template <>                                                     \
  RTCStatsMemberInterface::Type RTCStatsMember<T>::StaticType() { \
    return type;                                                  \
  }                                                               \
  template <>                                                     \
  bool RTCStatsMember<T>::is_sequence() const {                   \
    return is_seq;                                                \
  }                                                               \
  template <>                                                     \
  bool RTCStatsMember<T>::is_string() const {                     \
    return is_str;                                                \
  }                                                               \
  template class RTC_EXPORT_TEMPLATE_DEFINE(RTC_EXPORT) RTCStatsMember<T>

WEBRTC_DEFINE_RTCSTATSMEMBER(bool, kBool, false, false);
WEBRTC_DEFINE_RTCSTATSMEMBER(int32_t, kInt32, false, false);
WEBRTC_DEFINE_RTCSTATSMEMBER(uint32_t, kUint32, false, false);
WEBRTC_DEFINE_RTCSTATSMEMBER(int64_t, kInt64, false, false);
WEBRTC_DEFINE_RTCSTATSMEMBER(uint64_t, kUint64, false, false);
WEBRTC_DEFINE_RTCSTATSMEMBER(double, kDouble, false, false);
WEBRTC_DEFINE_RTCSTATSMEMBER(std::string, kString, false, true);
WEBRTC_DEFINE_RTCSTATSMEMBER(std::vector<bool>, kSequenceBool, true, false);
WEBRTC_DEFINE_RTCSTATSMEMBER(std::vector<int32_t>, kSequenceInt32, true, false);
WEBRTC_DEFINE_RTCSTATSMEMBER(std::vector<uint32_t>,
                             kSequenceUint32,
                             true,
                             false);
WEBRTC_DEFINE_RTCSTATSMEMBER(std::vector<int64_t>, kSequenceInt64, true, false);
WEBRTC_DEFINE_RTCSTATSMEMBER(std::vector<uint64_t>,
                             kSequenceUint64,
                             true,
                             false);
WEBRTC_DEFINE_RTCSTATSMEMBER(std::vector<double>, kSequenceDouble, true, false);
WEBRTC_DEFINE_RTCSTATSMEMBER(std::vector<std::string>,
                             kSequenceString,
                             true,
                             false);
WEBRTC_DEFINE_RTCSTATSMEMBER(rtc_stats_internal::MapStringUint64,
                             kMapStringUint64,
                             false,
                             false);
WEBRTC_DEFINE_RTCSTATSMEMBER(rtc_stats_internal::MapStringDouble,
                             kMapStringDouble,
                             false,
                             false);

}  // namespace webrtc
