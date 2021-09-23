// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JXL_BASE_TIME_H_
#define LIB_JXL_BASE_TIME_H_

// OS-specific function for timing.

namespace jxl {

// Returns current time [seconds] from a monotonic clock with unspecified
// starting point - only suitable for computing elapsed time.
double Now();

}  // namespace jxl

#endif  // LIB_JXL_BASE_TIME_H_
