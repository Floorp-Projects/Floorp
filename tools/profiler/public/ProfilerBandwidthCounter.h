/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ProfilerBandwidthCounter_h
#define ProfilerBandwidthCounter_h

#ifndef MOZ_GECKO_PROFILER

namespace mozilla {

inline void profiler_count_bandwidth_read_bytes(int64_t aCount) {}
inline void profiler_count_bandwidth_written_bytes(int64_t aCount) {}

}  // namespace mozilla

#else

#  include "mozilla/ProfilerMarkers.h"
#  include "mozilla/ProfilerCounts.h"

class ProfilerBandwidthCounter final : public BaseProfilerCount {
 public:
  ProfilerBandwidthCounter()
      : BaseProfilerCount("bandwidth", &mCounter, &mNumber, "Bandwidth",
                          "Amount of data transfered") {
    Register();
  }

  void Register() {
    profiler_add_sampled_counter(this);
    mRegistered = true;
  }

  bool IsRegistered() { return mRegistered; }
  void MarkUnregistered() { mRegistered = false; }

  void Add(int64_t aNumber) {
    if (!mRegistered) {
      Register();
    }
    mCounter += aNumber;
    mNumber++;
  }

  ProfilerAtomicSigned mCounter;
  ProfilerAtomicUnsigned mNumber;
  bool mRegistered;
};

namespace geckoprofiler::markers {

using namespace mozilla;

struct NetworkIOMarker {
  static constexpr Span<const char> MarkerTypeName() {
    return MakeStringSpan("NetIO");
  }
  static void StreamJSONMarkerData(baseprofiler::SpliceableJSONWriter& aWriter,
                                   int64_t aRead, int64_t aWritten) {
    if (aRead) {
      aWriter.IntProperty("read", aRead);
    }
    if (aWritten) {
      aWriter.IntProperty("written", aWritten);
    }
  }

  static MarkerSchema MarkerTypeDisplay() {
    using MS = MarkerSchema;
    MS schema{MS::Location::MarkerChart, MS::Location::MarkerTable};

    schema.AddKeyLabelFormat("read", "Read", MS::Format::Bytes);
    schema.AddKeyLabelFormat("written", "Written", MS::Format::Bytes);

    return schema;
  }
};

}  // namespace geckoprofiler::markers

void profiler_count_bandwidth_bytes(int64_t aCount);

namespace mozilla {

inline void profiler_count_bandwidth_read_bytes(int64_t aCount) {
  if (MOZ_UNLIKELY(profiler_feature_active(ProfilerFeature::Bandwidth))) {
    profiler_count_bandwidth_bytes(aCount);
  }
  // This marker will appear on the Socket Thread.
  PROFILER_MARKER("Read", NETWORK, {}, NetworkIOMarker, aCount, 0);
}

inline void profiler_count_bandwidth_written_bytes(int64_t aCount) {
  if (MOZ_UNLIKELY(profiler_feature_active(ProfilerFeature::Bandwidth))) {
    profiler_count_bandwidth_bytes(aCount);
  }
  // This marker will appear on the Socket Thread.
  PROFILER_MARKER("Write", NETWORK, {}, NetworkIOMarker, 0, aCount);
}

}  // namespace mozilla

#endif  // !MOZ_GECKO_PROFILER

#endif  // ProfilerBandwidthCounter_h
