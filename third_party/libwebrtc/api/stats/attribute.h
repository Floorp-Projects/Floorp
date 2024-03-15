/*
 *  Copyright 2024 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_STATS_ATTRIBUTE_H_
#define API_STATS_ATTRIBUTE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "absl/types/variant.h"
#include "api/stats/rtc_stats_member.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

// A light-weight wrapper of an RTCStats attribute (an individual metric).
class RTC_EXPORT Attribute : public RTCStatsMemberInterface {
 public:
  // TODO(https://crbug.com/webrtc/15164): Replace uses of RTCStatsMember<T>
  // with absl::optional<T> and update these pointer types.
  typedef absl::variant<const RTCStatsMember<bool>*,
                        const RTCStatsMember<int32_t>*,
                        const RTCStatsMember<uint32_t>*,
                        const RTCStatsMember<int64_t>*,
                        const RTCStatsMember<uint64_t>*,
                        const RTCStatsMember<double>*,
                        const RTCStatsMember<std::string>*,
                        const RTCStatsMember<std::vector<bool>>*,
                        const RTCStatsMember<std::vector<int32_t>>*,
                        const RTCStatsMember<std::vector<uint32_t>>*,
                        const RTCStatsMember<std::vector<int64_t>>*,
                        const RTCStatsMember<std::vector<uint64_t>>*,
                        const RTCStatsMember<std::vector<double>>*,
                        const RTCStatsMember<std::vector<std::string>>*,
                        const RTCStatsMember<std::map<std::string, uint64_t>>*,
                        const RTCStatsMember<std::map<std::string, double>>*>
      StatVariant;

  template <typename T>
  explicit Attribute(const RTCStatsMember<T>* attribute)
      : RTCStatsMemberInterface(attribute->name()), attribute_(attribute) {}
  ~Attribute() override;

  const char* name() const;
  const StatVariant& as_variant() const;

  bool has_value() const;
  template <typename T>
  bool holds_alternative() const {
    return absl::holds_alternative<const RTCStatsMember<T>*>(attribute_);
  }
  template <typename T>
  const T& get() const {
    RTC_CHECK(holds_alternative<T>());
    RTC_CHECK(has_value());
    return absl::get<const RTCStatsMember<T>*>(attribute_)->value();
  }

  static Attribute FromMemberInterface(const RTCStatsMemberInterface* member);
  // RTCStatsMemberInterface implementation.
  // TODO(https://crbug.com/webrtc/15164): Delete RTCStatsMemberInterface in
  // favor of absl::optional<T>.
  RTCStatsMemberInterface::Type type() const override;
  bool is_sequence() const override;
  bool is_string() const override;
  bool is_defined() const override;
  std::string ValueToString() const override;
  std::string ValueToJson() const override;

 protected:
  const RTCStatsMemberInterface* member_ptr() const override;
  bool IsEqual(const RTCStatsMemberInterface& other) const override;

 private:
  StatVariant attribute_;
};

Attribute MemberToAttribute(const RTCStatsMemberInterface* member);

}  // namespace webrtc

#endif  // API_STATS_ATTRIBUTE_H_
