// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JXL_BASE_PROFILER_H_
#define LIB_JXL_BASE_PROFILER_H_

// High precision, low overhead time measurements. Returns exact call counts and
// total elapsed time for user-defined 'zones' (code regions, i.e. C++ scopes).
//
// To use the profiler you must set the JPEGXL_ENABLE_PROFILER CMake flag, which
// defines PROFILER_ENABLED and links against the libjxl_profiler library.

// If zero, this file has no effect and no measurements will be recorded.
#ifndef PROFILER_ENABLED
#define PROFILER_ENABLED 0
#endif  // PROFILER_ENABLED

#if PROFILER_ENABLED

#include "lib/profiler/profiler.h"

#else  // !PROFILER_ENABLED

#define PROFILER_ZONE(name)
#define PROFILER_FUNC
#define PROFILER_PRINT_RESULTS()

#endif  // PROFILER_ENABLED

#endif  // LIB_JXL_BASE_PROFILER_H_
