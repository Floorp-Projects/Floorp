/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ProfilerMarkers.h"

template mozilla::ProfileBufferBlockIndex AddMarkerToBuffer(
    mozilla::ProfileChunkedBuffer&, const mozilla::ProfilerString8View&,
    const mozilla::MarkerCategory&, mozilla::MarkerOptions&&,
    mozilla::baseprofiler::markers::NoPayload);

template mozilla::ProfileBufferBlockIndex AddMarkerToBuffer(
    mozilla::ProfileChunkedBuffer&, const mozilla::ProfilerString8View&,
    const mozilla::MarkerCategory&, mozilla::MarkerOptions&&,
    mozilla::baseprofiler::markers::TextMarker, const std::string&);

template mozilla::ProfileBufferBlockIndex profiler_add_marker_impl(
    const mozilla::ProfilerString8View&, const mozilla::MarkerCategory&,
    mozilla::MarkerOptions&&, mozilla::baseprofiler::markers::TextMarker,
    const std::string&);

template mozilla::ProfileBufferBlockIndex profiler_add_marker_impl(
    const mozilla::ProfilerString8View&, const mozilla::MarkerCategory&,
    mozilla::MarkerOptions&&, mozilla::baseprofiler::markers::TextMarker,
    const nsCString&);

template mozilla::ProfileBufferBlockIndex profiler_add_marker_impl(
    const mozilla::ProfilerString8View&, const mozilla::MarkerCategory&,
    mozilla::MarkerOptions&&, mozilla::baseprofiler::markers::Tracing,
    const mozilla::ProfilerString8View&);
