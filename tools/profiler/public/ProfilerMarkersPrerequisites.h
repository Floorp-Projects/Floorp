/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This header contains basic definitions required to create marker types, and
// to add markers to the profiler buffers.
//
// In most cases, #include "mozilla/ProfilerMarkers.h" instead, or
// #include "mozilla/ProfilerMarkerTypes.h" for common marker types.

#ifndef ProfilerMarkersPrerequisites_h
#define ProfilerMarkersPrerequisites_h

#include "mozilla/BaseProfilerMarkersPrerequisites.h"
#include "mozilla/ProfilerState.h"

#ifdef MOZ_GECKO_PROFILER

namespace geckoprofiler::markers {

// Default marker payload types, with no extra information, not even a marker
// type and payload. This is intended for label-only markers.
using NoPayload = ::mozilla::baseprofiler::markers::NoPayload;

}  // namespace geckoprofiler::markers

#endif  // MOZ_GECKO_PROFILER

#endif  // ProfilerMarkersPrerequisites_h
