// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JXL_TESTDATA_H_
#define LIB_JXL_TESTDATA_H_

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <memory>
#include <string>

#include "lib/jxl/base/file_io.h"
#include "lib/jxl/common.h"

#if !defined(TEST_DATA_PATH)
#include "tools/cpp/runfiles/runfiles.h"
#endif

namespace jxl {

namespace {
#if defined(TEST_DATA_PATH)
std::string GetPath(const std::string& filename) {
  return std::string(TEST_DATA_PATH "/") + filename;
}
#else
using bazel::tools::cpp::runfiles::Runfiles;
const std::unique_ptr<Runfiles> kRunfiles(Runfiles::Create(""));
std::string GetPath(const std::string& filename) {
  return kRunfiles->Rlocation("__main__/testdata/" + filename);
}
#endif

}  // namespace

static inline PaddedBytes ReadTestData(const std::string& filename) {
  std::string full_path = GetPath(filename);
  PaddedBytes data;
  JXL_CHECK(ReadFile(full_path, &data));
  printf("Test data %s is %d bytes long.\n", filename.c_str(),
         static_cast<int>(data.size()));
  return data;
}

}  // namespace jxl

#endif  // LIB_JXL_TESTDATA_H_
