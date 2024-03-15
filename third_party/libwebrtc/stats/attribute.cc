/*
 *  Copyright 2024 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/stats/attribute.h"

#include "absl/types/variant.h"
#include "rtc_base/checks.h"

namespace webrtc {

namespace {

struct VisitIsSequence {
  // Any type of vector is a sequence.
  template <typename T>
  bool operator()(const RTCStatsMember<std::vector<T>>* attribute) {
    return true;
  }
  // Any other type is not.
  template <typename T>
  bool operator()(const RTCStatsMember<T>* attribute) {
    return false;
  }
};

struct VisitIsEqual {
  template <typename T>
  bool operator()(const RTCStatsMember<T>* attribute) {
    return attribute->IsEqual(other);
  }

  const RTCStatsMemberInterface& other;
};

}  // namespace

Attribute::~Attribute() {}

// static
Attribute Attribute::FromMemberInterface(
    const RTCStatsMemberInterface* member) {
  switch (member->type()) {
    case RTCStatsMemberInterface::Type::kBool:
      return Attribute(&member->cast_to<RTCStatsMember<bool>>());
    case RTCStatsMemberInterface::Type::kInt32:
      return Attribute(&member->cast_to<RTCStatsMember<int32_t>>());
    case RTCStatsMemberInterface::Type::kUint32:
      return Attribute(&member->cast_to<RTCStatsMember<uint32_t>>());
    case RTCStatsMemberInterface::Type::kInt64:
      return Attribute(&member->cast_to<RTCStatsMember<int64_t>>());
    case RTCStatsMemberInterface::Type::kUint64:
      return Attribute(&member->cast_to<RTCStatsMember<uint64_t>>());
    case RTCStatsMemberInterface::Type::kDouble:
      return Attribute(&member->cast_to<RTCStatsMember<double>>());
    case RTCStatsMemberInterface::Type::kString:
      return Attribute(&member->cast_to<RTCStatsMember<std::string>>());
    case RTCStatsMemberInterface::Type::kSequenceBool:
      return Attribute(&member->cast_to<RTCStatsMember<std::vector<bool>>>());
    case RTCStatsMemberInterface::Type::kSequenceInt32:
      return Attribute(
          &member->cast_to<RTCStatsMember<std::vector<int32_t>>>());
    case RTCStatsMemberInterface::Type::kSequenceUint32:
      return Attribute(
          &member->cast_to<RTCStatsMember<std::vector<uint32_t>>>());
    case RTCStatsMemberInterface::Type::kSequenceInt64:
      return Attribute(
          &member->cast_to<RTCStatsMember<std::vector<int64_t>>>());
    case RTCStatsMemberInterface::Type::kSequenceUint64:
      return Attribute(
          &member->cast_to<RTCStatsMember<std::vector<uint64_t>>>());
    case RTCStatsMemberInterface::Type::kSequenceDouble:
      return Attribute(&member->cast_to<RTCStatsMember<std::vector<double>>>());
    case RTCStatsMemberInterface::Type::kSequenceString:
      return Attribute(
          &member->cast_to<RTCStatsMember<std::vector<std::string>>>());
    case RTCStatsMemberInterface::Type::kMapStringUint64:
      return Attribute(
          &member->cast_to<RTCStatsMember<std::map<std::string, uint64_t>>>());
    case RTCStatsMemberInterface::Type::kMapStringDouble:
      return Attribute(
          &member->cast_to<RTCStatsMember<std::map<std::string, double>>>());
    default:
      RTC_CHECK_NOTREACHED();
  }
}

const char* Attribute::name() const {
  return absl::visit([](const auto* attr) { return attr->name(); }, attribute_);
}

const Attribute::StatVariant& Attribute::as_variant() const {
  return attribute_;
}

bool Attribute::has_value() const {
  return absl::visit([](const auto* attr) { return attr->has_value(); },
                     attribute_);
}

RTCStatsMemberInterface::Type Attribute::type() const {
  return absl::visit([](const auto* attr) { return attr->type(); }, attribute_);
}

const RTCStatsMemberInterface* Attribute::member_ptr() const {
  return absl::visit(
      [](const auto* attr) {
        return static_cast<const RTCStatsMemberInterface*>(attr);
      },
      attribute_);
}

bool Attribute::is_sequence() const {
  return absl::visit(VisitIsSequence(), attribute_);
}

bool Attribute::is_string() const {
  return absl::holds_alternative<const RTCStatsMember<std::string>*>(
      attribute_);
}

bool Attribute::is_defined() const {
  return absl::visit([](const auto* attr) { return attr->is_defined(); },
                     attribute_);
}

std::string Attribute::ValueToString() const {
  return absl::visit([](const auto* attr) { return attr->ValueToString(); },
                     attribute_);
}

std::string Attribute::ValueToJson() const {
  return absl::visit([](const auto* attr) { return attr->ValueToJson(); },
                     attribute_);
}

bool Attribute::IsEqual(const RTCStatsMemberInterface& other) const {
  return absl::visit(VisitIsEqual{.other = *other.member_ptr()}, attribute_);
}

}  // namespace webrtc
