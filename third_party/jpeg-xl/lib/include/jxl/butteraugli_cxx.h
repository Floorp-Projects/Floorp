// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

/// @addtogroup libjxl_butteraugli
/// @{
///
/// @file butteraugli_cxx.h
/// @brief C++ header-only helper for @ref butteraugli.h.
///
/// There's no binary library associated with the header since this is a header
/// only library.

#ifndef JXL_BUTTERAUGLI_CXX_H_
#define JXL_BUTTERAUGLI_CXX_H_

#include <memory>

#include "jxl/butteraugli.h"

#if !(defined(__cplusplus) || defined(c_plusplus))
#error "This a C++ only header. Use jxl/butteraugli.h from C sources."
#endif

/// Struct to call JxlButteraugliApiDestroy from the JxlButteraugliApiPtr
/// unique_ptr.
struct JxlButteraugliApiDestroyStruct {
  /// Calls @ref JxlButteraugliApiDestroy() on the passed api.
  void operator()(JxlButteraugliApi* api) { JxlButteraugliApiDestroy(api); }
};

/// std::unique_ptr<> type that calls JxlButteraugliApiDestroy() when releasing
/// the pointer.
///
/// Use this helper type from C++ sources to ensure the api is destroyed and
/// their internal resources released.
typedef std::unique_ptr<JxlButteraugliApi, JxlButteraugliApiDestroyStruct>
    JxlButteraugliApiPtr;

/// Struct to call JxlButteraugliResultDestroy from the JxlButteraugliResultPtr
/// unique_ptr.
struct JxlButteraugliResultDestroyStruct {
  /// Calls @ref JxlButteraugliResultDestroy() on the passed result object.
  void operator()(JxlButteraugliResult* result) {
    JxlButteraugliResultDestroy(result);
  }
};

/// std::unique_ptr<> type that calls JxlButteraugliResultDestroy() when
/// releasing the pointer.
///
/// Use this helper type from C++ sources to ensure the result object is
/// destroyed and their internal resources released.
typedef std::unique_ptr<JxlButteraugliResult, JxlButteraugliResultDestroyStruct>
    JxlButteraugliResultPtr;

#endif  // JXL_BUTTERAUGLI_CXX_H_

/// @}
