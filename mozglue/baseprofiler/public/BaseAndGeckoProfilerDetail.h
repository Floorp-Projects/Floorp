/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Internal Base and Gecko Profiler utilities.
// It should declare or define things that are used in both profilers, but not
// needed outside of the profilers.
// In particular, it is *not* included in popular headers like BaseProfiler.h
// and GeckoProfiler.h, to avoid rebuilding the world when this is modified.

#ifndef BaseAndGeckoProfilerDetail_h
#define BaseAndGeckoProfilerDetail_h

namespace mozilla::profiler::detail {

;  // TODO Add APIs here.

}  // namespace mozilla::profiler::detail

#endif  // BaseAndGeckoProfilerDetail_h
