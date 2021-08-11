/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProfilerIOInterposeObserver.h"
#include "GeckoProfiler.h"

using namespace mozilla;

namespace geckoprofiler::markers {
struct FileIOMarker {
  static constexpr Span<const char> MarkerTypeName() {
    return MakeStringSpan("FileIO");
  }
  static void StreamJSONMarkerData(baseprofiler::SpliceableJSONWriter& aWriter,
                                   const ProfilerString8View& aOperation,
                                   const ProfilerString8View& aSource,
                                   const ProfilerString8View& aFilename,
                                   MarkerThreadId aOperationThreadId) {
    aWriter.StringProperty("operation", aOperation);
    aWriter.StringProperty("source", aSource);
    if (aFilename.Length() != 0) {
      aWriter.StringProperty("filename", aFilename);
    }
    if (!aOperationThreadId.IsUnspecified()) {
      // Tech note: If `ToNumber()` returns a uint64_t, the conversion to
      // int64_t is "implementation-defined" before C++20. This is acceptable
      // here, because this is a one-way conversion to a unique identifier
      // that's used to visually separate data by thread on the front-end.
      aWriter.IntProperty(
          "threadId",
          static_cast<int64_t>(aOperationThreadId.ThreadId().ToNumber()));
    }
  }
  static MarkerSchema MarkerTypeDisplay() {
    using MS = MarkerSchema;
    MS schema{MS::Location::markerChart, MS::Location::markerTable,
              MS::Location::timelineFileIO};
    schema.AddKeyLabelFormatSearchable("operation", "Operation",
                                       MS::Format::string,
                                       MS::Searchable::searchable);
    schema.AddKeyLabelFormatSearchable("source", "Source", MS::Format::string,
                                       MS::Searchable::searchable);
    schema.AddKeyLabelFormatSearchable("filename", "Filename",
                                       MS::Format::filePath,
                                       MS::Searchable::searchable);
    return schema;
  }
};
}  // namespace geckoprofiler::markers

static auto GetFilename(IOInterposeObserver::Observation& aObservation) {
  AUTO_PROFILER_STATS(IO_filename);
  constexpr size_t scExpectedMaxFilename = 512;
  nsAutoStringN<scExpectedMaxFilename> filename16;
  aObservation.Filename(filename16);
  nsAutoCStringN<scExpectedMaxFilename> filename8;
  if (!filename16.IsEmpty()) {
    CopyUTF16toUTF8(filename16, filename8);
  }
  return filename8;
}

void ProfilerIOInterposeObserver::Observe(Observation& aObservation) {
  if (profiler_is_locked_on_current_thread()) {
    // Don't observe I/Os originating from the profiler itself (when internally
    // locked) to avoid deadlocks when calling profiler functions.
    AUTO_PROFILER_STATS(IO_profiler_locked);
    return;
  }

  Maybe<uint32_t> maybeFeatures = profiler_features_if_active_and_unpaused();
  if (maybeFeatures.isNothing()) {
    return;
  }
  uint32_t features = *maybeFeatures;

  if (!profiler_can_accept_markers()) {
    return;
  }

  AUTO_PROFILER_LABEL("ProfilerIOInterposeObserver", PROFILER);
  const bool doCaptureStack = !(features & ProfilerFeature::NoIOStacks);
  if (IsMainThread()) {
    // This is the main thread.
    // Capture a marker if any "IO" feature is on.
    // If it's not being profiled, we have nowhere to store FileIO markers.
    if (!profiler_thread_is_being_profiled() ||
        !(features & ProfilerFeature::MainThreadIO)) {
      return;
    }
    AUTO_PROFILER_STATS(IO_MT);
    nsAutoCString type{aObservation.FileType()};
    type.AppendLiteral("IO");

    // Store the marker in the current thread.
    PROFILER_MARKER(
        type, OTHER,
        MarkerOptions(
            MarkerTiming::Interval(aObservation.Start(), aObservation.End()),
            MarkerStack::MaybeCapture(doCaptureStack)),
        FileIOMarker,
        // aOperation
        ProfilerString8View::WrapNullTerminatedString(
            aObservation.ObservedOperationString()),
        // aSource
        ProfilerString8View::WrapNullTerminatedString(aObservation.Reference()),
        // aFilename
        GetFilename(aObservation),
        // aOperationThreadId - Do not include a thread ID, as it's the same as
        // the markers. Only include this field when the marker is being sent
        // from another thread.
        MarkerThreadId{});

  } else if (profiler_thread_is_being_profiled()) {
    // This is a non-main thread that is being profiled.
    if (!(features & ProfilerFeature::FileIO)) {
      return;
    }
    AUTO_PROFILER_STATS(IO_off_MT);

    nsAutoCString type{aObservation.FileType()};
    type.AppendLiteral("IO");

    // Share a backtrace between the marker on this thread, and the marker on
    // the main thread.
    UniquePtr<ProfileChunkedBuffer> backtrace =
        doCaptureStack ? profiler_capture_backtrace() : nullptr;

    // Store the marker in the current thread.
    PROFILER_MARKER(
        type, OTHER,
        MarkerOptions(
            MarkerTiming::Interval(aObservation.Start(), aObservation.End()),
            backtrace ? MarkerStack::UseBacktrace(*backtrace)
                      : MarkerStack::NoStack()),
        FileIOMarker,
        // aOperation
        ProfilerString8View::WrapNullTerminatedString(
            aObservation.ObservedOperationString()),
        // aSource
        ProfilerString8View::WrapNullTerminatedString(aObservation.Reference()),
        // aFilename
        GetFilename(aObservation),
        // aOperationThreadId - Do not include a thread ID, as it's the same as
        // the markers. Only include this field when the marker is being sent
        // from another thread.
        MarkerThreadId{});

    // Store the marker in the main thread as well, with a distinct marker name
    // and thread id.
    type.AppendLiteral(" (non-main thread)");
    PROFILER_MARKER(
        type, OTHER,
        MarkerOptions(
            MarkerTiming::Interval(aObservation.Start(), aObservation.End()),
            backtrace ? MarkerStack::UseBacktrace(*backtrace)
                      : MarkerStack::NoStack(),
            // This is the important piece that changed.
            // It will send a marker to the main thread.
            MarkerThreadId::MainThread()),
        FileIOMarker,
        // aOperation
        ProfilerString8View::WrapNullTerminatedString(
            aObservation.ObservedOperationString()),
        // aSource
        ProfilerString8View::WrapNullTerminatedString(aObservation.Reference()),
        // aFilename
        GetFilename(aObservation),
        // aOperationThreadId - Include the thread ID in the payload.
        MarkerThreadId::CurrentThread());

  } else {
    // This is a thread that is not being profiled. We still want to capture
    // file I/Os (to the main thread) if the "FileIOAll" feature is on.
    if (!(features & ProfilerFeature::FileIOAll)) {
      return;
    }
    AUTO_PROFILER_STATS(IO_other);
    nsAutoCString type{aObservation.FileType()};
    if (profiler_is_active_and_thread_is_registered()) {
      type.AppendLiteral("IO (non-profiled thread)");
    } else {
      type.AppendLiteral("IO (unregistered thread)");
    }

    // Only store this marker on the main thread, as this thread was not being
    // profiled.
    PROFILER_MARKER(
        type, OTHER,
        MarkerOptions(
            MarkerTiming::Interval(aObservation.Start(), aObservation.End()),
            doCaptureStack ? MarkerStack::Capture() : MarkerStack::NoStack(),
            // Store this marker on the main thread.
            MarkerThreadId::MainThread()),
        FileIOMarker,
        // aOperation
        ProfilerString8View::WrapNullTerminatedString(
            aObservation.ObservedOperationString()),
        // aSource
        ProfilerString8View::WrapNullTerminatedString(aObservation.Reference()),
        // aFilename
        GetFilename(aObservation),
        // aOperationThreadId - Note which thread this marker is coming from.
        MarkerThreadId::CurrentThread());
  }
}
