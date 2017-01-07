/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// IWYU pragma: private, include "GeckoProfiler.h"

#ifndef PROFILER_TYPES_H
#define PROFILER_TYPES_H

#include "mozilla/UniquePtr.h"

class ProfilerBacktrace;

struct ProfilerBacktraceDestructor
{
  void operator()(ProfilerBacktrace*);
};
using UniqueProfilerBacktrace =
  mozilla::UniquePtr<ProfilerBacktrace, ProfilerBacktraceDestructor>;

#endif // PROFILER_TYPES_H
