/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Markers are useful to delimit something important happening such as the first
// paint. Unlike labels, which are only recorded in the profile buffer if a
// sample is collected while the label is on the label stack, markers will
// always be recorded in the profile buffer.
//
// This header contains definitions necessary to add markers to the Gecko
// Profiler buffer.
//
// It #include's "mozilla/BaseProfilerMarkers.h", see that header for base
// definitions necessary to create marker types.
//
// If common marker types are needed, #include "ProfilerMarkerTypes.h" instead.
//
// But if you want to create your own marker type locally, you can #include this
// header only; look at ProfilerMarkerTypes.h for examples of how to define
// types.
//
// To then record markers:
// - Use `baseprofiler::AddMarker(...)` from mozglue or other libraries that are
//   outside of xul, especially if they may happen outside of xpcom's lifetime
//   (typically startup, shutdown, or tests).
// - Otherwise #include "ProfilerMarkers.h" instead, and use
//   `profiler_add_marker(...)`.
// See these functions for more details.

#ifndef ProfilerMarkers_h
#define ProfilerMarkers_h

#include "mozilla/BaseProfilerMarkers.h"
#include "mozilla/ProfilerMarkersDetail.h"
#include "mozilla/ProfilerLabels.h"
#include "nsJSUtils.h"  // for nsJSUtils::GetCurrentlyRunningCodeInnerWindowID

class nsIDocShell;

namespace geckoprofiler::markers::detail {
// Please do not use anything from the detail namespace outside the profiler.

#ifdef MOZ_GECKO_PROFILER
mozilla::Maybe<uint64_t> profiler_get_inner_window_id_from_docshell(
    nsIDocShell* aDocshell);
#else
inline mozilla::Maybe<uint64_t> profiler_get_inner_window_id_from_docshell(
    nsIDocShell* aDocshell) {
  return mozilla::Nothing();
}
#endif  // MOZ_GECKO_PROFILER

}  // namespace geckoprofiler::markers::detail

// This is a helper function to get the Inner Window ID from DocShell but it's
// not a recommended method to get it and it's not encouraged to use this
// function. If there is a computed inner window ID, `window`, or `Document`
// available in the call site, please use them. Use this function as a last
// resort.
inline mozilla::MarkerInnerWindowId MarkerInnerWindowIdFromDocShell(
    nsIDocShell* aDocshell) {
  mozilla::Maybe<uint64_t> id = geckoprofiler::markers::detail::
      profiler_get_inner_window_id_from_docshell(aDocshell);
  if (!id) {
    return mozilla::MarkerInnerWindowId::NoId();
  }
  return mozilla::MarkerInnerWindowId(*id);
}

// This is a helper function to get the Inner Window ID from a JS Context but
// it's not a recommended method to get it and it's not encouraged to use this
// function. If there is a computed inner window ID, `window`, or `Document`
// available in the call site, please use them. Use this function as a last
// resort.
inline mozilla::MarkerInnerWindowId MarkerInnerWindowIdFromJSContext(
    JSContext* aContext) {
  return mozilla::MarkerInnerWindowId(
      nsJSUtils::GetCurrentlyRunningCodeInnerWindowID(aContext));
}

// Bring category names from Base Profiler into the geckoprofiler::category
// namespace, for consistency with other Gecko Profiler identifiers.
namespace geckoprofiler::category {
using namespace ::mozilla::baseprofiler::category;
}

#ifdef MOZ_GECKO_PROFILER
// Forward-declaration. TODO: Move to more common header, see bug 1681416.
bool profiler_capture_backtrace_into(
    mozilla::ProfileChunkedBuffer& aChunkedBuffer,
    mozilla::StackCaptureOptions aCaptureOptions);

// Add a marker to a given buffer. `AddMarker()` and related macros should be
// used in most cases, see below for more information about them and the
// paramters; This function may be useful when markers need to be recorded in a
// local buffer outside of the main profiler buffer.
template <typename MarkerType, typename... PayloadArguments>
mozilla::ProfileBufferBlockIndex AddMarkerToBuffer(
    mozilla::ProfileChunkedBuffer& aBuffer,
    const mozilla::ProfilerString8View& aName,
    const mozilla::MarkerCategory& aCategory, mozilla::MarkerOptions&& aOptions,
    MarkerType aMarkerType, const PayloadArguments&... aPayloadArguments) {
  AUTO_PROFILER_LABEL("AddMarkerToBuffer", PROFILER);
  mozilla::Unused << aMarkerType;  // Only the empty object type is useful.
  return mozilla::base_profiler_markers_detail::AddMarkerToBuffer<MarkerType>(
      aBuffer, aName, aCategory, std::move(aOptions),
      profiler_active_without_feature(ProfilerFeature::NoMarkerStacks)
          ? ::profiler_capture_backtrace_into
          : nullptr,
      aPayloadArguments...);
}

// Add a marker (without payload) to a given buffer.
inline mozilla::ProfileBufferBlockIndex AddMarkerToBuffer(
    mozilla::ProfileChunkedBuffer& aBuffer,
    const mozilla::ProfilerString8View& aName,
    const mozilla::MarkerCategory& aCategory,
    mozilla::MarkerOptions&& aOptions = {}) {
  return AddMarkerToBuffer(aBuffer, aName, aCategory, std::move(aOptions),
                           mozilla::baseprofiler::markers::NoPayload{});
}
#endif

[[nodiscard]] inline bool profiler_thread_is_being_profiled_for_markers() {
  return profiler_thread_is_being_profiled(ThreadProfilingFeatures::Markers);
}

[[nodiscard]] inline bool profiler_thread_is_being_profiled_for_markers(
    const ProfilerThreadId& aThreadId) {
  return profiler_thread_is_being_profiled(aThreadId,
                                           ThreadProfilingFeatures::Markers);
}

// Add a marker to the Gecko Profiler buffer.
// - aName: Main name of this marker.
// - aCategory: Category for this marker.
// - aOptions: Optional settings (such as timing, inner window id,
//   backtrace...), see `MarkerOptions` for details.
// - aMarkerType: Empty object that specifies the type of marker.
// - aPayloadArguments: Arguments expected by this marker type's
// ` StreamJSONMarkerData` function.
template <typename MarkerType, typename... PayloadArguments>
mozilla::ProfileBufferBlockIndex profiler_add_marker_impl(
    const mozilla::ProfilerString8View& aName,
    const mozilla::MarkerCategory& aCategory, mozilla::MarkerOptions&& aOptions,
    MarkerType aMarkerType, const PayloadArguments&... aPayloadArguments) {
#ifndef MOZ_GECKO_PROFILER
  return {};
#else
  if (!profiler_thread_is_being_profiled_for_markers(
          aOptions.ThreadId().ThreadId())) {
    return {};
  }
  AUTO_PROFILER_LABEL("profiler_add_marker", PROFILER);
  return ::AddMarkerToBuffer(profiler_get_core_buffer(), aName, aCategory,
                             std::move(aOptions), aMarkerType,
                             aPayloadArguments...);
#endif
}

// Add a marker (without payload) to the Gecko Profiler buffer.
inline mozilla::ProfileBufferBlockIndex profiler_add_marker_impl(
    const mozilla::ProfilerString8View& aName,
    const mozilla::MarkerCategory& aCategory,
    mozilla::MarkerOptions&& aOptions = {}) {
  return profiler_add_marker_impl(aName, aCategory, std::move(aOptions),
                                  mozilla::baseprofiler::markers::NoPayload{});
}

// `profiler_add_marker` is a macro rather than a function so that arguments to
// it aren't unconditionally evaluated when not profiled. Some of the arguments
// might be non-trivial, see bug 1843534.
//
// The check used around `::profiler_add_marker_impl()` is a bit subtle.
// Naively, you might want to do
// `profiler_thread_is_being_profiled_for_markers()`, but markers can be
// targeted to different threads.
// So we do a cheaper `profiler_is_active_and_unpaused()` check instead to
// avoid any marker overhead when not profiling.
#define profiler_add_marker(...)               \
  do {                                         \
    if (profiler_is_active_and_unpaused()) {   \
      ::profiler_add_marker_impl(__VA_ARGS__); \
    }                                          \
  } while (false)

// Same as `profiler_add_marker()` (without payload). This macro is safe to use
// even if MOZ_GECKO_PROFILER is not #defined.
#define PROFILER_MARKER_UNTYPED(markerName, categoryName, ...)               \
  do {                                                                       \
    AUTO_PROFILER_STATS(PROFILER_MARKER_UNTYPED);                            \
    profiler_add_marker(markerName, ::geckoprofiler::category::categoryName, \
                        ##__VA_ARGS__);                                      \
  } while (false)

// Same as `profiler_add_marker()` (with payload). This macro is safe to use
// even if MOZ_GECKO_PROFILER is not #defined.
#define PROFILER_MARKER(markerName, categoryName, options, MarkerType, ...)  \
  do {                                                                       \
    AUTO_PROFILER_STATS(PROFILER_MARKER_with_##MarkerType);                  \
    profiler_add_marker(markerName, ::geckoprofiler::category::categoryName, \
                        options, ::geckoprofiler::markers::MarkerType{},     \
                        ##__VA_ARGS__);                                      \
  } while (false)

namespace geckoprofiler::markers {
// Most common marker types. Others are in ProfilerMarkerTypes.h.
using TextMarker = ::mozilla::baseprofiler::markers::TextMarker;
using Tracing = mozilla::baseprofiler::markers::Tracing;
}  // namespace geckoprofiler::markers

// Add a text marker. This macro is safe to use even if MOZ_GECKO_PROFILER is
// not #defined.
#define PROFILER_MARKER_TEXT(markerName, categoryName, options, text)        \
  do {                                                                       \
    AUTO_PROFILER_STATS(PROFILER_MARKER_TEXT);                               \
    profiler_add_marker(markerName, ::geckoprofiler::category::categoryName, \
                        options, ::geckoprofiler::markers::TextMarker{},     \
                        text);                                               \
  } while (false)

// RAII object that adds a PROFILER_MARKER_TEXT when destroyed; the marker's
// timing will be the interval from construction (unless an instant or start
// time is already specified in the provided options) until destruction.
class MOZ_RAII AutoProfilerTextMarker {
 public:
  AutoProfilerTextMarker(const char* aMarkerName,
                         const mozilla::MarkerCategory& aCategory,
                         mozilla::MarkerOptions&& aOptions,
                         const nsACString& aText)
      : mMarkerName(aMarkerName),
        mCategory(aCategory),
        mOptions(std::move(aOptions)),
        mText(aText) {
    MOZ_ASSERT(mOptions.Timing().EndTime().IsNull(),
               "AutoProfilerTextMarker options shouldn't have an end time");
    if (profiler_is_active_and_unpaused() &&
        mOptions.Timing().StartTime().IsNull()) {
      mOptions.Set(mozilla::MarkerTiming::InstantNow());
    }
  }

  ~AutoProfilerTextMarker() {
    if (profiler_is_active_and_unpaused()) {
      AUTO_PROFILER_LABEL("TextMarker", PROFILER);
      mOptions.TimingRef().SetIntervalEnd();
      AUTO_PROFILER_STATS(AUTO_PROFILER_MARKER_TEXT);
      profiler_add_marker(
          mozilla::ProfilerString8View::WrapNullTerminatedString(mMarkerName),
          mCategory, std::move(mOptions), geckoprofiler::markers::TextMarker{},
          mText);
    }
  }

 protected:
  const char* mMarkerName;
  mozilla::MarkerCategory mCategory;
  mozilla::MarkerOptions mOptions;
  nsCString mText;
};

// Creates an AutoProfilerTextMarker RAII object.  This macro is safe to use
// even if MOZ_GECKO_PROFILER is not #defined.
#define AUTO_PROFILER_MARKER_TEXT(markerName, categoryName, options, text)  \
  AutoProfilerTextMarker PROFILER_RAII(                                     \
      markerName, ::mozilla::baseprofiler::category::categoryName, options, \
      text)

class MOZ_RAII AutoProfilerTracing {
 public:
  AutoProfilerTracing(const char* aCategoryString, const char* aMarkerName,
                      mozilla::MarkerCategory aCategoryPair,
                      const mozilla::Maybe<uint64_t>& aInnerWindowID)
      : mCategoryString(aCategoryString),
        mMarkerName(aMarkerName),
        mCategoryPair(aCategoryPair),
        mInnerWindowID(aInnerWindowID) {
    profiler_add_marker(
        mozilla::ProfilerString8View::WrapNullTerminatedString(mMarkerName),
        mCategoryPair,
        {mozilla::MarkerTiming::IntervalStart(),
         mozilla::MarkerInnerWindowId(mInnerWindowID)},
        geckoprofiler::markers::Tracing{},
        mozilla::ProfilerString8View::WrapNullTerminatedString(
            mCategoryString));
  }

  AutoProfilerTracing(
      const char* aCategoryString, const char* aMarkerName,
      mozilla::MarkerCategory aCategoryPair,
      mozilla::UniquePtr<mozilla::ProfileChunkedBuffer> aBacktrace,
      const mozilla::Maybe<uint64_t>& aInnerWindowID)
      : mCategoryString(aCategoryString),
        mMarkerName(aMarkerName),
        mCategoryPair(aCategoryPair),
        mInnerWindowID(aInnerWindowID) {
    profiler_add_marker(
        mozilla::ProfilerString8View::WrapNullTerminatedString(mMarkerName),
        mCategoryPair,
        {mozilla::MarkerTiming::IntervalStart(),
         mozilla::MarkerInnerWindowId(mInnerWindowID),
         mozilla::MarkerStack::TakeBacktrace(std::move(aBacktrace))},
        geckoprofiler::markers::Tracing{},
        mozilla::ProfilerString8View::WrapNullTerminatedString(
            mCategoryString));
  }

  ~AutoProfilerTracing() {
    profiler_add_marker(
        mozilla::ProfilerString8View::WrapNullTerminatedString(mMarkerName),
        mCategoryPair,
        {mozilla::MarkerTiming::IntervalEnd(),
         mozilla::MarkerInnerWindowId(mInnerWindowID)},
        geckoprofiler::markers::Tracing{},
        mozilla::ProfilerString8View::WrapNullTerminatedString(
            mCategoryString));
  }

 protected:
  const char* mCategoryString;
  const char* mMarkerName;
  const mozilla::MarkerCategory mCategoryPair;
  const mozilla::Maybe<uint64_t> mInnerWindowID;
};

// Adds a START/END pair of tracing markers.
#define AUTO_PROFILER_TRACING_MARKER(categoryString, markerName, categoryPair) \
  AutoProfilerTracing PROFILER_RAII(categoryString, markerName,                \
                                    geckoprofiler::category::categoryPair,     \
                                    mozilla::Nothing())
#define AUTO_PROFILER_TRACING_MARKER_INNERWINDOWID(                        \
    categoryString, markerName, categoryPair, innerWindowId)               \
  AutoProfilerTracing PROFILER_RAII(categoryString, markerName,            \
                                    geckoprofiler::category::categoryPair, \
                                    mozilla::Some(innerWindowId))
#define AUTO_PROFILER_TRACING_MARKER_DOCSHELL(categoryString, markerName, \
                                              categoryPair, docShell)     \
  AutoProfilerTracing PROFILER_RAII(                                      \
      categoryString, markerName, geckoprofiler::category::categoryPair,  \
      geckoprofiler::markers::detail::                                    \
          profiler_get_inner_window_id_from_docshell(docShell))

#ifdef MOZ_GECKO_PROFILER
extern template mozilla::ProfileBufferBlockIndex AddMarkerToBuffer(
    mozilla::ProfileChunkedBuffer&, const mozilla::ProfilerString8View&,
    const mozilla::MarkerCategory&, mozilla::MarkerOptions&&,
    mozilla::baseprofiler::markers::NoPayload);

extern template mozilla::ProfileBufferBlockIndex AddMarkerToBuffer(
    mozilla::ProfileChunkedBuffer&, const mozilla::ProfilerString8View&,
    const mozilla::MarkerCategory&, mozilla::MarkerOptions&&,
    mozilla::baseprofiler::markers::TextMarker, const std::string&);

extern template mozilla::ProfileBufferBlockIndex profiler_add_marker_impl(
    const mozilla::ProfilerString8View&, const mozilla::MarkerCategory&,
    mozilla::MarkerOptions&&, mozilla::baseprofiler::markers::TextMarker,
    const std::string&);

extern template mozilla::ProfileBufferBlockIndex profiler_add_marker_impl(
    const mozilla::ProfilerString8View&, const mozilla::MarkerCategory&,
    mozilla::MarkerOptions&&, mozilla::baseprofiler::markers::TextMarker,
    const nsCString&);

extern template mozilla::ProfileBufferBlockIndex profiler_add_marker_impl(
    const mozilla::ProfilerString8View&, const mozilla::MarkerCategory&,
    mozilla::MarkerOptions&&, mozilla::baseprofiler::markers::Tracing,
    const mozilla::ProfilerString8View&);
#endif  // MOZ_GECKO_PROFILER

#endif  // ProfilerMarkers_h
