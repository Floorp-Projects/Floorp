/*
 *  Copyright 2023 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_STATS_RTC_STATS_MEMBER_H_
#define API_STATS_RTC_STATS_MEMBER_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "rtc_base/checks.h"
#include "rtc_base/system/rtc_export.h"
#include "rtc_base/system/rtc_export_template.h"

namespace webrtc {

// Interface for `RTCStats` members, which have a name and a value of a type
// defined in a subclass. Only the types listed in `Type` are supported, these
// are implemented by `RTCStatsMember<T>`. The value of a member may be
// undefined, the value can only be read if `is_defined`.
class RTCStatsMemberInterface {
 public:
  // Member value types.
  enum Type {
    kBool,    // bool
    kInt32,   // int32_t
    kUint32,  // uint32_t
    kInt64,   // int64_t
    kUint64,  // uint64_t
    kDouble,  // double
    kString,  // std::string

    kSequenceBool,    // std::vector<bool>
    kSequenceInt32,   // std::vector<int32_t>
    kSequenceUint32,  // std::vector<uint32_t>
    kSequenceInt64,   // std::vector<int64_t>
    kSequenceUint64,  // std::vector<uint64_t>
    kSequenceDouble,  // std::vector<double>
    kSequenceString,  // std::vector<std::string>

    kMapStringUint64,  // std::map<std::string, uint64_t>
    kMapStringDouble,  // std::map<std::string, double>
  };

  virtual ~RTCStatsMemberInterface() {}

  virtual Type type() const = 0;
  virtual bool is_sequence() const = 0;
  virtual bool is_string() const = 0;
  virtual bool is_defined() const = 0;
  // Type and value comparator. The names are not compared. These operators are
  // exposed for testing.
  bool operator==(const RTCStatsMemberInterface& other) const {
    return IsEqual(other);
  }
  bool operator!=(const RTCStatsMemberInterface& other) const {
    return !(*this == other);
  }
  virtual std::string ValueToString() const = 0;
  // This is the same as ValueToString except for kInt64 and kUint64 types,
  // where the value is represented as a double instead of as an integer.
  // Since JSON stores numbers as floating point numbers, very large integers
  // cannot be accurately represented, so we prefer to display them as doubles
  // instead.
  virtual std::string ValueToJson() const = 0;

  virtual const RTCStatsMemberInterface* member_ptr() const { return this; }
  template <typename T>
  const T& cast_to() const {
    RTC_DCHECK_EQ(type(), T::StaticType());
    return static_cast<const T&>(*member_ptr());
  }

 protected:
  virtual bool IsEqual(const RTCStatsMemberInterface& other) const = 0;
};

// Template implementation of `RTCStatsMemberInterface`.
// The supported types are the ones described by
// `RTCStatsMemberInterface::Type`.
template <typename T>
class RTCStatsMember : public RTCStatsMemberInterface {
 public:
  RTCStatsMember() {}
  explicit RTCStatsMember(const T& value) : value_(value) {}

  static Type StaticType();
  Type type() const override { return StaticType(); }
  bool is_sequence() const override;
  bool is_string() const override;
  bool is_defined() const override { return value_.has_value(); }
  std::string ValueToString() const override;
  std::string ValueToJson() const override;

  template <typename U>
  inline T ValueOrDefault(U default_value) const {
    return value_.value_or(default_value);
  }

  // Assignment operators.
  T& operator=(const T& value) {
    value_ = value;
    return value_.value();
  }
  T& operator=(const T&& value) {
    value_ = std::move(value);
    return value_.value();
  }

  // Getter methods that look the same as absl::optional<T>. Please prefer these
  // in order to unblock replacing RTCStatsMember<T> with absl::optional<T> in
  // the future (https://crbug.com/webrtc/15164).
  bool has_value() const { return value_.has_value(); }
  const T& value() const { return value_.value(); }
  T& value() { return value_.value(); }
  T& operator*() {
    RTC_DCHECK(value_);
    return *value_;
  }
  const T& operator*() const {
    RTC_DCHECK(value_);
    return *value_;
  }
  T* operator->() {
    RTC_DCHECK(value_);
    return &(*value_);
  }
  const T* operator->() const {
    RTC_DCHECK(value_);
    return &(*value_);
  }

  bool IsEqual(const RTCStatsMemberInterface& other) const override {
    if (type() != other.type())
      return false;
    const RTCStatsMember<T>& other_t =
        static_cast<const RTCStatsMember<T>&>(other);
    return value_ == other_t.value_;
  }

 private:
  absl::optional<T> value_;
};

namespace rtc_stats_internal {

typedef std::map<std::string, uint64_t> MapStringUint64;
typedef std::map<std::string, double> MapStringDouble;

}  // namespace rtc_stats_internal

#define WEBRTC_DECLARE_RTCSTATSMEMBER(T)                                    \
  template <>                                                               \
  RTC_EXPORT RTCStatsMemberInterface::Type RTCStatsMember<T>::StaticType(); \
  template <>                                                               \
  RTC_EXPORT bool RTCStatsMember<T>::is_sequence() const;                   \
  template <>                                                               \
  RTC_EXPORT bool RTCStatsMember<T>::is_string() const;                     \
  template <>                                                               \
  RTC_EXPORT std::string RTCStatsMember<T>::ValueToString() const;          \
  template <>                                                               \
  RTC_EXPORT std::string RTCStatsMember<T>::ValueToJson() const;            \
  extern template class RTC_EXPORT_TEMPLATE_DECLARE(RTC_EXPORT)             \
      RTCStatsMember<T>

WEBRTC_DECLARE_RTCSTATSMEMBER(bool);
WEBRTC_DECLARE_RTCSTATSMEMBER(int32_t);
WEBRTC_DECLARE_RTCSTATSMEMBER(uint32_t);
WEBRTC_DECLARE_RTCSTATSMEMBER(int64_t);
WEBRTC_DECLARE_RTCSTATSMEMBER(uint64_t);
WEBRTC_DECLARE_RTCSTATSMEMBER(double);
WEBRTC_DECLARE_RTCSTATSMEMBER(std::string);
WEBRTC_DECLARE_RTCSTATSMEMBER(std::vector<bool>);
WEBRTC_DECLARE_RTCSTATSMEMBER(std::vector<int32_t>);
WEBRTC_DECLARE_RTCSTATSMEMBER(std::vector<uint32_t>);
WEBRTC_DECLARE_RTCSTATSMEMBER(std::vector<int64_t>);
WEBRTC_DECLARE_RTCSTATSMEMBER(std::vector<uint64_t>);
WEBRTC_DECLARE_RTCSTATSMEMBER(std::vector<double>);
WEBRTC_DECLARE_RTCSTATSMEMBER(std::vector<std::string>);
WEBRTC_DECLARE_RTCSTATSMEMBER(rtc_stats_internal::MapStringUint64);
WEBRTC_DECLARE_RTCSTATSMEMBER(rtc_stats_internal::MapStringDouble);

}  // namespace webrtc

#endif  // API_STATS_RTC_STATS_MEMBER_H_
