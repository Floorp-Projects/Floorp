/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/BaseProfilerMarkers.h"

#include "mozilla/Likely.h"

#include <limits>

namespace mozilla {
namespace base_profiler_markers_detail {

// We need an atomic type that can hold a `DeserializerTag`. (Atomic doesn't
// work with too-small types.)
using DeserializerTagAtomic = unsigned;

// Number of currently-registered deserializers.
static Atomic<DeserializerTagAtomic, MemoryOrdering::Relaxed>
    sDeserializerCount{0};

// This needs to be big enough to handle all possible sub-types of
// ProfilerMarkerPayload. If one day this needs to be higher, the underlying
// DeserializerTag type will have to be changed.
static constexpr DeserializerTagAtomic DeserializerMax = 250;

static_assert(
    DeserializerMax <= std::numeric_limits<Streaming::DeserializerTag>::max(),
    "The maximum number of deserializers must fit in the DeserializerTag type");

// Array of deserializer.
// 1-based, i.e.: [0] -> tag 1, [DeserializerMax - 1] -> tag DeserializerMax.
static Streaming::Deserializer sDeserializers1Based[DeserializerMax];

/* static */ Streaming::DeserializerTag Streaming::TagForDeserializer(
    Streaming::Deserializer aDeserializer) {
  MOZ_RELEASE_ASSERT(!!aDeserializer);

  DeserializerTagAtomic tag = ++sDeserializerCount;
  MOZ_RELEASE_ASSERT(
      tag <= DeserializerMax,
      "Too many deserializers, consider increasing DeserializerMax. "
      "Or is a deserializer stored again and again?");
  sDeserializers1Based[tag - 1] = aDeserializer;

  return static_cast<DeserializerTag>(tag);
}

/* static */ Streaming::Deserializer Streaming::DeserializerForTag(
    Streaming::DeserializerTag aTag) {
  MOZ_RELEASE_ASSERT(
      aTag > 0 && static_cast<DeserializerTagAtomic>(aTag) <=
                      static_cast<DeserializerTagAtomic>(sDeserializerCount),
      "Out-of-range tag value");
  return sDeserializers1Based[aTag - 1];
}

}  // namespace base_profiler_markers_detail
}  // namespace mozilla
