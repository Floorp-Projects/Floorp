/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ProfilerMarkersDetail_h
#define ProfilerMarkersDetail_h

#ifndef ProfilerMarkers_h
#  error "This header should only be #included by ProfilerMarkers.h"
#endif

#include "mozilla/ProfilerMarkersPrerequisites.h"

#ifdef MOZ_GECKO_PROFILER

//                        ~~ HERE BE DRAGONS ~~
//
// Everything below is internal implementation detail, you shouldn't need to
// look at it unless working on the profiler code.

// Header that specializes the (de)serializers for xpcom types.
#  include "mozilla/ProfileBufferEntrySerializationGeckoExtensions.h"

// Implemented in platform.cpp
mozilla::ProfileChunkedBuffer& profiler_get_core_buffer();

#endif  // MOZ_GECKO_PROFILER

#endif  // ProfilerMarkersDetail_h
