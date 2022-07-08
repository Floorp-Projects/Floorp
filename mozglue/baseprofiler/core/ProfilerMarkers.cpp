/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/BaseProfilerMarkers.h"

#include "mozilla/BaseProfilerUtils.h"

#include <limits>

namespace mozilla {
namespace base_profiler_markers_detail {

// We need an atomic type that can hold a `DeserializerTag`. (Atomic doesn't
// work with too-small types.)
using DeserializerTagAtomic = unsigned;

// Number of currently-registered deserializers and other marker type functions.
static Atomic<DeserializerTagAtomic, MemoryOrdering::Relaxed>
    sDeserializerCount{0};

// This needs to be big enough to handle all possible marker types. If one day
// this needs to be higher, the underlying DeserializerTag type will have to be
// changed.
static constexpr DeserializerTagAtomic DeserializerMax = 250;

static_assert(
    DeserializerMax <= std::numeric_limits<Streaming::DeserializerTag>::max(),
    "The maximum number of deserializers must fit in the DeserializerTag type");

// Array of marker type functions.
// 1-based, i.e.: [0] -> tag 1, [DeserializerMax - 1] -> tag DeserializerMax.
// Elements are added at the next available atomically-incremented
// `sDeserializerCount` (minus 1) whenever a new marker type is used in a
// Firefox session; the content is kept between profiler runs in that session.
// There is theoretically a race between the increment and the time the entry is
// fully written, but in practice all new elements are written (during
// profiling, using a marker type for the first time) long before they are read
// (after profiling is paused).
static Streaming::MarkerTypeFunctions
    sMarkerTypeFunctions1Based[DeserializerMax];

/* static */ Streaming::DeserializerTag Streaming::TagForMarkerTypeFunctions(
    Streaming::MarkerDataDeserializer aDeserializer,
    Streaming::MarkerTypeNameFunction aMarkerTypeNameFunction,
    Streaming::MarkerSchemaFunction aMarkerSchemaFunction) {
  MOZ_RELEASE_ASSERT(!!aDeserializer);
  MOZ_RELEASE_ASSERT(!!aMarkerTypeNameFunction);
  MOZ_RELEASE_ASSERT(!!aMarkerSchemaFunction);

  DeserializerTagAtomic tag = ++sDeserializerCount;
  MOZ_RELEASE_ASSERT(
      tag <= DeserializerMax,
      "Too many deserializers, consider increasing DeserializerMax. "
      "Or is a deserializer stored again and again?");
  sMarkerTypeFunctions1Based[tag - 1] = {aDeserializer, aMarkerTypeNameFunction,
                                         aMarkerSchemaFunction};

  return static_cast<DeserializerTag>(tag);
}

/* static */ Streaming::MarkerDataDeserializer Streaming::DeserializerForTag(
    Streaming::DeserializerTag aTag) {
  MOZ_RELEASE_ASSERT(
      aTag > 0 && static_cast<DeserializerTagAtomic>(aTag) <=
                      static_cast<DeserializerTagAtomic>(sDeserializerCount),
      "Out-of-range tag value");
  return sMarkerTypeFunctions1Based[aTag - 1].mMarkerDataDeserializer;
}

/* static */ Span<const Streaming::MarkerTypeFunctions>
Streaming::MarkerTypeFunctionsArray() {
  return {sMarkerTypeFunctions1Based, sDeserializerCount};
}

// Only accessed on the main thread.
// Both profilers (Base and Gecko) could be active at the same time, so keep a
// ref-count to only allocate at most one buffer at any time.
static int sBufferForMainThreadAddMarkerRefCount = 0;
static ProfileChunkedBuffer* sBufferForMainThreadAddMarker = nullptr;

ProfileChunkedBuffer* GetClearedBufferForMainThreadAddMarker() {
  if (!mozilla::baseprofiler::profiler_is_main_thread()) {
    return nullptr;
  }

  if (sBufferForMainThreadAddMarker) {
    MOZ_ASSERT(sBufferForMainThreadAddMarker->IsInSession(),
               "sBufferForMainThreadAddMarker should always be in-session");
    sBufferForMainThreadAddMarker->Clear();
    MOZ_ASSERT(
        sBufferForMainThreadAddMarker->IsInSession(),
        "Cleared sBufferForMainThreadAddMarker should still be in-session");
  }

  return sBufferForMainThreadAddMarker;
}

MFBT_API void EnsureBufferForMainThreadAddMarker() {
  if (!mozilla::baseprofiler::profiler_is_main_thread()) {
    return;
  }

  if (sBufferForMainThreadAddMarkerRefCount++ == 0) {
    // First `Ensure`, allocate the buffer.
    MOZ_ASSERT(!sBufferForMainThreadAddMarker);
    sBufferForMainThreadAddMarker = new ProfileChunkedBuffer(
        ProfileChunkedBuffer::ThreadSafety::WithoutMutex,
        MakeUnique<ProfileBufferChunkManagerSingle>(
            ProfileBufferChunkManager::scExpectedMaximumStackSize));
    MOZ_ASSERT(sBufferForMainThreadAddMarker);
    MOZ_ASSERT(sBufferForMainThreadAddMarker->IsInSession());
  }
}

MFBT_API void ReleaseBufferForMainThreadAddMarker() {
  if (!mozilla::baseprofiler::profiler_is_main_thread()) {
    return;
  }

  if (sBufferForMainThreadAddMarkerRefCount == 0) {
    // Unexpected Release! This should not normally happen, but it's harmless in
    // practice, it means the buffer is not alive anyway.
    return;
  }

  MOZ_ASSERT(sBufferForMainThreadAddMarker);
  MOZ_ASSERT(sBufferForMainThreadAddMarker->IsInSession());
  if (--sBufferForMainThreadAddMarkerRefCount == 0) {
    // Last `Release`, destroy the buffer.
    delete sBufferForMainThreadAddMarker;
    sBufferForMainThreadAddMarker = nullptr;
  }
}

}  // namespace base_profiler_markers_detail

void MarkerSchema::Stream(JSONWriter& aWriter,
                          const Span<const char>& aName) && {
  // The caller should have started a JSON array, in which we can add an object
  // that defines a marker schema.

  if (mLocations.empty()) {
    // SpecialFrontendLocation case, don't output anything for this type.
    return;
  }

  aWriter.StartObjectElement();
  {
    aWriter.StringProperty("name", aName);

    if (!mChartLabel.empty()) {
      aWriter.StringProperty("chartLabel", mChartLabel);
    }

    if (!mTooltipLabel.empty()) {
      aWriter.StringProperty("tooltipLabel", mTooltipLabel);
    }

    if (!mTableLabel.empty()) {
      aWriter.StringProperty("tableLabel", mTableLabel);
    }

    aWriter.StartArrayProperty("display");
    {
      for (Location location : mLocations) {
        aWriter.StringElement(LocationToStringSpan(location));
      }
    }
    aWriter.EndArray();

    aWriter.StartArrayProperty("data");
    {
      for (const DataRow& row : mData) {
        aWriter.StartObjectElement();
        {
          row.match(
              [&aWriter](const DynamicData& aData) {
                aWriter.StringProperty("key", aData.mKey);
                if (aData.mLabel) {
                  aWriter.StringProperty("label", *aData.mLabel);
                }
                aWriter.StringProperty("format",
                                       FormatToStringSpan(aData.mFormat));
                if (aData.mSearchable) {
                  aWriter.BoolProperty(
                      "searchable",
                      *aData.mSearchable == Searchable::Searchable);
                }
              },
              [&aWriter](const StaticData& aStaticData) {
                aWriter.StringProperty("label", aStaticData.mLabel);
                aWriter.StringProperty("value", aStaticData.mValue);
              });
        }
        aWriter.EndObject();
      }
    }
    aWriter.EndArray();
  }
  aWriter.EndObject();
}

/* static */
Span<const char> MarkerSchema::LocationToStringSpan(
    MarkerSchema::Location aLocation) {
  switch (aLocation) {
    case Location::MarkerChart:
      return mozilla::MakeStringSpan("marker-chart");
    case Location::MarkerTable:
      return mozilla::MakeStringSpan("marker-table");
    case Location::TimelineOverview:
      return mozilla::MakeStringSpan("timeline-overview");
    case Location::TimelineMemory:
      return mozilla::MakeStringSpan("timeline-memory");
    case Location::TimelineIPC:
      return mozilla::MakeStringSpan("timeline-ipc");
    case Location::TimelineFileIO:
      return mozilla::MakeStringSpan("timeline-fileio");
    case Location::StackChart:
      return mozilla::MakeStringSpan("stack-chart");
    default:
      MOZ_CRASH("Unexpected Location enum");
      return {};
  }
}

/* static */
Span<const char> MarkerSchema::FormatToStringSpan(
    MarkerSchema::Format aFormat) {
  switch (aFormat) {
    case Format::Url:
      return mozilla::MakeStringSpan("url");
    case Format::FilePath:
      return mozilla::MakeStringSpan("file-path");
    case Format::String:
      return mozilla::MakeStringSpan("string");
    case Format::Duration:
      return mozilla::MakeStringSpan("duration");
    case Format::Time:
      return mozilla::MakeStringSpan("time");
    case Format::Seconds:
      return mozilla::MakeStringSpan("seconds");
    case Format::Milliseconds:
      return mozilla::MakeStringSpan("milliseconds");
    case Format::Microseconds:
      return mozilla::MakeStringSpan("microseconds");
    case Format::Nanoseconds:
      return mozilla::MakeStringSpan("nanoseconds");
    case Format::Bytes:
      return mozilla::MakeStringSpan("bytes");
    case Format::Percentage:
      return mozilla::MakeStringSpan("percentage");
    case Format::Integer:
      return mozilla::MakeStringSpan("integer");
    case Format::Decimal:
      return mozilla::MakeStringSpan("decimal");
    default:
      MOZ_CRASH("Unexpected Format enum");
      return {};
  }
}

}  // namespace mozilla

namespace mozilla::baseprofiler {
template MFBT_API ProfileBufferBlockIndex AddMarker(const ProfilerString8View&,
                                                    const MarkerCategory&,
                                                    MarkerOptions&&,
                                                    markers::TextMarker,
                                                    const std::string&);

template MFBT_API ProfileBufferBlockIndex
AddMarkerToBuffer(ProfileChunkedBuffer&, const ProfilerString8View&,
                  const MarkerCategory&, MarkerOptions&&, markers::NoPayload);

template MFBT_API ProfileBufferBlockIndex AddMarkerToBuffer(
    ProfileChunkedBuffer&, const ProfilerString8View&, const MarkerCategory&,
    MarkerOptions&&, markers::TextMarker, const std::string&);
}  // namespace mozilla::baseprofiler
