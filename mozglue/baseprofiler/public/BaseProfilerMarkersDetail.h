/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BaseProfilerMarkersDetail_h
#define BaseProfilerMarkersDetail_h

#ifndef BaseProfilerMarkers_h
#  error "This header should only be #included by BaseProfilerMarkers.h"
#endif

#include "mozilla/BaseProfilerMarkersPrerequisites.h"

#ifdef MOZ_GECKO_PROFILER

//                        ~~ HERE BE DRAGONS ~~
//
// Everything below is internal implementation detail, you shouldn't need to
// look at it unless working on the profiler code.

#  include <limits>

namespace mozilla::baseprofiler {
// Implemented in platform.cpp
MFBT_API ProfileChunkedBuffer& profiler_get_core_buffer();
}  // namespace mozilla::baseprofiler

namespace mozilla::base_profiler_markers_detail {

// Get the core buffer from the profiler, and cache it in a
// non-templated-function static reference.
inline ProfileChunkedBuffer& CachedBaseCoreBuffer() {
  static ProfileChunkedBuffer& coreBuffer =
      baseprofiler::profiler_get_core_buffer();
  return coreBuffer;
}

}  // namespace mozilla::base_profiler_markers_detail

namespace mozilla {

// ----------------------------------------------------------------------------
// Serializer, Deserializer: ProfilerStringView<CHAR>

// The serialization starts with a ULEB128 number that encodes both whether the
// ProfilerStringView is literal (Least Significant Bit = 0) or not (LSB = 1),
// plus the string length (excluding null terminator) in bytes, shifted left by
// 1 bit. Following that number:
// - If literal, the string pointer value.
// - If non-literal, the contents as bytes (excluding null terminator if any).
template <typename CHAR>
struct ProfileBufferEntryWriter::Serializer<ProfilerStringView<CHAR>> {
  static Length Bytes(const ProfilerStringView<CHAR>& aString) {
    MOZ_RELEASE_ASSERT(
        aString.Length() < std::numeric_limits<Length>::max() / 2,
        "Double the string length doesn't fit in Length type");
    const Length stringLength = static_cast<Length>(aString.Length());
    if (aString.IsLiteral()) {
      // Literal -> Length shifted left and LSB=0, then pointer.
      return ULEB128Size(stringLength << 1 | 0u) +
             static_cast<ProfileChunkedBuffer::Length>(sizeof(const CHAR*));
    }
    // Non-literal -> Length shifted left and LSB=1, then string size in bytes.
    return ULEB128Size((stringLength << 1) | 1u) + stringLength * sizeof(CHAR);
  }

  static void Write(ProfileBufferEntryWriter& aEW,
                    const ProfilerStringView<CHAR>& aString) {
    MOZ_RELEASE_ASSERT(
        aString.Length() < std::numeric_limits<Length>::max() / 2,
        "Double the string length doesn't fit in Length type");
    const Length stringLength = static_cast<Length>(aString.Length());
    if (aString.IsLiteral()) {
      // Literal -> Length shifted left and LSB=0, then pointer.
      aEW.WriteULEB128(stringLength << 1 | 0u);
      aEW.WriteObject(WrapProfileBufferRawPointer(aString.Data()));
      return;
    }
    // Non-literal -> Length shifted left and LSB=1, then string size in bytes.
    aEW.WriteULEB128(stringLength << 1 | 1u);
    aEW.WriteBytes(aString.Data(), stringLength * sizeof(CHAR));
  }
};

template <typename CHAR>
struct ProfileBufferEntryReader::Deserializer<ProfilerStringView<CHAR>> {
  static void ReadInto(ProfileBufferEntryReader& aER,
                       ProfilerStringView<CHAR>& aString) {
    const Length lengthAndIsLiteral = aER.ReadULEB128<Length>();
    const Length stringLength = lengthAndIsLiteral >> 1;
    if ((lengthAndIsLiteral & 1u) == 0u) {
      // LSB==0 -> Literal string, read the string pointer.
      aString.mStringView = std::basic_string_view<CHAR>(
          aER.ReadObject<const CHAR*>(), stringLength);
      aString.mOwnership = ProfilerStringView<CHAR>::Ownership::Literal;
      return;
    }
    // LSB==1 -> Not a literal string, allocate a buffer to store the string
    // (plus terminal, for safety), and give it to the ProfilerStringView; Note
    // that this is a secret use of ProfilerStringView, which is intended to
    // only be used between deserialization and JSON streaming.
    CHAR* buffer = new CHAR[stringLength + 1];
    aER.ReadBytes(buffer, stringLength * sizeof(CHAR));
    buffer[stringLength] = CHAR(0);
    aString.mStringView = std::basic_string_view<CHAR>(buffer, stringLength);
    aString.mOwnership =
        ProfilerStringView<CHAR>::Ownership::OwnedThroughStringView;
  }

  static ProfilerStringView<CHAR> Read(ProfileBufferEntryReader& aER) {
    const Length lengthAndIsLiteral = aER.ReadULEB128<Length>();
    const Length stringLength = lengthAndIsLiteral >> 1;
    if ((lengthAndIsLiteral & 1u) == 0u) {
      // LSB==0 -> Literal string, read the string pointer.
      return ProfilerStringView<CHAR>(
          aER.ReadObject<const CHAR*>(), stringLength,
          ProfilerStringView<CHAR>::Ownership::Literal);
    }
    // LSB==1 -> Not a literal string, allocate a buffer to store the string
    // (plus terminal, for safety), and give it to the ProfilerStringView; Note
    // that this is a secret use of ProfilerStringView, which is intended to
    // only be used between deserialization and JSON streaming.
    CHAR* buffer = new CHAR[stringLength + 1];
    aER.ReadBytes(buffer, stringLength * sizeof(CHAR));
    buffer[stringLength] = CHAR(0);
    return ProfilerStringView<CHAR>(
        buffer, stringLength,
        ProfilerStringView<CHAR>::Ownership::OwnedThroughStringView);
  }
};

// Serializer, Deserializer: MarkerCategory

// The serialization contains both category numbers encoded as ULEB128.
template <>
struct ProfileBufferEntryWriter::Serializer<MarkerCategory> {
  static Length Bytes(const MarkerCategory& aCategory) {
    return ULEB128Size(static_cast<uint32_t>(aCategory.CategoryPair())) +
           ULEB128Size(static_cast<uint32_t>(aCategory.Category()));
  }

  static void Write(ProfileBufferEntryWriter& aEW,
                    const MarkerCategory& aCategory) {
    aEW.WriteULEB128(static_cast<uint32_t>(aCategory.CategoryPair()));
    aEW.WriteULEB128(static_cast<uint32_t>(aCategory.Category()));
  }
};

template <>
struct ProfileBufferEntryReader::Deserializer<MarkerCategory> {
  static void ReadInto(ProfileBufferEntryReader& aER,
                       MarkerCategory& aCategory) {
    aCategory.mCategoryPair = static_cast<baseprofiler::ProfilingCategoryPair>(
        aER.ReadULEB128<uint32_t>());
    aCategory.mCategory = static_cast<baseprofiler::ProfilingCategory>(
        aER.ReadULEB128<uint32_t>());
  }

  static MarkerCategory Read(ProfileBufferEntryReader& aER) {
    return MarkerCategory(static_cast<baseprofiler::ProfilingCategoryPair>(
                              aER.ReadULEB128<uint32_t>()),
                          static_cast<baseprofiler::ProfilingCategory>(
                              aER.ReadULEB128<uint32_t>()));
  }
};

// ----------------------------------------------------------------------------
// Serializer, Deserializer: MarkerTiming

// The serialization starts with the marker phase, followed by one or two
// timestamps as needed.
template <>
struct ProfileBufferEntryWriter::Serializer<MarkerTiming> {
  static Length Bytes(const MarkerTiming& aTiming) {
    MOZ_ASSERT(!aTiming.IsUnspecified());
    const auto phase = aTiming.MarkerPhase();
    switch (phase) {
      case MarkerTiming::Phase::Instant:
        return SumBytes(phase, aTiming.StartTime());
      case MarkerTiming::Phase::Interval:
        return SumBytes(phase, aTiming.StartTime(), aTiming.EndTime());
      case MarkerTiming::Phase::IntervalStart:
        return SumBytes(phase, aTiming.StartTime());
      case MarkerTiming::Phase::IntervalEnd:
        return SumBytes(phase, aTiming.EndTime());
      default:
        MOZ_RELEASE_ASSERT(phase == MarkerTiming::Phase::Instant ||
                           phase == MarkerTiming::Phase::Interval ||
                           phase == MarkerTiming::Phase::IntervalStart ||
                           phase == MarkerTiming::Phase::IntervalEnd);
        return 0;  // Only to avoid build errors.
    }
  }

  static void Write(ProfileBufferEntryWriter& aEW,
                    const MarkerTiming& aTiming) {
    MOZ_ASSERT(!aTiming.IsUnspecified());
    const auto phase = aTiming.MarkerPhase();
    switch (phase) {
      case MarkerTiming::Phase::Instant:
        aEW.WriteObjects(phase, aTiming.StartTime());
        return;
      case MarkerTiming::Phase::Interval:
        aEW.WriteObjects(phase, aTiming.StartTime(), aTiming.EndTime());
        return;
      case MarkerTiming::Phase::IntervalStart:
        aEW.WriteObjects(phase, aTiming.StartTime());
        return;
      case MarkerTiming::Phase::IntervalEnd:
        aEW.WriteObjects(phase, aTiming.EndTime());
        return;
      default:
        MOZ_RELEASE_ASSERT(phase == MarkerTiming::Phase::Instant ||
                           phase == MarkerTiming::Phase::Interval ||
                           phase == MarkerTiming::Phase::IntervalStart ||
                           phase == MarkerTiming::Phase::IntervalEnd);
        return;
    }
  }
};

template <>
struct ProfileBufferEntryReader::Deserializer<MarkerTiming> {
  static void ReadInto(ProfileBufferEntryReader& aER, MarkerTiming& aTiming) {
    aTiming.mPhase = aER.ReadObject<MarkerTiming::Phase>();
    switch (aTiming.mPhase) {
      case MarkerTiming::Phase::Instant:
        aTiming.mStartTime = aER.ReadObject<TimeStamp>();
        aTiming.mEndTime = TimeStamp{};
        break;
      case MarkerTiming::Phase::Interval:
        aTiming.mStartTime = aER.ReadObject<TimeStamp>();
        aTiming.mEndTime = aER.ReadObject<TimeStamp>();
        break;
      case MarkerTiming::Phase::IntervalStart:
        aTiming.mStartTime = aER.ReadObject<TimeStamp>();
        aTiming.mEndTime = TimeStamp{};
        break;
      case MarkerTiming::Phase::IntervalEnd:
        aTiming.mStartTime = TimeStamp{};
        aTiming.mEndTime = aER.ReadObject<TimeStamp>();
        break;
      default:
        MOZ_RELEASE_ASSERT(aTiming.mPhase == MarkerTiming::Phase::Instant ||
                           aTiming.mPhase == MarkerTiming::Phase::Interval ||
                           aTiming.mPhase ==
                               MarkerTiming::Phase::IntervalStart ||
                           aTiming.mPhase == MarkerTiming::Phase::IntervalEnd);
        break;
    }
  }

  static MarkerTiming Read(ProfileBufferEntryReader& aER) {
    TimeStamp start;
    TimeStamp end;
    auto phase = aER.ReadObject<MarkerTiming::Phase>();
    switch (phase) {
      case MarkerTiming::Phase::Instant:
        start = aER.ReadObject<TimeStamp>();
        break;
      case MarkerTiming::Phase::Interval:
        start = aER.ReadObject<TimeStamp>();
        end = aER.ReadObject<TimeStamp>();
        break;
      case MarkerTiming::Phase::IntervalStart:
        start = aER.ReadObject<TimeStamp>();
        break;
      case MarkerTiming::Phase::IntervalEnd:
        end = aER.ReadObject<TimeStamp>();
        break;
      default:
        MOZ_RELEASE_ASSERT(phase == MarkerTiming::Phase::Instant ||
                           phase == MarkerTiming::Phase::Interval ||
                           phase == MarkerTiming::Phase::IntervalStart ||
                           phase == MarkerTiming::Phase::IntervalEnd);
        break;
    }
    return MarkerTiming(start, end, phase);
  }
};

}  // namespace mozilla

#endif  // MOZ_GECKO_PROFILER

#endif  // BaseProfilerMarkersDetail_h
