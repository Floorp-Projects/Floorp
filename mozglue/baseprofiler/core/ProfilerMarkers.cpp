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

// The atomic sDeserializerCount still also include bits that act as a "RWLock":
// Whoever can set this bit gets exclusive access to the count and the whole
// sMarkerTypeFunctions1Based array, guaranteeing that it cannot be modified.
static constexpr DeserializerTagAtomic scExclusiveLock = 0x80'00'00'00u;
// Code that wants shared access can add this value, then ensure there is no
// exclusive lock, after which it's guaranteed that no exclusive lock can be
// taken until the shared lock count goes back to zero.
static constexpr DeserializerTagAtomic scSharedLockUnit = 0x00'01'00'00u;
// This mask isolates the actual count value from the lock bits.
static constexpr DeserializerTagAtomic scTagMask = 0x00'00'FF'FFu;

// Number of currently-registered deserializers and other marker type functions.
// The high bits contain lock bits, see above.
static Atomic<DeserializerTagAtomic, MemoryOrdering::ReleaseAcquire>
    sDeserializerCount{0};

// This needs to be big enough to handle all possible marker types. If one day
// this needs to be higher, the underlying DeserializerTag type will have to be
// changed.
static constexpr DeserializerTagAtomic DeserializerMax = 250;
static_assert(DeserializerMax <= scTagMask,
              "DeserializerMax doesn't fit in scTagMask");

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

  // Add a shared lock request, which will prevent future exclusive locking.
  DeserializerTagAtomic tagWithLock = (sDeserializerCount += scSharedLockUnit);

  // An exclusive locker may have arrived before us, just wait for it to finish.
  while ((tagWithLock & scExclusiveLock) != 0u) {
    tagWithLock = sDeserializerCount;
  }

  MOZ_ASSERT(
      // This is equivalent to shifting right to only keep the lock counts.
      tagWithLock / scSharedLockUnit <
          // This is effectively half of the permissible shared lock range,
          // that would mean way too many threads doing this work here!
          scExclusiveLock / scSharedLockUnit / 2,
      "The shared lock count is getting unexpectedly high, verify the "
      "algorithm, and tweak constants if needed");

  // Reserve a tag. Even if there are multiple shared-lock holders here, each
  // one will get a different value, and therefore will access a different part
  // of the sMarkerTypeFunctions1Based array.
  const DeserializerTagAtomic tag = ++sDeserializerCount & scTagMask;

  MOZ_RELEASE_ASSERT(
      tag <= DeserializerMax,
      "Too many deserializers, consider increasing DeserializerMax. "
      "Or is a deserializer stored again and again?");
  sMarkerTypeFunctions1Based[tag - 1] = {aDeserializer, aMarkerTypeNameFunction,
                                         aMarkerSchemaFunction};

  // And release our shared lock, to allow exclusive readers.
  sDeserializerCount -= scSharedLockUnit;

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

Streaming::LockedMarkerTypeFunctionsList::LockedMarkerTypeFunctionsList() {
  for (;;) {
    const DeserializerTagAtomic count = sDeserializerCount;
    if ((count & scTagMask) != count) {
      // Someone already has a lock, loop around.
      continue;
    }

    // There are currently no locks, try to add our exclusive lock.
    if (!sDeserializerCount.compareExchange(count, count | scExclusiveLock)) {
      // Someone else modified sDeserializerCount since our read, loop around.
      continue;
    }

    // We applied our exclusive lock, we can now read the list of functions,
    // without interference until ~LockedMarkerTypeFunctionsList().
    // (Note that sDeserializerCount may receive shared lock requests, but the
    // count won't change.)
    mMarkerTypeFunctionsSpan = {sMarkerTypeFunctions1Based, count};
    break;
  }
}

Streaming::LockedMarkerTypeFunctionsList::~LockedMarkerTypeFunctionsList() {
  MOZ_ASSERT(
      (sDeserializerCount & scExclusiveLock) == scExclusiveLock,
      "sDeserializerCount should still have the the exclusive lock bit set");
  MOZ_ASSERT(
      (sDeserializerCount & scTagMask) ==
          DeserializerTagAtomic(mMarkerTypeFunctionsSpan.size()),
      "sDeserializerCount should have the same count since construction");
  sDeserializerCount &= ~scExclusiveLock;
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

    if (!mGraphs.empty()) {
      aWriter.StartArrayProperty("graphs");
      {
        for (const GraphData& graph : mGraphs) {
          aWriter.StartObjectElement();
          {
            aWriter.StringProperty("key", graph.mKey);
            aWriter.StringProperty("type", GraphTypeToStringSpan(graph.mType));
            if (graph.mColor) {
              aWriter.StringProperty("color",
                                     GraphColorToStringSpan(*graph.mColor));
            }
          }
          aWriter.EndObject();
        }
      }
      aWriter.EndArray();
    }
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
    case Format::UniqueString:
      return mozilla::MakeStringSpan("unique-string");
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

/* static */
Span<const char> MarkerSchema::GraphTypeToStringSpan(
    MarkerSchema::GraphType aType) {
  switch (aType) {
    case GraphType::Line:
      return mozilla::MakeStringSpan("line");
    case GraphType::Bar:
      return mozilla::MakeStringSpan("bar");
    case GraphType::FilledLine:
      return mozilla::MakeStringSpan("line-filled");
    default:
      MOZ_CRASH("Unexpected GraphType enum");
      return {};
  }
}

/* static */
Span<const char> MarkerSchema::GraphColorToStringSpan(
    MarkerSchema::GraphColor aColor) {
  switch (aColor) {
    case GraphColor::Blue:
      return mozilla::MakeStringSpan("blue");
    case GraphColor::Green:
      return mozilla::MakeStringSpan("green");
    case GraphColor::Grey:
      return mozilla::MakeStringSpan("grey");
    case GraphColor::Ink:
      return mozilla::MakeStringSpan("ink");
    case GraphColor::Magenta:
      return mozilla::MakeStringSpan("magenta");
    case GraphColor::Orange:
      return mozilla::MakeStringSpan("orange");
    case GraphColor::Purple:
      return mozilla::MakeStringSpan("purple");
    case GraphColor::Red:
      return mozilla::MakeStringSpan("red");
    case GraphColor::Teal:
      return mozilla::MakeStringSpan("teal");
    case GraphColor::Yellow:
      return mozilla::MakeStringSpan("yellow");
    default:
      MOZ_CRASH("Unexpected GraphColor enum");
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
