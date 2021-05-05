// Copyright (c) the JPEG XL Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "lib/jxl/base/time.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <ctime>

#include "lib/jxl/base/os_macros.h"  // for JXL_OS_*

#if JXL_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif  // NOMINMAX
#include <windows.h>
#endif  // JXL_OS_WIN

#if JXL_OS_MAC
#include <mach/mach.h>
#include <mach/mach_time.h>
#endif  // JXL_OS_MAC

#if JXL_OS_HAIKU
#include <OS.h>
#endif  // JXL_OS_HAIKU

namespace jxl {

double Now() {
#if JXL_OS_WIN
  LARGE_INTEGER counter;
  (void)QueryPerformanceCounter(&counter);
  LARGE_INTEGER freq;
  (void)QueryPerformanceFrequency(&freq);
  return double(counter.QuadPart) / freq.QuadPart;
#elif JXL_OS_MAC
  const auto t = mach_absolute_time();
  // On OSX/iOS platform the elapsed time is cpu time unit
  // We have to query the time base information to convert it back
  // See https://developer.apple.com/library/mac/qa/qa1398/_index.html
  static mach_timebase_info_data_t timebase;
  if (timebase.denom == 0) {
    (void)mach_timebase_info(&timebase);
  }
  return double(t) * timebase.numer / timebase.denom * 1E-9;
#elif JXL_OS_HAIKU
  return double(system_time_nsecs()) * 1E-9;
#else
  timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec + t.tv_nsec * 1E-9;
#endif
}

}  // namespace jxl
