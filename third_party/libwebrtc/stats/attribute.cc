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
    if (!other.holds_alternative<T>()) {
      return false;
    }
    absl::optional<T> attribute_as_optional =
        attribute->has_value() ? absl::optional<T>(attribute->value())
                               : absl::nullopt;
    return attribute_as_optional == other.as_optional<T>();
  }

  const Attribute& other;
};

}  // namespace

const char* Attribute::name() const {
  return name_;
}

const Attribute::StatVariant& Attribute::as_variant() const {
  return attribute_;
}

bool Attribute::has_value() const {
  return absl::visit([](const auto* attr) { return attr->has_value(); },
                     attribute_);
}

bool Attribute::is_sequence() const {
  return absl::visit(VisitIsSequence(), attribute_);
}

bool Attribute::is_string() const {
  return absl::holds_alternative<const RTCStatsMember<std::string>*>(
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

bool Attribute::operator==(const Attribute& other) const {
  return absl::visit(VisitIsEqual{.other = other}, attribute_);
}

bool Attribute::operator!=(const Attribute& other) const {
  return !(*this == other);
}

AttributeInit::AttributeInit(const char* name,
                             const Attribute::StatVariant& variant)
    : name(name), variant(variant) {}

}  // namespace webrtc
