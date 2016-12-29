/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_BASE_OPTIONAL_H_
#define WEBRTC_BASE_OPTIONAL_H_

#include <algorithm>
#include <utility>

#include "webrtc/base/checks.h"

namespace rtc {

// Simple std::experimental::optional-wannabe. It either contains a T or not.
// In order to keep the implementation simple and portable, this implementation
// actually contains a (default-constructed) T even when it supposedly doesn't
// contain a value; use e.g. rtc::scoped_ptr<T> instead if that's too
// expensive.
//
// A moved-from Optional<T> may only be destroyed, and assigned to if T allows
// being assigned to after having been moved from. Specifically, you may not
// assume that it just doesn't contain a value anymore.
//
// Examples of good places to use Optional:
//
// - As a class or struct member, when the member doesn't always have a value:
//     struct Prisoner {
//       std::string name;
//       Optional<int> cell_number;  // Empty if not currently incarcerated.
//     };
//
// - As a return value for functions that may fail to return a value on all
//   allowed inputs. For example, a function that searches an array might
//   return an Optional<size_t> (the index where it found the element, or
//   nothing if it didn't find it); and a function that parses numbers might
//   return Optional<double> (the parsed number, or nothing if parsing failed).
//
// Examples of bad places to use Optional:
//
// - As a return value for functions that may fail because of disallowed
//   inputs. For example, a string length function should not return
//   Optional<size_t> so that it can return nothing in case the caller passed
//   it a null pointer; the function should probably use RTC_[D]CHECK instead,
//   and return plain size_t.
//
// - As a return value for functions that may fail to return a value on all
//   allowed inputs, but need to tell the caller what went wrong. Returning
//   Optional<double> when parsing a single number as in the example above
//   might make sense, but any larger parse job is probably going to need to
//   tell the caller what the problem was, not just that there was one.
//
// TODO(kwiberg): Get rid of this class when the standard library has
// std::optional (and we're allowed to use it).
template <typename T>
class Optional final {
 public:
  // Construct an empty Optional.
  Optional() : has_value_(false) {}

  // Construct an Optional that contains a value.
  explicit Optional(const T& val) : value_(val), has_value_(true) {}
  explicit Optional(T&& val) : value_(std::move(val)), has_value_(true) {}

  // Copy and move constructors.
  // TODO(kwiberg): =default the move constructor when MSVC supports it.
  Optional(const Optional&) = default;
  Optional(Optional&& m)
      : value_(std::move(m.value_)), has_value_(m.has_value_) {}

  // Assignment.
  // TODO(kwiberg): =default the move assignment op when MSVC supports it.
  Optional& operator=(const Optional&) = default;
  Optional& operator=(Optional&& m) {
    value_ = std::move(m.value_);
    has_value_ = m.has_value_;
    return *this;
  }

  friend void swap(Optional& m1, Optional& m2) {
    using std::swap;
    swap(m1.value_, m2.value_);
    swap(m1.has_value_, m2.has_value_);
  }

  // Conversion to bool to test if we have a value.
  explicit operator bool() const { return has_value_; }

  // Dereferencing. Only allowed if we have a value.
  const T* operator->() const {
    RTC_DCHECK(has_value_);
    return &value_;
  }
  T* operator->() {
    RTC_DCHECK(has_value_);
    return &value_;
  }
  const T& operator*() const {
    RTC_DCHECK(has_value_);
    return value_;
  }
  T& operator*() {
    RTC_DCHECK(has_value_);
    return value_;
  }

  // Dereference with a default value in case we don't have a value.
  const T& value_or(const T& default_val) const {
    return has_value_ ? value_ : default_val;
  }

  // Equality tests. Two Optionals are equal if they contain equivalent values,
  // or
  // if they're both empty.
  friend bool operator==(const Optional& m1, const Optional& m2) {
    return m1.has_value_ && m2.has_value_ ? m1.value_ == m2.value_
                                          : m1.has_value_ == m2.has_value_;
  }
  friend bool operator!=(const Optional& m1, const Optional& m2) {
    return m1.has_value_ && m2.has_value_ ? m1.value_ != m2.value_
                                          : m1.has_value_ != m2.has_value_;
  }

 private:
  // Invariant: Unless *this has been moved from, value_ is default-initialized
  // (or copied or moved from a default-initialized T) if !has_value_.
  T value_;
  bool has_value_;
};

}  // namespace rtc

#endif  // WEBRTC_BASE_OPTIONAL_H_
