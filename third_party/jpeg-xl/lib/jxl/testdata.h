// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JXL_TESTDATA_H_
#define LIB_JXL_TESTDATA_H_

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <string>

#include "lib/jxl/base/file_io.h"

namespace jxl {

static inline PaddedBytes ReadTestData(const std::string& filename) {
  std::string full_path = std::string(TEST_DATA_PATH "/") + filename;
  PaddedBytes data;
  JXL_CHECK(ReadFile(full_path, &data));
  return data;
}

}  // namespace jxl

#endif  // LIB_JXL_TESTDATA_H_
