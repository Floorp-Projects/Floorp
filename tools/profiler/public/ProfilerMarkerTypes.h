/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ProfilerMarkerTypes_h
#define ProfilerMarkerTypes_h

// This header contains common marker type definitions that rely on xpcom.
//
// It #include's "mozilla/BaseProfilerMarkerTypess.h" and "ProfilerMarkers.h",
// see these files for more marker types, how to define other marker types, and
// how to add markers to the profiler buffers.

// !!!                       /!\ WORK IN PROGRESS /!\                       !!!
// This file contains draft marker definitions, but most are not used yet.
// Further work is needed to complete these definitions, and use them to convert
// existing PROFILER_ADD_MARKER calls. See meta bug 1661394.

#include "mozilla/BaseProfilerMarkerTypes.h"
#include "mozilla/ProfilerMarkers.h"
#include "js/ProfilingFrameIterator.h"
#include "js/Utility.h"
#include "mozilla/Preferences.h"
#include "mozilla/ServoTraversalStatistics.h"

namespace geckoprofiler::markers {

// Import some common markers from mozilla::baseprofiler::markers.
using MediaSampleMarker = mozilla::baseprofiler::markers::MediaSampleMarker;
using VideoFallingBehindMarker =
    mozilla::baseprofiler::markers::VideoFallingBehindMarker;
using ContentBuildMarker = mozilla::baseprofiler::markers::ContentBuildMarker;
using MediaEngineMarker = mozilla::baseprofiler::markers::MediaEngineMarker;
using MediaEngineTextMarker =
    mozilla::baseprofiler::markers::MediaEngineTextMarker;

}  // namespace geckoprofiler::markers

#endif  // ProfilerMarkerTypes_h
