/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ProfilerThreadPlatformData_h
#define ProfilerThreadPlatformData_h

#include "mozilla/ProfilerUtils.h"

namespace mozilla::profiler {

class PlatformData {
  // TODO in bug 1722261: Platform-specific implementations.

 public:
  explicit PlatformData(ProfilerThreadId aThreadId) {}
};

}  // namespace mozilla::profiler

#endif  // ProfilerThreadPlatformData_h
