/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_STATS_RTC_STATS_H_
#define API_STATS_RTC_STATS_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "api/stats/attribute.h"
#include "api/stats/rtc_stats_member.h"
#include "api/units/timestamp.h"
#include "rtc_base/checks.h"
#include "rtc_base/system/rtc_export.h"
#include "rtc_base/system/rtc_export_template.h"

namespace webrtc {

// Abstract base class for RTCStats-derived dictionaries, see
// https://w3c.github.io/webrtc-stats/.
//
// All derived classes must have the following static variable defined:
//   static const char kType[];
// It is used as a unique class identifier and a string representation of the
// class type, see https://w3c.github.io/webrtc-stats/#rtcstatstype-str*.
// Use the `WEBRTC_RTCSTATS_IMPL` macro when implementing subclasses, see macro
// for details.
//
// Derived classes list their dictionary members, RTCStatsMember<T>, as public
// fields, allowing the following:
//
// RTCFooStats foo("fooId", Timestamp::Micros(GetCurrentTime()));
// foo.bar = 42;
// foo.baz = std::vector<std::string>();
// foo.baz->push_back("hello world");
// uint32_t x = *foo.bar;
//
// Pointers to all the members are available with `Members`, allowing iteration:
//
// for (const RTCStatsMemberInterface* member : foo.Members()) {
//   printf("%s = %s\n", member->name(), member->ValueToString().c_str());
// }
class RTC_EXPORT RTCStats {
 public:
  RTCStats(const std::string& id, Timestamp timestamp)
      : id_(id), timestamp_(timestamp) {}
  RTCStats(const RTCStats& other);
  virtual ~RTCStats();

  virtual std::unique_ptr<RTCStats> copy() const = 0;

  const std::string& id() const { return id_; }
  // Time relative to the UNIX epoch (Jan 1, 1970, UTC), in microseconds.
  Timestamp timestamp() const { return timestamp_; }

  // Returns the static member variable `kType` of the implementing class.
  virtual const char* type() const = 0;
  // Returns all attributes of this stats object, i.e. a list of its individual
  // metrics as viewed via the Attribute wrapper.
  std::vector<Attribute> Attributes() const;
  template <typename T>
  Attribute GetAttribute(const RTCStatsMember<T>& stat) const {
    for (const auto& attribute : Attributes()) {
      if (!attribute.holds_alternative<T>()) {
        continue;
      }
      if (absl::get<const RTCStatsMember<T>*>(attribute.as_variant()) ==
          &stat) {
        return attribute;
      }
    }
    RTC_CHECK_NOTREACHED();
  }
  // Returns Attributes() as `RTCStatsMemberInterface` pointers.
  // TODO(https://crbug.com/webrtc/15164): Update callers to use Attributes()
  // instead and delete this method as well as the RTCStatsMemberInterface.
  std::vector<const RTCStatsMemberInterface*> Members() const;
  // Checks if the two stats objects are of the same type and have the same
  // member values. Timestamps are not compared. These operators are exposed for
  // testing.
  bool operator==(const RTCStats& other) const;
  bool operator!=(const RTCStats& other) const;

  // Creates a JSON readable string representation of the stats
  // object, listing all of its members (names and values).
  std::string ToJson() const;

  // Downcasts the stats object to an `RTCStats` subclass `T`. DCHECKs that the
  // object is of type `T`.
  template <typename T>
  const T& cast_to() const {
    RTC_DCHECK_EQ(type(), T::kType);
    return static_cast<const T&>(*this);
  }

 protected:
  virtual std::vector<Attribute> AttributesImpl(
      size_t additional_capacity) const;

  std::string const id_;
  Timestamp timestamp_;

  // Because Members() return a raw pointers we need to cache attributes to
  // ensure the pointers are still valid after the method has returned. Mutable
  // to allow lazy instantiation the first time the method is called.
  // TODO(https://crbug.com/webrtc/15164): Migrate all uses of Members() to
  // Attributes() and delete Members() and `cached_attributes_`.
  mutable std::vector<Attribute> cached_attributes_;
};

// All `RTCStats` classes should use these macros.
// `WEBRTC_RTCSTATS_DECL` is placed in a public section of the class definition.
// `WEBRTC_RTCSTATS_IMPL` is placed outside the class definition (in a .cc).
//
// These macros declare (in _DECL) and define (in _IMPL) the static `kType` and
// overrides methods as required by subclasses of `RTCStats`: `copy`, `type` and
// `MembersOfThisObjectAndAncestors`. The |...| argument is a list of addresses
// to each member defined in the implementing class. The list must have at least
// one member.
//
// (Since class names need to be known to implement these methods this cannot be
// part of the base `RTCStats`. While these methods could be implemented using
// templates, that would only work for immediate subclasses. Subclasses of
// subclasses also have to override these methods, resulting in boilerplate
// code. Using a macro avoids this and works for any `RTCStats` class, including
// grandchildren.)
//
// Sample usage:
//
// rtcfoostats.h:
//   class RTCFooStats : public RTCStats {
//    public:
//     WEBRTC_RTCSTATS_DECL();
//
//     RTCFooStats(const std::string& id, Timestamp timestamp);
//
//     RTCStatsMember<int32_t> foo;
//     RTCStatsMember<int32_t> bar;
//   };
//
// rtcfoostats.cc:
//   WEBRTC_RTCSTATS_IMPL(RTCFooStats, RTCStats, "foo-stats"
//       &foo,
//       &bar);
//
//   RTCFooStats::RTCFooStats(const std::string& id, Timestamp timestamp)
//       : RTCStats(id, timestamp),
//         foo("foo"),
//         bar("bar") {
//   }
//
#define WEBRTC_RTCSTATS_DECL()                                              \
 protected:                                                                 \
  std::vector<webrtc::Attribute> AttributesImpl(size_t additional_capacity) \
      const override;                                                       \
                                                                            \
 public:                                                                    \
  static const char kType[];                                                \
                                                                            \
  std::unique_ptr<webrtc::RTCStats> copy() const override;                  \
  const char* type() const override

#define WEBRTC_RTCSTATS_IMPL(this_class, parent_class, type_str, ...)          \
  const char this_class::kType[] = type_str;                                   \
                                                                               \
  std::unique_ptr<webrtc::RTCStats> this_class::copy() const {                 \
    return std::make_unique<this_class>(*this);                                \
  }                                                                            \
                                                                               \
  const char* this_class::type() const {                                       \
    return this_class::kType;                                                  \
  }                                                                            \
                                                                               \
  std::vector<webrtc::Attribute> this_class::AttributesImpl(                   \
      size_t additional_capacity) const {                                      \
    const webrtc::RTCStatsMemberInterface* this_members[] = {__VA_ARGS__};     \
    size_t this_members_size = sizeof(this_members) / sizeof(this_members[0]); \
    std::vector<webrtc::Attribute> attributes =                                \
        parent_class::AttributesImpl(this_members_size + additional_capacity); \
    for (size_t i = 0; i < this_members_size; ++i) {                           \
      attributes.push_back(                                                    \
          webrtc::Attribute::FromMemberInterface(this_members[i]));            \
    }                                                                          \
    return attributes;                                                         \
  }

// A version of WEBRTC_RTCSTATS_IMPL() where "..." is omitted, used to avoid a
// compile error on windows. This is used if the stats dictionary does not
// declare any members of its own (but perhaps its parent dictionary does).
#define WEBRTC_RTCSTATS_IMPL_NO_MEMBERS(this_class, parent_class, type_str) \
  const char this_class::kType[] = type_str;                                \
                                                                            \
  std::unique_ptr<webrtc::RTCStats> this_class::copy() const {              \
    return std::make_unique<this_class>(*this);                             \
  }                                                                         \
                                                                            \
  const char* this_class::type() const {                                    \
    return this_class::kType;                                               \
  }                                                                         \
                                                                            \
  std::vector<webrtc::Attribute> this_class::AttributesImpl(                \
      size_t additional_capacity) const {                                   \
    return parent_class::AttributesImpl(0);                                 \
  }

}  // namespace webrtc

#endif  // API_STATS_RTC_STATS_H_
